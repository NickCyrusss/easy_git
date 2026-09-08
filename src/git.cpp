#include "git.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <filesystem>
#include <signal.h>
#include <spawn.h>
#include <sstream>
#include <stdexcept>
#include <sys/wait.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

extern char** environ;
namespace eg {
namespace {
constexpr size_t output_limit = 8 * 1024 * 1024;
std::string field(const std::string& bytes, size_t& pos) {
    if (pos >= bytes.size()) throw std::runtime_error("Incomplete Git output");
    auto end = bytes.find('\0', pos);
    if (end == std::string::npos) end = bytes.size();
    auto value = bytes.substr(pos, end - pos);
    pos = end + 1;
    return value;
}
std::string trim_line(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}
std::string read_output(FILE* stream, bool& truncated) {
    rewind(stream);
    std::string out;
    char buffer[8192];
    while (auto count = fread(buffer, 1, sizeof(buffer), stream)) {
        if (out.size() + count > output_limit) {
            out.append(buffer, output_limit - out.size());
            truncated = true;
            break;
        }
        out.append(buffer, count);
    }
    return out;
}
}

bool File::conflicted() const {
    return index == 'U' || worktree == 'U' ||
           (index == 'A' && worktree == 'A') || (index == 'D' && worktree == 'D');
}

Git::Git(std::string root, std::shared_ptr<std::atomic_bool> cancel)
    : root_(std::move(root)), cancel_(std::move(cancel)) {}

Result Git::run(const std::vector<std::string>& args) const {
    // No shell: filenames, branch names and commit messages stay literal arguments.
    std::vector<std::string> words = {"git", "--no-pager", "--literal-pathspecs", "-c",
        "color.ui=false", "-c", "core.quotepath=false", "-C", root_};
    words.insert(words.end(), args.begin(), args.end());
    std::vector<std::string> env;
    for (char** e = environ; *e; ++e) {
        std::string s = *e;
        auto key = s.substr(0, s.find('='));
        if (key != "GIT_TERMINAL_PROMPT" && key != "GIT_ASKPASS" && key != "SSH_ASKPASS" &&
            key != "GIT_DIR" && key != "GIT_WORK_TREE" && key != "GIT_INDEX_FILE" &&
            key != "GIT_COMMON_DIR" && key != "GIT_EDITOR" && key != "GIT_SEQUENCE_EDITOR") env.push_back(s);
    }
    env.insert(env.end(), {"GIT_TERMINAL_PROMPT=0", "GIT_ASKPASS=/bin/false",
        "SSH_ASKPASS=/bin/false", "GIT_EDITOR=true", "GIT_SEQUENCE_EDITOR=true"});
    std::vector<char*> envp;
    for (auto& e : env) envp.push_back(e.data());
    envp.push_back(nullptr);

    return run_process(std::move(words), cancel_, 120, envp.data());
}

Result run_process(std::vector<std::string> words, const std::shared_ptr<std::atomic_bool>& cancel,
                   int timeout_seconds, char* const* environment) {
    if (words.empty() || words.front().empty()) throw std::runtime_error("Missing executable");
    std::vector<char*> argv;
    for (auto& word : words) {
        if (word.find('\0') != std::string::npos) throw std::runtime_error("NUL in process argument");
        argv.push_back(word.data());
    }
    argv.push_back(nullptr);
    using Stream = std::unique_ptr<FILE, decltype(&fclose)>;
    Stream out(tmpfile(), fclose), err(tmpfile(), fclose);
    if (!out || !err) throw std::runtime_error("Cannot create process output files");
    fcntl(fileno(out.get()), F_SETFD, FD_CLOEXEC);
    fcntl(fileno(err.get()), F_SETFD, FD_CLOEXEC);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, fileno(out.get()), STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, fileno(err.get()), STDERR_FILENO);
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attr, 0);
    pid_t pid = 0;
    int spawn_error = posix_spawnp(&pid, words.front().c_str(), &actions, &attr, argv.data(), environment ? environment : environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    if (spawn_error) throw std::runtime_error(("Cannot start " + words.front() + ": ") + strerror(spawn_error));
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
    int status = 0;
    bool stopped = false, too_large = false;
    while (true) {
        auto state = waitpid(pid, &status, WNOHANG);
        if (state == pid) break;
        if (state < 0 && errno != EINTR) throw std::runtime_error("Cannot wait for child process");
        struct stat out_size{}, err_size{};
        fstat(fileno(out.get()), &out_size); fstat(fileno(err.get()), &err_size);
        too_large = out_size.st_size > off_t(output_limit) || err_size.st_size > off_t(output_limit);
        if (too_large || (cancel && *cancel) || (timeout_seconds > 0 && std::chrono::steady_clock::now() > deadline)) {
            kill(-pid, SIGTERM);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            kill(-pid, SIGKILL);
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
            stopped = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Result result;
    result.code = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
    result.out = read_output(out.get(), result.truncated);
    result.error = read_output(err.get(), result.truncated);
    if (stopped) {
        result.code = 124;
        result.error += too_large ? "\nProcess output exceeds 8 MiB. Narrow the query." :
            "\nProcess stopped (cancelled or timed out). Refresh to inspect repository state.";
    }
    return result;
}

std::string Git::checked(const std::vector<std::string>& args) const {
    auto r = run(args);
    if (r.code) throw std::runtime_error(r.error.empty() ? r.out : r.error);
    if (r.truncated) throw std::runtime_error("Git output exceeds 8 MiB. Narrow the query.");
    return r.out;
}

std::vector<File> parse_status(const std::string& bytes) {
    std::vector<File> files;
    size_t pos = 0;
    while (pos < bytes.size()) {
        auto line = field(bytes, pos);
        if (line.size() < 4 || line[2] != ' ') throw std::runtime_error("Invalid Git status record");
        File f{line.substr(3), {}, line[0], line[1]};
        if (f.index == 'R' || f.index == 'C' || f.worktree == 'R' || f.worktree == 'C')
            f.original = field(bytes, pos);
        files.push_back(std::move(f));
    }
    return files;
}

std::vector<Commit> parse_log(const std::string& bytes) {
    std::vector<Commit> commits;
    size_t pos = 0;
    while (pos < bytes.size()) {
        Commit c;
        c.id = field(bytes, pos);
        std::istringstream parents(field(bytes, pos));
        for (std::string p; parents >> p;) c.parents.push_back(p);
        c.author = field(bytes, pos);
        c.date = field(bytes, pos);
        c.subject = field(bytes, pos);
        c.refs = field(bytes, pos);
        commits.push_back(std::move(c));
    }
    return commits;
}

std::vector<GraphRow> layout_graph(const std::vector<Commit>& commits) {
    std::vector<std::string> lanes;
    std::vector<GraphRow> rows;
    // ponytail: scan active lanes; replace lookup with a map if thousands of parallel branches matter.
    for (const auto& c : commits) {
        GraphRow row;
        auto found = std::find(lanes.begin(), lanes.end(), c.id);
        if (found == lanes.end()) {
            auto empty = std::find(lanes.begin(), lanes.end(), "");
            row.lane = int(empty - lanes.begin());
            if (empty == lanes.end()) lanes.emplace_back();
        } else row.lane = int(found - lanes.begin());
        row.width = int(lanes.size());
        for (int i = 0; i < int(lanes.size()); ++i)
            if (!lanes[i].empty()) row.segments.push_back({i, i, i, true});
        lanes[row.lane].clear();
        for (int i = 0; i < int(lanes.size()); ++i)
            if (!lanes[i].empty()) row.segments.push_back({i, i, i, false});
        for (const auto& parent : c.parents) {
            auto target = std::find(lanes.begin(), lanes.end(), parent);
            int dest = int(target - lanes.begin());
            if (target == lanes.end()) {
                if (lanes[row.lane].empty()) dest = row.lane;
                else dest = int(std::find(lanes.begin(), lanes.end(), "") - lanes.begin());
                if (dest == int(lanes.size())) lanes.emplace_back();
                lanes[dest] = parent;
            }
            row.segments.push_back({row.lane, dest, dest, false});
        }
        row.width = std::max(row.width, int(lanes.size()));
        while (!lanes.empty() && lanes.back().empty()) lanes.pop_back();
        rows.push_back(std::move(row));
    }
    return rows;
}

Snapshot Git::load(int limit) const {
    Snapshot s;
    if (trim_line(checked({"rev-parse", "--is-inside-work-tree"})) != "true")
        throw std::runtime_error("Open a working repository; bare repositories are not supported.");
    s.root = trim_line(checked({"rev-parse", "--show-toplevel"}));
    // All file paths and subsequent operations are rooted at the worktree, even when opening a subfolder.
    Git repo(s.root, cancel_);
    s.operation = repo.operation_in_progress();
    s.has_head = repo.run({"rev-parse", "--verify", "HEAD"}).code == 0;
    auto branch = repo.run({"symbolic-ref", "--quiet", "--short", "HEAD"});
    s.branch = branch.code ? "Detached at " + trim_line(repo.checked({"rev-parse", "--short", "HEAD"}))
                           : trim_line(branch.out);
    s.files = parse_status(repo.checked({"status", "--porcelain=v1", "-z", "--untracked-files=all"}));
    auto refs = repo.checked({"for-each-ref", "--format=%(refname)%00%(refname:short)%00%(objectname)%00%(*objectname)",
        "refs/heads", "refs/remotes", "refs/tags"});
    std::istringstream lines(refs);
    for (std::string line; std::getline(lines, line);) {
        size_t pos = 0;
        Ref ref;
        ref.full = field(line, pos); ref.name = field(line, pos); ref.id = field(line, pos);
        if (pos < line.size()) { auto peeled = field(line, pos); if (!peeled.empty()) ref.id = peeled; }
        s.refs.push_back(std::move(ref));
    }
    if (s.has_head || !s.refs.empty()) {
        std::vector<std::string> args = {"log", "--topo-order", "--branches", "--remotes", "--tags", "-z",
            "--max-count=" + std::to_string(limit + 1), "--format=%H%x00%P%x00%an%x00%aI%x00%s%x00%D"};
        if (s.has_head) args.push_back("HEAD");
        args.push_back("--");
        s.commits = parse_log(repo.checked(args));
        s.more = int(s.commits.size()) > limit;
        if (s.more) s.commits.resize(limit);
    }
    auto stash = repo.checked({"stash", "list", "-z", "--format=%gd%x00%H%x00%s"});
    size_t pos = 0;
    while (pos < stash.size()) {
        Stash entry; entry.ref = field(stash,pos); entry.id = field(stash,pos); entry.subject = field(stash,pos);
        s.stashes.push_back(std::move(entry));
    }
    return s;
}

std::string Git::commit_detail(const std::string& id) const {
    return checked({"show", "--no-ext-diff", "--no-textconv", "--no-color", "--first-parent",
        "--format=fuller", "--stat", "--patch", id, "--"});
}
std::vector<File> parse_changed_files(const std::string& bytes) {
    std::vector<File> files;
    size_t pos = 0;
    while (pos < bytes.size()) {
        auto status = field(bytes, pos);
        if (status.empty() || std::string("ACDMRTUXB").find(status[0]) == std::string::npos)
            throw std::runtime_error("Invalid changed-file status");
        File file; file.index = status[0]; file.path = field(bytes, pos);
        if (file.index == 'R' || file.index == 'C') {
            file.original = std::move(file.path); file.path = field(bytes, pos);
        }
        files.push_back(std::move(file));
    }
    return files;
}
std::vector<File> Git::commit_files(const Commit& commit) const {
    std::vector<std::string> args = {"diff-tree", "--no-commit-id", "--root", "-r", "-M", "--name-status", "-z"};
    if (!commit.parents.empty()) args.push_back(commit.parents.front());
    args.insert(args.end(), {commit.id, "--"});
    return parse_changed_files(checked(args));
}
std::string Git::commit_diff(const Commit& commit, const File& file) const {
    std::vector<std::string> args;
    if (commit.parents.empty()) args = {"show", "--format=", commit.id};
    else args = {"diff", commit.parents.front(), commit.id};
    args.insert(args.end(), {"--no-ext-diff", "--no-textconv", "--no-color", "-M", "--", file.path});
    if (!file.original.empty()) args.push_back(file.original);
    return checked(args);
}
Commit Git::read_commit(const std::string& id) const {
    auto commits = parse_log(checked({"log", "-1", "-z", "--format=%H%x00%P%x00%an%x00%aI%x00%s%x00%D", id, "--"}));
    if (commits.size() != 1) throw std::runtime_error("Commit is no longer available. Refresh the repository.");
    return commits.front();
}
std::vector<File> Git::stash_files(const Stash& stash) const {
    auto commit = read_commit(stash.id);
    if (commit.parents.size() < 2) throw std::runtime_error("Invalid stash commit");
    auto files = commit_files(commit);
    // Git stores saved untracked files in the stash's third parent, a separate root commit.
    if (commit.parents.size() > 2) {
        auto untracked = commit_files(read_commit(commit.parents[2]));
        for (auto& file : untracked) { file.stash_untracked = true; files.push_back(std::move(file)); }
    }
    return files;
}
std::string Git::stash_diff(const Stash& stash, const File& file) const {
    Commit commit;
    commit.id = stash.id + (file.stash_untracked ? "^3" : "");
    if (!file.stash_untracked) commit.parents.push_back(stash.id + "^1");
    return commit_diff(commit,file);
}
void Git::apply_stash(const Stash& stash, bool restore_index) const {
    require_idle();
    std::vector<std::string> args = {"stash", "apply"};
    if (restore_index) args.push_back("--index");
    args.push_back(stash.id); // Pin the chosen object; a later push must not apply a different stash.
    checked(args);
}
void Git::delete_stash(const Stash& stash) const {
    if (stash.ref.rfind("stash@{",0) != 0 ||
        trim_line(checked({"rev-parse", "--verify", stash.ref})) != stash.id)
        throw std::runtime_error("The stash list changed. Refresh and select the stash again before deleting it.");
    checked({"stash", "drop", stash.ref});
}
void Git::save_stash(const std::string& message) const {
    require_idle();
    checked({"stash", "push", "--include-untracked", "-m", message.empty() ? "Saved from easy git" : message});
}
std::string Git::operation_in_progress() const {
    auto dir = std::filesystem::path(trim_line(checked({"rev-parse", "--absolute-git-dir"})));
    for (const auto& item : {std::pair<const char*,const char*>{"MERGE_HEAD","merge"},
        {"CHERRY_PICK_HEAD","cherry-pick"}, {"REVERT_HEAD","revert"}})
        if (std::filesystem::exists(dir / item.first)) return item.second;
    std::ifstream todo(dir / "sequencer/todo"); std::string action; todo >> action;
    if (action == "pick") return "cherry-pick";
    if (action == "revert") return "revert";
    if (std::filesystem::exists(dir / "rebase-merge") || std::filesystem::exists(dir / "rebase-apply")) return "rebase";
    return {};
}
void Git::require_idle() const {
    auto operation = operation_in_progress();
    if (!operation.empty()) throw std::runtime_error("Finish or abort the current " + operation + " operation first.");
}
namespace {
std::vector<std::string> replay_args(const char* operation, const Commit& commit, int mainline) {
    std::vector<std::string> args = {operation, "--no-edit"};
    if (commit.parents.size() > 1) {
        if (mainline < 1 || mainline > int(commit.parents.size()))
            throw std::runtime_error("Select a valid mainline parent for this merge commit.");
        args.insert(args.end(), {"-m", std::to_string(mainline)});
    }
    args.insert(args.end(), {"--", commit.id});
    return args;
}
}
void Git::cherry_pick(const Commit& commit, int mainline) const {
    require_idle(); checked(replay_args("cherry-pick",commit,mainline));
}
void Git::revert(const Commit& commit, int mainline) const {
    require_idle(); checked(replay_args("revert",commit,mainline));
}
void Git::merge(const std::string& id) const {
    require_idle(); checked({"merge", "--no-edit", "--ff", "--", id});
}
void Git::reset(const std::string& id, const std::string& mode) const {
    if (mode != "soft" && mode != "mixed" && mode != "hard") throw std::runtime_error("Invalid reset mode");
    require_idle(); checked({"reset", "--" + mode, id, "--"});
}
void Git::resolve_operation(const std::string& operation, const std::string& action) const {
    if ((operation != "merge" && operation != "cherry-pick" && operation != "revert" && operation != "rebase") ||
        (action != "continue" && action != "abort" && action != "skip") || (operation == "merge" && action == "skip"))
        throw std::runtime_error("Invalid operation action");
    if (operation_in_progress() != operation) throw std::runtime_error("The active operation changed. Refresh the repository.");
    checked({operation, "--" + action});
}
std::string Git::diff(const File& file, bool staged) const {
    if (file.index == '?' && !staged) {
        auto r = run({"diff", "--no-index", "--no-ext-diff", "--no-textconv", "--no-color", "--", "/dev/null", file.path});
        if (r.code > 1) throw std::runtime_error(r.error);
        if (r.truncated) r.out += "\n[Preview truncated at 8 MiB]";
        return r.out;
    }
    std::vector<std::string> args = {"diff", "--no-ext-diff", "--no-textconv", "--no-color"};
    if (staged) args.push_back("--cached");
    args.insert(args.end(), {"--", file.path});
    if (!file.original.empty()) args.push_back(file.original);
    auto text = checked(args);
    return text.empty() ? "No textual diff. This may be a mode change or a submodule change." : text;
}
void Git::stage(const File& file) const { checked({"add", "--", file.path}); }
void Git::discard(const File& file) const { discard_files({file}); }
void Git::discard_files(const std::vector<File>& selected) const {
    if (selected.empty()) throw std::runtime_error("Select files to discard.");
    // Validate the entire selection before touching any file; statuses may have changed since confirmation.
    auto files = parse_status(checked({"status", "--porcelain=v1", "-z", "--untracked-files=all"}));
    for (const auto& file : selected) {
        auto current = std::find_if(files.begin(),files.end(),[&](const auto& f) { return f.path == file.path; });
        if (current == files.end() || current->index != file.index || current->worktree != file.worktree ||
            current->original != file.original)
            throw std::runtime_error("File status changed: " + file.path + "\nRefresh and review the selection before discarding it.");
        if (!current->unstaged() || current->conflicted())
            throw std::runtime_error("Discard requires files with non-conflicting unstaged changes: " + file.path);
        auto path = std::filesystem::path(root_) / file.path;
        if (std::filesystem::is_directory(std::filesystem::symlink_status(path)))
            throw std::runtime_error("Discard directories and submodule changes inside their own repository or file manager: " + file.path);
    }
    for (const auto& file : selected) {
        try {
            if (file.index == '?') checked({"clean", "-f", "--", file.path});
            else checked({"restore", "--worktree", "--", file.path});
        } catch (const std::exception& e) {
            throw std::runtime_error("Discard stopped at " + file.path + ". Earlier files may already be discarded.\n" + e.what());
        }
    }
}
void Git::unstage(const File& file, bool has_head) const {
    std::vector<std::string> args = has_head ? std::vector<std::string>{"restore", "--staged", "--"}
                                            : std::vector<std::string>{"rm", "--cached", "-f", "--"};
    args.push_back(file.path);
    if (has_head && !file.original.empty()) args.push_back(file.original);
    checked(args);
}
void Git::commit(const std::string& message) const {
    if (message.find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::runtime_error("Enter a commit message.");
    checked({"commit", "-m", message});
}
}
