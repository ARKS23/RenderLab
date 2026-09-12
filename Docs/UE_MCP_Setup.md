# RenderLab UE 5.8 MCP 接入方案与验证记录

日期：2026-09-11。目标：本机 Windows / Unreal Engine 5.8 的最小可运行 MCP，不开发渲染功能。

**最终结果：最小接入已完成并验证。** UE 5.8 本机编译成功；Codex 自身发现 9 个工具域；MCP 到 Editor 的 Level、Actor、Component、Blueprint Graph 和材质基本信息读取通过。当前正在进行的 Codex 会话未热加载新工具，重新进入本项目会话后使用。

## 1. 修改前检查

- 项目：`E:\ue_project\pratice\RenderLab\RenderLab.uproject`，C++ 空白工程，无 Git 仓库。
- `EngineAssociation = 5.8`；两个 Target 使用 `BuildSettingsVersion.V7` 和 `EngineIncludeOrderVersion.Unreal5_8`。
- 实际引擎：`E:\ue\UE_5.8`，`Engine/Build/Build.version` 为 **5.8.0 / CL 55116800 / ++UE5+Release-5.8**；现有 Editor 日志和进程路径一致。
- `Source/RenderLab` 只有默认模块；`Content` 无现有资产；不存在项目 `Plugins` 目录。
- `.uproject` 原来仅显式启用 `ModelingToolsEditorMode`。
- `Config`、`Saved/Config` 未发现 MCP、Python Remote Execution、Remote Control 或 Automation Bridge 配置。
- PythonScriptPlugin 已由引擎插件依赖加载，不等于已启用远程执行。
- `Automation_RenderLab.sln/.slnx` 是解决方案文件，不能据此认定存在 Editor 自动化桥。
- Codex 全局配置存在其他 MCP，未发现 Unreal MCP；RenderLab 已是 trusted project。不改动全局配置。
- 已开启的 Editor 必须保护其未保存状态；不强杀，不自动保存所有资产。

## 2. 方案选择

选用 **GenOrca/unreal-mcp / UnrealMCPython 2.2.0** 的源码快照：

```text
Repository: https://github.com/GenOrca/unreal-mcp
Commit: f7986db239516aa4299ddc6f54d713253bd82631
Commit date: 2026-07-07
License: Apache-2.0
```

选择依据来自源码，不是仅凭 README：

- 插件为 `Editor` 模块，`PostEngineInit` 加载；仅声明 PythonScriptPlugin 插件依赖。
- C++ `MCPythonHelper` 提供 `GetBlueprintGraphInfo`、`ListBlueprintVariables`、`ListCallableFunctions`、节点创建与 Pin 连接接口。
- Graph JSON 包含节点名称、类、位置、可见 Pin、方向、类型、默认值和 `linked_to`。
- Python 动作提供关卡 Actor 列表、Actor Component 查询、资产信息、材质相关接口。
- TCP 仅监听 `127.0.0.1:12029`；外部 MCP 使用 stdio，不需要云服务、账户或 API key。
- 保留上游插件源码以便追踪和回滚，不搬入示例工程、示例资产或测试内容。

对比过的方案：

| 方案 | 调研结论 |
| --- | --- |
| chongdashu/unreal-mcp | 2025-04-22 后无仓库推送；较旧的 5.5 实验实现，不能用 README 的 5.5+ 推断 5.8。 |
| runreal/unreal-mcp | 不需要自定义 C++ 插件，但依赖 Python Remote Execution；没有等价的原生 Blueprint Graph/Pin 读取层。 |
| db-lyon/ue-mcp | 活跃并已有 5.8 代码；插件强制依赖 GAS、PCG、ControlRig 等多个领域，超出本次范围。 |
| ChiR24/Unreal_mcp | 活跃，支持原生 HTTP MCP；大型 Automation Bridge 带多个自动启用依赖，首次最小接入成本更高。 |
| UE 5.8 ToolsetRegistry | 本机确有原生 ToolsetRegistry，但注册表不是供 Codex 连接的 MCP Server；MCPClientToolset 是客户端，不能混淆方向。此次不启用 AllToolsets/AIAssistant。 |

仓库元数据、描述和源码于 2026-09-11 在线获取；仍需本机编译和端到端测试才能认定此快照可用。

