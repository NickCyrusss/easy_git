#include "git.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace eg {
namespace {
namespace fs = std::filesystem;
std::string trim(std::string s) { while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back(); return s; }
std::vector<std::string> lines(const std::string& text) {
    std::vector<std::string> result;
    for (size_t i = 0; i < text.size();) {
        auto end = text.find('\n',i); if (end == std::string::npos) end = text.size()-1;
        result.push_back(text.substr(i,end-i+1)); i = end+1;
    }
    return result;
}
std::string read_file(const fs::path& path) {
    if (fs::file_size(path) > 1024*1024) throw std::runtime_error("The editor supports files up to 1 MiB.");
    std::ifstream stream(path,std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot read file.");
    return {std::istreambuf_iterator<char>(stream),{}};
}
// Never follow a conflict path or its parents out of the working repository.
fs::path regular_path(const std::string& root,const std::string& name) {
    fs::path rel(name), base = fs::canonical(root);
    if (rel.empty() || rel.is_absolute()) throw std::runtime_error("Invalid file path.");
    for (const auto& part : rel) if (part == ".." || part == ".git") throw std::runtime_error("Invalid file path.");
    auto path = base / rel;
    auto parent = fs::weakly_canonical(path.parent_path()).lexically_relative(base);
    if (parent.empty() || *parent.begin() == "..") throw std::runtime_error("File path escapes the repository.");
    auto status = fs::symlink_status(path);
    if (fs::exists(status) && !fs::is_regular_file(status)) throw std::runtime_error("Use whole-file Git operations for symlinks and submodules.");
    return path;
}
struct TempFile {
    std::string path;
    explicit TempFile(const std::string& text,std::string pattern = "/tmp/easy-git-content-XXXXXX") : path(std::move(pattern)) {
        int fd = mkstemp(path.data()); if (fd < 0) throw std::runtime_error("Cannot create temporary file.");
        size_t at = 0; bool ok = true;
        while (at < text.size()) { auto n = write(fd,text.data()+at,text.size()-at); if (n <= 0) { ok = false; break; } at += size_t(n); }
        if (close(fd) != 0) ok = false;
        if (!ok) { unlink(path.c_str()); throw std::runtime_error("Cannot write temporary file."); }
    }
    ~TempFile() { unlink(path.c_str()); }
};
struct Entry { std::string mode, oid; };
Entry index_entry(const std::string& listing) {
    Entry e; std::istringstream stream(listing); stream >> e.mode >> e.oid; return e;
}
std::pair<std::string,std::string> remote_branch(const Git& git,const Ref& ref) {
    auto name = ref.full.substr(std::string("refs/remotes/").size());
    std::istringstream names(git.checked({"remote"})); std::string best;
    for (std::string remote; std::getline(names,remote);)
        if (name.rfind(remote+"/",0) == 0 && remote.size() > best.size()) best = remote;
    if (best.empty() || name.substr(best.size()+1) == "HEAD") throw std::runtime_error("Select a remote branch, not a symbolic remote HEAD.");
    return {best,"refs/heads/"+name.substr(best.size()+1)};
}
}
void Git::stage_lines(const File& file,bool unstage,const std::string& preview,const std::vector<int>& selected) const {
    if (file.conflicted() || !file.original.empty()) throw std::runtime_error("Resolve conflicts or stage renames as a whole file first.");
    auto path = regular_path(root_,file.path);
    auto signature = checked({"ls-files","--stage","-z","--",file.path});
    if (diff(file,unstage) != preview) throw std::runtime_error("The diff changed. Refresh and select lines again.");
    auto entry = index_entry(signature);
    if (!entry.mode.empty() && entry.mode != "100644" && entry.mode != "100755") throw std::runtime_error("Partial staging requires a regular text file.");
    auto baseline = entry.oid.empty() ? std::string() : checked({"cat-file","blob",entry.oid});
    if (baseline.find('\0') != std::string::npos || preview.find("GIT binary patch") != std::string::npos)
        throw std::runtime_error("Stage binary files as a whole file.");
    auto original = lines(baseline), patch = lines(preview);
    std::set<int> selection(selected.begin(),selected.end());
    if (selection.empty()) throw std::runtime_error("Select added or removed lines.");
    std::string result; size_t cursor = 0; bool hunk = false; int used = 0;
    auto append = [&](const std::string& line) {
        if (!result.empty() && result.back() != '\n' && !line.empty())
            throw std::runtime_error("This selection joins lines without a final newline. Select the complete replacement hunk.");
        result += line;
    };
    for (int i = 0; i < int(patch.size()); ++i) {
        auto row = patch[i];
        if (row.rfind("@@ ",0) == 0) {
            int start = 0, count = 1; auto offset = unstage ? row.find(" +") : row.find(" -");
            if (offset == std::string::npos || sscanf(row.c_str()+offset+2,"%d,%d",&start,&count) < 1)
                throw std::runtime_error("Invalid diff hunk.");
            size_t position = size_t(count == 0 ? start : start-1);
            if (position < cursor || position > original.size()) throw std::runtime_error("Diff does not match the index.");
            while (cursor < position) append(original[cursor++]);
            hunk = true; continue;
        }
        if (!hunk || row.empty() || row[0] == '\\') continue;
        char sign = row[0]; if (sign != ' ' && sign != '+' && sign != '-') { hunk = false; continue; }
        std::string content = row.substr(1);
        if (i+1 < int(patch.size()) && patch[i+1].rfind("\\ No newline",0) == 0 && !content.empty() && content.back() == '\n') content.pop_back();
        bool chosen = selection.count(i) && sign != ' ';
        if (chosen) ++used;
        char action = unstage ? (sign == '+' ? '-' : sign == '-' ? '+' : ' ') : sign;
        if (action == '+') { if (chosen) append(content); }
        else {
            if (cursor >= original.size() || original[cursor] != content) throw std::runtime_error("Diff content no longer matches the index.");
            if (action == ' ' || !chosen) append(original[cursor]);
            ++cursor;
        }
    }
    if (used != int(selection.size())) throw std::runtime_error("Select only changed text lines inside a diff hunk.");
    while (cursor < original.size()) append(original[cursor++]);
    if (entry.mode.empty()) {
        if (unstage) {
            auto tree = checked({"ls-tree","-z","HEAD","--",file.path});
            entry.mode = index_entry(tree).mode;
        } else entry.mode = (fs::status(path).permissions() & fs::perms::owner_exec) != fs::perms::none ? "100755" : "100644";
    }
    bool remove = result.empty() && (unstage ? file.index == 'A' : file.worktree == 'D');
    TempFile content(result);
    auto oid = trim(checked({"hash-object","-w","--no-filters",content.path}));
    if (checked({"ls-files","--stage","-z","--",file.path}) != signature || diff(file,unstage) != preview)
        throw std::runtime_error("File or index changed. Refresh and select lines again.");
    if (remove) checked({"update-index","--force-remove","--",file.path});
    else checked({"update-index","--add","--cacheinfo",entry.mode,oid,file.path});
}
Conflict Git::read_conflict(const File& file) const {
    Conflict c; c.path = file.path;
    auto path = regular_path(root_,file.path);
    c.stages = checked({"ls-files","--unmerged","-z","--",file.path});
    if (c.stages.empty()) throw std::runtime_error("This file no longer has a conflict. Refresh the repository.");
    std::istringstream records(c.stages);
    for (std::string record; std::getline(records,record,'\0');) {
        std::istringstream fields(record); std::string mode,oid; int stage = 0; fields >> mode >> oid >> stage;
        if (mode != "100644" && mode != "100755") throw std::runtime_error("Resolve symlink and submodule conflicts outside the text editor.");
        auto content = checked({"cat-file","blob",oid});
        if (content.size() > 1024*1024) throw std::runtime_error("The conflict editor supports files up to 1 MiB.");
        if (stage == 1) c.base = content;
        if (stage == 2) { c.ours = content; c.has_ours = true; }
        if (stage == 3) { c.theirs = content; c.has_theirs = true; }
    }
    c.exists = fs::exists(path); if (c.exists) c.working = read_file(path);
    c.binary = c.ours.find('\0') != std::string::npos || c.theirs.find('\0') != std::string::npos || c.working.find('\0') != std::string::npos;
    return c;
}
void Git::save_resolution(const Conflict& c,const std::string& result,bool remove) const {
    auto current = read_conflict(File{c.path,{}});
    if (current.stages != c.stages || current.working != c.working || current.exists != c.exists)
        throw std::runtime_error("Conflict or working file changed. Reopen the editor before saving.");
    if (result.size() > 1024*1024) throw std::runtime_error("Resolution exceeds 1 MiB.");
    if (!remove && !c.binary) {
        for (const auto& line : lines(result))
            if (line.rfind("<<<<<<<",0) == 0 || line.rfind("=======",0) == 0 || line.rfind(">>>>>>>",0) == 0)
                throw std::runtime_error("Resolve all conflict markers before marking the file resolved.");
    }
    auto path = regular_path(root_,c.path);
    if (remove) { if (c.exists) fs::remove(path); }
    else {
        fs::create_directories(path.parent_path());
        TempFile temp(result,(path.parent_path()/".easy-git-resolution-XXXXXX").string());
        auto perms = c.exists ? fs::status(path).permissions() : fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read | fs::perms::others_read;
        fs::permissions(temp.path,perms); fs::rename(temp.path,path);
    }
    checked({"add","-A","--",c.path});
}
void Git::clone(const std::string& url,const std::string& destination,std::shared_ptr<std::atomic_bool> cancel) {
    if (url.empty() || destination.empty()) throw std::runtime_error("Enter a clone URL and destination.");
    auto path = fs::absolute(destination);
    if (fs::exists(path) && (!fs::is_directory(path) || !fs::is_empty(path))) throw std::runtime_error("Clone destination must be new or empty.");
    fs::create_directories(path.parent_path());
    auto source = fs::exists(url) ? fs::absolute(url).string() : url;
    Git(path.parent_path().string(),cancel).checked({"clone","--",source,path.string()});
}
void Git::initialize(const std::string& destination,const std::string& branch) {
    if (destination.empty() || branch.empty()) throw std::runtime_error("Enter a destination and initial branch.");
    auto path = fs::absolute(destination); fs::create_directories(path);
    Git git(path.string());
    if (!git.run({"rev-parse","--git-dir"}).code) throw std::runtime_error("This folder is already inside a Git repository.");
    git.checked({"check-ref-format","--branch",branch});
    git.checked({"init","--initial-branch="+branch,"--",path.string()});
}
void Git::delete_branch(const Ref& ref,bool force) const {
    require_idle();
    if (trim(checked({"rev-parse","--verify",ref.full})) != ref.id) throw std::runtime_error("Branch changed. Refresh before deleting.");
    if (ref.full.rfind("refs/heads/",0) == 0) checked({"branch",force ? "-D" : "-d","--",ref.full.substr(11)});
    else if (ref.full.rfind("refs/remotes/",0) == 0) {
        auto [remote,branch] = remote_branch(*this,ref);
        checked({"push","--force-with-lease="+branch+":"+ref.id,"--",remote,":"+branch});
    } else throw std::runtime_error("Select a local or remote branch.");
}
PushTarget Git::push_target() const {
    PushTarget target; target.branch = trim(checked({"symbolic-ref","--quiet","--short","HEAD"}));
    target.local = trim(checked({"rev-parse","--verify","HEAD"}));
    auto format = checked({"for-each-ref","--format=%(upstream:remotename)%0a%(upstream:remoteref)%0a%(upstream)","refs/heads/"+target.branch});
    std::istringstream fields(format); std::string tracking;
    std::getline(fields,target.remote); std::getline(fields,target.ref); std::getline(fields,tracking);
    if (target.remote.empty() || target.remote == "." || target.ref.rfind("refs/heads/",0) != 0 || tracking.empty())
        throw std::runtime_error("Force push requires a remote tracking branch. Set an upstream and fetch first.");
    target.expected = trim(checked({"rev-parse","--verify",tracking})); return target;
}
void Git::force_push(const PushTarget& target) const {
    if (trim(checked({"rev-parse","--verify","HEAD"})) != target.local ||
        trim(checked({"symbolic-ref","--quiet","--short","HEAD"})) != target.branch)
        throw std::runtime_error("The local branch changed. Review the force push again.");
    checked({"push","--force-with-lease="+target.ref+":"+target.expected,"--",target.remote,target.local+":"+target.ref});
}
}
