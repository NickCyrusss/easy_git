#include "git.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace fs = std::filesystem;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F action, const char* message) {
    bool failed = false;
    try { action(); } catch (const std::exception&) { failed = true; }
    require(failed,message);
}
void write(const fs::path& path, const std::string& text) { std::ofstream(path) << text; }
std::string read(const fs::path& path) { std::ifstream in(path); return {std::istreambuf_iterator<char>(in),{}}; }

int main() {
    char pattern[] = "/tmp/easy-git-operations-XXXXXX";
    auto* temp = mkdtemp(pattern); if (!temp) return 1;
    fs::path root(temp);
    try {
        eg::Git git(root.string());
        git.checked({"init","--initial-branch=main"});
        git.checked({"config","user.name","Operations Test"});
        git.checked({"config","user.email","test@example.invalid"});
        git.checked({"config","commit.gpgsign","false"});
        git.checked({"config","core.autocrlf","false"});
        auto commit = [&](const char* message) { git.checked({"add","--all"}); git.commit(message); return git.read_commit("HEAD"); };
        write(root / "shared.txt","base\n"); auto base = commit("Base");
        auto changed = [&](const std::string& path) {
            auto files = git.load().files;
            auto found = std::find_if(files.begin(),files.end(),[&](const auto& f) { return f.path == path; });
            require(found != files.end(),"Missing discard test file"); return *found;
        };
        write(root / "shared.txt","staged version\n"); git.checked({"add","shared.txt"});
        write(root / "shared.txt","unstaged version\n");
        auto saved_index = git.checked({"write-tree"});
        git.discard(changed("shared.txt"));
        require(read(root / "shared.txt") == "staged version\n" && git.checked({"write-tree"}) == saved_index,
            "Discard lost staged content");
        write(root / "batch-one.txt","one\n"); write(root / "batch-two.txt","two\n");
        auto one = changed("batch-one.txt"), two = changed("batch-two.txt");
        git.checked({"add","batch-two.txt"});
        rejects([&] { git.discard_files({one,two}); },"Batch discard accepted stale status");
        require(fs::exists(root / "batch-one.txt"),"Batch discard deleted an earlier file before validating the entire selection");
        git.unstage(changed("batch-two.txt"),true);
        write(root / "shared.txt","unstaged batch edit\n");
        git.discard_files({changed("shared.txt"),one,two});
        require(!fs::exists(root / "batch-one.txt") && !fs::exists(root / "batch-two.txt") &&
            read(root / "shared.txt") == "staged version\n" && git.checked({"write-tree"}) == saved_index,"Batch discard failed to preserve staged content");
        fs::remove(root / "shared.txt"); git.discard(changed("shared.txt"));
        require(read(root / "shared.txt") == "staged version\n","Discard did not restore a deleted file");
        const std::string literal = "草稿\n[x] $(literal).txt";
        write(root / literal,"delete only this file\n"); write(root / "keep.txt","keep\n");
        git.discard(changed(literal));
        require(!fs::exists(root / literal) && read(root / "keep.txt") == "keep\n","Untracked discard expanded path or removed another file");
        fs::create_symlink(root / "keep.txt",root / "link.txt");
        git.discard(changed("link.txt"));
        require(!fs::is_symlink(root / "link.txt") && read(root / "keep.txt") == "keep\n","Discard followed a symlink");
        auto stale = changed("keep.txt"); git.checked({"add","keep.txt"});
        rejects([&] { git.discard(stale); },"Discard accepted stale untracked status");
        require(read(root / "keep.txt") == "keep\n","Stale discard removed newly staged file");
        git.reset(base.id,"hard");
        git.checked({"mv","shared.txt","renamed.txt"}); write(root / "renamed.txt","unstaged rename edits\n");
        saved_index = git.checked({"write-tree"}); git.discard(changed("renamed.txt"));
        require(read(root / "renamed.txt") == "base\n" && !fs::exists(root / "shared.txt") &&
            git.checked({"write-tree"}) == saved_index,"Discard undid a staged rename");
        git.reset(base.id,"hard");
        git.checked({"switch","-c","feature"});
        write(root / "feature.txt","feature\n"); auto feature = commit("Feature");
        git.checked({"switch","main"});
        git.cherry_pick(feature);
        require(read(root / "feature.txt") == "feature\n" && git.load().operation.empty(),"Cherry-pick failed");
        git.revert(git.read_commit("HEAD"));
        require(!fs::exists(root / "feature.txt") && git.read_commit("HEAD").subject.find("Revert") == 0,"Revert failed");
        git.reset(base.id,"hard");
        write(root / "main.txt","main\n"); auto main = commit("Main");
        git.merge(feature.id); auto merge = git.read_commit("HEAD");
        require(merge.parents.size() == 2 && fs::exists(root / "feature.txt"),"Diverged merge failed");
        rejects([&] { git.revert(merge); },"Merge revert accepted missing mainline");
        git.revert(merge,1);
        require(!fs::exists(root / "feature.txt") && fs::exists(root / "main.txt"),"Merge revert mainline failed");
        git.reset(main.id,"hard"); git.cherry_pick(merge,1);
        require(fs::exists(root / "feature.txt"),"Cherry-pick merge mainline failed");
        git.reset(base.id,"hard"); git.merge(feature.id);
        require(git.read_commit("HEAD").id == feature.id,"Fast-forward merge failed");

        write(root / "shared.txt","local work\n");
        git.reset(base.id,"soft");
        require(git.read_commit("HEAD").id == base.id && git.checked({"diff","--cached","--name-only"}) == "feature.txt\n" &&
            read(root / "shared.txt") == "local work\n","Soft reset did not preserve index/worktree");
        git.reset(base.id,"mixed");
        require(git.checked({"diff","--cached"}).empty() && fs::exists(root / "feature.txt") &&
            read(root / "shared.txt") == "local work\n","Mixed reset lost working changes");
        git.reset(feature.id,"hard");
        require(read(root / "shared.txt") == "base\n" && git.load().files.empty(),"Hard reset failed");
        rejects([&] { git.reset(base.id,"invalid"); },"Invalid reset mode accepted");

        // Stash previews must include untracked paths without changing HEAD, index or worktree.
        write(root / "shared.txt","staged\n"); git.checked({"add","shared.txt"});
        write(root / "shared.txt","staged\nworking\n");
        const std::string odd = "草稿\n[x].txt"; write(root / odd,"saved untracked\n");
        git.save_stash("Preview one"); auto saved = git.load().stashes.at(0);
        require(saved.ref == "stash@{0}" && saved.subject.find("Preview one") != std::string::npos,"Stash metadata parse failed");
        write(root / "current.txt","untouched\n");
        auto status = git.checked({"status","--porcelain=v1","-z"});
        auto index = git.checked({"write-tree"}); auto head = git.read_commit("HEAD").id;
        auto files = git.stash_files(saved);
        require(files.size() == 2,"Stash did not list tracked and untracked files");
        for (const auto& file : files) {
            auto patch = git.stash_diff(saved,file);
            if (file.path == odd) require(file.stash_untracked && patch.find("+saved untracked") != std::string::npos,"Untracked stash diff failed");
            else require(file.path == "shared.txt" && !file.stash_untracked && patch.find("+working") != std::string::npos,"Tracked stash diff failed");
        }
        require(git.checked({"status","--porcelain=v1","-z"}) == status && git.checked({"write-tree"}) == index &&
            git.read_commit("HEAD").id == head && read(root / "current.txt") == "untouched\n","Stash preview mutated repository");
        git.save_stash("Preview two");
        rejects([&] { git.delete_stash(saved); },"Stale stash selector deleted the wrong entry");
        require(git.load().stashes.size() == 2,"Stale delete changed stash list");
        git.apply_stash(saved,true);
        require(read(root / odd) == "saved untracked\n" && read(root / "shared.txt") == "staged\nworking\n" &&
            git.checked({"show",":shared.txt"}) == "staged\n" && !fs::exists(root / "current.txt"),"Pinned stash apply or restore index failed");
        auto stashes = git.load().stashes;
        require(stashes.size() == 2,"Apply removed stash");
        git.delete_stash(stashes[1]);
        require(git.load().stashes.size() == 1 && git.load().stashes[0].id == stashes[0].id,"Delete removed incorrect stash");
        git.reset(feature.id,"hard"); fs::remove(root / odd);
        git.apply_stash(git.load().stashes.front());
        require(read(root / "current.txt") == "untouched\n","Default stash apply failed");
        fs::remove(root / "current.txt");

        // Real conflicting operations exercise detection, safe blocking and resolution.
        git.reset(base.id,"hard"); git.checked({"switch","-c","conflicting"});
        write(root / "shared.txt","theirs\n"); auto theirs = commit("Theirs");
        git.checked({"switch","main"}); write(root / "shared.txt","ours\n"); auto ours = commit("Ours");
        rejects([&] { git.cherry_pick(theirs); },"Expected cherry-pick conflict");
        auto state = git.load();
        require(state.operation == "cherry-pick" && state.files[0].conflicted(),"Cherry-pick conflict state missing");
        auto conflict_text = read(root / "shared.txt");
        rejects([&] { git.discard(state.files[0]); },"Discard accepted an unresolved conflict");
        require(read(root / "shared.txt") == conflict_text,"Discard overwrote conflict content");
        rejects([&] { git.reset(base.id,"hard"); },"Reset accepted during cherry-pick");
        rejects([&] { git.merge(feature.id); },"Merge accepted during cherry-pick");
        rejects([&] { git.resolve_operation("merge","abort"); },"Wrong operation was aborted");
        git.resolve_operation("cherry-pick","abort");
        require(git.read_commit("HEAD").id == ours.id && read(root / "shared.txt") == "ours\n","Cherry-pick abort lost original state");
        rejects([&] { git.cherry_pick(theirs); },"Expected cherry-pick conflict again");
        git.resolve_operation("cherry-pick","skip");
        require(git.load().operation.empty() && git.read_commit("HEAD").id == ours.id,"Cherry-pick skip failed");
        rejects([&] { git.cherry_pick(theirs); },"Expected cherry-pick resolution conflict");
        write(root / "shared.txt","resolved\n"); git.checked({"add","shared.txt"});
        git.resolve_operation("cherry-pick","continue");
        require(git.load().operation.empty() && read(root / "shared.txt") == "resolved\n","Cherry-pick continue failed");
        git.reset(ours.id,"hard");
        rejects([&] { git.merge(theirs.id); },"Expected merge conflict");
        require(git.load().operation == "merge","Merge state missing");
        git.resolve_operation("merge","abort");
        require(git.read_commit("HEAD").id == ours.id && read(root / "shared.txt") == "ours\n","Merge abort failed");
        rejects([&] { git.merge(theirs.id); },"Expected merge conflict again");
        write(root / "shared.txt","resolved merge\n"); git.checked({"add","shared.txt"});
        git.resolve_operation("merge","continue");
        require(git.read_commit("HEAD").parents.size() == 2 && git.load().operation.empty(),"Merge continue failed");
        rejects([&] { git.revert(ours); },"Expected revert conflict");
        require(git.load().operation == "revert","Revert state missing");
        git.resolve_operation("revert","abort");
        rejects([&] { git.revert(ours); },"Expected revert conflict again");
        write(root / "shared.txt","resolved revert\n"); git.checked({"add","shared.txt"});
        git.resolve_operation("revert","continue");
        require(git.load().operation.empty() && read(root / "shared.txt") == "resolved revert\n","Revert continue failed");
        git.reset(ours.id,"hard");
        require(git.run({"rebase","conflicting"}).code != 0 && git.load().operation == "rebase","External rebase conflict detection failed");
        git.resolve_operation("rebase","abort");
        require(git.read_commit("HEAD").id == ours.id,"Rebase abort failed");
        // Worktrees use a .git file and must read their own operation state.
        auto linked = root / "linked-worktree";
        git.checked({"worktree","add","--detach",linked.string(),ours.id});
        eg::Git worktree(linked.string());
        rejects([&] { worktree.cherry_pick(theirs); },"Expected linked worktree conflict");
        require(worktree.load().operation == "cherry-pick" && git.load().operation.empty(),"Operation state crossed worktrees");
        worktree.resolve_operation("cherry-pick","abort");
        git.checked({"worktree","remove",linked.string()});
        std::cout << "PASS: cherry-pick/merge/revert, mainline, reset modes, stash previews/apply/index/delete, conflicts/continue/abort/skip\n";
        fs::remove_all(root); return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\nFixture: " << root << '\n'; return 1;
    }
}