## 3. 架构

```text
Codex
  -> stdio MCP initialize / tools/list / tools/call
  -> Tools/UnrealMCP 中的 Python MCP Server（项目独立 .venv）
  -> localhost TCP JSON :12029
  -> Plugins/UnrealMCPython 中的 C++ Editor Bridge
  -> Game Thread / Unreal Python / C++ Blueprint helper / Editor APIs
```

MCP Server 不在 Unreal 的 Python 环境中安装依赖。Editor Plugin 内部调用引擎 Python，C++ helper 补充 Python 未直接暴露的 Graph/Pin 数据。

## 4. 文件及配置范围

| 路径 | 用途 |
| --- | --- |
| `Docs/UE_MCP_Setup.md` | 本方案、操作指南、风险与最终验证记录；先于工程修改创建。 |
| `Plugins/UnrealMCPython/` | 上游插件的 Source、Content/Python、描述符和许可证。 |
| `Tools/UnrealMCP/server/` | 固定提交的 MCP Server；不使用每次启动联网升级的 npx/uv run。 |
| `Tools/UnrealMCP/requirements*.txt` | FastMCP 兼容版本及实际解析的完整依赖锁定。 |
| `Tools/UnrealMCP/*.py` / `*.ps1` | 项目启动、身份核验和最小测试脚本；后续记录具体文件。 |
| `Tools/UnrealMCP/UPSTREAM.md` | 来源、commit、许可证、本地补丁记录。 |
| `.codex/config.toml` | 仅 RenderLab 生效的 MCP 启动命令与工具域白名单。 |
| `Saved/UnrealMCP/` | 修改前备份、编译日志、测试 JSON、临时输出，不是游戏内容。 |

需要修改 `.uproject`：只追加 Editor-only 的 `UnrealMCPython`、`PythonScriptPlugin`、`EditorScriptingUtilities` 插件条目。后者是上游 Python 动作实际使用 `EditorAssetLibrary` / `EditorLevelLibrary` 的运行依赖，原上游描述符未声明，显式启用避免依赖其他插件偶然加载。

不修改 `Source/RenderLab`、Target、渲染 Config、Engine 源码或全局 Codex 配置。不启用 GAS、PCG、ControlRig 等额外插件。插件自身使用的引擎 C++ 模块不等于新安装第三方包。

Python 需要启用；**Python Remote Execution 不需要，Remote Control 不需要，Web Remote Control 不需要**。不开放 UDP 广播、不新增防火墙规则、不绑定 `0.0.0.0`。

外部 Python 使用本机 UE 5.8 附带的 Python 3.11 创建项目 `.venv`，只将 FastMCP 及其 MCP 运行依赖安装到 `.venv`；不向 Engine Python 或系统 Python 执行 pip install。最终使用上游锁文件的 **FastMCP 2.7.1 / MCP SDK 1.9.3**，36 个运行依赖，无 pytest 等开发包。FastMCP 2.14.7 试装后发现不兼容旧版 `Image` 导出，已退回固定兼容集，额外依赖通过 pip 卸载，`pip check` 通过。不是实时追随上游最新版；旧依赖组合只用于可信本机 stdio，未做完整 CVE 审计，不应用于公网 HTTP 服务。

## 5. Codex 配置

采用项目级 `.codex/config.toml`，不使用 Claude/Cursor 的 `mcp.json`：

```toml
[mcp_servers.unreal_renderlab]
command = "E:/ue_project/pratice/RenderLab/Tools/UnrealMCP/.venv/Scripts/python.exe"
args = ["E:/ue_project/pratice/RenderLab/Tools/UnrealMCP/start_server.py"]
cwd = "E:/ue_project/pratice/RenderLab"
startup_timeout_sec = 30
tool_timeout_sec = 60
enabled = true
enabled_tools = ["actor", "asset", "blueprint", "editor", "level", "material", "static_mesh", "texture", "util"]
```

启动器已固定上游 import 路径、仅使用本地包，每次 Editor 调用前验证绝对项目目录和 5.8 引擎版本，拒绝误连其他工程；顺序发送短 TCP 请求。工具目录不触发 Editor 连接。多工程不要同时争用端口 12029。身份检查是客户端防误操作，不是 Bridge 端身份认证或跨进程原子锁。

