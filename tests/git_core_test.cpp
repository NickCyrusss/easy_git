#include "git.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace fs = std::filesystem;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void write(const fs::path& path, const std::string& text) { std::ofstream(path) << text; }
const eg::File& file_named(const eg::Snapshot& s, const std::string& name) {
    auto f = std::find_if(s.files.begin(),s.files.end(),[&](const auto& f) { return f.path == name; });
    require(f != s.files.end(),"Expected changed file is missing"); return *f;
}

int main(int argc, char** argv) {
    char path[] = "/tmp/easy-git-test-XXXXXX";
    auto* temp = mkdtemp(path);
    if (!temp) return 1;
    fs::path base(temp), root = base / "orbit workspace 中文";
    bool keep = argc > 1 && std::string(argv[1]) == "--keep";
    try {
        auto literal = eg::run_process({"printf", "%s", "中文 folder $(literal)\n"});
        require(literal.code == 0 && literal.out == "中文 folder $(literal)\n", "Process arguments were not preserved");
        require(eg::run_process({"false"}).code == 1, "Dialog cancellation exit code lost");
        auto cancelled = std::make_shared<std::atomic_bool>(true);
        require(eg::run_process({"sleep", "10"}, cancelled, 0).code == 124, "Dialog process did not cancel");
        fs::create_directory(root);
        eg::Git git(root.string());
        git.checked({"init","--initial-branch=main"});
        git.checked({"config","user.name","Alex Chen"});
        git.checked({"config","user.email","test@example.invalid"});
        git.checked({"config","commit.gpgsign","false"});
        auto empty = git.load();
        require(!empty.has_head && empty.commits.empty() && empty.branch == "main","Empty repository failed");

        std::string special = "设计 notes ; $(touch SHOULD_NOT_EXIST) [x].txt";
        write(root / special,"first version\n");
        auto s = git.load(); auto f = file_named(s,special);
        require(f.index == '?' && git.diff(f,false).find("+first version") != std::string::npos,"Untracked diff failed");
        git.stage(f);
        write(root / special,"second version\n");
        s = git.load(); f = file_named(s,special);
        require(f.staged() && f.unstaged(),"Partial staging state lost");
        git.unstage(f,false);
        require(fs::exists(root / special) && file_named(git.load(),special).index == '?',"Unborn unstage lost content");
        git.stage(f); git.commit("Initialize repository workspace");
        require(!fs::exists(root / "SHOULD_NOT_EXIST"),"Shell injection in filename");
        require(git.load().files.empty(),"First commit did not clean index");
        require(git.commit_detail(git.load().commits[0].id).find("second version") != std::string::npos,"Root commit diff failed");
        auto first = git.load().commits.front();
        auto first_files = git.commit_files(first);
        require(first_files.size() == 1 && first_files[0].path == special && first_files[0].index == 'A',"Root commit files failed");
        require(git.commit_diff(first,first_files[0]).find("+second version") != std::string::npos,"Root per-file diff failed");

        fs::create_directory(root / "src"); write(root / "src" / "app.cpp","int main() { return 0; }\n");
        git.checked({"add","--all"}); git.commit("Set up the desktop application");
        require(eg::Git((root / "src").string()).load().root == root.string(),"Subdirectory root detection failed");
        write(root / special,"working tree change\n");
        s = git.load(); f = file_named(s,special); git.stage(f);
        require(git.diff(f,true).find("+working tree change") != std::string::npos,"Staged diff failed");
        git.unstage(file_named(git.load(),special),true);
        require(!file_named(git.load(),special).staged(),"Unstage failed");
        git.stage(file_named(git.load(),special)); git.commit("Refine workspace notes");

        std::string renamed = "renamed 中文\nnotes.txt";
        git.checked({"mv","--",special,renamed});
        f = file_named(git.load(),renamed);
        require(f.index == 'R' && f.original == special,"NUL-separated rename parsing failed");
        git.unstage(f,true);
        require(fs::exists(root / renamed),"Unstage rename deleted working file");
        git.checked({"add","--all"}); git.commit("Organize design documentation");
        auto rename_commit = git.load().commits.front();
        auto renamed_files = git.commit_files(rename_commit);
        require(renamed_files.size() == 1 && renamed_files[0].path == renamed && renamed_files[0].original == special,
            "Commit rename paths with newline were lost");
        require(git.commit_diff(rename_commit,renamed_files[0]).find("rename from") != std::string::npos,"Rename diff failed");

        git.checked({"switch","-c","feature/commit-graph"});
        write(root / "graph.cpp","// draw lanes\n"); git.checked({"add","--all"}); git.commit("Draw colored commit lanes");
        write(root / "graph.cpp","// draw lanes and merge curves\n"); git.checked({"add","--all"}); git.commit("Connect merge parents with curves");
        git.checked({"switch","--","main"});
        write(root / "theme.cpp","// slate and mint\n"); git.checked({"add","--all"}); git.commit("Add the slate workspace theme");
        git.checked({"merge","--no-ff","feature/commit-graph","-m","Merge commit graph into workspace"});
        git.checked({"tag","--","v0.1.0"});
        s = git.load();
        require(s.commits.front().parents.size() == 2,"Merge parents lost");
        auto merged_files = git.commit_files(s.commits.front());
        require(merged_files.size() == 1 && merged_files[0].path == "graph.cpp","Merge file list is not relative to first parent");
        auto merged_diff = git.commit_diff(s.commits.front(),merged_files[0]);
        require(merged_diff.find("+// draw lanes and merge curves") != std::string::npos && merged_diff.find("theme.cpp") == std::string::npos,
            "Commit diff included unrelated files");
        auto graph = eg::layout_graph(s.commits);
        require(graph.size() == s.commits.size() && graph.front().width == 2,"Graph fork missing");
        for (const auto& row : graph) for (const auto& edge : row.segments)
            require(edge.from >= 0 && edge.to >= 0 && edge.from < row.width && edge.to < row.width,"Graph lane out of bounds");
        auto page = git.load(2);
        require(page.commits.size() == 2 && page.more,"History pagination failed");
        git.checked({"switch","--detach",s.commits.back().id});
        require(git.load().branch.rfind("Detached at ",0) == 0,"Detached HEAD failed");
        git.checked({"switch","--","main"});

        git.checked({"switch","-c","feature/file-preview"});
        fs::remove(root / "theme.cpp"); write(root / "asset.bin",std::string("binary\0data",11));
        git.checked({"add","--all"}); git.commit("Preview deleted and binary files");
        auto preview_commit = git.load().commits.front();
        auto preview_files = git.commit_files(preview_commit);
        require(preview_files.size() == 2,"Deleted/binary commit file list failed");
        for (const auto& file : preview_files) {
            auto patch = git.commit_diff(preview_commit,file);
            if (file.path == "theme.cpp") require(file.index == 'D' && patch.find("deleted file mode") != std::string::npos,"Deleted file diff failed");
            else require(file.path == "asset.bin" && patch.find("Binary files") != std::string::npos,"Binary file diff failed");
        }
        git.checked({"switch","--","main"});

        write(root / "draft.md","draft\n");
        git.checked({"stash","push","--include-untracked","-m","Inspector layout draft"});
        require(git.load().stashes.size() == 1 && !fs::exists(root / "draft.md"),"Stash save failed");
        git.checked({"stash","apply","stash@{0}"});
        require(fs::exists(root / "draft.md") && git.load().stashes.size() == 1,"Stash apply removed saved stash");

        fs::path remote = base / "remote.git"; fs::create_directory(remote);
        eg::Git(remote.string()).checked({"init","--bare","--initial-branch=main"});
        git.checked({"remote","add","origin",remote.string()});
        git.checked({"push","-u","origin","main"}); git.checked({"fetch","--all"}); git.checked({"pull","--ff-only"});
        require(!git.run({"switch","--","missing-branch"}).error.empty(),"Git failure was hidden");

        // Octopus merge, shared ancestor, and an unrelated root exercise lane continuity.
        auto node = [](const char* id, std::vector<std::string> parents) { eg::Commit c; c.id=id; c.parents=std::move(parents); return c; };
        std::vector<eg::Commit> synthetic = {node("M",{"A","B","C"}),node("A",{"R"}),
            node("B",{"R"}),node("C",{"R"}),node("R",{}),node("U",{})};
        auto rows = eg::layout_graph(synthetic);
        require(rows[0].width == 3 && rows[4].lane == 0 && rows[5].lane == 0,"Octopus graph layout failed");
        std::vector<std::string> active;
        for (size_t i=0; i<synthetic.size(); ++i) {
            const auto& row=rows[i];
            for (const auto& edge : row.segments) if (edge.incoming)
                require(size_t(edge.from)<active.size() && !active[edge.from].empty(),"Broken incoming graph lane");
            active.resize(row.width);
            active[row.lane].clear();
            size_t parent = 0;
            for (const auto& edge : row.segments) if (!edge.incoming && edge.from == row.lane)
                active[edge.to] = synthetic[i].parents.at(parent++);
            require(parent == synthetic[i].parents.size(),"Missing parent edge");
        }
        std::string conflict = "UU conflict.txt"; conflict.push_back('\0');
        require(eg::parse_status(conflict)[0].conflicted(),"Conflict detection failed");
        bool malformed = false;
        try { eg::parse_status("R  incomplete"); } catch (const std::exception&) { malformed = true; }
        require(malformed,"Malformed rename record accepted");
        bool blank = false;
        try { git.commit(" \n\t"); } catch (const std::exception&) { blank = true; }
        require(blank,"Blank commit accepted");

        write(root / "src" / "app.cpp","int main() {\n    // Restore window state\n    return 0;\n}\n");
        git.stage(file_named(git.load(),"src/app.cpp"));
        std::cout << "PASS: empty repo, literal paths, diff, stage/unstage, commit, rename, merge graph, pagination, detached HEAD, stash and local remote\n";
        if (keep) std::cout << root.string() << '\n';
        else fs::remove_all(base);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\nFixture: " << root << '\n';
        if (!keep) fs::remove_all(base);
        return 1;
    }
}
