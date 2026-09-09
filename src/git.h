#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace eg {
// Link the old/new sides of replacements; unequal blocks are selected as a unit.
std::vector<int> expand_line_selection(const std::string& preview, const std::vector<int>& selected);
struct Result {
    int code = 0;
    std::string out, error;
    bool truncated = false;
};
// A zero timeout allows user-driven dialogs to stay open until selection or cancellation.
Result run_process(std::vector<std::string> args, const std::shared_ptr<std::atomic_bool>& cancel = {},
                   int timeout_seconds = 120, char* const* environment = nullptr);
struct File {
    std::string path, original;
    char index = ' ', worktree = ' ';
    bool staged() const { return index != ' ' && index != '?'; }
    bool unstaged() const { return worktree != ' '; }
    bool conflicted() const;
    bool stash_untracked = false;
};
struct Commit {
    std::string id, author, date, subject, refs;
    std::vector<std::string> parents;
};
struct Ref { std::string full, name, id; };
struct Stash { std::string ref, id, subject; };
struct Snapshot {
    std::string root, branch, operation;
    bool has_head = false, more = false;
    std::vector<File> files;
    std::vector<Commit> commits;
    std::vector<Ref> refs;
    std::vector<Stash> stashes;
};
struct Segment { int from, to, color; bool incoming; };
struct GraphRow { int lane = 0, width = 1; std::vector<Segment> segments; };
struct Conflict {
    std::string path, stages, base, ours, theirs, working;
    bool has_ours = false, has_theirs = false, exists = false, binary = false;
};
struct PushTarget { std::string remote, ref, expected, local, branch; };

std::vector<File> parse_status(const std::string& bytes);
std::vector<Commit> parse_log(const std::string& bytes);
std::vector<File> parse_changed_files(const std::string& bytes);
std::vector<GraphRow> layout_graph(const std::vector<Commit>& commits);

class Git {
public:
    explicit Git(std::string root, std::shared_ptr<std::atomic_bool> cancel = {});
    Result run(const std::vector<std::string>& args) const;
    std::string checked(const std::vector<std::string>& args) const;
    Snapshot load(int limit = 300) const;
    std::string commit_detail(const std::string& id) const;
    std::vector<File> commit_files(const Commit& commit) const;
    std::string commit_diff(const Commit& commit, const File& file) const;
    Commit read_commit(const std::string& id) const;
    std::vector<File> stash_files(const Stash& stash) const;
    std::string stash_diff(const Stash& stash, const File& file) const;
    void apply_stash(const Stash& stash, bool restore_index = false) const;
    void delete_stash(const Stash& stash) const;
    void save_stash(const std::string& message) const;
    void cherry_pick(const Commit& commit, int mainline = 0) const;
    void revert(const Commit& commit, int mainline = 0) const;
    void merge(const std::string& id) const;
    void reset(const std::string& id, const std::string& mode) const;
    std::string operation_in_progress() const;
    void resolve_operation(const std::string& operation, const std::string& action) const;
    std::string diff(const File& file, bool staged) const;
    void stage(const File& file) const;
    void unstage(const File& file, bool has_head) const;
    void discard(const File& file) const;
    void discard_files(const std::vector<File>& files) const;
    void commit(const std::string& message) const;
    void stage_lines(const File& file, bool unstage, const std::string& preview, const std::vector<int>& lines) const;
    void discard_lines(const File& file, const std::string& preview, const std::vector<int>& lines) const;
    Conflict read_conflict(const File& file) const;
    void save_resolution(const Conflict& conflict, const std::string& result, bool remove = false) const;
    static void clone(const std::string& url, const std::string& destination, std::shared_ptr<std::atomic_bool> cancel = {});
    static void initialize(const std::string& destination, const std::string& branch);
    void delete_branch(const Ref& ref, bool force = false) const;
    PushTarget push_target() const;
    void force_push(const PushTarget& target) const;
private:
    void require_idle() const;
    std::string root_;
    std::shared_ptr<std::atomic_bool> cancel_;
};
}