项目配置仅在受信任项目加载。已有 Codex 会话不保证热加载新服务器：配置保存后重新打开本项目的 Codex 会话，必要时重启 Codex。`codex mcp list/get` 只证明配置可见，不等于 `tools/list` 或实际 UE 调用成功。

官方配置依据：

```text
https://developers.openai.com/codex/mcp.md
```

## 6. 启动顺序

1. 一次性安装依赖并用 `E:\ue\UE_5.8` 编译插件，不使用其他 UE 版本的二进制。
2. 保存用户需要保留的编辑工作，正常重启 RenderLab Editor；不使用 Live Coding 首次加载新插件。
3. 确认 Plugins 中 `UnrealMCPython`、`Python Editor Script Plugin`、`Editor Scripting Utilities` 已启用。
4. Editor 完成初始化并打开关卡；日志应有 `LogMCPython`，端口 12029 的实际 owner 应是 RenderLab Editor。
5. 在 RenderLab 目录启动/重新加载 Codex。Codex 自动以 stdio 启动 MCP Server，不需要另开一个常驻 Server 窗口。
6. 先查询项目身份，再执行只读 Level / Actor / Blueprint 测试。

独立手工启动 stdio Server 后没有输出是正常的：它等待 MCP JSON-RPC，不是 HTTP 服务，不能用浏览器打开 12029。

## 7. 最小验证计划

- 独立 MCP Client 完成 `initialize` 和 `tools/list`，验证 Server 即使 Editor 未启动也可被发现。
- 用 Codex 自身的 MCP 状态/工具目录验证发现，不把自写 Client 冒充为 Codex。
- `util.get_project_info` 必须返回 RenderLab 且引擎版本为 5.8；确认真实 Bridge 通信。
- `level.get_current_level_path`：无破坏测试，不保存/切换地图。
- `level.list_level_actors`：记录 Actor 数量和示例，确认来自当前 Editor World。
- `actor.list_actor_components`：查询已有 Actor 的 Component，不仅查询 Blueprint 的 SCS。
- 工程无 Blueprint 时先查 Engine 自带 Blueprint，优先只读；必要时使用隔离的瞬态测试对象，禁止覆盖同名已有资产。
- `asset.get_asset_info` / `blueprint.list_blueprint_variables` / `list_callable_functions`。
- `blueprint.get_blueprint_graph_info`：检查节点、Pin、非空连接和目标节点/Pin 一致性。
- 节点创建/连接只在明确隔离的测试对象上验证；不支持的功能只标记，不扩写新领域工具。
- 保存原始 JSON 结果、编译日志和能力矩阵。工具返回 HTTP/TCP 成功不等于动作 `success` 为 true。

## 8. UE 5.8 兼容性与限制

