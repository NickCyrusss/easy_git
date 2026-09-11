Easy Git v0.1.2

- Commit and stash timestamps follow the computer’s local timezone, including daylight saving. Graph rows show date and time; details and tooltips include seconds and timezone.
- Conflict editor with Current/Incoming panes and editable Output: choose lines or blocks, control combination order, navigate conflicts, and undo choices. Unresolved conflict markers prevent saving; binary files and deletions retain whole-version handling.
- Automatically refresh the active unstaged diff after external edits, atomic saves, deletion, or restoration.
- Updated English/Chinese documentation and conflict editor screenshot.

Download the Ubuntu 22.04 amd64 (x86-64) package and install:

```sh
sudo apt install ./easy-git_0.1.2_amd64.deb
```

Launch **Easy Git** from the application menu or run `easy_git`. Settings remain in `~/.easy_git`. `SHA256SUMS` contains the package checksum. Other distributions and architectures are not validated by this release.

---

- Commit 和 Stash 时间按电脑本地时区显示，支持夏令时；提交图显示日期和时分，详情与悬停提示包含秒及时区。
- 冲突编辑器提供 Current／Incoming 对照与可编辑 Output：支持逐行／区块选择、组合顺序、冲突导航和撤销选择；残留冲突标记时禁止保存，二进制文件与删除冲突保留整版本处理。
- 在外部编辑、原子保存、删除或恢复文件后，自动刷新当前未暂存文件的 Diff。
- 更新中英文说明及冲突编辑器截图。

下载 Ubuntu 22.04 amd64（x86-64）安装包，执行上述安装命令。随后从应用菜单打开 **Easy Git**，或运行 `easy_git`；配置保存在 `~/.easy_git`。`SHA256SUMS` 提供安装包校验值。本次发布未验证其他发行版及架构。
