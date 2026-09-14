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