1. **JSON 键变化已查源**：本机 `Engine/Source/Runtime/Json/Public/Dom/JsonObject.h` 有 `FSharedString` 键模式。上游 `MCPythonTcpServer.cpp` 已用 `FString KeyString = *Pair.Key` 兼容此变化；旧快照直接把 Key 当 FString 的地方必须检查。仍以当前 CL 编译为准。
2. **BuildSettings V7 / include order**：不能拿 5.6/5.7 DLL 直接加载；本机 UBT、UHT、MSVC 编译和链接是必要验证。
3. **Python API**：上游部分动作使用已弃用的 `EditorLevelLibrary`，5.8 中需核查实际可用性。若失败，仅把触及的动作改用 `EditorActorSubsystem` / `UnrealEditorSubsystem`，不批量重构。
4. **Graph 范围**：`FindGraphByName` 只搜索 `UbergraphPages` 和 `FunctionGraphs`；不递归枚举宏图、折叠子图。读图会跳过 `bHidden` Pin，不能宣称完整无损图导出。
5. **变量/函数范围**：变量来自 `NewVariables`，非全部继承变量或函数局部变量；函数来自 GeneratedClass 的 `FUNC_BlueprintCallable` 反射，非全部函数源码。已知名称的函数图可以读取。实测发现 `ListCallableFunctions` 遍历 `TFieldIterator<FProperty>` 未筛选 `CPF_Parm`，所以 `parameters` 含 `CallFunc_*` 编译临时变量，不能当精确函数签名。可能修复位置为插件 `Private/MCPythonHelper.cpp` 的参数循环；此次仅标为部分支持，未扩写。
6. **Actor 范围**：当前已加载 Editor World；不等于 World Partition 所有尚未加载的 Actor。Actor label 可能重复，必须先查列表识别目标。
7. **网络**：上游是本地无鉴权 Python 执行桥，能读写项目和调用任意 UE Python；localhost 并非完整安全沙箱。只在可信本机使用，不隧道转发；用完关闭 Editor 即停止监听。监听成功必须检查实际 Socket，不能只看上游启动日志。
8. **TCP 边界**：上游无长度帧，单次可读数据当作请求，长/分片请求可能失败；首次只用短请求、顺序调用，不宣称任意大小可靠。后续若实测出现，才做传输修复。
9. **游戏线程**：操作调度到 Game Thread；模态对话框、编译、PIE 或繁重任务可能阻塞，不能把超时操作盲目重试为第二次写入。
10. **渲染开发边界**：材质/场景可通过工具辅助检查；RDG/RHI/Shader 源码仍用 Codex 文件工具阅读。此次不提供 GPU capture、RenderDoc/PIX、RDG 运行时追踪或 Ray Tracing 调试控制，也不以通用 Python 执行能力冒充这些专用工具。
11. **节点写入边界**：`ConnectBlueprintPins` 检查 Schema 后直接 `MakeLinkTo`，并未完整处理所有自动转换/组合断链响应；只把基础创建/连接认定为源码可用、写入未验证，不能宣称所有 Blueprint 节点都可可靠编辑。节点工具会修改资产，调用前需明确对象与用户意图。此次没有修改任何 Blueprint。

## 9. 回滚

1. 正常关闭 RenderLab Editor 和本项目 MCP 进程。
2. 从 `.uproject` 的 Plugins 数组移除本次追加的三个条目；保留原有插件和其他后续变更。修改前备份在 `Saved/UnrealMCP/Backups`。只有文件未发生其他编辑时才可整体恢复备份。
3. 移除 `.codex/config.toml` 中的 `[mcp_servers.unreal_renderlab]` 表；保留日后添加的其他配置。
4. 移走或删除本次新增的 `Plugins/UnrealMCPython` 和 `Tools/UnrealMCP`。递归操作必须先确认绝对路径位于 RenderLab 下。
5. 可保留 Docs 和测试证据；按需删除只属于插件的生成物。不要清空整个 Content、Config、Source、Saved 或 Engine。
6. 本方案不改 Engine、全局 Python、全局 Codex、Remote Execution、Remote Control，因此无对应系统级回滚。

## 10. 实际执行与能力矩阵

2026-09-11 完成本机验证；以下是实测，不是 README 能力声明：

