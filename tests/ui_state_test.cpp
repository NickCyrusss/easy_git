// Exercise the real tab controller and ImGui state without an X server or a test framework.
#define EASY_GIT_UI_TEST
#include "../src/main.cpp"
#include "imgui_internal.h"
#include <iostream>
#include <thread>
#include <unistd.h>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void frame(App& app) {
    ImGui::NewFrame(); app.frame(); ImGui::Render();
}
void settle(App& app) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    do {
        frame(app); std::this_thread::sleep_for(std::chrono::milliseconds(5));
        require(std::chrono::steady_clock::now() < deadline,"Background work did not finish");
    } while (app.busy());
    frame(app); frame(app);
    for (const auto& tab : app.tabs) require(tab->error.empty(),tab->error.c_str());
}

int main() {
    char pattern[] = "/tmp/easy-git-tabs-test-XXXXXX";
    char* dir = mkdtemp(pattern); if (!dir) return 1;
    fs::path base(dir), root_a = base / "仓库 A", root_b = base / "仓库 B";
    ImGui::CreateContext();
    auto& io = ImGui::GetIO(); io.DisplaySize = {1440,900}; io.DeltaTime = 1.0f/60;
    io.IniFilename = nullptr; body_font = mono_font = io.Fonts->AddFontDefault();
    unsigned char* pixels; int w,h; io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    theme(); App app;
    try {
        for (const auto& root : {root_a,root_b}) {
            fs::create_directories(root / "src");
            eg::Git git(root.string()); git.checked({"init","--initial-branch=main"});
            git.checked({"config","user.name","Tab Test"}); git.checked({"config","user.email","tabs@example.invalid"});
            git.checked({"config","commit.gpgsign","false"});
            std::ofstream(root / "src/shared.txt") << "first " << root.filename().string() << '\n';
            git.checked({"add","."}); git.commit("First commit");
            std::ofstream(root / "src/shared.txt") << "changed " << root.filename().string() << '\n';
            std::ofstream(root / "notes.txt") << "staged notes\n"; git.checked({"add","notes.txt"});
            app.open_repository(root.string());
        }
        settle(app);
        app.config_path = (base / ".easy_git").string();
        require(app.tabs.size() == 2,"Opening the second repository replaced the first");
        auto* a = app.tabs[0].get(); auto* b = app.tabs[1].get(); int a_id = a->id, b_id = b->id;
        io.AddMousePosEvent(30,18); frame(app);
        io.AddMouseButtonEvent(0,true); frame(app);
        io.AddMouseButtonEvent(0,false); frame(app); frame(app);
        require(app.active == a_id,"Clicking the first repository tab did not activate it");
        snprintf(a->message,sizeof(a->message),"Draft for A"); snprintf(a->description,sizeof(a->description),"Description A");
        snprintf(b->message,sizeof(b->message),"Draft for B");
        auto change = *std::find_if(a->repo.files.begin(),a->repo.files.end(),[](const auto& f) { return f.path == "src/shared.txt"; });
        a->select_file(change,false); app.active = app.focus = b_id; settle(app);
        require(a->detail.find("changed 仓库 A") != std::string::npos && b->detail.empty(),"A's async diff leaked into B");
        require(a->selected_file == "src/shared.txt" && a->show_diff,"Selected file was lost while switching tabs");
        bool numbered = false;
        for (const auto& line : a->detail_lines) if (line.text.rfind("+changed",0) == 0) numbered = line.after == 1 && line.before == 0;
        require(numbered,"Inline diff line numbers are incorrect");

        b->select_commit(b->repo.commits.front()); settle(app);
        require(b->changed_files.size() == 1 && b->changed_files[0].path == "src/shared.txt","Commit files missing");
        b->select_file(b->changed_files[0],false); settle(app);
        require(b->detail.find("first 仓库 B") != std::string::npos && b->detail.find("仓库 A") == std::string::npos,"Commit preview crossed repositories");
        b->tree_view = true; frame(app);
        require(b->file_trees[2].directories.count("src") == 1,"Commit file tree omitted parent folder");

        a->change_file(change,false); app.active = app.focus = b_id; settle(app);
        auto staged_a = eg::Git(root_a.string()).checked({"diff","--cached","--name-only"});
        auto staged_b = eg::Git(root_b.string()).checked({"diff","--cached","--name-only"});
        require(staged_a.find("src/shared.txt") != std::string::npos && staged_b.find("src/shared.txt") == std::string::npos,
            "Background staging changed the wrong repository");
        require(a->show_diff && a->selected_staged && a->selected_file == "src/shared.txt","Staging lost the file preview");
        require(std::string(a->message) == "Draft for A" && std::string(a->description) == "Description A" &&
            std::string(b->message) == "Draft for B","Switching or refreshing overwrote commit drafts");

        eg::Git git_a(root_a.string()), git_b(root_b.string());
        git_a.save_stash("Preview A"); a->load(root_a.string()); settle(app);
        auto saved = a->repo.stashes.at(0);
        auto status_a = git_a.checked({"status","--porcelain=v1","-z"});
        auto status_b = git_b.checked({"status","--porcelain=v1","-z"});
        a->select_stash(saved); app.active = app.focus = b_id; settle(app);
        require(a->stash_view && !a->workspace && a->pending_kind.empty() && a->changed_files.size() == 2,
            "Stash selection did not enter read-only file preview");
        a->select_file(a->changed_files.back(),false); settle(app);
        require(a->detail.find("changed 仓库 A") != std::string::npos && !b->stash_view,
            "Stash diff missing or crossed repository tabs");
        require(git_a.checked({"status","--porcelain=v1","-z"}) == status_a &&
            git_b.checked({"status","--porcelain=v1","-z"}) == status_b,"Stash selection applied changes");
        a->mutate("Apply stash",[saved](const eg::Git& git) { git.apply_stash(saved,true); }); settle(app);
        require(a->workspace && !a->stash_view && a->repo.stashes.size() == 1 &&
            git_b.checked({"status","--porcelain=v1","-z"}) == status_b,"Stash apply refresh or tab isolation failed");
        a->select_commit(a->repo.commits.front()); settle(app);
        require(!a->stash_view && !a->workspace,"Commit selection retained stash mode");

        // A branch beyond the loaded page must be selected and scrolled into view, even from a filtered diff.
        auto first = git_a.read_commit("HEAD");
        auto tree = git_a.checked({"rev-parse","HEAD^{tree}"}); tree.pop_back();
        auto head = first.id;
        for (int i = 0; i < 48; ++i) {
            head = git_a.checked({"commit-tree",tree,"-p",head,"-m","Navigation " + std::to_string(i)});
            head.pop_back();
        }
        git_a.checked({"update-ref","refs/heads/main",head});
        git_a.checked({"branch","old-local",first.id});
        git_a.checked({"update-ref","refs/remotes/origin/old-remote",first.id});
        a->limit = 5; a->load(root_a.string()); settle(app);
        require(a->repo.commits.size() == 5 && a->repo.more,"Navigation fixture was not paginated");
        auto reference = [&](const std::string& name) {
            auto f = std::find_if(a->repo.refs.begin(),a->repo.refs.end(),[&](const auto& ref) { return ref.full == name; });
            require(f != a->repo.refs.end(),"Missing navigation reference"); return *f;
        };
        snprintf(a->search,sizeof(a->search),"no matching commits"); a->filter(); a->show_diff = true;
        auto before_navigation = git_a.checked({"status","--porcelain=v1","-z"});
        a->navigate_ref(reference("refs/heads/old-local")); app.active = app.focus = b_id; settle(app);
        require(a->selected_commit == first.id && a->selected_ref == "refs/heads/old-local" && !a->show_diff &&
            !a->workspace && !a->search[0] && a->repo.commits.size() == 49 && b->selected_ref.empty(),
            "Branch navigation did not load/select the target or crossed repository tabs");
        app.active = app.focus = a_id; settle(app);
        bool scrolled = false;
        for (auto* window : ImGui::GetCurrentContext()->Windows)
            if (std::string(window->Name).find("history_panel") != std::string::npos && window->Scroll.y > 500) scrolled = true;
        require(scrolled && a->scroll_to_commit.empty(),"Branch target outside the viewport was not scrolled into view");
        a->select_workspace();
        a->navigate_ref(reference("refs/remotes/origin/old-remote")); settle(app);
        require(a->selected_ref == "refs/remotes/origin/old-remote" && a->selected_commit == first.id &&
            git_a.checked({"status","--porcelain=v1","-z"}) == before_navigation &&
            git_a.read_commit("HEAD").id == head,"Remote navigation changed HEAD or working files");

        a->select_workspace();
        std::ofstream(root_a / "src/shared.txt") << "discard me\n";
        a->load(root_a.string()); settle(app);
        auto discard_file = *std::find_if(a->repo.files.begin(),a->repo.files.end(),[](const auto& f) { return f.path == "src/shared.txt"; });
        a->select_file(discard_file,false); settle(app);
        a->mutate("Discard file",[discard_file](const eg::Git& git) { git.discard(discard_file); });
        app.active = app.focus = b_id; settle(app);
        require(a->show_diff && a->selected_staged && a->detail.find("discard me") == std::string::npos &&
            git_b.checked({"status","--porcelain=v1","-z"}) == status_b,"Discard preview refresh or repository isolation failed");

        fs::create_directories(root_a / "dir");
        for (const auto* name : {"alpha.txt","dir/a.txt","dir/b.txt","omega.txt"}) std::ofstream(root_a / name) << name << '\n';
        a->load(root_a.string()); settle(app); app.active = app.focus = a_id;
        auto index_of = [&](const std::string& path) {
            auto found = std::find_if(a->repo.files.begin(),a->repo.files.end(),[&](const auto& f) { return f.path == path; });
            require(found != a->repo.files.end(),"Missing selection test file"); return int(found-a->repo.files.begin());
        };
        a->pick_file(index_of("alpha.txt"),0,false,false);
        a->pick_file(index_of("dir/b.txt"),0,true,false);
        require(a->file_selection[0].size() == 2,"Ctrl click lost selection during an active preview");
        settle(app);
        require(a->selected_file == "dir/b.txt" && a->detail.find("dir/b.txt") != std::string::npos,"Queued preview did not follow latest selected file");
        a->pick_file(index_of("dir/b.txt"),0,true,false); settle(app);
        require(a->file_selection[0] == std::set<std::string>{"alpha.txt"},"Ctrl click did not toggle selection off");
        a->pick_file(index_of("alpha.txt"),0,false,false); settle(app);
        a->pick_file(index_of("omega.txt"),0,false,true); settle(app);
        require(a->file_selection[0].size() == 4,"Shift did not select the displayed range");
        a->pick_file(index_of("dir/a.txt"),0,false,true); settle(app);
        require(a->file_selection[0].size() == 2,"Shift did not contract the range from its original anchor");
        a->pick_file(index_of("omega.txt"),0,true,true); settle(app);
        require(a->file_selection[0].size() == 4,"Ctrl+Shift did not extend selection");
        a->pick_file(index_of("notes.txt"),1,false,false); settle(app);
        require(a->file_selection[0].empty() && a->file_selection[1].size() == 1,"Selection crossed staged/unstaged groups");
        a->tree_view = true; a->closed_folders[0].insert("dir/"); a->rebuild_visible_files();
        a->pick_file(index_of("alpha.txt"),0,false,false); settle(app);
        a->pick_file(index_of("omega.txt"),0,false,true); settle(app);
        require(a->file_selection[0] == std::set<std::string>{"alpha.txt","omega.txt"},"Tree range selected files in a collapsed folder");
        a->closed_folders[0].clear(); a->rebuild_visible_files();
        a->pick_file(index_of("alpha.txt"),0,false,false); settle(app);
        a->pick_file(index_of("dir/b.txt"),0,false,true); settle(app);
        require(a->file_selection[0] == std::set<std::string>{"dir/b.txt","alpha.txt"},"Tree range used path order instead of displayed folder-first order");
        snprintf(a->file_search,sizeof(a->file_search),"dir/"); a->rebuild_file_lists();
        require(a->file_selection[0] == std::set<std::string>{"dir/b.txt"} && a->selection_anchor[0].empty(),"Filtering retained hidden selected files or anchor");
        a->file_search[0] = 0; a->tree_view = false; a->rebuild_file_lists();
        a->pick_file(index_of("alpha.txt"),0,false,false); settle(app);
        a->pick_file(index_of("omega.txt"),0,false,true); settle(app);
        a->change_files(a->chosen_files(0),false); app.active = app.focus = b_id; settle(app);
        require(git_a.checked({"diff","--cached","--name-only"}).find("omega.txt") != std::string::npos &&
            git_b.checked({"status","--porcelain=v1","-z"}) == status_b,"Batch stage lost files or crossed repository tabs");
        a->pick_file(index_of("alpha.txt"),1,false,false); settle(app);
        a->pick_file(index_of("dir/a.txt"),1,true,false); settle(app);
        a->change_files(a->chosen_files(1),true); settle(app);
        require(git_a.checked({"diff","--cached","--name-only"}).find("alpha.txt") == std::string::npos &&
            git_a.checked({"diff","--cached","--name-only"}).find("dir/b.txt") != std::string::npos,"Batch unstage changed files outside the selection");
        a->pick_file(index_of("alpha.txt"),0,false,false); settle(app);
        a->pick_file(index_of("dir/a.txt"),0,true,false); settle(app);
        a->request_discard(a->chosen_files(0));
        require(a->pending_files.size() == 2 && a->pending_kind == "discard files","Discard confirmation did not capture selection");
        auto discard_selection = a->pending_files; a->pending_kind.clear();
        a->mutate("Discard selected files",[discard_selection](const eg::Git& git) { git.discard_files(discard_selection); }); settle(app);
        require(!fs::exists(root_a / "alpha.txt") && !fs::exists(root_a / "dir/a.txt") && fs::exists(root_a / "dir/b.txt"),"Batch discard removed files outside the selection");

        app.open_repository((root_a / "src").string()); settle(app);
        require(app.tabs.size() == 2 && app.active == a_id && std::string(a->message) == "Draft for A",
            "Opening a repository subfolder duplicated the tab or discarded its state");
        app.request_close(a_id);
        require(app.pending_close == a_id && app.tabs.size() == 2,"Closing a tab silently lost its draft");
        app.pending_close = 0; a->message[0] = a->description[0] = 0; app.request_close(a_id);
        require(app.tabs.size() == 1 && app.find(b_id) == b && std::string(b->message) == "Draft for B","Closing A damaged B");

        auto stop = b->cancel;
        b->launch("Background read",[stop] { eg::run_process({"sleep","0.1"},stop); return JobResult{}; });
        app.request_close(b_id);
        require(app.tabs.size() == 1 && b->busy(),"Closed a repository while its operation was active");
        settle(app);
        std::cout << "PASS: independent tabs/drafts/selections, background operations, commit/stash files, stash apply isolation, diff numbering, duplicate roots, close protection\n";
        app.settings.light = true; theme(true); app.settings.ai.model = "persisted-test-model";
        app.shutdown();
        App restored; restored.initialize(app.config_path); settle(restored);
        require(restored.settings.light && light_theme && restored.settings.ai.model == "persisted-test-model" &&
            restored.tabs.size() == 1 && restored.tabs[0]->repo.root == root_b.string(),"Theme/settings/open repositories did not survive restart or a closed repository reopened");
        auto settings_button = [&](float x) {
            auto* modal = ImGui::FindWindowByName("Settings"); require(modal && modal->Active,"Settings did not open");
            io.AddMousePosEvent(modal->Pos.x+x,modal->Pos.y+modal->Size.y-40); frame(restored);
            io.AddMouseButtonEvent(0,true); frame(restored);
            io.AddMouseButtonEvent(0,false); frame(restored); frame(restored);
            require(!ImGui::FindWindowByName("Settings")->Active,"Settings button did not close the dialog");
        };
        restored.settings_open = true; frame(restored); frame(restored);
        eg::select_ai_provider(restored.settings_draft,eg::ai_presets()[1]);
        restored.settings_draft.ai.model = "cancelled-model";
        settings_button(165); // Cancel must discard all profile edits, including switched-away ones.
        require(restored.settings.ai.provider == "DeepSeek" && !restored.settings.ai_profiles.count("Kimi"),"Cancel changed saved provider settings");
        restored.settings_open = true; frame(restored); frame(restored);
        require(restored.settings_draft.ai.provider == "DeepSeek" && !restored.settings_draft.ai_profiles.count("Kimi"),"Reopening settings retained cancelled edits");
        eg::select_ai_provider(restored.settings_draft,eg::ai_presets()[1]);
        restored.settings_draft.ai.model = "saved-kimi-model";
        eg::select_ai_provider(restored.settings_draft,eg::ai_presets()[0]);
        settings_button(60);
        auto saved_config = eg::read_settings(restored.config_path);
        require(saved_config.ai.model == "persisted-test-model" && saved_config.ai_profiles.at("Kimi").model == "saved-kimi-model", "Save settings lost a switched-away profile");
        RepoTab editor;
        editor.set_resolution("before\n<<<<<<< HEAD\nours\n||||||| base\nbase\n=======\ntheirs >>>>>>> inline\n>>>>>>> side\nafter\n<<<<<<< HEAD\nleft\n=======\nright\n>>>>>>> side\n");
        require(editor.conflict_blocks().size() == 2,"Conflict editor did not parse diff3/multiple blocks");
        editor.choose_conflict_block(2);
        require(editor.conflict_blocks().size() == 1 && std::string(editor.resolution.data()).find("ours\ntheirs >>>>>>> inline\nafter") != std::string::npos,"Use both damaged surrounding text or kept base markers");
        editor.choose_conflict_block(1);
        require(std::string(editor.resolution.data()) == "before\nours\ntheirs >>>>>>> inline\nafter\nright\n","Conflict choices did not preserve context");
        editor.set_detail("diff --git a/file b/file\n@@ -1,2 +1,2 @@\n-old\n+new\n same\n");
        editor.pick_line(2,false,false); editor.pick_line(3,false,true);
        require(editor.selected_lines.size() == 2,"Shift did not select changed diff lines");
        editor.pick_line(2,true,false);
        require(editor.selected_lines.size() == 1 && editor.selected_lines.count(3),"Ctrl did not toggle a diff line");
        editor.set_detail("refreshed"); require(editor.selected_lines.empty(),"Diff refresh retained stale line selections");
        snprintf(restored.tabs[0]->message,sizeof(restored.tabs[0]->message),"Keep existing draft");
        restored.tabs[0]->generate_message();
        while (restored.busy()) { frame(restored); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
        require(std::string(restored.tabs[0]->message) == "Keep existing draft" && !restored.tabs[0]->error.empty(),"Failed AI request overwrote draft");
        restored.shutdown(); ImGui::DestroyContext(); fs::remove_all(base); return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        app.shutdown(); ImGui::DestroyContext(); fs::remove_all(base); return 1;
    }
}
