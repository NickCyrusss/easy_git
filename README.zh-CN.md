# Easy Git

[English](README.md) | **简体中文**

使用 **C++17 + Dear ImGui + GLFW + OpenGL** 实现的 Linux 桌面 Git 客户端。提供多仓库标签页、提交关系图、浅色/深色主题和可选的 AI 提交信息生成。界面参考 GitKraken；本项目独立开发，与 GitKraken 无隶属或合作关系。

项目处于早期开发阶段，采用 [MIT 许可证](LICENSE)。

![实际运行截图：多仓库与文件面板](docs/multiple-repositories.png)

## 构建与启动

依赖：Git、CMake 3.22+、C++17 编译器、OpenGL、X11、libcurl 和 json-c 开发库。Ubuntu/Debian 可安装：

```sh
sudo apt install build-essential cmake ninja-build git libgl1-mesa-dev \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
  fonts-dejavu-core fonts-noto-cjk zenity pkg-config libcurl4-openssl-dev libjson-c-dev
```

克隆项目并构建：

```sh
git clone https://github.com/NickCyrusss/easy_git.git
cd easy_git
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
./build/easy_git /绝对路径/你的仓库
# 同时打开多个仓库
./build/easy_git /绝对路径/仓库A /绝对路径/仓库B
```

也可直接运行 `./build/easy_git`，点击 **Browse...** 浏览并选择文件夹，再点击 **Open repository**；也支持手动输入路径。取消浏览会保留原路径。文件夹浏览使用系统 Zenity 选择器，未安装时会提示并保留手动输入入口。可打开仓库子目录，操作会自动定位至仓库根目录。

ImGui 固定为 `v1.92.5`，GLFW 固定为 `3.4`，首次配置时由 CMake 下载并校验 SHA-256。离线构建可通过 `FETCHCONTENT_SOURCE_DIR_IMGUI`、`FETCHCONTENT_SOURCE_DIR_GLFW` 指定本地源码；也支持 `.deps/imgui-1.92.5` 和 `.deps/glfw-3.4`。

构建默认不要求代理。若网络需要，可在运行 CMake 前设置 `https_proxy` 和 `http_proxy`，例如 `http://127.0.0.1:32766`；应用内 AI 代理单独配置，默认为空。

## 主题、AI 与配置

- 右上角 **Light / Dark** 一键切换浅色与深色主题，覆盖提交图、文件列表、Diff 和弹窗。
- 右上角 **Settings** 可启用 AI 提交信息，并配置服务商预设、API Base URL、模型/Endpoint ID、API Key、代理、输出语言、超时、输出 token 上限、Diff 字节上限及风格提示词。
- 预设 DeepSeek、Kimi、Qwen（通义千问）、Doubao（豆包）及 Custom。预设值可编辑，以匹配账户开通的模型、业务空间和地域；各服务商分别保留 API Key、模型名称、接口地址、代理及生成参数；切换时恢复对应配置，首次选择使用预设值。点击 **Save settings** 会将本次编辑过的所有服务商配置一并写入 `~/.easy_git`，重启后继续保留；**Cancel** 放弃本次编辑。兼容旧版单服务商配置。
- 默认关闭 AI。启用并保存设置后，先暂存文件，再点击 **COMMIT** 右侧 **AI Generate**。请求只发送暂存区的 Diff 与统计，不发送未暂存改动；返回结果填入摘要和描述，仍需手动检查并提交。生成中可 **Cancel AI**，错误不会清空现有草稿；暂存内容在请求期间变化时会拒绝过期结果。
- AI 默认代理为空，直接连接；可在设置中填写代理地址，本机模型跳过代理。已保存的代理配置保持不变。接口使用 Chat Completions 兼容协议，远程使用 HTTPS，本机可使用 HTTP；API Base URL 也接受完整 `/chat/completions` 地址。
- 默认暂存 Diff 上限 65536 字节，超出时提示调整配置或减少暂存文件，不静默截断。调用会产生所选服务商的 API 用量。
- 已打开仓库、当前仓库、主题和 AI 设置自动覆盖保存到 **`~/.easy_git`**（JSON）。采用临时文件替换，权限为 **0600**；API Key 明文保存在这个仅当前用户可读写的文件中。启动时恢复仓库和设置，关闭的仓库不再恢复；不存在的仓库会显示错误并保留路径，便于下次重试。
- 配置损坏时保留原文件并提示；在 Settings 保存可用当前设置覆盖。可用 `--config /tmp/example.easy_git` 指定测试配置，避免修改正式配置。