| 能力 | 状态 | 证据或限制 |
| --- | --- | --- |
| UE 5.8 插件编译/链接 | 已支持 | UHT、8 项 C++/资源编译、DLL 链接等 11 个 action 全部成功，36.82 秒；无 C++ 移植补丁。 |
| 正确版本二进制 | 已支持 | Engine 和插件 `UnrealEditor.modules` BuildId 都为 `55116800`。 |
| MCP Server 启动/握手 | 已支持 | stdio `initialize`、`tools/list` 成功；独立目录测试不调用 Editor。 |
| Codex 发现工具 | 已支持 | 本机 `codex app-server` 的 `mcpServerStatus/list` 实际返回 `unreal_renderlab` 与 9 个工具域，不是只检查 TOML。 |
| 当前会话热加载 | 部分支持 | 进行中的会话工具目录不自动更新；需要重新打开本项目会话或重启 Codex。 |
| Editor/Bridge 通信 | 已支持 | `util.get_project_info` 返回 RenderLab、`5.8.0-55116800+++UE5+Release-5.8`。 |
| 当前关卡查询 | 已支持 | `/Temp/Untitled_1.Untitled_1`，未保存模板地图，没有新建/切换/保存关卡。 |
| Actor 列表 | 已支持 | 138 个已加载 Actor；与 Editor Outliner 数量一致，不含未加载 World Partition Actor。 |
| Actor Component 查询 | 已支持 | DirectionalLight 返回 DirectionalLightComponent、ArrowComponent、BillboardComponent，共 3 个。 |
| Blueprint 基本信息 | 已支持 | 只读 `/Engine/EngineSky/BP_Sky_Sphere`，名称、类型 Blueprint、包路径正确。 |
| Blueprint 自有成员变量 | 已支持 | 读取 17 个变量；继承变量与局部变量不在此接口范围。 |
| Blueprint 函数 | 部分支持 | 过滤 Sky 后返回 RefreshMaterial、UpdateSunDirection、UserConstructionScript；参数列表混入临时变量。 |
| Blueprint Event/Function Graph | 已支持 | UserConstructionScript：8 个节点、25 个可见 Pin、18 条双向 link 记录，即 9 条连接；端点存在且互相引用。 |
| 完整 Graph/隐藏 Pin/宏图 | 部分支持 | 只读 Event/Function Graph，隐藏 Pin、宏图、折叠子图不完整。 |
| Blueprint SCS Component | 已支持 | Sky Blueprint 返回 2 个 SCS Component。 |
| 节点创建/连接 | 部分支持 | 上游接口已编译并被发现；未对资产做写入实测，不保证所有 Schema 情形。 |
| Editor 交互 | 已支持 | 关卡/资产选择查询和 Python 只读身份查询成功；未启动 PIE、修改视口或保存资产。 |
| 材质读取 | 部分支持 | `/Engine/EngineSky/M_Sky_Panning_Clouds2` 基本信息和表达式列表查询通过，表达式计数 74；返回的 position 为 null，列表还含注释对象，不能直接把列表长度等同表达式计数。不是完整材质 Pin/连线导出，也未测写入。 |
| GPU capture / RDG / RHI 专用调试 | 不支持 | 此 MCP 没有经过验证的专用工具；可另用 Codex 文件读取分析 UE 源码，此次不扩展。 |
| 全离线运行 | 已支持 | 安装阶段需要获取依赖；运行阶段只用项目 Python、stdio 和 loopback TCP，不依赖云端。Codex 模型自身的联网方式不属于 MCP Bridge。 |

运行记录：

- `Saved/UnrealMCP/Build.log`：本机 UBT 编译成功。
- `Saved/UnrealMCP/smoke-test.json`：完整 MCP 工具返回、Actor 列表、Blueprint 节点/Pin/连接、材质基本信息。
- `Saved/UnrealMCP/smoke-offline.json`：不查询 Editor 的 MCP 握手/工具目录结果。
- `Saved/UnrealMCP/codex-discovery.json`：真实 Codex App Server 工具发现结果。
- `Saved/UnrealMCP/codex-discovery.stderr.log`：Codex 发现过程诊断。
- `Saved/UnrealMCP/adapter-tests.log`：10 项项目适配器测试结果。
- `Saved/UnrealMCP/Backups/RenderLab.uproject.pre-mcp`：原始工程描述符备份。
- 10 项 `test_adapter.py` 单元测试通过：正确/错误项目、错误引擎、无效返回、连接失败、误连不转发、正常转发、离线发现、内层成功/失败保留。
- 首次构造图请求使用 UI 显示名 ConstructionScript 失败；查引擎源码与函数列表确认内部名 UserConstructionScript 后全流程通过。这不是修复 Engine API。
- 初始 UBA 有本地辅助端口 1345 bind 警告，但本地编译成功；这不是 MCP 端口。未修改防火墙或 UBA 全局设置。

## 11. 最终文件清单

已有文件仅修改 `RenderLab.uproject`，追加三个 Editor-only 插件条目；原 ModelingToolsEditorMode 保留。

新增第三方组件：

- `Plugins/UnrealMCPython/`：62 个上游源文件/资源/许可证（不计本机构建生成物），约 0.79 MB；仅一个项目 Editor Plugin。上游 Python 动作按需导入，不启用额外 Gameplay/PCG 等插件。
- `Tools/UnrealMCP/server/`：固定提交的 MCP Server 源码、上游锁文件、许可证和测试源码。
- `Tools/UnrealMCP/.venv/`：本机生成的 Python 环境，36 个运行包，不包含开发测试依赖。

新增项目维护文件：

