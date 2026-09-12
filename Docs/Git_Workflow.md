# RenderLab Git 版本管理

建立日期：2026-09-12。范围：本地 Git 仓库和 Git LFS，不创建远程仓库、不上传项目。

## 1. 仓库约定

- 主分支：`main`。
- 首次提交用于保存学习开始前的工程、MCP 接入和文档基线。
- 使用本机已配置的提交身份 `ARKS23`，未改动全局 Git 用户设置。
- Git LFS 通过 `git lfs install --local` 安装到当前仓库，未启用文件锁。
- 只修改版本管理相关文件，不修改 Engine、渲染配置或现有 MCP 实现。

本地提交可以恢复已提交的源码和资产，但不是异机备份。当前没有远程仓库，磁盘损坏仍可能丢失全部历史；需要时再单独配置远程或备份。

## 2. 跟踪与忽略范围

| 内容 | 处理方式 |
| --- | --- |
| `.uproject`、`.vsconfig` | 跟踪，保留工程与工具链声明。 |
| `Source/`、`Config/`、`Docs/` | 跟踪源码、项目默认配置和学习记录。 |
| `Content/` | 跟踪资产；不忽略 World Partition 的 External Actors/Objects。 |
| `Plugins/` | 跟踪插件源码、描述符、资源和许可证，忽略可重新编译的产物。 |
| `Tools/UnrealMCP/` | 跟踪启动、安装、测试脚本、Server 源码和依赖锁文件。 |
| `.codex/config.toml` | 跟踪项目 MCP 配置；其他 `.codex` 本机状态不跟踪。 |
| `Binaries/`、`Intermediate/`、`Saved/`、`DerivedDataCache/` | 忽略，包括源码插件中的同名目录。 |
| `.vs/`、`.idea/`、`.vscode/`、生成的 `.sln`/`.slnx`/`.vcxproj` | 忽略本机 IDE 状态和生成文件。 |
| Python 虚拟环境、字节码、测试缓存 | 忽略，可根据已跟踪脚本和锁文件重建。 |
| GPU 抓帧、Trace、日志和转储 | 默认忽略；重要结论和截图整理进实验文档。 |
| `.env`、私钥/证书容器等本机文件 | 默认忽略；提交前仍需检查，规则不是完整的秘密检测工具。 |

不整体忽略 `Build/`，避免以后丢失需要版本管理的打包资源。当前插件有源码，忽略其 `Binaries/`；如果以后引入仅有二进制的插件，必须重新评估规则并明确添加例外。

`.codex/config.toml` 当前含本机绝对路径。为了保持现有 MCP 可用，本次没有改写；迁移工程目录或换机器时，需要按实际路径调整该文件，并检查 MCP 安装文档中的本机路径。

## 3. Git LFS

`.gitattributes` 为 `.uasset` 和 `.umap` 配置 LFS。这些规则适用于后续创建的资产；初始化时项目中尚无这两类文件。

- 普通 Git 历史存放 LFS 指针，完整内容保存在本机 LFS 对象目录。
- 当前未配置远程，所以 LFS 不会把资产自动上传到云端。
- 以后配置远程时，需要确认其 LFS 支持、容量和带宽限制。
- `.fbx`、大型纹理、音频等原始资源引入前，再按实际大小决定是否纳入 LFS。
- 小型插件图标等现有二进制文件由普通 Git 管理。

检查命令：

```powershell
git lfs ls-files
git lfs status
git check-attr filter diff merge text -- Content/RenderLab/Example.uasset
```

最后一条只检查路径适用的属性，不会创建示例资产。当前 `git lfs ls-files` 为空是正常的。

## 4. 日常提交

1. 在 Unreal Editor 中保存本次准备提交的关卡和资产；Git 无法保存尚未写入磁盘的编辑状态。
2. 检查变化，把同一个学习任务的代码、资产和记录放在同一次或一组有明确含义的提交中。
3. 只暂存本次需要的文件，检查后提交。版本管理不会替代编译、运行和效果验收。

```powershell
git status --short
git diff
git add -- Docs/UE_Rendering_Learning_Roadmap.md
git diff --cached --stat
git diff --cached
git commit -m "docs: update rendering learning roadmap"
```

上面以文档为例。实际提交实验时，将 `git add` 的路径换成这次修改的源码、资产和记录；二进制资产不能通过文本 diff 完整审查，应在 Editor 中验证。

开始第一个实验时，可选择建立分支；本次初始化只创建 `main`，没有提前创建学习分支：

```powershell
git switch -c lab/01-materials
```

## 5. 查看历史与恢复原则

```powershell
git log --oneline --decorate -10
git show --stat HEAD
git status
```

- 回退已提交的学习改动时，优先评估 `git revert`，让撤销本身也留下记录。
- 不直接使用 `git reset --hard` 或 `git clean -fdx`；它们可能破坏未提交内容或删除被忽略的本机文件。
- 切换涉及 UE 资产的历史前，先保存或备份当前工作，并关闭相关编辑器，避免内存中的旧资产覆盖磁盘版本。
- 不把“切回旧提交”当作无风险操作，尤其是插件、模块和资产结构同时变化时。

本次只准备版本管理，不执行任何历史回退、缓存删除或远程发布。

## 6. 新机器或重新取出工程

以下为未来恢复流程，不是本次已执行的操作：

1. 准备匹配的 UE 5.8、C++ 工具链、Git 与 Git LFS。
2. 取得仓库并执行 `git lfs install --local`；如果来源有 LFS 远程，执行 `git lfs pull` 并确认资产不是仅有指针。
3. 重新生成项目文件并编译项目及插件；生成的解决方案和二进制不在仓库内。
4. 按 [MCP 接入说明](E:/ue_project/pratice/RenderLab/Docs/UE_MCP_Setup.md) 重建 Python 虚拟环境，并调整本机路径。
5. 打开工程并验证资产、插件、关卡；确认需要 MCP 时再进行连接测试。

本地手动备份若要保留完整历史和 LFS 内容，不能只复制工作区文件；还需要保留 `.git` 目录及其中的 LFS 对象。
