Easy Git for Linux — Debian package built on Ubuntu 22.04, amd64 (x86-64).

Download the `.deb` and install it with:

```sh
sudo apt install ./easy-git_0.1.1_amd64.deb
```

Launch **Easy Git** from your application menu or run `easy_git`. The package includes the executable, desktop launcher, icon, and runtime dependency metadata. Settings stay in `~/.easy_git`. Other distributions and architectures are not validated by this release.

Includes commit graphs, multiple repositories, partial stage/unstage/discard, stash previews, resizable sidebar sections, conflict editing, and optional AI commit messages. `SHA256SUMS` contains the package checksum.

---

Linux 版 Easy Git：基于 Ubuntu 22.04 构建的 amd64（x86-64）Debian 安装包。

下载 `.deb` 后执行上述安装命令，随后从应用菜单打开 **Easy Git**，或运行 `easy_git`。安装包包含程序、桌面入口、图标和运行依赖信息；配置保存在 `~/.easy_git`。本次发布未验证其他发行版及架构。

包含提交图、多仓库、按行／区块暂存及丢弃、Stash 预览、可伸缩侧栏、冲突编辑器和可选 AI 提交信息功能。`SHA256SUMS` 提供安装包校验值。

Fixes Git 2.55 stash cleanup with untracked files. / 修复 Git 2.55 下 Stash 未清理未跟踪文件的问题。
