Easy Git v0.1.5

- Pull keeps compatible local changes, including staged changes, even when Git is configured to rebase. If a temporary backup cannot be restored, it remains in Stash for recovery.
- Open a terminal in the current repository from the toolbar.
- Choose interface and diff fonts and sizes independently in Settings.
- Double-click sidebar branches to switch, or a stash to apply it. Type after selecting a sidebar entry to filter; the branch selector now focuses its search field when opened.

Download the Ubuntu 22.04 amd64 package or the Windows x64 portable archive:

```sh
sudo apt install ./easy-git_0.1.5_amd64.deb
```

On Windows, extract `easy_git-win64.zip` and run `easy_git.exe`; Git for Windows must be installed and on `PATH`. `SHA256SUMS` contains the Debian package checksum.

---

- Pull 可保留不冲突的本地修改和暂存状态，Git 配置为 rebase 时也可使用；临时备份无法恢复时会保留在 Stash 中。
- 工具栏新增 Terminal，在当前仓库目录打开终端。
- 设置中可分别选择界面和 Diff 字体及字号。
- 双击侧栏分支切换，双击 Stash 应用；选择侧栏条目后可直接输入筛选，打开分支下拉框时自动聚焦搜索。

下载 Ubuntu 22.04 amd64 安装包或 Windows x64 免安装压缩包。Windows 用户解压 `easy_git-win64.zip` 后运行 `easy_git.exe`，需安装 Git for Windows 并加入 `PATH`。`SHA256SUMS` 提供 Debian 安装包校验值。

---

Easy Git v0.1.4

- Windows support: the Git process backend now uses CreateProcess with a job object, so the desktop app builds and runs on Windows 10/11 (x64) alongside Linux. Windows releases ship as a portable folder with the MinGW runtime, libcurl, and json-c DLLs.
- Windows integration: the native folder picker replaces Zenity, Segoe UI and Consolas are used with a CJK fallback, the executable is a GUI-subsystem binary so no console window opens beside the window, and command-line arguments, the environment, and file paths are handled as UTF-8.
- Line endings: partial staging and discard respect `core.autocrlf`, which Git for Windows enables by default.
- Scratch files fall back to `%LOCALAPPDATA%\easy_git\tmp`, the executable directory, and then the working directory when the system temporary directory is not writable, so a restricted launch can still open repositories.
- Tests cover both platforms: POSIX-only checks (symlinks, magic pathspecs, newlines in file names, the IANA timezone matrix) are skipped on Windows, and all five suites pass there.

Download the Ubuntu 22.04 amd64 (x86-64) package or the Windows x64 portable folder:

```sh
sudo apt install ./easy-git_0.1.4_amd64.deb
```

Launch **Easy Git** from the application menu or run `easy_git`. Settings remain in `~/.easy_git`. `SHA256SUMS` contains the package checksum. On Windows, extract `easy_git-win64.zip` and run `easy_git.exe`; Git for Windows must be installed and on `PATH`.

---

- 新增 Windows 支持：Git 进程后端改用 CreateProcess 与作业对象，桌面程序可在 Windows 10/11（x64）与 Linux 上构建和运行；Windows 版以免安装目录形式发布，附带 MinGW 运行库、libcurl 和 json-c。
- Windows 集成：使用系统原生文件夹选择器替代 Zenity；正文与等宽字体改为 Segoe UI、Consolas，并合并中文字形；可执行文件为 GUI 子系统程序，不再附带控制台窗口；命令行参数、环境变量和文件路径统一按 UTF-8 处理。
- 换行符：按行暂存与丢弃兼容 `core.autocrlf`（Git for Windows 默认开启）。
- 系统临时目录不可写时，临时文件依次回退到 `%LOCALAPPDATA%\easy_git\tmp`、程序所在目录和当前工作目录，受限环境下仍可打开仓库。
- 测试覆盖两个平台：符号链接、魔法路径、文件名换行、IANA 时区矩阵等仅限 POSIX 的检查在 Windows 上跳过，五组测试在 Windows 上全部通过。

下载 Ubuntu 22.04 amd64（x86-64）安装包，或 Windows x64 免安装压缩包，执行上方安装命令。随后从应用菜单打开 **Easy Git**，或运行 `easy_git`；配置保存在 `~/.easy_git`。`SHA256SUMS` 提供安装包校验值。Windows 下解压 `easy_git-win64.zip` 后运行 `easy_git.exe`，需已安装 Git for Windows 并位于 `PATH`。

Easy Git v0.1.3

- File history from file-list context menus: browse commits and their file diffs, follow renames, inspect deleted files, load more entries, and copy patches. Includes a history screenshot.
- Unified context-menu font and spacing for sidebar, file lists, and Working changes lines.
- One sidebar filter covers Workspace, Local, Remote, Tags, and Stash.
- First push of a local branch without an upstream publishes its same-name branch on origin and sets tracking. Existing upstream behavior and non-fast-forward protection are preserved.
- Switching repositories refreshes local status, history, and the open working-file diff while preserving previews and drafts. Busy tasks finish first. Fetch remains manual.

Download the Ubuntu 22.04 amd64 (x86-64) package and install:

```sh
sudo apt install ./easy-git_0.1.3_amd64.deb
```

Launch **Easy Git** from the application menu or run `easy_git`. Settings remain in `~/.easy_git`. `SHA256SUMS` contains the package checksum. Other distributions and architectures are not validated by this release.

---

- 文件列表右键新增文件历史：查看相关提交和文件 Diff、追溯重命名、查看删除文件历史、加载更多及复制 Patch；附实际运行截图。
- 统一左侧、文件列表和 Working changes 逐行右键菜单的字体与间距。
- 左侧统一搜索框覆盖 Workspace、Local、Remote、Tags 和 Stash。
- 无 upstream 的本地分支首次 Push 时，自动创建 origin 同名分支并设置跟踪关系；保留已有推送配置和非快进保护。
- 切换仓库自动刷新本地状态、提交历史和当前工作区文件 Diff，保留预览及草稿，忙碌时延后执行；Fetch 保持手动触发。

下载 Ubuntu 22.04 amd64（x86-64）安装包，执行上述安装命令。随后从应用菜单打开 **Easy Git**，或运行 `easy_git`；配置保存在 `~/.easy_git`。`SHA256SUMS` 提供安装包校验值。本次发布未验证其他发行版及架构。