```text
.codex/config.toml
Docs/UE_MCP_Setup.md
Tools/UnrealMCP/.gitignore
Tools/UnrealMCP/UPSTREAM.md
Tools/UnrealMCP/requirements.txt
Tools/UnrealMCP/requirements-lock.txt
Tools/UnrealMCP/Setup.ps1
Tools/UnrealMCP/Build.ps1
Tools/UnrealMCP/start_server.py
Tools/UnrealMCP/smoke_test.py
Tools/UnrealMCP/verify_codex.py
Tools/UnrealMCP/test_adapter.py
```

上游副本仅两处手动修改：`UnrealMCPython.uplugin` 补充 EditorScriptingUtilities 依赖；`server/src/unreal_mcp/dispatcher.py` 增加 MCP 初始化使用边界提示。C++ 实现未修改。

UE 编译/启动正常更新了项目 `Intermediate`、`Binaries`、`Saved`、DDC 等生成目录，并产生插件自身 `Binaries/Intermediate`。这些不是对业务源码的修改。未修改 Engine 源码、Source、Config、已有场景/材质/Blueprint 内容或全局 Codex 配置。没有创建临时 Actor/资产，Engine Sky Blueprint 仅加载读取，未保存。

## 12. 完整启动与最小复测

### 一次性安装/重新编译

当前机器已经完成，只有删除 `.venv`、迁移机器、更新插件或升级 UE 5.8 构建时才需要重新执行。请先正常关闭 Editor，再在 PowerShell 运行：

```powershell
& 'E:\ue_project\pratice\RenderLab\Tools\UnrealMCP\Setup.ps1' -EngineRoot 'E:\ue\UE_5.8'
& 'E:\ue_project\pratice\RenderLab\Tools\UnrealMCP\Build.ps1' -EngineRoot 'E:\ue\UE_5.8'
```

如果移动项目目录，应更新 `.codex/config.toml` 的 command/args/cwd，并重新创建 `.venv`（venv 不可直接跨路径搬运）。

### 每次使用

1. 打开 `E:\ue_project\pratice\RenderLab\RenderLab.uproject`，确认用 UE 5.8。
2. 等待 Editor 初始化完成和当前地图加载。三个必要插件已经通过 `.uproject` 启用，不需要再手工勾选；若迁移后未加载，检查 Edit > Plugins 中的 UnrealMCPython、Python Editor Script Plugin、Editor Scripting Utilities。
3. 不开启 Python Remote Execution、Remote Control 或额外网络服务。
4. 在 RenderLab 项目目录重新打开 Codex 会话。Codex 自动启动 stdio MCP Server，先启动 UE 再用 Codex 工具；不要另行常驻启动第二个 Server。
5. 查询 `util(action="get_project_info")`，确认目标正确后再检查内容。

### 可复制执行的最小只读测试

```powershell
& 'E:\ue_project\pratice\RenderLab\Tools\UnrealMCP\.venv\Scripts\python.exe' 'E:\ue_project\pratice\RenderLab\Tools\UnrealMCP\smoke_test.py'
& 'E:\ue_project\pratice\RenderLab\Tools\UnrealMCP\.venv\Scripts\python.exe' 'E:\ue_project\pratice\RenderLab\Tools\UnrealMCP\verify_codex.py'
```

第一条自动启动/关闭测试 MCP 子进程，只读 Editor，将证据写到 Saved；第二条通过本机 Codex App Server 检查工具发现，不创建新任务、不调用模型。

Codex 中的最小工具流程：

```text
util:      action=get_project_info, params={}
level:     action=get_current_level_path, params={}
level:     action=list_level_actors, params={}
actor:     action=list_actor_components, params={"actor_label":"DirectionalLight"}
blueprint: action=list_blueprint_variables, params={"asset_path":"/Engine/EngineSky/BP_Sky_Sphere"}
blueprint: action=get_blueprint_graph_info,
           params={"asset_path":"/Engine/EngineSky/BP_Sky_Sphere","graph_name":"UserConstructionScript"}
```

对于其他操作先调用该域 `action=list_actions`，不要猜参数名或把 UI 显示名称当内部 Graph 名称。当前锁定链路版本由 UPSTREAM.md 与 requirements-lock.txt 共同确定；MCP initialize 显示的 `1.9.3` 是 SDK serverInfo 版本，不是 Unreal Engine 或插件版本。