预设接口参考：[DeepSeek](https://api-docs.deepseek.com/)、[Kimi K2.6](https://platform.kimi.com/docs/guide/kimi-k2-6-quickstart)、[Qwen 兼容接口](https://help.aliyun.com/zh/model-studio/model-calling-in-sub-workspace)、[豆包 Chat API](https://www.volcengine.com/docs/82379/1494384)。

![浅色主题与生成的提交草稿](docs/ai-light-theme.png)

![AI 模型设置](docs/ai-settings.png)

## 使用

- 顶部仓库标签页支持切换、拖动排序和关闭；点击 `+` 或 **+ Open repository** 添加仓库。各仓库独立保存文件选择、搜索、面板状态、提交摘要和描述，后台操作不会写入其他仓库。重复打开同一仓库或其子目录时切换到已有标签。
- 左侧选择工作区，或点击中间的 **// WIP**。右侧按 GitKraken 的 Commit Panel 形式展示 **Unstaged Files / Staged Files** 两个可折叠区域，支持 **Path / Tree**、文件筛选、状态标记、悬停暂存/取消暂存按钮及右键操作；Path / Tree 切换居中，Discard 位于左侧。区块标题右侧在有选中文件时批量暂存/取消暂存所选文件，无选择时操作全部文件；行内按钮仍只操作该行文件。
- 文件列表支持 **Ctrl + 点击** 逐个添加/取消选择、**Shift + 点击** 从锚点选择连续范围、**Ctrl + Shift + 点击** 追加范围。Path / Tree、工作区、提交和 Stash 文件列表均支持；跨区块选择会切换到该区块。Tree 的范围按屏幕目录顺序，跳过折叠目录；筛选或折叠后清除不可见文件的选择。
- **Unstaged Files** 选中文件后，点击左侧 **Discard** 或右键 **Discard selected files...**。确认窗口列出完整选择，执行前检查所有文件状态；丢弃未暂存改动并保留已暂存内容，未跟踪文件会永久删除。冲突文件、目录和子模块需在对应编辑器或仓库中处理。
- 输入 **Commit summary** 和可选描述后，点击 **Commit changes**。有未解决冲突或暂存区为空时不能提交。未发送的草稿在标签页名称中用 `*` 标记；关闭此标签前会确认，Git 操作执行期间不关闭标签。
- 中间显示分叉、合并和多父提交关系。点击提交后，右侧显示提交信息及 **Changed Files** 列表，也支持 Path / Tree 和筛选；点击其中一个文件只查看该文件的 Diff，包含重命名和删除文件。
- 工作区和提交文件的 Diff 都在中间展开，显示旧/新行号及增删背景。点击 **< Commit graph** 返回提交图；暂存/取消暂存当前文件后，预览会随其新状态更新。
- 提交历史每次加载 300 条，底部按钮继续加载。搜索只匹配已加载提交；搜索时隐藏关系线，以免省略中间提交后产生错误连线。
- 点击左侧 Local / Remote 分支，自动返回提交图、清除提交搜索并滚动到该分支的最新提交；超出已加载历史时自动补充加载。右侧同步显示该提交的文件。
- 顶部分支下拉框或分支右键菜单切换分支。支持新建分支、标签，以及 Fetch、仅快进 Pull、普通 Push。
- 左侧 **Local / Remote / Tags / Stash** 区域可以收起、展开；折叠状态在各仓库标签内独立保留。
- 提交右键菜单或右侧 **Actions** 提供 **Cherry-pick、Merge、Revert、Reset**；分支右键也可合并到当前分支。对合并提交执行 Cherry-pick / Revert 时可选择主线父提交。
- **Reset** 支持 Soft（保留暂存区与工作文件）、Mixed（重置暂存区，保留工作文件）、Hard（覆盖暂存区与工作文件）；执行前确认，Hard 还需勾选丢弃本地改动。Hard 也可能删除阻碍恢复路径的未跟踪文件。
- **Stash changes** 保存已跟踪和未跟踪文件。**单击 Stash 仅预览**：右侧显示 Stash Files，点击文件在中央查看 Diff；**右键 Apply / Delete**，执行前确认。Apply 保留原 Stash，可勾选恢复暂存状态；列表已变化时，Delete 拒绝使用过期的位置删除。
- Merge / Cherry-pick / Revert 遇到冲突后，右侧显示 **Continue / Abort**，适用时提供 **Skip**。在编辑器解决冲突并暂存后点击 Continue；Abort / Skip 前确认。也可继续或中止外部启动的 Rebase。Stash Apply 冲突没有 Git 序列状态，解决并暂存后按普通工作区流程提交。
- 拖动两条面板分隔线调整宽度；按 **F5** 刷新。Git 操作异步执行，失败会显示实际错误。

![分支定位](docs/branch-navigation.png)

![文件多选与居中工具栏](docs/file-multiselect.png)

![批量 Discard 确认](docs/discard-multiple.png)

![提交右键 Git 操作菜单](docs/git-operations.png)

![Stash 预览与右键菜单](docs/stash-preview.png)

![提交文件 Tree 视图与独立文件 Diff](docs/commit-file-tree.png)

布局与交互参考：[GitKraken 界面与 Commit Panel](https://help.gitkraken.com/gitkraken-desktop/interface/)、[文件暂存交互](https://help.gitkraken.com/gitkraken-desktop/staging/)。

首次提交需要已有 Git 作者配置；网络认证使用已有 Git credential helper / SSH 配置。程序不保存 Git 凭据，不弹终端密码输入；AI API Key 按上文保存到本地配置。HTTP(S) 远程操作继承启动环境中的代理变量；SSH 代理仍由 SSH 配置决定。

## 验证

```sh
ctest --test-dir build --output-on-failure
```

测试只在 `/tmp` 创建临时仓库。`git_workflow` 验证空仓库、中文/空格/换行路径、字面量参数、部分暂存、提交、重命名、合并图、多父提交布局、分页、Detached HEAD、Stash、本地远程推拉，以及根提交/合并提交/删除/二进制文件的文件列表和单文件 Diff。

`git_operations` 验证 Cherry-pick / Merge / Revert、合并提交主线选择、三种 Reset、Stash 只读预览与未跟踪文件、恢复暂存状态、过期删除保护，以及真实冲突的继续、中止和跳过、外部 Rebase 中止、独立 Worktree 的操作状态，以及 Discard 对部分暂存、删除、重命名、特殊路径、符号链接和过期状态的处理，以及批量 Discard 的整组选中文件检查和暂存内容保留。

`settings_and_ai` 使用本地回环 HTTP 模拟服务验证 JSON 请求、Bearer 认证、仅暂存区内容、中文响应、HTTP/格式错误、取消请求、暂存区变化保护，以及配置覆盖保存、各服务商配置切换与重启恢复、旧配置迁移、0600 权限与损坏配置处理；不调用付费模型。运行此测试需要允许本机监听端口。

`repository_tabs` 使用真实 ImGui 状态（无需 X 服务）验证标签点击、独立草稿与文件选择、切换期间的后台 Diff/暂存、相同根目录去重、关闭保护、文件树及 Diff 行号，以及 Stash / Discard 的仓库隔离、分支跨分页定位和提交图滚动、Ctrl / Shift / Ctrl+Shift 多选、连续点击的 Diff 更新、Tree 可见范围、筛选及批量暂存/取消暂存/丢弃、主题/模型配置与仓库重启恢复、AI 失败保留草稿。

无需图形依赖也可单独测试 Git 后端：

```sh
cmake -S . -B build-core -DEASY_GIT_BUILD_GUI=OFF
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

可选图形烟雾检查（需要 `xvfb`、`xauth`）：

```sh
xvfb-run -a -s '-screen 0 1440x900x24' \
  ./build/easy_git /你的仓库 --config /tmp/easy-git-smoke.json \
  --frames 30 --screenshot /tmp/easy-git.ppm
```

`docs/` 的截图来自真实临时测试仓库。新版界面已用实际鼠标、键盘检查多仓库切换、草稿保留、单文件暂存、提交文件列表与 Diff、Path / Tree 切换及 1080×720 布局。Git 操作界面另验证了 Local / Remote / Stash 折叠、Stash 预览与 Apply / Delete、Cherry-pick 和 Hard Reset 的勾选保护及执行。新增分支定位检查使用 341 个提交验证跨页跳转及 Remote 定位；Discard 检查验证取消、确认及保留暂存内容；另用真实 Ctrl / Shift 按键和鼠标验证多选、Tree 折叠范围、批量暂存/丢弃及窄窗口工具栏。主题/AI 界面已验证中文生成结果填入、浅色与深色切换、设置编辑保存和重启后当前仓库恢复。

## 当前边界

- 当前仅支持 Linux / X11（Wayland 桌面可通过 XWayland 运行），最低窗口尺寸为 1080×720；未实现 Windows/macOS 进程后端。
- 合并提交展示相对第一父提交的 Diff；工作区支持完整文件级暂存，尚无按行/区块暂存、冲突编辑器、交互式 Rebase 或拖拽提交。
- 仓库、当前仓库、主题和 AI 设置持久化；提交草稿、文件选择和面板布局仅保留在当前会话，退出后不恢复；外部修改需手动刷新。未实现 Clone、仓库初始化、删除分支及强制推送。
- 单次 Git 命令超时 120 秒，输出预览上限 8 MiB。超限时明确报错，不显示不完整的仓库结构。
- 远程操作已使用本地远程仓库验证；AI 请求使用本地模拟接口验证，尚未使用用户的 API Key 调用真实服务商。

实现参考：[Dear ImGui 官方 GLFW/OpenGL 示例](https://github.com/ocornut/imgui/tree/v1.92.5/examples/example_glfw_opengl3)、[Git status 格式](https://git-scm.com/docs/git-status)、[Git log 格式](https://git-scm.com/docs/pretty-formats)。

## 参与贡献

欢迎提交 [Issue](https://github.com/NickCyrusss/easy_git/issues) 或 Pull Request。报告问题时请提供 Linux 发行版、复现步骤和相关错误信息；不要附带 API Key、个人配置文件或私有仓库内容。修改 Git 操作、配置或 AI 请求后，请运行相关测试，并说明界面改动的验证方式。

## 许可证与致谢

项目代码采用 [MIT 许可证](LICENSE)。感谢 [Dear ImGui](https://github.com/ocornut/imgui)、[GLFW](https://github.com/glfw/glfw)、[libcurl](https://curl.se/libcurl/)、[json-c](https://github.com/json-c/json-c) 和 [Git](https://git-scm.com/)。第三方项目遵循各自的许可证。
