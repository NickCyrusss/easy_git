#include "git.h"
#include "app_services.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <ctime>

namespace {
namespace fs = std::filesystem;
ImU32 lane_colors[] = {IM_COL32(87,205,181,255), IM_COL32(173,143,238,255),
    IM_COL32(239,184,104,255), IM_COL32(108,170,238,255), IM_COL32(237,133,152,255), IM_COL32(122,196,123,255)};
ImVec4 mint(0.34f, 0.80f, 0.71f, 1), muted(0.56f, 0.60f, 0.69f, 1);
ImVec4 red(0.94f, 0.48f, 0.52f, 1);
bool light_theme = false;
ImFont* body_font = nullptr;
ImFont* mono_font = nullptr;

std::string local_commit_time(const std::string& date) {
    std::tm parsed{};
    const char* end = strptime(date.c_str(),"%Y-%m-%dT%H:%M:%S%z",&parsed);
    if (!end || *end) return date;
    auto offset = parsed.tm_gmtoff;
    std::time_t instant = timegm(&parsed)-offset;
    std::tm local{}; char result[96];
    if (!localtime_r(&instant,&local) || !std::strftime(result,sizeof(result),"%Y-%m-%d %H:%M:%S %Z",&local)) return date;
    return result;
}

void label(const char* text) { ImGui::TextColored(muted, "%s", text); }
bool button(const char* text, bool enabled = true, ImVec2 size = {}) {
    ImGui::BeginDisabled(!enabled);
    bool clicked = ImGui::Button(text, size);
    ImGui::EndDisabled();
    return clicked;
}
void section(const char* text) {
    ImGui::Spacing(); ImGui::Spacing();
    label(text); ImGui::Spacing();
}
void text_clipped(ImDrawList* draw, ImVec2 at, const std::string& text, ImU32 color, ImVec2 end) {
    draw->PushClipRect(at, end, true);
    draw->AddText(at, color, text.c_str());
    draw->PopClipRect();
}
std::string visible_path(std::string text) {
    std::string out;
    for (char c : text) {
        if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

void theme(bool light = false) {
    light_theme = light;
    mint = light ? ImVec4(0.04f,0.43f,0.35f,1) : ImVec4(0.34f,0.80f,0.71f,1);
    muted = light ? ImVec4(0.35f,0.40f,0.47f,1) : ImVec4(0.56f,0.60f,0.69f,1);
    red = light ? ImVec4(0.75f,0.16f,0.24f,1) : ImVec4(0.94f,0.48f,0.52f,1);
    const ImU32 dark_lanes[] = {IM_COL32(87,205,181,255),IM_COL32(173,143,238,255),IM_COL32(239,184,104,255),IM_COL32(108,170,238,255),IM_COL32(237,133,152,255),IM_COL32(122,196,123,255)};
    const ImU32 light_lanes[] = {IM_COL32(10,110,89,255),IM_COL32(119,74,178,255),IM_COL32(152,96,17,255),IM_COL32(34,104,173,255),IM_COL32(185,58,90,255),IM_COL32(61,119,51,255)};
    std::copy(light ? light_lanes : dark_lanes,(light ? light_lanes : dark_lanes)+6,lane_colors);
    if (light) ImGui::StyleColorsLight(); else ImGui::StyleColorsDark();
    auto& s = ImGui::GetStyle();
    s.WindowPadding = {16, 14}; s.FramePadding = {10, 7};
    s.ItemSpacing = {10, 8}; s.CellPadding = {10, 6};
    s.WindowRounding = 0; s.ChildRounding = 0; s.FrameRounding = 5;
    s.PopupRounding = 6; s.ScrollbarRounding = 6; s.GrabRounding = 4;
    s.WindowBorderSize = 0; s.ChildBorderSize = 0;
    s.ScrollbarSize = 10;
    auto* c = s.Colors;
    if (light) {
        c[ImGuiCol_Text] = {0.12f,0.16f,0.21f,1}; c[ImGuiCol_TextDisabled] = muted;
        c[ImGuiCol_WindowBg] = {0.92f,0.94f,0.96f,1}; c[ImGuiCol_ChildBg] = {0.97f,0.98f,0.99f,1};
        c[ImGuiCol_PopupBg] = {0.98f,0.99f,1,1}; c[ImGuiCol_Border] = {0.73f,0.78f,0.82f,1};
        c[ImGuiCol_FrameBg] = {0.88f,0.91f,0.94f,1}; c[ImGuiCol_FrameBgHovered] = {0.81f,0.87f,0.89f,1};
        c[ImGuiCol_Button] = {0.85f,0.89f,0.92f,1}; c[ImGuiCol_ButtonHovered] = {0.72f,0.84f,0.83f,1};
        c[ImGuiCol_ButtonActive] = {0.63f,0.78f,0.76f,1};
        c[ImGuiCol_Header] = {0.73f,0.86f,0.82f,1}; c[ImGuiCol_HeaderHovered] = {0.82f,0.91f,0.88f,1}; c[ImGuiCol_HeaderActive] = {0.64f,0.81f,0.75f,1};
        c[ImGuiCol_Tab] = {0.85f,0.89f,0.92f,1}; c[ImGuiCol_TabSelected] = {0.97f,0.98f,0.99f,1}; c[ImGuiCol_TabHovered] = {0.73f,0.86f,0.82f,1};
        c[ImGuiCol_TableHeaderBg] = {0.89f,0.92f,0.94f,1}; c[ImGuiCol_TableRowBgAlt] = {0,0,0,0.025f};
        c[ImGuiCol_Separator] = {0.72f,0.77f,0.82f,1};
        c[ImGuiCol_CheckMark] = c[ImGuiCol_NavCursor] = c[ImGuiCol_TabSelectedOverline] = mint;
        c[ImGuiCol_TextSelectedBg] = {0.37f,0.70f,0.62f,0.35f};
        return;
    }
    c[ImGuiCol_Text] = {0.86f,0.89f,0.94f,1}; c[ImGuiCol_TextDisabled] = muted;
    c[ImGuiCol_WindowBg] = {0.094f,0.106f,0.145f,1};
    c[ImGuiCol_ChildBg] = {0.115f,0.13f,0.175f,1};
    c[ImGuiCol_PopupBg] = {0.14f,0.16f,0.21f,1};
    c[ImGuiCol_Border] = {0.22f,0.25f,0.32f,1};
    c[ImGuiCol_FrameBg] = {0.16f,0.18f,0.235f,1};
    c[ImGuiCol_FrameBgHovered] = {0.21f,0.25f,0.31f,1};
    c[ImGuiCol_FrameBgActive] = {0.23f,0.29f,0.35f,1};
    c[ImGuiCol_Button] = {0.18f,0.21f,0.27f,1};
    c[ImGuiCol_ButtonHovered] = {0.24f,0.31f,0.36f,1};
    c[ImGuiCol_ButtonActive] = {0.22f,0.43f,0.42f,1};
    c[ImGuiCol_Header] = {0.19f,0.30f,0.33f,1};
    c[ImGuiCol_HeaderHovered] = {0.20f,0.25f,0.32f,1};
    c[ImGuiCol_HeaderActive] = {0.23f,0.36f,0.38f,1};
    c[ImGuiCol_Separator] = {0.22f,0.25f,0.31f,1};
    c[ImGuiCol_SeparatorHovered] = mint; c[ImGuiCol_SeparatorActive] = mint;
    c[ImGuiCol_CheckMark] = mint; c[ImGuiCol_NavCursor] = mint;
    c[ImGuiCol_TableHeaderBg] = {0.12f,0.14f,0.185f,1};
    c[ImGuiCol_TableRowBgAlt] = {1,1,1,0.018f};
    c[ImGuiCol_TextSelectedBg] = {0.26f,0.55f,0.52f,0.5f};
    c[ImGuiCol_Tab] = {0.12f,0.14f,0.18f,1};
    c[ImGuiCol_TabSelected] = {0.19f,0.23f,0.28f,1};
    c[ImGuiCol_TabHovered] = {0.23f,0.30f,0.34f,1};
    c[ImGuiCol_TabSelectedOverline] = mint;
}

struct JobResult {
    bool external_diff = false;
    eg::Snapshot snapshot;
    bool ai_generated = false;
    eg::CommitMessage generated;
    std::string original_summary, original_description;
    eg::Conflict conflict;
    eg::PushTarget push;
    bool conflict_selection = false, push_selection = false;
    std::string created_path;
    eg::Commit commit;
    std::vector<eg::File> files;
    std::string detail, message, error, folder, preview_file, navigation_ref;
    int history_limit = 0;
    bool preview_staged = false;
    bool reload = false, clear_commit = false, clear_summary = false, folder_selection = false, commit_selection = false, stash_selection = false;
};
struct FileTree {
    std::map<std::string, FileTree> directories;
    std::vector<int> files;
};
struct DiffLine { std::string text; int before = 0, after = 0; };
struct RepoTab {
    int id = 0;
    const eg::AiSettings* ai_settings = nullptr;
    std::string opened_path;
    bool generating = false;
    std::set<int> selected_lines;
    int line_anchor = -1, open_mode = 0, conflict_number = 0;
    char clone_url[4096] = {}, initial_branch[256] = "main";
    eg::Ref pending_branch;
    eg::PushTarget pending_push;
    eg::Conflict conflict;
    std::vector<char> resolution;
    std::string binary_resolution;
    std::array<std::set<int>,2> conflict_lines;
    std::string conflict_selection_key;
    bool incoming_first = false;
    std::vector<std::string> resolution_undo;
    bool conflict_open = false, resolving_conflict = false, remove_resolution = false, binary_chosen = false;
    bool just_opened = false, show_diff = false, tree_view = false;
    std::string requested_open, commit_body;
    eg::Commit viewed_commit;
    std::vector<eg::File> changed_files;
    std::array<std::vector<int>, 3> file_lists;
    std::array<FileTree, 3> file_trees;
    std::array<std::set<std::string>, 3> file_selection, closed_folders;
    std::array<std::string, 3> selection_anchor;
    std::array<std::vector<int>, 3> visible_files;
    std::vector<eg::File> pending_files;
    bool pending_discard_all = false;
    std::string pending_diff;
    std::vector<int> pending_diff_lines;
    bool previewing = false;
    int queued_preview_kind = -1;
    eg::File queued_preview;

    eg::Snapshot repo;
    std::vector<eg::Commit> history_entries;
    std::vector<eg::GraphRow> graph;
    std::vector<int> matches;
    std::vector<DiffLine> detail_lines;
    std::future<JobResult> job;
    std::shared_ptr<std::atomic_bool> cancel = std::make_shared<std::atomic_bool>(false);
    std::string status = "Open a repository to get started", error, detail, selected_commit, selected_file;
    std::string new_kind, pending_kind, pending_operation;
    eg::Stash viewed_stash, pending_stash;
    eg::Commit pending_commit;
    std::string selected_ref, scroll_to_commit;
    bool stash_view = false, restore_index = false, hard_confirm = false;
    int reset_mode = 1, mainline = 1;
    char sidebar_search[256] = {};
    char path[4096] = {}, search[256] = {}, message[8192] = {}, description[8192] = {}, new_name[256] = {}, file_search[256] = {};
    int limit = 300;
    float sidebar_width = 210, detail_width = 410;
    std::array<bool,4> sidebar_open = {true,true,true,true};
    std::array<float,4> sidebar_weights = {1,1,1,1};
    bool workspace = true, selected_staged = false, opening = false;
    double diff_checked_at = 0;
    std::string watched_stamp;
    bool busy() const { return job.valid(); }
    bool ready() const { return !repo.root.empty() && !busy(); }

    bool idle() const { return ready() && repo.operation.empty(); }

    void set_detail(std::string text) {
        detail = std::move(text); detail_lines.clear(); selected_lines.clear(); line_anchor = -1;
        std::istringstream lines(detail);
        int before = 0, after = 0; bool hunk = false;
        for (std::string line; std::getline(lines, line);) {
            DiffLine row{line};
            if (line.rfind("@@ ", 0) == 0) {
                // The optional range lengths do not affect the first line numbers.
                auto plus = line.find(" +");
                hunk = sscanf(line.c_str(), "@@ -%d", &before) == 1 && plus != std::string::npos &&
                    sscanf(line.c_str() + plus, " +%d", &after) == 1;
            } else if (line.rfind("diff ", 0) == 0) hunk = false;
            else if (hunk && !line.empty()) {
                if (line[0] == ' ' || line[0] == '-') row.before = before++;
                if (line[0] == ' ' || line[0] == '+') row.after = after++;
            }
            detail_lines.push_back(std::move(row));
        }
    }
    void rebuild_file_lists() {
        for (int kind = 0; kind < 3; ++kind) {
            auto& list = file_lists[kind]; auto& tree = file_trees[kind];
            list.clear(); tree = {};
            const auto& files = kind == 2 ? changed_files : repo.files;
            for (int i = 0; i < int(files.size()); ++i) {
                const auto& f = files[i];
                if (kind == 0 && !f.unstaged()) continue;
                if (kind == 1 && !f.staged()) continue;
                if (file_search[0] && f.path.find(file_search) == std::string::npos &&
                    f.original.find(file_search) == std::string::npos) continue;
                list.push_back(i);
            }
            std::sort(list.begin(), list.end(), [&](int a, int b) { return files[a].path < files[b].path; });
            for (int i : list) {
                auto* node = &tree;
                for (const auto& part : fs::path(files[i].path).parent_path())
                    node = &node->directories[part.string()];
                node->files.push_back(i);
            }
        }
        rebuild_visible_files();
    }
    void rebuild_visible_files() {
        for (int kind = 0; kind < 3; ++kind) {
            auto& order = visible_files[kind]; order.clear();
            if (!tree_view) order = file_lists[kind];
            else {
                std::function<void(const FileTree&,const std::string&)> visit = [&](const FileTree& node,const std::string& path) {
                    for (const auto& entry : node.directories) {
                        auto folder = path + entry.first + "/";
                        if (!closed_folders[kind].count(folder)) visit(entry.second,folder);
                    }
                    order.insert(order.end(),node.files.begin(),node.files.end());
                };
                visit(file_trees[kind],"");
            }
            const auto& files = kind == 2 ? changed_files : repo.files;
            std::set<std::string> visible;
            for (int index : order) visible.insert(files[index].path);
            auto& selected = file_selection[kind];
            for (auto it = selected.begin(); it != selected.end();)
                if (!visible.count(*it)) it = selected.erase(it); else ++it;
            if (!visible.count(selection_anchor[kind])) selection_anchor[kind].clear();
        }
    }
    void clear_file_selection() {
        for (auto& selected : file_selection) selected.clear();
        for (auto& anchor : selection_anchor) anchor.clear();
        queued_preview_kind = -1;
    }
    std::vector<eg::File> chosen_files(int kind) const {
        std::vector<eg::File> chosen;
        const auto& files = kind == 2 ? changed_files : repo.files;
        for (int index : visible_files[kind])
            if (file_selection[kind].count(files[index].path)) chosen.push_back(files[index]);
        return chosen;
    }
    bool has_file_selection() const {
        return std::any_of(file_selection.begin(),file_selection.end(),[](const auto& files) { return !files.empty(); });
    }
    std::vector<eg::File> discard_targets() const {
        auto files = chosen_files(0);
        if (!has_file_selection()) for (const auto& file : repo.files) if (file.unstaged()) files.push_back(file);
        return files;
    }
    void return_to_graph() {
        show_diff = false; clear_file_selection(); selected_file.clear(); selected_staged = false; set_detail("");
    }
    void pick_file(int index, int kind, bool ctrl, bool shift) {
        if (busy() && !previewing) return;
        const auto& files = kind == 2 ? changed_files : repo.files;
        const auto& file = files[index];
        for (int other = 0; other < 3; ++other) if (other != kind) file_selection[other].clear();
        auto& selected = file_selection[kind]; const auto& order = visible_files[kind];
        auto from = std::find_if(order.begin(),order.end(),[&](int i) { return files[i].path == selection_anchor[kind]; });
        auto to = std::find(order.begin(),order.end(),index);
        if (shift && from != order.end() && to != order.end()) {
            if (!ctrl) selected.clear();
            if (from > to) std::swap(from,to);
            for (auto it = from; it != to+1; ++it) selected.insert(files[*it].path);
        } else {
            if (!ctrl) selected.clear();
            if (ctrl && selected.count(file.path)) selected.erase(file.path); else selected.insert(file.path);
            selection_anchor[kind] = file.path;
        }
        const eg::File* preview = selected.count(file.path) ? &file : nullptr;
        if (!preview) for (int i : order) if (selected.count(files[i].path)) { preview = &files[i]; break; }
        queued_preview_kind = -1;
        if (!preview) show_diff = false;
        else if (busy()) { queued_preview = *preview; queued_preview_kind = kind; }
        else select_file(*preview,kind == 1);
    }
    void change_files(std::vector<eg::File> files, bool staged) {
        bool head = repo.has_head;
        mutate(staged ? "Unstage selected files" : "Stage selected files",[files,staged,head](const eg::Git& git) {
            for (const auto& file : files) {
                if (staged) git.unstage(file,head); else git.stage(file);
            }
        });
    }
    void request_discard(std::vector<eg::File> files,bool all = false) {
        if (files.empty() || !ready()) return;
        pending_files = std::move(files); pending_discard_all = all; pending_kind = "discard files";
    }
    void rebuild_history() {
        history_entries.clear();
        std::map<std::string,std::vector<const eg::Stash*>> by_parent;
        for (const auto& stash : repo.stashes) by_parent[stash.parent].push_back(&stash);
        for (const auto& commit : repo.commits) {
            auto found = by_parent.find(commit.id);
            if (found != by_parent.end()) for (const auto* stash : found->second)
                history_entries.push_back({stash->id,stash->author,stash->date,stash->subject,stash->ref,{stash->parent}});
            history_entries.push_back(commit);
        }
        graph = eg::layout_graph(history_entries); filter();
    }
    void filter() {
        matches.clear();
        std::string query = search;
        auto lower = [](std::string s) {
            for (char& c : s) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
            return s;
        };
        query = lower(query);
        for (int i = 0; i < int(history_entries.size()); ++i) {
            const auto& c = history_entries[i];
            if (query.empty() || lower(c.subject + c.author + c.id + c.refs).find(query) != std::string::npos)
                matches.push_back(i);
        }
    }
    void launch(std::string activity, std::function<JobResult()> action) {
        if (busy()) return;
        *cancel = false; error.clear(); status = std::move(activity);
        job = std::async(std::launch::async, std::move(action));
    }
    void load(const std::string& root) {
        opened_path = root;
        auto stop = cancel;
        int count = limit;
        launch("Reading repository...", [root, count, stop] {
            JobResult r; r.snapshot = eg::Git(root, stop).load(count); r.reload = true;
            r.message = "Repository up to date"; return r;
        });
    }
    void browse_folder() {
        std::string start = path;
        auto stop = cancel;
        launch("Choose a repository folder...", [start, stop] {
            JobResult r; r.folder_selection = true;
            try {
                std::vector<std::string> args = {"zenity", "--file-selection", "--directory",
                    "--title=Select repository folder"};
                std::error_code ec;
                if (!start.empty() && fs::is_directory(start, ec))
                    args.push_back("--filename=" + fs::absolute(start).string() + "/");
                auto selected = eg::run_process(std::move(args), stop, 0);
                if (selected.code == 1) r.message = "Folder selection cancelled";
                else if (selected.code || selected.truncated)
                    throw std::runtime_error(selected.error.empty() ? "Folder selection failed." : selected.error);
                else {
                    // Zenity appends one newline; preserve any newlines in the folder name itself.
                    if (!selected.out.empty() && selected.out.back() == '\n') selected.out.pop_back();
                    if (selected.out.empty() || selected.out.size() >= sizeof(RepoTab::path) ||
                        selected.out.find('\0') != std::string::npos || !fs::is_directory(selected.out, ec))
                        throw std::runtime_error("The selected folder is unavailable or its path is too long.");
                    r.folder = std::move(selected.out);
                    r.message = "Folder selected. Click Open repository to continue.";
                }
            } catch (const std::exception& e) {
                r.error = std::string("Could not browse folders: ") + e.what() +
                    "\nEnsure Zenity is installed, or enter the repository path manually.";
            }
            return r;
        });
    }
    void mutate(std::string activity, std::function<void(const eg::Git&)> action, bool clear = false, bool clear_summary = false) {
        auto root = repo.root; auto stop = cancel; int count = limit;
        std::string summary = message;
        auto preview = workspace && show_diff ? selected_file : std::string(); bool staged = selected_staged;
        launch(activity + "...", [root, stop, count, action, activity, clear, clear_summary, summary, preview, staged] {
            eg::Git git(root, stop);
            JobResult r;
            try { action(git); }
            catch (const std::exception& e) { r.error = e.what(); }
            // A failed merge/stash can still change the worktree. Always read its actual state.
            try {
                r.snapshot = git.load(count); r.reload = true;
                auto found = std::find_if(r.snapshot.files.begin(),r.snapshot.files.end(),[&](const auto& f) { return f.path == preview; });
                if (!preview.empty() && found != r.snapshot.files.end()) {
                    r.preview_file = preview;
                    r.preview_staged = found->staged() && (staged || !found->unstaged());
                    r.detail = git.diff(*found,r.preview_staged);
                }
            }
            catch (const std::exception& e) {
                r.error += std::string("\nRefresh failed: ") + e.what();
            }
            r.message = r.error.empty() ? activity + " completed" : activity + " failed; review the error";
            r.clear_commit = clear && r.error.empty();
            r.clear_summary = clear_summary && r.error.empty(); r.original_summary = summary; return r;
        });
    }
    void command(std::string activity, std::vector<std::string> args) {
        mutate(std::move(activity), [args](const eg::Git& git) { git.checked(args); });
    }
    void poll() {
        if (!busy() || job.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        try {
            auto r = job.get(); previewing = false; generating = false;
            if (!r.created_path.empty()) { requested_open = r.created_path; status = r.message; return; }
            if (r.push_selection) { pending_push = std::move(r.push); pending_kind = "force push"; hard_confirm = false; return; }
            if (r.conflict_selection) {
                conflict = std::move(r.conflict); set_resolution(conflict.working);
                conflict_lines = {}; conflict_selection_key.clear(); resolution_undo.clear(); incoming_first = false;
                conflict_number = 0; remove_resolution = false; binary_chosen = false; conflict_open = true;
                status = "Resolve conflict: " + conflict.path; return;
            }
            if (resolving_conflict) { resolving_conflict = false; if (r.error.empty()) conflict_open = false; }
            if (r.ai_generated) {
                if (r.original_summary != message || r.original_description != description)
                    throw std::runtime_error("Commit draft changed during AI generation. Generate again to replace the current draft.");
                snprintf(message,sizeof(message),"%s",r.generated.summary.c_str());
                snprintf(description,sizeof(description),"%s",r.generated.description.c_str());
                status = "AI commit message generated. Review before committing.";
                return;
            }
            if (r.folder_selection) {
                if (!r.folder.empty()) snprintf(path, sizeof(path), "%s", r.folder.c_str());
                status = r.message; browse_error = r.error;
                return;
            }
            if (r.external_diff) {
                repo.files = std::move(r.files); rebuild_file_lists();
                if (workspace && show_diff && !selected_staged && selected_file == r.preview_file && detail != r.detail)
                    set_detail(std::move(r.detail));
            } else if (r.reload) {
                bool keep_preview = show_diff;
                clear_file_selection();
                just_opened = repo.root.empty();
                repo = std::move(r.snapshot);
                rebuild_history();
                selected_file.clear(); selected_commit.clear(); set_detail(""); workspace = true; show_diff = false;
                changed_files.clear(); commit_body.clear(); stash_view = false; viewed_stash = {}; rebuild_file_lists();
                selected_ref.clear(); scroll_to_commit.clear();
                if (!r.preview_file.empty() && keep_preview) {
                    selected_file = r.preview_file; selected_staged = r.preview_staged;
                    file_selection[r.preview_staged ? 1 : 0].insert(r.preview_file);
                    show_diff = true; set_detail(std::move(r.detail));
                }
                snprintf(path, sizeof(path), "%s", repo.root.c_str());
            } else if (r.commit_selection || r.stash_selection) {
                if (r.stash_selection) viewed_commit = std::move(r.commit);
                changed_files = std::move(r.files); commit_body = std::move(r.detail); rebuild_file_lists();
            } else set_detail(std::move(r.detail));
            if (!r.navigation_ref.empty()) {
                limit = r.history_limit; search[0] = 0; filter();
                workspace = false; viewed_commit = std::move(r.commit);
                selected_ref = r.navigation_ref; selected_commit = scroll_to_commit = viewed_commit.id;
                changed_files = std::move(r.files); commit_body = std::move(r.detail); rebuild_file_lists();
            }
            if (r.clear_commit) { message[0] = 0; description[0] = 0; }
            if (r.clear_summary && r.original_summary == message) message[0] = 0;
            status = r.message;
            error = r.error;
        } catch (const std::exception& e) {
            previewing = false; generating = false;
            error = e.what(); status = "Operation failed. Review the error; refresh if needed.";
        }
        if (queued_preview_kind >= 0) {
            int kind = queued_preview_kind; queued_preview_kind = -1;
            if (file_selection[kind].count(queued_preview.path)) select_file(queued_preview,kind == 1);
        }
    }
    void generate_message() {
        if (!ready() || !ai_settings) return;
        auto config = *ai_settings; auto root = repo.root; auto stop = cancel;
        std::string original_summary = message, original_description = description;
        launch("Generating commit message...",[root,stop,config,original_summary,original_description] {
            JobResult r; r.generated = eg::generate_commit_message(eg::Git(root,stop),config,stop);
            r.ai_generated = true; r.original_summary = original_summary; r.original_description = original_description; return r;
        });
        generating = true;
    }
    void select_commit(const eg::Commit& commit) {
        if (busy()) return;
        clear_file_selection();
        selected_ref.clear(); scroll_to_commit.clear();
        workspace = false; stash_view = false; show_diff = false; selected_file.clear(); selected_commit = commit.id;
        viewed_commit = commit; changed_files.clear(); commit_body.clear(); set_detail(""); rebuild_file_lists();
        auto root = repo.root; auto stop = cancel;
        launch("Loading commit files...", [root, commit, stop] {
            eg::Git git(root, stop);
            JobResult r; r.commit_selection = true; r.files = git.commit_files(commit);
            r.detail = git.checked({"show", "--no-patch", "--format=%B", commit.id, "--"});
            r.message = "Commit " + commit.id.substr(0, 8); return r;
        });
    }
    void navigate_ref(const eg::Ref& ref) {
        if (!ready()) return;
        auto found = std::find_if(repo.commits.begin(),repo.commits.end(),[&](const auto& c) { return c.id == ref.id; });
        if (found != repo.commits.end()) {
            search[0] = 0; filter(); select_commit(*found);
            selected_ref = ref.full; scroll_to_commit = ref.id;
            return;
        }
        auto root = repo.root; auto stop = cancel; int count = limit;
        launch("Locating branch in history...",[root,stop,count,ref]() mutable {
            eg::Git git(root,stop); JobResult r;
            for (;;) {
                count += std::max(300,count); r.snapshot = git.load(count);
                auto found = std::find_if(r.snapshot.commits.begin(),r.snapshot.commits.end(),[&](const auto& c) { return c.id == ref.id; });
                if (found != r.snapshot.commits.end()) { r.commit = *found; break; }
                if (!r.snapshot.more) throw std::runtime_error("Branch target is no longer in the history. Refresh the repository.");
            }
            r.reload = true; r.history_limit = count; r.navigation_ref = ref.full;
            r.files = git.commit_files(r.commit);
            r.detail = git.checked({"show","--no-patch","--format=%B",ref.id,"--"});
            r.message = "Viewing " + ref.name; return r;
        });
    }
    void select_stash(const eg::Stash& stash) {
        if (busy()) return;
        clear_file_selection();
        selected_ref.clear(); scroll_to_commit.clear();
        workspace = false; stash_view = true; show_diff = false; viewed_stash = stash;
        selected_commit.clear(); selected_file.clear(); viewed_commit = {};
        changed_files.clear(); commit_body.clear(); set_detail(""); rebuild_file_lists();
        auto root = repo.root; auto stop = cancel;
        launch("Loading stash files...", [root, stop, stash] {
            eg::Git git(root,stop); JobResult r; r.stash_selection = true;
            r.files = git.stash_files(stash); r.commit = git.read_commit(stash.id);
            r.detail = stash.subject; r.message = "Previewing " + stash.subject; return r;
        });
    }
    void select_file(const eg::File& file, bool staged) {
        if (busy()) return;
        selected_file = file.path; selected_staged = staged; show_diff = true; set_detail("");
        watched_stamp = file_stamp();
        auto root = repo.root; auto stop = cancel; bool working = workspace; auto commit = viewed_commit;
        bool saved = stash_view; auto stash = viewed_stash;
        launch("Loading diff...", [root, stop, file, staged, working, commit, saved, stash] {
            eg::Git git(root, stop); JobResult r;
            if (working && file.conflicted()) { r.conflict = git.read_conflict(file); r.conflict_selection = true; return r; }
            r.detail = working ? git.diff(file, staged) : saved ? git.stash_diff(stash,file) : git.commit_diff(commit, file);
            r.message = "Viewing " + visible_path(file.path); return r;
        });
        previewing = true;
    }
    std::string file_stamp() const {
        struct stat info{};
        if (stat((fs::path(repo.root)/selected_file).c_str(),&info) != 0) return {};
        return std::to_string(info.st_dev)+":"+std::to_string(info.st_ino)+":"+std::to_string(info.st_size)+":"+
            std::to_string(info.st_mtim.tv_sec)+":"+std::to_string(info.st_mtim.tv_nsec)+":"+
            std::to_string(info.st_ctim.tv_sec)+":"+std::to_string(info.st_ctim.tv_nsec);
    }
    void refresh_external_diff() {
        if (!ready() || !workspace || !show_diff || selected_staged || selected_file.empty() ||
            conflict_open || !pending_kind.empty() || ImGui::GetTime()-diff_checked_at < 0.5) return;
        diff_checked_at = ImGui::GetTime();
        auto stamp = file_stamp();
        if (stamp == watched_stamp) return;
        watched_stamp = std::move(stamp);
        auto root = repo.root; auto stop = cancel; auto name = selected_file;
        launch("Updating file diff...",[root,stop,name] {
            eg::Git git(root,stop); JobResult r; r.external_diff = true; r.preview_file = name;
            r.files = eg::parse_status(git.checked({"status","--porcelain=v1","-z","--untracked-files=all"}));
            auto found = std::find_if(r.files.begin(),r.files.end(),[&](const auto& f) { return f.path == name; });
            if (found != r.files.end() && found->unstaged() && !found->conflicted()) r.detail = git.diff(*found,false);
            r.message = "Updated " + visible_path(name); return r;
        });
        previewing = true;
    }
    void select_workspace() {
        if (busy()) return;
        clear_file_selection();
        selected_ref.clear(); scroll_to_commit.clear();
        workspace = true; stash_view = false; show_diff = false; selected_commit.clear(); selected_file.clear(); set_detail("");
    }

    void header() {
        ImGui::BeginChild("toolbar", {0, 68}, ImGuiChildFlags_AlwaysUseWindowPadding);
        bool compact = ImGui::GetContentRegionAvail().x < 1150;
        auto* draw = ImGui::GetWindowDrawList();
        auto p = ImGui::GetCursorScreenPos();
        draw->AddBezierCubic({p.x+8,p.y+29}, {p.x+8,p.y+14}, {p.x+30,p.y+26}, {p.x+30,p.y+9}, lane_colors[0], 2.5f);
        draw->AddLine({p.x+8,p.y+4}, {p.x+8,p.y+30}, lane_colors[0], 2.5f);
        for (auto v : {ImVec2(8,4), ImVec2(8,30), ImVec2(30,9)})
            draw->AddCircleFilled({p.x+v.x,p.y+v.y}, 4, lane_colors[0]);
        ImGui::Dummy({40,36}); ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::PushFont(body_font, 20); ImGui::TextUnformatted("easy git"); ImGui::PopFont();
        ImGui::PushFont(body_font, 12); label("YOUR REPOSITORY, CONNECTED"); ImGui::PopFont();
        ImGui::EndGroup(); ImGui::SameLine(285);
        auto name = repo.root.empty() ? "Open repository" : "+ Open repository";
        if (button((std::string(name) + "##open_repo").c_str(), !busy(), {compact ? 180.0f : 200.0f,36})) opening = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", repo.root.empty() ? "Open a local Git repository" : repo.root.c_str());
        ImGui::SameLine(); ImGui::SetNextItemWidth(compact ? 150 : 180);
        ImGui::BeginDisabled(!idle());
        if (ImGui::BeginCombo("##branch", repo.branch.empty() ? "No branch" : repo.branch.c_str())) {
            for (const auto& ref : repo.refs) if (ref.full.rfind("refs/heads/",0) == 0) {
                if (ImGui::Selectable(ref.name.c_str(), ref.name == repo.branch))
                    command("Switch branch", {"switch", "--", ref.name});
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        ImGui::SameLine(); if (button("Fetch", ready(), {70,36})) command("Fetch", {"fetch", "--all"});
        ImGui::SameLine(); if (button("Pull", idle(), {64,36})) command("Pull", {"pull", "--ff-only"});
        ImGui::SameLine(); if (button("Push", ready(), {64,36})) mutate("Push", [](const eg::Git& git) { git.push(); });
        if (ImGui::BeginPopupContextItem("push_menu")) {
            if (ImGui::MenuItem("Force push with lease...",nullptr,false,ready() && repo.has_head)) prepare_force_push();
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        if (button("Stash",idle() && repo.has_head && !repo.files.empty(),{64,36})) {
            std::string summary = message;
            mutate("Stash",[summary](const eg::Git& git) { git.save_stash(summary); },false,true);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Save all staged, unstaged and untracked changes (excluding ignored files).\nName: %s",message[0] ? message : "Saved from easy git");
        ImGui::SameLine(); if (button("Refresh", ready(), {83,36})) load(repo.root);
        ImGui::EndChild();
    }

    bool sidebar_matches(const std::string& name) const {
        std::string query = sidebar_search;
        auto lower = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a'-'A') : c; };
        return std::search(name.begin(),name.end(),query.begin(),query.end(),
            [&](unsigned char a,unsigned char b) { return lower(a)==lower(b); }) != name.end() || query.empty();
    }
    void refs_section(const std::string& prefix) {
        int count = 0;
        for (const auto& ref : repo.refs) {
            if (ref.full.rfind(prefix,0) != 0 || !sidebar_matches(ref.name)) continue;
            ImGui::PushID(ref.full.c_str()); ++count;
            auto pos = ImGui::GetCursorScreenPos();
            bool current = ref.full == "refs/heads/" + repo.branch;
            bool selected = selected_ref.empty() ? current : selected_ref == ref.full;
            if (ImGui::Selectable("##ref", selected, 0, {0,26}) && !busy()) navigate_ref(ref);
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddCircleFilled({pos.x+7,pos.y+12},3, current ? lane_colors[0] : IM_COL32(121,132,155,255));
            text_clipped(draw, {pos.x+22,pos.y+3}, ref.name,
                ImGui::GetColorU32(selected ? mint : ImGui::GetStyleColorVec4(ImGuiCol_Text)),
                {pos.x+ImGui::GetContentRegionAvail().x,pos.y+26});
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", ref.name.c_str());
            if (ImGui::BeginPopupContextItem()) {
                if (prefix == "refs/heads/" && ImGui::MenuItem("Switch to branch", nullptr, false, idle()))
                    command("Switch branch", {"switch", "--", ref.name});
                if (ImGui::MenuItem("Merge into current branch...",nullptr,false,idle() && repo.has_head)) {
                    eg::Commit target; target.id = ref.id; target.subject = ref.name;
                    request_operation("merge",target);
                }
                if ((prefix == "refs/heads/" || prefix == "refs/remotes/") &&
                    ImGui::MenuItem("Delete branch...",nullptr,false,idle() && !current && ref.name.find("/HEAD") == std::string::npos)) {
                    pending_branch = ref; pending_kind = "delete branch"; hard_confirm = false;
                }
                if (current && ImGui::MenuItem("Force push with lease...",nullptr,false,ready())) prepare_force_push();
                if (ImGui::MenuItem("Copy reference")) ImGui::SetClipboardText(ref.full.c_str());
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        if (!count) label(sidebar_search[0] ? "  No matches" : "  None");
        if (prefix == "refs/heads/" && button("+ New branch",idle(),{-1,0})) { new_kind = "branch"; new_name[0] = 0; }
        if (prefix == "refs/tags/" && button("+ New tag",ready() && repo.has_head,{-1,0})) { new_kind = "tag"; new_name[0] = 0; }
    }
    std::array<float,4> sidebar_heights(float space) const {
        std::array<float,4> heights{};
        int count = std::count(sidebar_open.begin(),sidebar_open.end(),true);
        if (!count) return heights;
        space = std::max(0.0f,space);
        float minimum = std::min(32.0f,space/count), total = 0;
        for (int i=0;i<4;++i) if (sidebar_open[i]) total += sidebar_weights[i];
        for (int i=0;i<4;++i) if (sidebar_open[i])
            heights[i] = minimum + (space-minimum*count)*(total > 0 ? sidebar_weights[i]/total : 1.0f/count);
        return heights;
    }
    void sidebar() {
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##sidebar_search","Filter sidebar...",sidebar_search,sizeof(sidebar_search));
        section("WORKSPACE");
        if (sidebar_matches("Working changes")) {
            if (ImGui::Selectable("Working changes", workspace, 0, {0,30})) select_workspace();
            label((std::to_string(repo.files.size()) + " changed files").c_str());
        } else label("  No matches");
        const char* titles[] = {"LOCAL","REMOTE","TAGS","STASH"};
        const char* prefixes[] = {"refs/heads/","refs/remotes/","refs/tags/"};
        auto pos = ImGui::GetCursorScreenPos();
        float width = ImGui::GetContentRegionAvail().x, header = ImGui::GetFrameHeight();
        float space = std::max(0.0f,ImGui::GetContentRegionAvail().y-4*(header+2)-3*6);
        auto heights = sidebar_heights(space);
        auto open = sidebar_open;
        for (int i=0;i<4;++i) {
            ImGui::PushID(titles[i]);
            ImGui::SetCursorScreenPos(pos);
            ImGui::SetNextItemOpen(open[i],ImGuiCond_Always);
            sidebar_open[i] = ImGui::CollapsingHeader(titles[i]);
            pos.y += header+2;
            if (open[i] && heights[i] > 0) {
                ImGui::SetCursorScreenPos(pos);
                if (ImGui::BeginChild("items",{width,heights[i]},ImGuiChildFlags_None)) {
                    if (i<3) refs_section(prefixes[i]); else stash_section();
                }
                ImGui::EndChild();
                pos.y += heights[i];
            }
            if (i<3) {
                ImGui::SetCursorScreenPos(pos);
                int next = i+1; while (next<4 && !open[next]) ++next;
                ImGui::BeginDisabled(!open[i] || next==4);
                ImGui::InvisibleButton("resize",{width,6});
                bool active = ImGui::IsItemActive(), hovered = ImGui::IsItemHovered();
                if (active || hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                ImGui::GetWindowDrawList()->AddLine({pos.x,pos.y+3},{pos.x+width,pos.y+3},
                    ImGui::GetColorU32(active ? ImGuiCol_SeparatorActive : hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator),2);
                if (active && ImGui::GetIO().MouseDelta.y != 0 && next<4) {
                    int count = std::count(open.begin(),open.end(),true);
                    float minimum = std::min(32.0f,space/count);
                    float delta = std::clamp(ImGui::GetIO().MouseDelta.y,minimum-heights[i],heights[next]-minimum);
                    auto resized = heights; resized[i] += delta; resized[next] -= delta;
                    float total = 0, extra = space-minimum*count;
                    for (int j=0;j<4;++j) if (open[j]) total += sidebar_weights[j];
                    if (extra > 0) for (int j=0;j<4;++j) if (open[j])
                        sidebar_weights[j] = std::max(0.0f,resized[j]-minimum)*(total > 0 ? total : float(count))/extra;
                }
                ImGui::EndDisabled();
                pos.y += 6;
            }
            ImGui::PopID();
        }
    }
    void stash_section() {
        int count = 0;
        for (const auto& stash : repo.stashes) {
            if (!sidebar_matches(stash.subject)) continue;
            ++count;
            ImGui::PushID(stash.ref.c_str());
            auto pos = ImGui::GetCursorScreenPos();
            if (ImGui::Selectable("##stash",stash_view && viewed_stash.id == stash.id,0,{0,26}) && ready()) select_stash(stash);
            text_clipped(ImGui::GetWindowDrawList(),{pos.x+5,pos.y+4},stash.subject,
                ImGui::GetColorU32(ImGuiCol_Text),{pos.x+ImGui::GetContentRegionAvail().x,pos.y+26});
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\nClick to preview",stash.subject.c_str());
            if (ImGui::BeginPopupContextItem()) {
                stash_actions(stash);
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        if (!count) label(sidebar_search[0] ? "  No matches" : "  None");
        if (button("Stash changes",idle() && repo.has_head && !repo.files.empty(),{-1,0})) {
            new_kind = "stash"; new_name[0] = 0;
        }
    }
    void stash_actions(const eg::Stash& stash) {
        if (ImGui::MenuItem("Apply...",nullptr,false,idle())) {
            pending_stash = stash; pending_kind = "apply stash"; restore_index = false;
        }
        if (ImGui::MenuItem("Delete...",nullptr,false,ready())) {
            pending_stash = stash; pending_kind = "delete stash";
        }
    }
    void request_operation(const std::string& kind, const eg::Commit& commit) {
        pending_kind = kind; pending_commit = commit;
        reset_mode = 1; mainline = 1; hard_confirm = false;
    }
    void commit_actions(const eg::Commit& commit) {
        bool enabled = idle() && repo.has_head;
        if (ImGui::MenuItem("Cherry-pick...",nullptr,false,enabled)) request_operation("cherry-pick",commit);
        if (ImGui::MenuItem("Merge into current branch...",nullptr,false,enabled)) request_operation("merge",commit);
        if (ImGui::MenuItem("Revert...",nullptr,false,enabled)) request_operation("revert",commit);
        if (ImGui::MenuItem("Reset current branch here...",nullptr,false,enabled)) request_operation("reset",commit);
    }

    void graph_row(const eg::GraphRow& row, ImVec2 p, float height) {
        auto* draw = ImGui::GetWindowDrawList();
        auto x = [&](int lane) { return p.x + 15 + lane * 19.0f; };
        float mid = p.y + height * 0.5f;
        for (const auto& edge : row.segments) {
            float y1 = edge.incoming ? p.y : mid, y2 = edge.incoming ? mid : p.y + height;
            auto color = lane_colors[edge.color % 6];
            if (edge.from == edge.to) draw->AddLine({x(edge.from),y1}, {x(edge.to),y2}, color, 2);
            else draw->AddBezierCubic({x(edge.from),y1}, {x(edge.from),y2}, {x(edge.to),y1}, {x(edge.to),y2}, color, 2);
        }
        draw->AddCircleFilled({x(row.lane),mid}, 6, ImGui::GetColorU32(ImGuiCol_ChildBg));
        draw->AddCircle({x(row.lane),mid}, 4.5f, lane_colors[row.lane % 6], 16, 2);
    }
    void history() {
        ImGui::PushFont(body_font, 22); ImGui::TextUnformatted("Commit graph"); ImGui::PopFont();
        ImGui::SameLine(); ImGui::TextColored(muted, "  %zu commits / %zu stashes", repo.commits.size(),repo.stashes.size());
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint("##search", "Search commits, stashes, authors, refs or SHA...", search, sizeof(search))) filter();
        if (search[0]) label("Filtered results: graph hidden to avoid false connections.");
        else label("All branches  /  stashes beside their base commits");
        ImGui::Spacing();
        if (ImGui::Selectable(("// WIP    " + std::to_string(repo.files.size()) + " file changes").c_str(),
            workspace, 0, {0, 30})) select_workspace();
        const float row_height = 34;
        int max_lanes = 1;
        for (const auto& row : graph) max_lanes = std::max(max_lanes, row.width);
        float graph_width = search[0] ? 0 : std::max(92.0f, 30.0f + 19.0f * max_lanes);
        auto flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV;
        float footer = repo.more ? 44.0f : 6.0f;
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {8,0});
        if (ImGui::BeginTable("history", search[0] ? 3 : 4, flags, {0, -footer})) {
            ImGui::TableSetupScrollFreeze(0,1);
            if (!search[0]) ImGui::TableSetupColumn("GRAPH", ImGuiTableColumnFlags_WidthFixed, graph_width);
            ImGui::TableSetupColumn("COMMIT", ImGuiTableColumnFlags_WidthStretch, 1, 1);
            ImGui::TableSetupColumn("AUTHOR", ImGuiTableColumnFlags_WidthFixed, 100, 2);
            ImGui::TableSetupColumn("LOCAL TIME", ImGuiTableColumnFlags_WidthFixed, 155, 3);
            ImGui::TableHeadersRow();
            ImGuiListClipper clipper; clipper.Begin(int(matches.size()), row_height);
            if (!scroll_to_commit.empty()) {
                auto target = std::find_if(matches.begin(),matches.end(),[&](int i) { return history_entries[i].id == scroll_to_commit; });
                if (target != matches.end()) clipper.IncludeItemByIndex(int(target-matches.begin()));
            }
            while (clipper.Step()) for (int n = clipper.DisplayStart; n < clipper.DisplayEnd; ++n) {
                int i = matches[n]; const auto& c = history_entries[i];
                auto saved = std::find_if(repo.stashes.begin(),repo.stashes.end(),[&](const auto& s) { return s.id == c.id && s.ref == c.refs; });
                const eg::Stash* stash = saved == repo.stashes.end() ? nullptr : &*saved;
                ImGui::PushID(stash ? stash->ref.c_str() : c.id.c_str());
                ImGui::TableNextRow(0,row_height);
                int col = 0;
                if (!search[0]) {
                    ImGui::TableSetColumnIndex(col++);
                    graph_row(graph[i], ImGui::GetCursorScreenPos(), row_height);
                    ImGui::Dummy({graph_width,row_height});
                }
                ImGui::TableSetColumnIndex(col++);
                auto p = ImGui::GetCursorScreenPos();
                float width = ImGui::GetContentRegionAvail().x;
                if (ImGui::Selectable("##commit", stash ? stash_view && viewed_stash.id == c.id : selected_commit == c.id, ImGuiSelectableFlags_SpanAllColumns,
                    {0,row_height}) && !busy()) { if (stash) select_stash(*stash); else select_commit(c); }
                if (scroll_to_commit == c.id) {
                    ImGui::SetScrollHereY(0.5f); ImGui::SetScrollX(0); scroll_to_commit.clear();
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%s\n%s\n%s", c.subject.c_str(), stash ? "Stash" : c.refs.c_str(), c.id.c_str(),local_commit_time(c.date).c_str());
                if (ImGui::BeginPopupContextItem()) {
                    if (stash) stash_actions(*stash);
                    else {
                        if (ImGui::MenuItem("Copy commit SHA")) ImGui::SetClipboardText(c.id.c_str());
                        if (ImGui::MenuItem("View commit", nullptr, false, !busy())) select_commit(c);
                        ImGui::Separator(); commit_actions(c);
                    }
                    ImGui::EndPopup();
                }
                auto* draw = ImGui::GetWindowDrawList();
                float offset = 0;
                if (stash || !c.refs.empty()) {
                    std::string ref = stash ? "STASH" : c.refs;
                    if (ref.size() > 26) ref = ref.substr(0,23) + "...";
                    float badge = std::min(ImGui::CalcTextSize(ref.c_str()).x + 14, std::max(0.0f, width*0.48f));
                    ImU32 background = stash ? (light_theme ? IM_COL32(234,220,250,255) : IM_COL32(70,49,94,255))
                                             : (light_theme ? IM_COL32(204,231,222,255) : IM_COL32(43,81,78,255));
                    ImU32 foreground = stash ? (light_theme ? IM_COL32(105,53,150,255) : IM_COL32(213,177,248,255)) : lane_colors[0];
                    draw->AddRectFilled({p.x,p.y+6}, {p.x+badge,p.y+28}, background, 4);
                    text_clipped(draw, {p.x+7,p.y+7}, ref, foreground, {p.x+badge-3,p.y+29});
                    offset = badge + 9;
                }
                text_clipped(draw, {p.x+offset,p.y+8}, c.subject, ImGui::GetColorU32(ImGuiCol_Text), {p.x+width,p.y+33});
                ImGui::TableSetColumnIndex(col++);
                p = ImGui::GetCursorScreenPos();
                text_clipped(draw, {p.x,p.y+8}, c.author, ImGui::GetColorU32(muted), {p.x+ImGui::GetContentRegionAvail().x,p.y+33});
                ImGui::TableSetColumnIndex(col);
                p = ImGui::GetCursorScreenPos();
                text_clipped(draw, {p.x,p.y+8}, local_commit_time(c.date).substr(0,16), ImGui::GetColorU32(muted), {p.x+ImGui::GetContentRegionAvail().x,p.y+33});
                ImGui::PopID();
            }
            if (matches.empty()) {
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
                ImGui::TextWrapped("%s", repo.root.empty() ? "Open a repository to explore its history." :
                    repo.commits.empty() ? "No commits yet. Stage a file and make your first commit." : "No matching commits.");
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
        if (repo.more && button("Load 300 more commits", ready(), {-1,30})) { limit += 300; load(repo.root); }
    }

    void create_repository() {
        std::string destination = fs::absolute(path).string(), url = clone_url, branch = initial_branch;
        int mode = open_mode; auto stop = cancel;
        launch(mode == 1 ? "Cloning repository..." : "Initializing repository...",[destination,url,branch,mode,stop] {
            if (mode == 1) eg::Git::clone(url,destination,stop); else eg::Git::initialize(destination,branch);
            eg::Git(destination,stop).load(1);
            JobResult r; r.created_path = destination; r.message = "Repository ready"; return r;
        });
    }
    void prepare_force_push() {
        auto root = repo.root; auto stop = cancel;
        launch("Reading push target...",[root,stop] {
            JobResult r; r.push = eg::Git(root,stop).push_target(); r.push_selection = true; return r;
        });
    }
    bool partial_available() const {
        if (!workspace || !ready()) return false;
        auto found = std::find_if(repo.files.begin(),repo.files.end(),[&](const auto& f) { return f.path == selected_file; });
        return found != repo.files.end() && !found->conflicted() && found->original.empty();
    }
    bool changed_line(int i) const {
        const auto& row = detail_lines[i];
        return (row.before || row.after) && !row.text.empty() && (row.text[0] == '+' || row.text[0] == '-');
    }
    void pick_line(int index,bool ctrl,bool shift) {
        if (!ctrl) selected_lines.clear();
        if (shift && line_anchor >= 0) {
            for (int i = std::min(index,line_anchor); i <= std::max(index,line_anchor); ++i)
                if (changed_line(i)) selected_lines.insert(i);
        } else {
            auto linked = eg::expand_line_selection(detail,{index});
            bool remove = ctrl && selected_lines.count(index);
            for (int i : linked) { if (remove) selected_lines.erase(i); else selected_lines.insert(i); }
            line_anchor = index;
        }
        auto linked = eg::expand_line_selection(detail,{selected_lines.begin(),selected_lines.end()});
        selected_lines = {linked.begin(),linked.end()};
    }
    void stage_diff_lines(std::vector<int> selection) {
        auto found = std::find_if(repo.files.begin(),repo.files.end(),[&](const auto& f) { return f.path == selected_file; });
        if (!partial_available() || found == repo.files.end()) return;
        auto file = *found; auto patch = detail; bool unstage = selected_staged;
        mutate(unstage ? "Unstage selected lines" : "Stage selected lines",[file,patch,unstage,selection](const eg::Git& git) {
            git.stage_lines(file,unstage,patch,selection);
        });
    }
    std::vector<int> hunk_lines(int header) const {
        std::vector<int> selected;
        for (int i = header+1; i < int(detail_lines.size()); ++i) {
            if (detail_lines[i].text.rfind("@@",0) == 0 || detail_lines[i].text.rfind("diff ",0) == 0) break;
            if (changed_line(i)) selected.push_back(i);
        }
        return selected;
    }
    void stage_hunk(int header) { stage_diff_lines(hunk_lines(header)); }
    void request_discard_lines(std::vector<int> selection,bool hunk = false) {
        if (!partial_available() || selected_staged || selection.empty()) return;
        auto found = std::find_if(repo.files.begin(),repo.files.end(),[&](const auto& f) { return f.path == selected_file; });
        if (found == repo.files.end()) return;
        pending_files = {*found}; pending_diff = detail; pending_diff_lines = eg::expand_line_selection(detail,selection);
        pending_kind = hunk ? "discard hunk" : "discard lines";
    }
    void set_resolution(const std::string& text) {
        resolution.assign(1024*1024+1,0);
        std::copy_n(text.data(),std::min(text.size(),resolution.size()-1),resolution.data());
    }
    struct ConflictBlock { size_t begin, ours, base, divider, theirs, closing, end; };
    std::vector<ConflictBlock> conflict_blocks() const {
        std::string text(resolution.empty() ? "" : resolution.data()); std::vector<ConflictBlock> blocks;
        auto marker = [&](const char* token,size_t start) {
            for (auto pos = text.find(token,start); pos != std::string::npos; pos = text.find(token,pos+1))
                if (pos == 0 || text[pos-1] == '\n') return pos;
            return std::string::npos;
        };
        size_t pos = 0;
        while ((pos = marker("<<<<<<<",pos)) != std::string::npos) {
            auto ours = text.find('\n',pos), divider = marker("=======",pos), end = marker(">>>>>>>",pos);
            if (ours == std::string::npos || divider == std::string::npos || end == std::string::npos || divider > end) break;
            auto theirs = text.find('\n',divider), after = text.find('\n',end), base = marker("|||||||",ours);
            if (theirs == std::string::npos) break;
            blocks.push_back({pos,ours+1,base < divider ? base : divider,divider,theirs+1,end,after == std::string::npos ? text.size() : after+1});
            pos = blocks.back().end;
        }
        return blocks;
    }
    void choose_conflict_block(int choice) {
        auto blocks = conflict_blocks(); if (blocks.empty()) return;
        auto block = blocks[std::min(conflict_number,int(blocks.size())-1)]; std::string text(resolution.data());
        auto closing = block.closing;
        auto ours = text.substr(block.ours,block.base-block.ours), theirs = text.substr(block.theirs,closing-block.theirs);
        text.replace(block.begin,block.end-block.begin,choice == 0 ? ours : choice == 1 ? theirs : incoming_first ? theirs+ours : ours+theirs);
        replace_resolution(text);
    }
    void replace_resolution(const std::string& text) {
        if (text.size()>1024*1024) { error="Resolution exceeds 1 MiB."; return; }
        if (resolution_undo.size()==16) resolution_undo.erase(resolution_undo.begin());
        resolution_undo.emplace_back(resolution.data());
        set_resolution(text); conflict_selection_key.clear(); conflict_lines = {};
    }
    std::array<std::vector<std::string>,2> conflict_choices() {
        std::array<std::vector<std::string>,2> result;
        auto blocks=conflict_blocks(); if (blocks.empty()) return result;
        conflict_number=std::clamp(conflict_number,0,int(blocks.size())-1);
        auto b=blocks[conflict_number]; std::string text(resolution.data());
        auto key=std::to_string(b.begin)+":"+text.substr(b.begin,b.end-b.begin);
        if (key!=conflict_selection_key) { conflict_lines={}; conflict_selection_key=std::move(key); }
        for (int side=0;side<2;++side) {
            size_t start=side==0 ? b.ours : b.theirs, end=side==0 ? b.base : b.closing;
            while (start<end) {
                auto newline=text.find('\n',start);
                size_t next=newline==std::string::npos ? end : std::min(end,newline+1);
                result[side].push_back(text.substr(start,next-start)); start=next;
            }
        }
        return result;
    }
    void apply_conflict_lines() {
        auto choices=conflict_choices(); auto blocks=conflict_blocks();
        if (blocks.empty() || (conflict_lines[0].empty() && conflict_lines[1].empty())) return;
        std::string replacement;
        for (int turn=0;turn<2;++turn) {
            int side=incoming_first ? 1-turn : turn;
            for (int i : conflict_lines[side]) if (i>=0 && i<int(choices[side].size())) replacement+=choices[side][i];
        }
        auto block=blocks[conflict_number]; std::string text(resolution.data());
        text.replace(block.begin,block.end-block.begin,replacement); replace_resolution(text);
    }
    void conflict_dialog() {
        if (conflict_open && !ImGui::IsPopupOpen("Resolve conflict")) ImGui::OpenPopup("Resolve conflict");
        auto size = ImGui::GetMainViewport()->WorkSize;
        ImGui::SetNextWindowSize({std::min(1280.0f,size.x-40),std::min(850.0f,size.y-50)},ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal("Resolve conflict",nullptr)) return;
        if (!conflict_open) { ImGui::CloseCurrentPopup(); ImGui::EndPopup(); return; }
        ImGui::TextWrapped("%s",visible_path(conflict.path).c_str());
        label("Compare versions, choose lines or blocks, then review the output.");
        if (repo.operation == "rebase") ImGui::TextWrapped("Rebase: Current is the rebased base; Incoming is the commit being replayed.");
        ImGui::BeginDisabled(busy());
        auto blocks=conflict_blocks();
        if (!blocks.empty() && !conflict.binary && !remove_resolution) {
            conflict_number=std::clamp(conflict_number,0,int(blocks.size())-1);
            if (button("< Previous",conflict_number>0)) --conflict_number;
            ImGui::SameLine(); if (button("Next >",conflict_number+1<int(blocks.size()))) ++conflict_number;
            if (!ImGui::GetIO().WantTextInput) {
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && conflict_number>0) --conflict_number;
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && conflict_number+1<int(blocks.size())) ++conflict_number;
            }
            ImGui::SameLine(); ImGui::Text("Conflict %d / %zu",conflict_number+1,blocks.size());
        }
        auto choices=conflict_choices();
        float source_height=std::clamp(ImGui::GetContentRegionAvail().y*0.38f,100.0f,250.0f);
        if (ImGui::BeginTable("versions",2,ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable)) {
            for (int side=0;side<2;++side) {
                ImGui::TableNextColumn(); ImGui::PushID(side);
                const auto& content=side==0 ? conflict.ours : conflict.theirs;
                bool exists=side==0 ? conflict.has_ours : conflict.has_theirs;
                ImGui::TextColored(side==0 ? mint : ImVec4(0.45f,0.65f,0.95f,1),"%s",side==0 ? "CURRENT / ours" : "INCOMING / theirs");
                if (button(exists ? "Use entire file" : "Accept deletion")) {
                    replace_resolution(content); remove_resolution=!exists; binary_chosen=true; binary_resolution=content;
                }
                ImGui::SameLine();
                if (button("Take block",!blocks.empty() && !conflict.binary && !remove_resolution)) choose_conflict_block(side);
                ImGui::BeginChild("source",{0,source_height},ImGuiChildFlags_Borders,ImGuiWindowFlags_HorizontalScrollbar);
                if (!exists) label("Deleted in this version");
                else if (conflict.binary) label("Binary file: choose an entire version");
                else {
                    ImGui::PushFont(mono_font,13);
                    if (!blocks.empty() && !remove_resolution) {
                        for (int i=0;i<int(choices[side].size());++i) {
                            ImGui::PushID(i); bool selected=conflict_lines[side].count(i);
                            if (ImGui::Checkbox("##include",&selected)) {
                                if (selected) conflict_lines[side].insert(i); else conflict_lines[side].erase(i);
                            }
                            ImGui::SameLine(); ImGui::TextColored(muted,"%3d",i+1); ImGui::SameLine();
                            auto line=choices[side][i]; if (!line.empty() && line.back()=='\n') line.pop_back();
                            ImGui::TextUnformatted(line.c_str()); ImGui::PopID();
                        }
                        if (choices[side].empty()) label("No lines on this side of the conflict");
                    } else ImGui::TextUnformatted(content.c_str());
                    ImGui::PopFont();
                }
                ImGui::EndChild(); ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (!conflict.binary && !remove_resolution) {
            if (button("Apply selected lines",!conflict_lines[0].empty() || !conflict_lines[1].empty())) apply_conflict_lines();
            ImGui::SameLine(); if (button("Take both blocks",!conflict_blocks().empty())) choose_conflict_block(2);
            ImGui::SameLine(); ImGui::Checkbox("Incoming first",&incoming_first);
            ImGui::SameLine();
            if (button("Undo choice",!resolution_undo.empty())) {
                auto previous=std::move(resolution_undo.back()); resolution_undo.pop_back();
                set_resolution(previous); conflict_selection_key.clear(); conflict_lines={};
            }
            bool unresolved=eg::has_conflict_markers(resolution.data());
            ImGui::TextColored(unresolved ? ImVec4(0.9f,0.65f,0.3f,1) : mint,"OUTPUT - %s",unresolved ? "unresolved conflicts remain" : "ready to save");
            ImGui::PushFont(mono_font,14);
            ImGui::InputTextMultiline("##resolution",resolution.data(),resolution.size(),{-1,std::max(70.0f,ImGui::GetContentRegionAvail().y-58)},ImGuiInputTextFlags_AllowTabInput);
            ImGui::PopFont();
        } else ImGui::TextUnformatted(remove_resolution ? "Output: delete this file" : binary_chosen ? "Output: selected binary version" : "Choose a whole version for the output.");
        bool can_save=remove_resolution || (conflict.binary ? binary_chosen : !eg::has_conflict_markers(resolution.data()));
        if (button("Save and mark resolved",can_save)) {
            auto saved=conflict; bool remove=remove_resolution;
            auto result=conflict.binary ? binary_resolution : std::string(resolution.data());
            mutate("Resolve conflict",[saved,result,remove](const eg::Git& git) { git.save_resolution(saved,result,remove); });
            resolving_conflict=true;
        }
        ImGui::SameLine(); if (button("Cancel")) { conflict_open=false; ImGui::CloseCurrentPopup(); }
        ImGui::EndDisabled(); ImGui::EndPopup();
    }
    void diff_view() {
        ImGui::BeginChild("diff", {0,0}, 0, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PushFont(mono_font, 14);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0,0});
        if (detail_lines.empty()) {
            ImGui::TextColored(muted, "%s", busy() ? "Loading file diff..." : "No textual changes in this file.");
        } else {
            const float h = 23;
            ImGuiListClipper clipper; clipper.Begin(int(detail_lines.size()), h);
            while (clipper.Step()) for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const auto& row = detail_lines[i]; const auto& line = row.text;
                auto p = ImGui::GetCursorScreenPos(); auto* draw = ImGui::GetWindowDrawList();
                float width = std::max(ImGui::GetContentRegionAvail().x, ImGui::CalcTextSize(line.c_str()).x + 110);
                ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                if (!line.empty() && (row.before || row.after) && (line[0] == '+' || line[0] == '-')) {
                    bool added = line[0] == '+';
                    draw->AddRectFilled(p, {p.x+width,p.y+h}, added ? IM_COL32(44,92,69,35) : IM_COL32(125,51,64,30));
                    color = added ? mint : red;
                } else if (line.rfind("@@",0) == 0 || line.rfind("diff ",0) == 0) color = light_theme ? ImVec4(0.43f,0.27f,0.66f,1) : ImVec4(0.66f,0.57f,0.91f,1);
                if (selected_lines.count(i)) draw->AddRectFilled(p,{p.x+width,p.y+h},ImGui::GetColorU32(ImGuiCol_Header));
                auto number = [&](int n, float x) {
                    if (n) draw->AddText({x,p.y+3},ImGui::GetColorU32(muted),std::to_string(n).c_str());
                };
                number(row.before,p.x+4); number(row.after,p.x+48);
                draw->AddLine({p.x+90,p.y},{p.x+90,p.y+h},IM_COL32(61,69,84,120));
                draw->AddText({p.x+103,p.y+3},ImGui::GetColorU32(color),line.c_str());
                ImGui::PushID(i);
                if (partial_available() && line.rfind("@@ ",0) == 0) {
                    ImGui::SetCursorScreenPos({p.x+std::max(0.0f,ImGui::GetContentRegionAvail().x-(selected_staged ? 114 : 232)),p.y});
                    if (button(selected_staged ? "Unstage hunk" : "Stage hunk",true,{114,h})) stage_hunk(i);
                    if (!selected_staged) {
                        ImGui::SameLine(0,4);
                        if (button("Discard hunk",!busy(),{114,h})) request_discard_lines(hunk_lines(i),true);
                    }
                    ImGui::SetCursorScreenPos({p.x,p.y+h});
                } else if (partial_available() && changed_line(i)) {
                    if (ImGui::InvisibleButton("line",{width,h})) pick_line(i,ImGui::GetIO().KeyCtrl,ImGui::GetIO().KeyShift);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click / Ctrl / Shift to select changed lines");
                    if (ImGui::BeginPopupContextItem()) {
                        if (!selected_lines.count(i)) pick_line(i,false,false);
                        if (ImGui::MenuItem(selected_staged ? "Unstage selected lines" : "Stage selected lines")) stage_diff_lines({selected_lines.begin(),selected_lines.end()});
                        if (!selected_staged && ImGui::MenuItem("Discard selected lines...")) request_discard_lines({selected_lines.begin(),selected_lines.end()});
                        ImGui::EndPopup();
                    }
                } else ImGui::Dummy({width,h});
                ImGui::PopID();
            }
        }
        ImGui::PopStyleVar(); ImGui::PopFont(); ImGui::EndChild();
    }
    void center_panel() {
        if (!show_diff) { history(); return; }
        if (button("< Commit graph")) return_to_graph();
        ImGui::SameLine(); label(workspace ? (selected_staged ? "STAGED" : "UNSTAGED") : stash_view ? "STASH" : "COMMITTED");
        ImGui::SameLine();
        if (button("Copy patch", !busy() && !detail.empty())) ImGui::SetClipboardText(detail.c_str());
        ImGui::Spacing();
        ImGui::PushFont(body_font,20); ImGui::TextUnformatted(visible_path(selected_file).c_str()); ImGui::PopFont();
        label(workspace ? "Working changes / Inline diff" : stash_view ? "Saved changes / Inline diff" : "Commit changes / Inline diff");
        if (workspace) {
            if (button(selected_staged ? "Unstage lines" : "Stage lines",partial_available() && !selected_lines.empty()))
                stage_diff_lines({selected_lines.begin(),selected_lines.end()});
            if (!selected_staged) {
                ImGui::SameLine();
                if (button("Discard lines",partial_available() && !selected_lines.empty())) request_discard_lines({selected_lines.begin(),selected_lines.end()});
            }
            ImGui::SameLine(); ImGui::TextColored(muted,"%zu selected",selected_lines.size());
        }
        ImGui::Separator();
        diff_view();
    }
    void change_file(const eg::File& file, bool staged) {
        change_files({file},staged);
    }
    void file_row(int index, int kind) {
        const auto& f = kind == 2 ? changed_files[index] : repo.files[index];
        bool staged = kind == 1;
        bool selected = file_selection[kind].count(f.path) != 0;
        ImGui::PushID(index);
        auto p = ImGui::GetCursorScreenPos(); float width = ImGui::GetContentRegionAvail().x;
        float action_width = kind == 2 ? 0 : 76;
        bool clicked = ImGui::Selectable("##file",selected,0,{std::max(30.0f,width-action_width),30});
        // Keep the hover action alive while its mouse button is held, through release.
        bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseHoveringRect(p,{p.x+width,p.y+30});
        bool focused = ImGui::IsItemFocused();
        if (clicked) pick_file(index,kind,ImGui::GetIO().KeyCtrl,ImGui::GetIO().KeyShift);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !selected && (!busy() || previewing)) {
            clear_file_selection(); file_selection[kind].insert(f.path); selection_anchor[kind] = f.path;
        }
        if (hovered) {
            auto tip = f.original.empty() ? visible_path(f.path) : visible_path(f.original) + " -> " + visible_path(f.path);
            ImGui::SetTooltip("%s",tip.c_str());
        }
        if (ImGui::BeginPopupContextItem()) {
            auto chosen = chosen_files(kind);
            if (ImGui::MenuItem("View diff",nullptr,false,!busy())) select_file(f,staged);
            if (kind != 2 && ImGui::MenuItem(staged ? "Unstage selected files" : "Stage selected files",nullptr,false,ready() && !chosen.empty()))
                change_files(chosen,staged);
            bool discardable = !chosen.empty() && std::none_of(chosen.begin(),chosen.end(),[](const auto& file) { return file.conflicted(); });
            if (kind == 0 && ImGui::MenuItem("Discard selected files...",nullptr,false,ready() && discardable)) request_discard(chosen);
            if (ImGui::MenuItem("Copy selected paths",nullptr,false,!chosen.empty())) {
                std::string paths;
                for (const auto& file : chosen) { if (!paths.empty()) paths += '\n'; paths += file.path; }
                ImGui::SetClipboardText(paths.c_str());
            }
            ImGui::EndPopup();
        }
        char status = kind == 0 ? f.worktree : f.index;
        ImU32 color = (status == 'D' || f.conflicted()) ? ImGui::GetColorU32(red) :
            status == 'M' ? lane_colors[2] : status == 'R' ? lane_colors[3] : lane_colors[0];
        char marker[] = {status == '?' || status == 'A' ? '+' : status == 'D' ? '-' : status,0};
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddText({p.x+4,p.y+6},color,marker);
        auto name = visible_path(tree_view ? fs::path(f.path).filename().string() : f.path);
        text_clipped(draw,{p.x+25,p.y+6},name,ImGui::GetColorU32(ImGuiCol_Text),{p.x+width-action_width-3,p.y+30});
        if (kind != 2 && (hovered || selected || focused)) {
            auto after = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos({p.x+width-72,p.y+2});
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{5,4});
            if (button(staged ? "Unstage" : "Stage File",ready(),{72,26})) change_file(f,staged);
            ImGui::PopStyleVar(); ImGui::SetCursorScreenPos(after);
        }
        ImGui::PopID();
    }
    void draw_file_tree(const FileTree& node, int kind, const std::string& path = "") {
        for (const auto& entry : node.directories) {
            ImGui::PushID(entry.first.c_str());
            auto folder = path + entry.first + "/";
            ImGui::SetNextItemOpen(!closed_folders[kind].count(folder));
            bool open = ImGui::TreeNodeEx("##directory",ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth,
                "%s",visible_path(entry.first).c_str());
            if (ImGui::IsItemToggledOpen()) {
                if (open) closed_folders[kind].erase(folder); else closed_folders[kind].insert(folder);
                rebuild_visible_files();
            }
            if (open) { draw_file_tree(entry.second,kind,folder); ImGui::TreePop(); }
            ImGui::PopID();
        }
        for (int index : node.files) file_row(index,kind);
    }
    void file_controls() {
        auto p = ImGui::GetCursorScreenPos(); float width = ImGui::GetContentRegionAvail().x;
        if (workspace) {
            auto chosen = discard_targets();
            bool enabled = ready() && !chosen.empty() && std::none_of(chosen.begin(),chosen.end(),[](const auto& f) { return f.conflicted(); });
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{5,4});
            ImGui::PushFont(body_font,14); ImGui::PushStyleColor(ImGuiCol_Text,red);
            bool all = !has_file_selection();
            if (button("Discard",enabled,{64,28})) request_discard(chosen,all);
            ImGui::PopStyleColor(); ImGui::PopFont(); ImGui::PopStyleVar();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s",all ? "Discard ALL unstaged changes, including files hidden by the filter" : "Discard selected unstaged files");
        }
        float toggle_width = 140 + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorScreenPos({p.x+(width-toggle_width)*0.5f,p.y});
        bool previous = tree_view;
        ImGui::PushStyleColor(ImGuiCol_Button, !tree_view ? ImGui::GetStyleColorVec4(ImGuiCol_Header) : ImGui::GetStyleColorVec4(ImGuiCol_Button));
        if (button("Path",true,{70,28})) tree_view = false;
        ImGui::PopStyleColor(); ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, tree_view ? ImGui::GetStyleColorVec4(ImGuiCol_Header) : ImGui::GetStyleColorVec4(ImGuiCol_Button));
        if (button("Tree",true,{70,28})) tree_view = true;
        ImGui::PopStyleColor();
        if (previous != tree_view) rebuild_visible_files();
        size_t count = 0; for (const auto& selected : file_selection) count += selected.size();
        if (count) {
            auto after = ImGui::GetCursorScreenPos();
            ImGui::PushFont(body_font,12);
            auto text = std::to_string(count) + " selected";
            ImGui::SetCursorScreenPos({p.x+width-ImGui::CalcTextSize(text.c_str()).x,p.y+6}); label(text.c_str());
            ImGui::PopFont(); ImGui::SetCursorScreenPos(after);
        }
        ImGui::SetCursorScreenPos({p.x,p.y+28+ImGui::GetStyle().ItemSpacing.y});
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint("##file_search","Filter files...",file_search,sizeof(file_search))) rebuild_file_lists();
    }
    void file_group(int kind, float height) {
        bool staged = kind == 1;
        const auto& files = kind == 2 ? changed_files : repo.files;
        int total = kind == 2 ? int(files.size()) : int(std::count_if(files.begin(),files.end(),[&](const auto& f) {
            return staged ? f.staged() : f.unstaged();
        }));
        ImGui::PushID(kind);
        auto p = ImGui::GetCursorScreenPos(); float width = ImGui::GetContentRegionAvail().x;
        std::string title = kind == 2 ? (stash_view ? "Stash Files" : "Changed Files") : staged ? "Staged Files" : "Unstaged Files";
        title += " (" + std::to_string(total) + ")";
        title += "###file_group";
        ImGui::PushFont(body_font,14);
        ImGui::PushStyleColor(ImGuiCol_Header,light_theme ? ImVec4(0.87f,0.91f,0.93f,1) : ImVec4(0.15f,0.17f,0.22f,1));
        bool expanded = ImGui::CollapsingHeader(title.c_str(),ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::PopStyleColor();
        auto after = ImGui::GetCursorScreenPos();
        if (kind != 2) {
            float size = width >= 360 ? 142 : 92;
            ImGui::SetCursorScreenPos({p.x+width-size,p.y+2});
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{5,3});
            ImGui::PushStyleColor(ImGuiCol_Text,mint);
            const char* all_action = staged ? (size > 100 ? "Unstage All Changes" : "Unstage All") :
                (size > 100 ? "Stage All Changes" : "Stage All");
            auto chosen = chosen_files(kind);
            auto action = chosen.empty() ? std::string(all_action) : (staged ? "Unstage (" : "Stage (") + std::to_string(chosen.size()) + ")";
            if (button(action.c_str(),ready() && total > 0,{size,25})) {
                if (!chosen.empty()) change_files(chosen,staged);
                else if (!staged) command("Stage all",{"add","--all","--","."});
                else {
                    auto changes = repo.files; bool head = repo.has_head;
                    mutate("Unstage all",[changes,head](const eg::Git& git) {
                        for (const auto& f : changes) if (f.staged()) git.unstage(f,head);
                    });
                }
            }
            ImGui::PopStyleColor(); ImGui::PopStyleVar();
            ImGui::SetCursorScreenPos(after);
        }
        ImGui::PopFont();
        if (expanded) {
            ImGui::BeginChild("files",{0,std::max(32.0f,height-(after.y-p.y))});
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{4,0});
            if (file_lists[kind].empty()) {
                label(file_search[0] ? "No matching files" : kind == 0 ? "No unstaged files" : kind == 1 ? "No staged files" :
                    busy() ? "Loading files..." : "No changed files");
            } else if (tree_view) draw_file_tree(file_trees[kind],kind);
            else {
                ImGuiListClipper clipper; clipper.Begin(int(file_lists[kind].size()),30);
                while (clipper.Step()) for (int n=clipper.DisplayStart;n<clipper.DisplayEnd;++n) file_row(file_lists[kind][n],kind);
            }
            ImGui::PopStyleVar(); ImGui::EndChild();
        }
        ImGui::Separator(); ImGui::PopID();
    }
    void inspector() {
        if (!repo.operation.empty()) {
            ImGui::TextColored(mint,"%s in progress",repo.operation.c_str());
            bool conflicts = std::any_of(repo.files.begin(),repo.files.end(),[](const auto& f) { return f.conflicted(); });
            auto operation = repo.operation;
            if (button("Continue",ready() && !conflicts))
                mutate("Continue " + operation,[operation](const eg::Git& git) { git.resolve_operation(operation,"continue"); });
            ImGui::SameLine();
            if (button("Abort...",ready())) { pending_kind = "abort"; pending_operation = operation; }
            if (operation != "merge") {
                ImGui::SameLine();
                if (button("Skip...",ready())) { pending_kind = "skip"; pending_operation = operation; }
            }
            if (conflicts) ImGui::TextWrapped("Click a conflicted file to open the editor, then save and mark resolved.");
            ImGui::Separator();
        }
        if (workspace) {
            ImGui::Text("%zu file changes on",repo.files.size()); ImGui::SameLine();
            ImGui::TextColored(mint,"%s",repo.branch.empty() ? "-" : repo.branch.c_str());
            ImGui::Separator(); file_controls();
            constexpr float commit_height = 220;
            float groups = std::max(144.0f,ImGui::GetContentRegionAvail().y-commit_height-18);
            file_group(0,groups*0.5f); file_group(1,groups*0.5f);
            ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(),ImGui::GetWindowHeight()-commit_height-14));
            auto commit_pos = ImGui::GetCursorScreenPos(); float commit_width = ImGui::GetContentRegionAvail().x;
            label("COMMIT");
            ImGui::SetCursorScreenPos({commit_pos.x+commit_width-110,commit_pos.y-4});
            bool can_generate = ready() && ai_settings && ai_settings->enabled &&
                std::any_of(repo.files.begin(),repo.files.end(),[](const auto& f) { return f.staged(); }) &&
                std::none_of(repo.files.begin(),repo.files.end(),[](const auto& f) { return f.conflicted(); });
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{5,3});
            if (generating) { if (button("Cancel AI",true,{110,24})) *cancel = true; }
            else if (button("AI Generate",can_generate,{110,24})) generate_message();
            ImGui::PopStyleVar();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s",ai_settings && ai_settings->enabled ? "Send staged diff to your configured AI provider and fill this draft" : "Enable AI commit messages in Settings");
            ImGui::SetCursorScreenPos({commit_pos.x,commit_pos.y+24});
            ImGui::BeginDisabled(busy() || repo.root.empty());
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##message","Commit summary",message,sizeof(message));
            auto description_pos = ImGui::GetCursorScreenPos();
            ImGui::InputTextMultiline("##description",description,sizeof(description),{-1,72});
            if (!description[0] && !ImGui::IsItemActive())
                ImGui::GetWindowDrawList()->AddText({description_pos.x+10,description_pos.y+7},ImGui::GetColorU32(muted),"Description (optional)");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Optional commit description");
            ImGui::EndDisabled();
            bool staged = std::any_of(repo.files.begin(),repo.files.end(),[](const auto& f) { return f.staged(); });
            bool conflict = std::any_of(repo.files.begin(),repo.files.end(),[](const auto& f) { return f.conflicted(); });
            bool valid = std::string(message).find_first_not_of(" \t\r\n") != std::string::npos;
            ImGui::PushStyleColor(ImGuiCol_Button,light_theme ? ImVec4(0.66f,0.83f,0.76f,1) : ImVec4(0.17f,0.40f,0.36f,1));
            if (button(staged ? "Commit changes" : "Stage files to commit",idle() && staged && !conflict && valid,{-1,36})) {
                std::string text = message;
                if (description[0]) text += "\n\n" + std::string(description);
                mutate("Commit",[text](const eg::Git& git) { git.commit(text); },true);
            }
            ImGui::PopStyleColor();
            if (conflict) ImGui::TextColored(red,"Resolve conflicts before committing.");
        } else {
            const auto& oid = stash_view ? viewed_stash.id : selected_commit;
            ImGui::TextColored(mint,"%s",oid.substr(0,12).c_str()); ImGui::SameLine();
            if (button("Copy SHA")) ImGui::SetClipboardText(oid.c_str());
            if (!stash_view) {
                ImGui::SameLine(); if (button("Actions")) ImGui::OpenPopup("commit_actions");
                if (ImGui::BeginPopup("commit_actions")) { commit_actions(viewed_commit); ImGui::EndPopup(); }
            }
            ImGui::BeginChild("commit_metadata",{0,132});
            ImGui::TextWrapped("%s",commit_body.empty() ? viewed_commit.subject.c_str() : commit_body.c_str());
            ImGui::Spacing(); ImGui::TextColored(muted,"%s",viewed_commit.author.c_str());
            ImGui::TextColored(muted,"%s",local_commit_time(viewed_commit.date).c_str());
            ImGui::EndChild();
            if (stash_view) ImGui::TextWrapped("Saved working changes, including saved untracked files");
            else if (viewed_commit.parents.size() > 1) ImGui::TextWrapped("Changes against the first parent");
            ImGui::Separator(); file_controls();
            file_group(2,std::max(100.0f,ImGui::GetContentRegionAvail().y-12));
        }
    }

    void operation_dialog() {
        if (!pending_kind.empty() && !ImGui::IsPopupOpen("Confirm Git operation")) ImGui::OpenPopup("Confirm Git operation");
        ImGui::SetNextWindowSize({560,0},ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal("Confirm Git operation",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) return;
        ImGui::TextColored(mint,"%s",pending_kind.c_str());
        ImGui::TextWrapped("Repository: %s\nCurrent branch: %s",repo.root.c_str(),repo.branch.c_str());
        ImGui::Separator();
        bool stash = pending_kind == "apply stash" || pending_kind == "delete stash";
        bool resolve = pending_kind == "abort" || pending_kind == "skip";
        if (pending_kind == "force push") {
            ImGui::TextWrapped("Push %s (%s) to %s / %s",pending_push.branch.c_str(),pending_push.local.substr(0,12).c_str(),pending_push.remote.c_str(),pending_push.ref.c_str());
            ImGui::TextWrapped("Replace remote history only if its tip still matches %s. New remote commits will cause rejection.",pending_push.expected.substr(0,12).c_str());
            ImGui::Checkbox("I understand this rewrites remote branch history",&hard_confirm);
        } else if (pending_kind == "delete branch") {
            ImGui::TextWrapped("Delete %s at %s?",pending_branch.full.c_str(),pending_branch.id.substr(0,12).c_str());
            if (pending_branch.full.rfind("refs/heads/",0) == 0) ImGui::Checkbox("Allow deleting an unmerged branch",&hard_confirm);
            else ImGui::TextWrapped("This deletes the branch on the remote server. A changed remote tip will cause rejection.");
        } else if (pending_kind == "discard lines" || pending_kind == "discard hunk") {
            ImGui::TextWrapped("Discard %zu changed lines in %s?",pending_diff_lines.size(),visible_path(pending_files.front().path).c_str());
            ImGui::BeginChild("discard_selection",{0,180},ImGuiChildFlags_Borders,ImGuiWindowFlags_HorizontalScrollbar);
            std::istringstream patch(pending_diff); std::set<int> chosen(pending_diff_lines.begin(),pending_diff_lines.end());
            int i = 0;
            for (std::string line; std::getline(patch,line); ++i) if (chosen.count(i)) ImGui::TextUnformatted(line.c_str());
            ImGui::EndChild();
            ImGui::TextWrapped("Only these unstaged edits will be discarded. Staged changes and other lines are kept. Discard cannot be undone here.");
        } else if (pending_kind == "discard files") {
            ImGui::Text("Discard %sunstaged changes in %zu files?",pending_discard_all ? "ALL " : "selected ",pending_files.size());
            ImGui::BeginChild("discard_files",{0,std::min(160.0f,28.0f*pending_files.size()+10)},ImGuiChildFlags_Borders);
            for (const auto& file : pending_files) ImGui::TextWrapped("%s%s",file.index == '?' ? "[Delete] " : "",visible_path(file.path).c_str());
            ImGui::EndChild();
            ImGui::TextWrapped("Staged changes are kept. Untracked files are deleted permanently. Discarded edits cannot be undone here.");
        } else if (stash) {
            ImGui::TextWrapped("%s\n%s",pending_stash.subject.c_str(),pending_stash.id.substr(0,12).c_str());
            if (pending_kind == "apply stash") {
                ImGui::TextWrapped("Apply saved changes to this working tree. The stash is kept; conflicts may need resolving.");
                ImGui::Checkbox("Restore staged state (--index)",&restore_index);
            } else ImGui::TextWrapped("Delete this saved stash? Its saved changes will no longer be available in the stash list.");
        } else if (resolve) {
            ImGui::TextWrapped("%s the current %s operation?",pending_kind.c_str(),pending_operation.c_str());
            ImGui::TextWrapped(pending_kind == "abort" ? "Return to the state before this operation. Conflict resolution edits will be lost." :
                "Discard the current commit's changes and conflict resolution edits, then continue with any remaining commits.");
        } else {
            ImGui::TextWrapped("Target: %s\n%s",pending_commit.id.substr(0,12).c_str(),pending_commit.subject.c_str());
            if (pending_kind == "reset") {
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##mode",&reset_mode,"Soft\0Mixed\0Hard\0")) hard_confirm = false;
                const char* effects[] = {"Move HEAD to this commit; keep the index and working files.",
                    "Move HEAD to this commit and reset the index; keep working files as unstaged changes.",
                    "Move HEAD to this commit and overwrite the index and tracked working files. Untracked files obstructing restored paths may also be deleted."};
                ImGui::TextWrapped("%s",effects[reset_mode]);
                if (reset_mode == 2) ImGui::Checkbox("I understand these local changes will be discarded",&hard_confirm);
            } else {
                ImGui::TextWrapped(pending_kind == "merge" ? "Merge the target into the current branch; fast-forward when possible." :
                    pending_kind == "revert" ? "Create a new commit that reverses the selected commit's changes." :
                    "Apply the selected commit's changes as a new commit on the current branch.");
                if (pending_kind != "merge" && pending_commit.parents.size() > 1) {
                    label("Merge commit: choose the parent used as the mainline");
                    auto preview = "Parent " + std::to_string(mainline);
                    if (ImGui::BeginCombo("##mainline",preview.c_str())) {
                        for (int i = 0; i < int(pending_commit.parents.size()); ++i) {
                            auto name = "Parent " + std::to_string(i+1) + "  " + pending_commit.parents[i].substr(0,12);
                            if (ImGui::Selectable(name.c_str(),mainline == i+1)) mainline = i+1;
                        }
                        ImGui::EndCombo();
                    }
                }
            }
        }
        ImGui::Spacing();
        bool enabled = ready() && (pending_kind != "reset" || reset_mode != 2 || hard_confirm) && (pending_kind != "force push" || hard_confirm);
        if (button("Confirm",enabled)) {
            auto kind = pending_kind; auto commit = pending_commit; auto saved = pending_stash;
            auto files = pending_files; auto branch = pending_branch; auto push = pending_push; bool force = hard_confirm;
            auto operation = pending_operation; int parent = mainline, mode = reset_mode; bool index = restore_index;
            auto patch = pending_diff; auto lines = pending_diff_lines;
            mutate(kind,[kind,commit,saved,operation,parent,mode,index,files,branch,push,force,patch,lines](const eg::Git& git) {
                if (kind == "delete branch") git.delete_branch(branch,force);
                else if (kind == "force push") git.force_push(push);
                else if (kind == "cherry-pick") git.cherry_pick(commit,parent);
                else if (kind == "revert") git.revert(commit,parent);
                else if (kind == "merge") git.merge(commit.id);
                else if (kind == "reset") git.reset(commit.id,mode == 0 ? "soft" : mode == 1 ? "mixed" : "hard");
                else if (kind == "apply stash") git.apply_stash(saved,index);
                else if (kind == "delete stash") git.delete_stash(saved);
                else if (kind == "discard files") git.discard_files(files);
                else if (kind == "discard lines" || kind == "discard hunk") git.discard_lines(files.at(0),patch,lines);
                else git.resolve_operation(operation,kind);
            });
            pending_kind.clear(); ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (button("Cancel")) { pending_kind.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    std::string browse_error;
    void dialogs() {
        if (opening) { ImGui::OpenPopup("Open repository"); opening = false; browse_error.clear(); }
        ImGui::SetNextWindowSize({580,0}, ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("Open repository",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::BeginDisabled(busy());
            ImGui::RadioButton("Open",&open_mode,0); ImGui::SameLine(); ImGui::RadioButton("Clone",&open_mode,1);
            ImGui::SameLine(); ImGui::RadioButton("Initialize",&open_mode,2);
            if (open_mode == 1) { ImGui::SetNextItemWidth(-1); ImGui::InputTextWithHint("##clone_url","Repository URL (SSH / HTTPS / local path)",clone_url,sizeof(clone_url)); }
            if (open_mode == 2) { ImGui::TextUnformatted("Initial branch"); ImGui::SetNextItemWidth(-1); ImGui::InputTextWithHint("##initial_branch","Initial branch",initial_branch,sizeof(initial_branch)); }
            ImGui::EndDisabled();
            ImGui::TextWrapped(open_mode == 0 ? "Enter a repository path or browse for a folder." : open_mode == 1 ? "Choose a new or empty destination folder." : "Initialize a folder. Existing files are kept and are not committed automatically.");
            ImGui::BeginDisabled(busy());
            ImGui::SetNextItemWidth(-104);
            bool enter = ImGui::InputTextWithHint("##path", "/home/you/projects/repository",path,sizeof(path),ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if (button("Browse...", !busy(), {94,0})) { browse_error.clear(); browse_folder(); }
            bool valid = path[0] && (open_mode != 1 || clone_url[0]) && (open_mode != 2 || initial_branch[0]);
            bool open = button(open_mode == 0 ? "Open repository" : open_mode == 1 ? "Clone repository" : "Initialize repository", !busy() && valid);
            if ((enter || open) && !busy() && valid) {
                if (open_mode == 0) requested_open = path;
                else create_repository();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine(); if (button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndDisabled();
            if (busy()) label("Choose a folder in the system dialog, or cancel there.");
            if (!browse_error.empty()) ImGui::TextWrapped("%s", browse_error.c_str());
            ImGui::EndPopup();
        }
        if (!new_kind.empty() && !ImGui::IsPopupOpen("Create")) ImGui::OpenPopup("Create");
        if (ImGui::BeginPopupModal("Create",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Create %s",new_kind.c_str()); ImGui::SetNextItemWidth(360);
            ImGui::InputTextWithHint("##name",new_kind == "stash" ? "Optional stash message" : "Name",new_name,sizeof(new_name));
            if (new_kind == "stash") ImGui::TextWrapped("Save tracked and untracked changes in a stash, then clean those changes from the working tree.");
            if (button("Create",ready() && (new_name[0] || new_kind == "stash"))) {
                std::string name = new_name;
                if (new_kind == "branch") command("Create branch",{"switch","-c",name});
                else if (new_kind == "tag") command("Create tag",{"tag","--",name});
                else mutate("Stash",[name](const eg::Git& git) { git.save_stash(name); },false,true);
                new_kind.clear(); ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine(); if (button("Cancel")) { new_kind.clear(); ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
        conflict_dialog();
        operation_dialog();
        if (!error.empty() && !ImGui::IsPopupOpen("Git operation failed")) ImGui::OpenPopup("Git operation failed");
        ImGui::SetNextWindowSize({650,360},ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("Git operation failed",nullptr)) {
            ImGui::BeginChild("error",{0,-46}); ImGui::TextWrapped("%s",error.c_str()); ImGui::EndChild();
            if (button("Close")) { error.clear(); ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
    }
    void splitter(const char* id, float height, float& width, bool reverse) {
        ImGui::SameLine(0,0);
        ImGui::InvisibleButton(id,{6,height});
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive()) width += ImGui::GetIO().MouseDelta.x * (reverse ? -1 : 1);
        ImGui::SameLine(0,0);
    }
    void frame() {
        refresh_external_diff();
        header();
        float height = ImGui::GetContentRegionAvail().y - 36;
        float width = ImGui::GetContentRegionAvail().x;
        sidebar_width = std::clamp(sidebar_width,160.0f,std::max(160.0f,width*0.25f));
        detail_width = std::clamp(detail_width,320.0f,std::max(320.0f,width*0.43f));
        ImGui::BeginChild("sidebar",{sidebar_width,height},ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse); sidebar(); ImGui::EndChild();
        splitter("left_split",height,sidebar_width,false);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,light_theme ? ImVec4(0.94f,0.96f,0.98f,1) : ImVec4(0.105f,0.12f,0.16f,1));
        ImGui::BeginChild("history_panel",{std::max(200.0f,width-sidebar_width-detail_width-12),height},ImGuiChildFlags_AlwaysUseWindowPadding);
        center_panel(); ImGui::EndChild(); ImGui::PopStyleColor();
        splitter("right_split",height,detail_width,true);
        ImGui::BeginChild("inspector",{0,height},ImGuiChildFlags_AlwaysUseWindowPadding); inspector(); ImGui::EndChild();
        ImGui::SetCursorPosX(14);
        ImGui::TextColored(busy() ? mint : muted,"%s",busy() ? "o" : "+"); ImGui::SameLine();
        ImGui::TextUnformatted(status.c_str());
        if (ImGui::IsKeyPressed(ImGuiKey_F5) && ready()) load(repo.root);
        dialogs();
    }
};

struct App {
    eg::Settings settings;
    eg::Settings settings_draft;
    std::string config_path, config_error, last_saved, restore_active;
    bool settings_open = false, config_blocked = false, show_key = false;
    char ai_model_name[81]{}, ai_filter[128]{};
    std::string ai_model_error;
    double window_changed_at = -1;
    void initialize(const std::string& file,const std::vector<std::string>& roots = {}) {
        config_path = file;
        try { settings = eg::read_settings(file); }
        catch (const std::exception& e) { config_error = std::string("Could not load settings: ") + e.what(); config_blocked = true; }
        theme(settings.light);
        auto paths = settings.repositories;
        restore_active = roots.empty() ? settings.active_repository : "";
        for (const auto& root : roots) if (std::find(paths.begin(),paths.end(),root) == paths.end()) paths.push_back(root);
        for (const auto& path : paths) open_repository(path);
        if (tabs.empty()) add_tab().opening = true;
        if (!config_error.empty()) tabs.front()->status = "Settings could not be loaded. Open Settings for details.";
    }
    void persist(bool force = false) {
        if (config_path.empty() || config_blocked || !restore_active.empty()) return;
        settings.repositories.clear(); settings.active_repository.clear();
        for (const auto& tab : tabs) {
            auto root = tab->repo.root.empty() ? tab->opened_path : tab->repo.root;
            if (!root.empty()) settings.repositories.push_back(root);
            if (tab->id == active) settings.active_repository = root;
        }
        auto json = eg::settings_json(settings);
        if (json == last_saved) return;
        if (!force && ImGui::GetTime()-window_changed_at < 0.25) return;
        last_saved = json;
        try { eg::write_settings(config_path,settings); config_error.clear(); }
        catch (const std::exception& e) {
            config_error = e.what();
            if (auto* tab = find(active)) tab->status = "Settings could not be saved. Open Settings for details.";
        }
    }
    void settings_dialog() {
        if (settings_open) {
            settings_draft = settings; show_key = false; ai_model_name[0] = ai_filter[0] = 0; ai_model_error.clear();
            ImGui::OpenPopup("Settings"); settings_open = false;
        }
        ImGui::SetNextWindowSize({660,630},ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal("Settings",nullptr)) return;
        ImGui::BeginChild("settings_content",{0,-48});
        ImGui::TextWrapped("Saved to: %s",config_path.empty() ? "~/.easy_git" : config_path.c_str());
        auto& draft = settings_draft.ai;
        ImGui::Checkbox("Enable AI commit messages",&draft.enabled);
        ImGui::TextWrapped("AI Generate sends your staged diff to the configured provider. Review the generated draft before committing.");
        if (!config_error.empty()) ImGui::TextWrapped("%s",config_error.c_str());
        if (config_blocked) ImGui::TextWrapped("The existing file was kept. Save settings to replace it with these settings.");
        ImGui::Separator();
        if (ImGui::BeginTable("ai_settings",2,ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Label",ImGuiTableColumnFlags_WidthFixed,145);
            ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthStretch);
            auto row = [](const char* name) { ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(name); ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1); };
            auto text_field = [&](const char* name,std::string& value,bool password = false) {
                row(name); char buffer[8192]; snprintf(buffer,sizeof(buffer),"%s",value.c_str()); ImGui::PushID(name);
                if (ImGui::InputText("##value",buffer,sizeof(buffer),password ? ImGuiInputTextFlags_Password : 0)) value = buffer;
                ImGui::PopID();
            };
            row("AI Provider / Model");
            if (ImGui::BeginCombo("##provider",draft.provider.c_str(),ImGuiComboFlags_HeightLarge)) {
                ImGui::SetNextItemWidth(-1); ImGui::InputTextWithHint("##provider_search","Search providers / models",ai_filter,sizeof(ai_filter));
                ImGuiTextFilter filter(ai_filter);
                auto select = [&](const std::string& name) {
                    if (filter.PassFilter(name.c_str()) && ImGui::Selectable(name.c_str(),draft.provider == name)) {
                        eg::select_ai_model(settings_draft,name); show_key = false; ai_model_error.clear();
                    }
                };
                ImGui::TextDisabled("PRESETS");
                for (const auto& preset : eg::ai_presets()) select(preset.name);
                ImGui::Separator(); ImGui::TextDisabled("MY MODELS");
                // Selection can save the previous profile, so iterate a stable list.
                std::vector<std::string> names;
                for (const auto& [name,profile] : settings_draft.ai_profiles) { (void)profile; if (!eg::is_ai_preset(name)) names.push_back(name); }
                for (const auto& name : names) select(name);
                ImGui::EndCombo();
            }
            row("Manage models");
            if (button("Add model")) { ai_model_name[0] = 0; ai_model_error.clear(); ImGui::OpenPopup("Add AI model"); }
            ImGui::SameLine();
            ImGui::BeginDisabled(eg::is_ai_preset(draft.provider));
            if (button("Delete model")) ImGui::OpenPopup("Delete AI model");
            ImGui::EndDisabled();
            if (eg::is_ai_preset(draft.provider)) { ImGui::SameLine(); ImGui::TextDisabled("Preset (protected)"); }
            if (ImGui::BeginPopupModal("Add AI model",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted("Name this model configuration (for example, Work Claude).");
                ImGui::SetNextItemWidth(400); ImGui::InputText("##model_name",ai_model_name,sizeof(ai_model_name));
                if (!ai_model_error.empty()) ImGui::TextWrapped("%s",ai_model_error.c_str());
                if (button("Add")) {
                    try { eg::add_ai_model(settings_draft,ai_model_name); show_key = false; ai_model_error.clear(); ImGui::CloseCurrentPopup(); }
                    catch (const std::exception& e) { ai_model_error = e.what(); }
                }
                ImGui::SameLine(); if (button("Cancel")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            if (ImGui::BeginPopupModal("Delete AI model",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Delete model configuration '%s'?",draft.provider.c_str());
                ImGui::TextUnformatted("Save settings to keep this change, or Cancel settings to undo it.");
                if (button("Delete")) {
                    eg::delete_ai_model(settings_draft,draft.provider); show_key = false; ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine(); if (button("Cancel")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            row("Request format");
            if (ImGui::BeginCombo("##format",draft.format.c_str())) {
                for (const auto* format : {"OpenAI","Anthropic"}) if (ImGui::Selectable(format,draft.format == format)) draft.format = format;
                ImGui::EndCombo();
            }
            if (draft.format == "OpenAI") {
                row("Token limit field");
                if (ImGui::BeginCombo("##token_parameter",draft.token_parameter.c_str())) {
                    for (const auto* field : {"max_tokens","max_completion_tokens"}) if (ImGui::Selectable(field,draft.token_parameter == field)) draft.token_parameter = field;
                    ImGui::EndCombo();
                }
            }
            text_field("API base URL",draft.base_url); text_field("Model / Endpoint ID",draft.model);
            if (draft.provider == "Azure OpenAI") {
                row("Azure setup"); ImGui::TextWrapped("Use your resource URL ending in /openai/v1 and deployment name as Model.");
            }
            if (draft.provider == "iFlytek Spark") {
                row("Spark credential"); ImGui::TextWrapped("Enter the HTTP API's APIPassword as API key.");
            }
            if (draft.provider == "Amazon Bedrock") {
                row("Bedrock setup"); ImGui::TextWrapped("Use a Bedrock API key and a region where your model is available.");
            }
            if (draft.model.empty()) { row("Model required"); ImGui::TextWrapped("Enter a model ID available in your account or local server."); }
            text_field("API key",draft.api_key,!show_key); row(" "); ImGui::Checkbox("Show key",&show_key);
            text_field("Proxy (blank: direct)",draft.proxy); text_field("Output language",draft.language);
            row("Timeout (seconds)"); ImGui::InputInt("##timeout",&draft.timeout);
            row("Max output tokens"); ImGui::InputInt("##max_tokens",&draft.max_tokens);
            row("Max diff bytes"); ImGui::InputInt("##max_diff",&draft.max_diff_bytes);
            row("Style instructions"); char prompt[8192]; snprintf(prompt,sizeof(prompt),"%s",draft.instructions.c_str());
            if (ImGui::InputTextMultiline("##instructions",prompt,sizeof(prompt),{-1,68})) draft.instructions = prompt;
            ImGui::EndTable();
        }
        ImGui::TextWrapped("Each model saves its own format, URL, key and parameters. Presets cannot be deleted; model IDs and URLs remain editable for your account and region. API keys are stored locally with owner-only permissions (0600).");
        ImGui::EndChild();
        if (button("Save settings")) {
            try {
                if (draft.enabled) eg::validate_ai(draft);
                settings.ai = draft; settings.ai_profiles = settings_draft.ai_profiles; config_blocked = false; last_saved.clear(); persist(true);
                if (config_error.empty()) ImGui::CloseCurrentPopup();
            } catch (const std::exception& e) { config_error = e.what(); }
        }
        ImGui::SameLine(); if (button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    std::vector<std::unique_ptr<RepoTab>> tabs;
    int active = 0, next_id = 1, focus = 0, pending_close = 0;
    RepoTab& add_tab() {
        auto tab = std::make_unique<RepoTab>(); tab->id = next_id++; tab->ai_settings = &settings.ai;
        active = focus = tab->id; tabs.push_back(std::move(tab)); return *tabs.back();
    }
    RepoTab* find(int id) {
        for (auto& tab : tabs) if (tab->id == id) return tab.get();
        return nullptr;
    }
    bool busy() const {
        return std::any_of(tabs.begin(),tabs.end(),[](const auto& tab) { return tab->busy(); });
    }
    void shutdown() {
        persist(true);
        for (auto& tab : tabs) *tab->cancel = true;
        for (auto& tab : tabs) if (tab->job.valid()) tab->job.wait();
    }
    void open_repository(const std::string& path) {
        auto* tab = find(active);
        if (!tab || !tab->repo.root.empty() || tab->busy()) tab = &add_tab();
        tab->opening = false; tab->load(path);
    }
    void close_tab(int id) {
        auto it = std::find_if(tabs.begin(),tabs.end(),[&](const auto& tab) { return tab->id == id; });
        if (it == tabs.end() || (*it)->busy()) return;
        size_t index = size_t(it-tabs.begin());
        tabs.erase(it);
        if (tabs.empty()) add_tab();
        else if (active == id) active = focus = tabs[std::min(index,tabs.size()-1)]->id;
    }
    void request_close(int id) {
        auto* tab = find(id);
        if (!tab) return;
        if (tab->busy()) { tab->status = "Wait for the current operation before closing this repository."; return; }
        if (tab->message[0] || tab->description[0]) pending_close = id;
        else close_tab(id);
    }
    void frame() {
        for (auto& tab : tabs) tab->poll();
        // Resolve subfolder/symlink aliases only after Git has identified the worktree root.
        for (size_t i=0;i<tabs.size();) {
            auto& tab = *tabs[i]; bool duplicate = false;
            if (tab.just_opened) {
                tab.just_opened = false;
                for (size_t j=0;j<tabs.size();++j) if (i != j && !tabs[j]->repo.root.empty()) {
                    std::error_code ec;
                    if (fs::equivalent(tab.repo.root,tabs[j]->repo.root,ec)) {
                        if (active == tab.id) active = focus = tabs[j]->id;
                        tabs.erase(tabs.begin()+i); duplicate = true; break;
                    }
                }
            }
            if (!duplicate) ++i;
        }
        if (tabs.empty()) add_tab();
        if (!restore_active.empty() && !busy()) {
            for (const auto& tab : tabs) if (tab->repo.root == restore_active || tab->opened_path == restore_active) active = focus = tab->id;
            restore_active.clear();
        }
        const auto* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos); ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
        ImGui::Begin("Easy Git",nullptr,ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PopStyleVar();
        int close = 0; bool create = false;
        ImGui::BeginChild("repository_tabs",{-190,36});
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{16,10});
        if (ImGui::BeginTabBar("repositories",ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll |
            ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_DrawSelectedOverline)) {
            for (const auto& tab : tabs) {
                auto name = tab->repo.root.empty() ? "New repository" : fs::path(tab->repo.root).filename().string();
                // Render user-controlled names separately from ImGui's ID suffix syntax.
                std::replace(name.begin(),name.end(),'#','_');
                name = visible_path(name);
                if (tab->busy()) name += " ...";
                if (tab->message[0] || tab->description[0]) name += " *";
                if (!tab->error.empty()) name += " !";
                name += "###repo_" + std::to_string(tab->id);
                bool keep = true;
                auto flags = tab->id == focus ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
                if (ImGui::BeginTabItem(name.c_str(),&keep,flags)) {
                    active = tab->id; ImGui::EndTabItem();
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",tab->repo.root.empty() ? "Open another repository" : tab->repo.root.c_str());
                if (!keep) close = tab->id;
            }
            create = ImGui::TabItemButton("+",ImGuiTabItemFlags_Trailing);
            ImGui::EndTabBar();
        }
        ImGui::PopStyleVar(); focus = 0;
        ImGui::EndChild(); ImGui::SameLine();
        if (button(settings.light ? "Dark" : "Light",true,{80,36})) { settings.light = !settings.light; theme(settings.light); }
        ImGui::SameLine();
        if (!config_error.empty()) ImGui::PushStyleColor(ImGuiCol_Text,red);
        if (button("Settings",true,{90,36})) settings_open = true;
        if (!config_error.empty()) {
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",config_error.c_str());
        }
        auto* current = find(active);
        if (create && current && !current->busy()) current->opening = true;
        if (create && current && current->busy()) { current = &add_tab(); current->opening = true; }
        if (current) {
            ImGui::PushID(current->id); current->frame(); ImGui::PopID();
            if (!current->requested_open.empty()) {
                auto path = std::move(current->requested_open); current->requested_open.clear(); open_repository(path);
            }
        }
        if (close) request_close(close);
        if (pending_close && !ImGui::IsPopupOpen("Close repository?")) ImGui::OpenPopup("Close repository?");
        if (ImGui::BeginPopupModal("Close repository?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("This repository has an unsubmitted commit message.");
            ImGui::TextUnformatted("Close the tab and discard that message?");
            if (button("Discard message and close")) { close_tab(pending_close); pending_close = 0; ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            if (button("Keep tab")) { pending_close = 0; ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
        settings_dialog();
        persist();
        ImGui::End();
    }
};

void restore_window(GLFWwindow* window,const eg::WindowSettings& saved) {
    glfwSetWindowSizeLimits(window,1080,720,GLFW_DONT_CARE,GLFW_DONT_CARE);
    glfwSetWindowSize(window,std::max(1080,saved.width),std::max(720,saved.height));
    glfwShowWindow(window);
}
bool capture_window(GLFWwindow* window,eg::WindowSettings& saved) {
    if (glfwGetWindowAttrib(window,GLFW_ICONIFIED)) return false;
    int width, height; glfwGetWindowSize(window,&width,&height);
    if (width < 64 || height < 64 || (width == saved.width && height == saved.height)) return false;
    saved.width = width; saved.height = height; return true;
}

void save_frame(const char* path, int w, int h) {
    std::vector<unsigned char> pixels(size_t(w)*h*3);
    glPixelStorei(GL_PACK_ALIGNMENT,1); glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    std::ofstream out(path,std::ios::binary);
    out << "P6\n" << w << ' ' << h << "\n255\n";
    for (int y = h-1; y >= 0; --y) out.write(reinterpret_cast<char*>(pixels.data()+size_t(y)*w*3),w*3);
}
}

#ifndef EASY_GIT_UI_TEST
#include "app_icon.h"
int main(int argc, char** argv) {
    std::vector<std::string> roots; std::string screenshot, config_file; int frames = 0;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            puts("easy_git [repository ...] [--config file] [--frames N] [--screenshot image.ppm]\nSettings and open repositories are saved in ~/.easy_git by default.\nF5 refreshes the repository. Drag panel dividers to resize."); return 0;
        }
        if (arg == "--frames" && i+1 < argc) frames = std::max(1,atoi(argv[++i]));
        else if (arg == "--screenshot" && i+1 < argc) screenshot = argv[++i];
        else if (arg == "--config" && i+1 < argc) config_file = argv[++i];
        else roots.push_back(fs::absolute(arg).string());
    }
    glfwSetErrorCallback([](int code,const char* description) { fprintf(stderr,"GLFW %d: %s\n",code,description); });
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    glfwWindowHintString(GLFW_X11_CLASS_NAME,"easy_git");
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME,"easy_git");
    auto* window = glfwCreateWindow(1440,900,"Easy Git - repository workspace",nullptr,nullptr);
    if (!window) { glfwTerminate(); return 1; }
    GLFWimage icon{64,64,app_icon_pixels};
    glfwSetWindowIcon(window,1,&icon);
    glfwMakeContextCurrent(window); glfwSwapInterval(1);
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); theme();
    auto& io = ImGui::GetIO(); io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; io.IniFilename = nullptr;
    const char* body = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    body_font = fs::exists(body) ? io.Fonts->AddFontFromFileTTF(body,16) : io.Fonts->AddFontDefault();
    const char* cjk = "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc";
    if (fs::exists(cjk)) { ImFontConfig config; config.MergeMode = true; io.Fonts->AddFontFromFileTTF(cjk,16,&config); }
    const char* mono = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
    mono_font = fs::exists(mono) ? io.Fonts->AddFontFromFileTTF(mono,13) : body_font;
    if (mono_font != body_font && fs::exists(cjk)) {
        ImFontConfig config; config.MergeMode = true; io.Fonts->AddFontFromFileTTF(cjk,13,&config);
    }
    ImGui_ImplGlfw_InitForOpenGL(window,true); ImGui_ImplOpenGL3_Init("#version 330");
    App app;
    try { app.initialize(config_file.empty() ? eg::default_settings_path() : config_file,roots); }
    catch (const std::exception& e) { app.add_tab().error = e.what(); }
    restore_window(window,app.settings.window);
    int rendered = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (capture_window(window,app.settings.window)) app.window_changed_at = ImGui::GetTime();
        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        app.frame(); ImGui::Render();
        int w,h; glfwGetFramebufferSize(window,&w,&h);
        glViewport(0,0,w,h); glClearColor(0.09f,0.1f,0.14f,1); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ++rendered;
        bool finish = frames && rendered >= frames && !app.busy();
        if (finish && !screenshot.empty()) save_frame(screenshot.c_str(),w,h);
        glfwSwapBuffers(window);
        if (finish) break;
    }
    app.shutdown();
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
    glfwDestroyWindow(window); glfwTerminate();
    return 0;
}
#endif
