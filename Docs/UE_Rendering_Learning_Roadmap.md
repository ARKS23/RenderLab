# RenderLab UE 5.8 项目执行路线

创建日期：2026-09-12  
目标工程：RenderLab / Unreal Engine 5.8 / Windows  
整合日期：2026-09-30。

进度更新：2026-10-08。

当前进度：Lab03A/B 已有实现与提交，用户已反馈 Lab03C/D 完成；完整实验记录待归档。当前从 [Lab04 指南](tasks/Lab04_RDG_AO_Filtering_Workbench.md#lab04a)开始最小多 Pass 实践，不重做插件接入。Lab04 指南已建立，实现尚未开始；本次未重新编译、抓帧或打包。详细依据见[当前进度](#current-progress)。

## 1. 定位与目标

### 1.1 已有基础

- 已学习 GAMES101 和 LearnOpenGL，粗略了解 Vulkan。
- 熟悉 C++，掌握 UE 基础 Gameplay，但尚未系统学习 UE 材质和渲染架构。
- 按全年总计划主攻实时渲染／图形引擎，后续发展神经渲染能力，并保留 UE 客户端方向。

不重复安排完整的图形学入门课程，也不把深入 Vulkan 作为前置条件。遇到具体算法或同步问题时，再有针对性地补充理论。

### 1.2 主线

```text
UE 材质与 Gameplay 交互
    -> 后处理与一帧分析
    -> Global Shader + SceneViewExtension + 简单 RDG Pass
    -> Lab03 抓帧、生命周期与首次打包验收
    -> 最小多 Pass -> 深度/法线与位置重建 -> AO -> 引导滤波
    -> RenderLab V1：正确性、性能、打包与展示
    -> 按全年计划开展神经渲染专题与 ClientLab
    -> Mesh 渲染架构与性能深化
```

最终目标不是收集效果，而是能够：

1. 从可见画面追踪到输入资源、Shader、渲染 Pass 和相关 C++ 调用。
2. 在项目插件中实现可配置的图形功能，明确线程边界与资源所有权。
3. 通过抓帧和性能数据解释功能是否正确、为什么快或慢。
4. 处理窗口变化、关卡切换、PIE 生命周期和打包运行等实际问题。
5. 留下别人能复现、自己能解释的工程成果。

本路线侧重渲染工程能力，不等于完整的引擎或客户端开发课程。

### 1.3 文档分工与使用顺序

| 文档 | 唯一负责的内容 | 使用方式 |
| --- | --- | --- |
| [全年学习路线](图形与游戏引擎开发_全年学习路线_2026-2027.md) | 年度目标、日历、投入预算、神经渲染与客户端安排 | 阶段取舍和时间分配以总计划为准 |
| 本文 | RenderLab 的 Lab 顺序、当前进度、前置条件和阶段验收 | 每次学习先查看[下一步](#next-step)，不另排一套年度日程 |
| [Lab03 指南](tasks/Lab03_GlobalShader_Workbench.md)、[Lab04 指南](tasks/Lab04_RDG_AO_Filtering_Workbench.md)等任务文档 | 逐步实践、代码入口和详细测试清单 | 只阅读当前任务需要的部分；旧步骤用于复盘 |
| `Docs/Experiments/` 中的记录 | 环境、实际结果、抓帧和性能证据 | 作为验收依据，不以计划或提交代替运行结果 |

日期到达不代表前置能力已验收。具体 API、资源入口和兼容性仍以本机同版本源码与实验为准。已完成的实验继续沿用；不因整合文档重命名插件、迁移资产或重复建设项目。

## 2. 项目基线与实施原则

### 2.1 当前基线

截至 2026-09-30，根据项目文件、Git 记录和此前运行反馈：

- `RenderLab.uproject` 指向 UE 5.8，已有 `RenderLab` Runtime 模块。
- 项目配置使用 DX12，并开启 Substrate、Ray Tracing 支持等设置。
- 已有 Editor-only MCP/Python 配置；接入记录见 [UE_MCP_Setup.md](E:/ue_project/pratice/RenderLab/Docs/UE_MCP_Setup.md)。
- 已有项目 Runtime 插件 `Plugins/RenderLabRendering/`，包含 Global Shader、View Extension、World Subsystem 和灰度参数控制。
- 已建立 `main` 仓库并配置 Git LFS，`origin` 为 `https://github.com/ARKS23/RenderLab.git`；2026-09-28 已提交并推送 Lab03B 检查点 `bb1fac7`。约定见 [Git_Workflow.md](E:/ue_project/pratice/RenderLab/Docs/Git_Workflow.md)。
- 本次只整合文档，未重新编译、抓帧、运行 Editor 或打包；不能据此扩大已验证范围。

配置开启某个功能，不代表当前视图一定正在使用该功能，也不代表其性能或正确性已验证。每次实验以实际视图设置、日志和抓帧为准。

### 2.2 约束

1. 以本机 UE 5.8 源码和实际运行结果为最终依据，不因旧教程能编译就认定行为正确。
2. 第一轮学习不修改 Engine 源码；新增渲染功能优先放入项目 Runtime 插件。
3. 渲染 Runtime 插件与现有 MCP Editor 插件分开，游戏运行和打包不依赖 MCP。
4. 每个实验使用独立关卡，只引入当次需要的资产和依赖。
5. 不批量切换全局渲染配置。需要切换 Substrate、RHI、Shader 格式等设置时，单独说明影响并保留基线。
6. 首次只做主视图、单视口、非 VR 的明确范围，再扩展多视图等场景。
7. 优先做无历史状态的单帧效果，正确后再加入跨帧资源、异步计算和复杂 RT 功能。
8. 不在学习阶段同时引入大型开源项目、整套美术资产和多个渲染框架。

Git 初始化、LFS 和忽略规则已于 2026-09-12 单独接入，后续已开展材质与后处理实验。每个新实验仍需补齐本次环境与比较条件；本地 Git 不替代异机备份。

<a id="current-progress"></a>
## 3. 阶段总览与当前进度

统一使用全年计划的投入预算：常规周共约 20 小时，其中主项目约 10 小时，其余用于基础、阅读、记录和缓冲。原先每阶段的周数不再作为第二套排期；具体日期见全年计划，以验收标准决定推进范围。

| 阶段 | 执行位置 | 实验成果 | 状态（2026-10-08） |
| --- | --- | --- | --- |
| 0. 准备基线 | 已建立，逐次维护 | 可恢复工程、实验约定与环境记录 | Git/LFS/远程已配置；环境记录随实验补齐 |
| Lab01. 材质与 Gameplay | 保留已有成果 | C++ 驱动的材质实验台 | 已有实验与提交；文档状态待同步，不重做 |
| Lab02. 后处理与抓帧 | 保留已有成果 | 选中描边、深度显示与单帧分析 | A-C 已验收；抓帧学习基本完成，记录待补齐 |
| Lab03. Shader 渲染插件 | 保留基线，补齐归档 | 可开关的 Global Shader 效果 | A/B 已有实现；C/D 用户反馈完成，详细记录待归档 |
| Lab04. 多 Pass、AO 与引导滤波 | 当前实践入口，通向 V1 | 多 Pass 链、输入验证、AO 与滤波对照 | 指南已建立，实现待开始，详见 [Lab04A](tasks/Lab04_RDG_AO_Filtering_Workbench.md#lab04a) |
| Lab05. Mesh 渲染架构 | 全年计划的集中源码阶段 | 最小自定义 Primitive Component | 待开始，保留编号与验收，不作为 V1 前置 |
| Lab06. 优化与交付 | 基础验证随 V1；深化随后续专题 | 压力测试、性能报告与打包程序 | 深化待开始，不推迟当前效果的测量与打包 |

Lab 编号是任务标识，不再要求严格按数字顺序完成所有专题。先完成 Lab03/04 和 V1 基础交付，再按全年计划安排神经渲染、ClientLab、Lab05/06；源码阅读仍穿插在当前实验中。

### 3.1 已有成果与证据边界

| 检查点 | 当前依据 | 尚不能据此宣称 |
| --- | --- | --- |
| Lab02 | 原路线记录 A-C 已验收；用户反馈抓帧学习基本完成 | Lab02D 的事件、资源和源码记录已经补齐 |
| Lab03A | `855e357`（2026-09-24）包含灰度渲染链；用户反馈编译、启动和效果成功 | 所有诊断模式、视图尺寸和输出路径均已验收 |
| Lab03B | `bb1fac7`（2026-09-28）包含蓝图入口、GT/RT 快照、强度传参和退出关闭；用户反馈调用成功 | 关闭时 GPU 无效果 Pass、World 隔离和完整生命周期测试均已通过 |
| Lab03C | 用户反馈完成；对话中已确认 Tonemap → GrayscaleOutput、1280×720、R10G10B10A2_UNORM、Strength=0.70、Shader 对应关系与回放替换 | 本次重新执行了 G0/G1/G2 和材质版对照，或完整记录已经归档 |
| Lab03D | 用户反馈已完成，可以据此继续 Lab04 | 本次独立复测了全部边界和 Development 包，或日志与包路径已经归档 |
| Lab04 | 已核对现有插件与本机接口，建立分阶段实践指南 | 示例已经编译运行，或 A～E 的测试已经通过 |

`Docs/Experiments/Lab03_Results.md` 尚待整理既有结果时建立。用户的完成反馈用于推进学习；书面证据归档和本次独立复测是不同事项，不要求重做已经完成的实验，也不自动勾选所有细项。

## 4. 分阶段实践

### 阶段 0：建立可复现基线

**任务**

- 建立版本管理或备份，保护现有 MCP 配置和用户资产。
- 记录引擎版本、GPU/驱动、RHI、分辨率、屏幕百分比和主要画质设置。
- 约定资产命名、实验关卡与记录位置，不一次性创建全部实验。
- 为材质与后处理实验固定相机、光照和曝光；具体设置在任务开始时检查。

**验收**

- 工程能正常打开、编译，且存在可恢复的初始状态。
- 可以说明后续性能比较使用什么环境，以及哪些参数固定不变。

### 阶段 1：C++ 驱动的材质实验台

**学习重点**

- Material、Material Instance、Dynamic Material Instance、Material Function 的职责。
- 标量、向量、纹理参数与 Static Switch 的区别；运行时参数和编译变体不能混淆。
- UV、世界/切线空间、法线、纹理颜色空间，以及简单 Substrate 材质工作流。
- UObject 引用和动态材质实例生命周期；避免每帧重建 MID。

**实践顺序**

1. 做一个简单可参数化材质，建立金属、塑料等材质实例。
2. 写一个 C++ Actor，为各展示对象创建并保存动态材质实例。
3. 通过 Gameplay 调整颜色、粗糙度和发光强度，再增加溶解效果。
4. 观察材质统计、生成的 Shader 代码及参数变化，不要求此时读完材质编译器。

**验收**

- 多个对象可共享父材质，同时保持各自的参数状态。
- 能说明一次参数更新由哪段 C++ 发起，最终影响哪个材质输入。
- 能区分材质、直接光、间接光和曝光变化，不用“看起来差不多”替代解释。
- 退出 PIE 后不会留下意外资产修改，重复运行结果一致。

**交付**：实验关卡、材质/实例、控制 Actor、参数对照截图与简短记录。  
**资源**：R01、R02、R03。

### 阶段 2：后处理与一帧分析

详细指导：[Lab02_PostProcess_Workbench.md](E:/ue_project/pratice/RenderLab/Docs/tasks/Lab02_PostProcess_Workbench.md)。按 Lab02A 灰度、Lab02B 坐标与深度、Lab02C 描边、Lab02D 抓帧逐步推进。Lab02A-C 已验收；2026-09-21 用户反馈抓帧学习已基本完成，补齐已有捕获的事件、输入输出与源码记录后归档。

**学习重点**

- 后处理输入、SceneDepth、Custom Depth/Stencil、ViewRect 与纹理尺寸的区别。
- 深度编码/线性化、屏幕空间邻域采样、色调映射和时域处理位置。
- 先使用明确支持的输入，不直接照搬旧教程中的固定 GBuffer 通道解码。
- GPU 事件、输入输出纹理，以及编辑器开销与游戏视图开销的区别。

**实践顺序**

1. 已完成灰度和深度显示，验证采样范围与输入含义。
2. 已加入 Gameplay 射线选取，仅让指定对象参与描边。
3. 已验证描边遮挡规则，并明确当前轮廓的显示范围。
4. 将已学习的抓帧过程整理成后处理事件、输入输出资源与源码入口记录；未验证项目继续标注。

**验收**

- 抓帧中可以找到效果事件、输入纹理和输出。
- 能解释为什么选择当前插入位置，以及抗锯齿/色调映射对结果的影响。
- 能区分 `stat unit` 的线程/帧概览、GPU Pass 计时和单帧资源检查的用途。

**交付**：选中描边、深度可视化、标注截图和一页帧流程说明。  
**资源**：R03、R04、R05、R06。

### 阶段 3：Global Shader 与项目渲染插件

详细指导：[Lab03_GlobalShader_Workbench.md](tasks/Lab03_GlobalShader_Workbench.md)。A/B 的可调灰度核心功能已跑通，用户已反馈 C/D 完成；既有抓帧、边界和打包结果补齐归档后作为回归基线。保留下列全阶段目标供复盘，不重新创建插件；当前实践转入 Lab04。

**学习重点**

- Runtime 模块、Shader 路径映射、加载时机、`FGlobalShader` 和参数结构。
- SceneViewExtension 的回调位置、激活条件和生命周期。
- Game Thread 到 Render Thread 的数据传递：传递有明确所有权的数据，不在渲染回调中任意访问 Actor/UObject 状态。
- 使用简单 RDG Pass 启动 Shader，不先自行管理整套低层 RHI 资源。

**实践顺序**

1. 新建项目级运行时渲染插件，首个效果只交换颜色通道或转换灰度。
2. 增加开关和强度参数，由 Gameplay 通过明确接口更新。
3. 与阶段 2 的材质实现对比输入、执行位置、代码组织和开销。
4. 首次 Development 打包，检查 Shader 编译与插件加载。

**验收**

- 可以追踪 C++ 参数、Shader 参数结构、HLSL 入口与输出纹理。
- 效果关闭时不执行无意义的效果 Pass。
- 窗口缩放、反复 PIE、关卡切换无明显资源或生命周期错误。
- 编辑器工具未启动时，打包程序仍能独立运行效果。

**交付**：最小渲染插件、控制接口、调用链说明和首次打包记录。  
**资源**：R05、R07、R08、R09、R10。

<a id="lab04"></a>
### 阶段 4：多 Pass RDG、AO 与引导滤波

详细指导：[Lab04_RDG_AO_Filtering_Workbench.md](tasks/Lab04_RDG_AO_Filtering_Workbench.md)。当前先完成 A4 横向模糊检查点，再增加纵向 Pass 与合成；指南含文件清单、参考代码、输入验证、AO／滤波实验和 V1 验收。

**进入条件**：Lab03 参数链可解释，三状态 GPU 行为、基本生命周期与首次打包得到验证。若打包存在外部阻塞，先记录失败阶段和影响，再明确可继续的实验范围；不能把 Lab03 标为已验收。

本阶段把原路线的模糊练习与全年计划的 AO 作品合并，不并行开发两套框架。复用 `RenderLabRendering` 的模块和参数传递经验，保留灰度作为可回归的基线；AO 的资源入口与执行位置需重新核对。

**学习重点**

- RDG Texture/Buffer、SRV/UAV、参数声明和读写依赖。
- 图内临时资源与外部资源的区别；不把临时 RDG 引用跨帧保存。
- Pass 裁剪、资源生命周期、提取/导入，以及如何在需要时进行异步 Readback。
- 深度表示、法线空间、位置重建、采样半径与屏幕空间算法的限制。
- 视图范围、输入输出格式和分辨率转换；引入 Compute Shader 时再验证 SRV/UAV、线程组与越界保护。

**实践顺序**

| 子任务 | 最小实践 | 进入下一步前的证据 |
| --- | --- | --- |
| Lab04A：最小多 Pass | 全分辨率横向模糊、纵向模糊与合成；先沿用熟悉的 Raster 路径 | 各 Pass 输入输出、中间纹理和依赖图；有效区域与边缘采样正确 |
| Lab04B：输入与重建 | 在插件中分别显示深度、法线和重建位置，用简单平面与几何验证 | 写清深度表示、法线空间、背景处理、ViewRect 和输入分辨率；改变相机后仍可解释 |
| Lab04C：最小 AO | 独立输出 AO；控制半径和采样数，暂不接入完整光照 | 简单几何的遮蔽趋势合理；说明边缘、遮挡缺失等失败情况 |
| Lab04D：引导滤波 | 从深度引导开始，按需加入法线；比较原始、滤波、高采样参考 | 相同输入条件下的画质对照，解释边缘串色与细节损失；不把高采样参考称为真实 Ground Truth |
| Lab04E：V1 验证 | 至少三个简单测试场景，重复测量、边界回归、打包及展示 | 抓帧说明、性能数据、排错案例、复现步骤、运行结果与已知限制 |

Lab04 逐文件实践指南已于 2026-10-08 建立，尚未创建 Lab04 实现；每次只推进一个明确实验，示例需经过本机编译与运行验证。AO 与滤波原理沿用全年计划的 R19/R20，本文资源编号不与全年文档混用。

**范围控制**

- Lab04A 是短小的 RDG 练习，不先加入完整模糊工具、半分辨率和时域系统。
- Lab02 的材质深度显示不等于插件已取得并验证对应资源。Lab04B 开始前核对本机 UE5.8 的资源接口、Substrate 格式和处理时机，不直接照搬旧 GBuffer 通道或灰度的接入位置。
- Compute Shader 作为受控对照练习，先验证一个 Pass 的等价输出；不把转换全部 Pass 作为 AO 起步前置。
- 半分辨率／上采样与基础时域处理属于可选加深，第一轮至多选一项。Async Compute、跨帧资源和 Readback 按具体问题展开，不作为 V1 必做项；Readback 不放入每帧阻塞路径。
- 按全年缩减规则，AO 不可靠时可先交付已验证的引导滤波版本，但明确其范围，不声称 AO 已完成。

**验收**

- 每个 Pass 的输入、输出和资源用途明确，没有靠猜测生命周期维持运行。
- 深度、法线和位置重建先独立验证；AO 展示与正式参与光照的范围明确区分。
- 非整齐尺寸、不同屏幕百分比下没有越界、黑边或错误区域。
- 在一致环境下对比画质、GPU 时间和资源占用，说明优化的代价。
- 不预设 Async Compute 一定更快；先保证依赖正确，再测量是否有重叠收益。
- 保留 Lab03 的关闭路径、生命周期和打包回归；发现阻塞时记录为未通过，不以演示视频代替验收。

**交付**：RenderLab V1，包括多 Pass 效果、输入验证、AO／滤波对照、依赖图、性能报告和可复现工程；算法范围按实际结果标注。

**资源**：R09、R11、R16；算法资料见[全年路线的资源索引](图形与游戏引擎开发_全年学习路线_2026-2027.md#resources)。

<a id="lab05"></a>
### 阶段 5：从 Component 到 Mesh 绘制

按全年计划放入集中源码阶段，保留 Lab05 编号，不用 AO 占用此编号。继续保留“最小自定义 Primitive Component”的动手验收，不以阅读调用链代替。当前实验涉及的引擎代码仍随时按需阅读。

**学习重点**

以传统、非 Nanite 的 Mesh 路径为范围，追踪：

```text
UPrimitiveComponent
    -> FPrimitiveSceneProxy
    -> FMeshBatch
    -> FMeshPassProcessor
    -> FMeshDrawCommand
    -> RHI 命令与 GPU 绘制
```

- 区分静态缓存与动态提交、CPU 数据更新与 GPU Buffer 更新。
- 理解 Bounds、可见性、材质绑定、渲染资源创建与释放。
- 先复用合适的现有 Vertex Factory；自定义 Vertex Factory 作为后续扩展。

**实践顺序**

1. 阅读一种简单现有组件及其 Scene Proxy，再追踪相关 Mesh 提交流程。
2. 创建最小自定义 Primitive Component，绘制规则网格或带状面。
3. 增加材质、变换和动态顶点更新，验证 Bounds 与销毁行为。
4. 对比静态数据与动态数据的提交成本。

**验收**

- 能解释 Game Thread 对象与 Render Thread 代理的分工。
- 对象移出视锥、切换关卡、销毁时行为正确。
- 能在抓帧中找到绘制、顶点/索引资源和绑定材质。
- 明确第一版仅验证主视图光栅路径，不声称自动支持 Nanite、RT 或全部阴影路径。

**交付**：最小自定义组件、线程与资源生命周期图、Mesh 调用链记录。  
**资源**：R05、R08、R12。

本阶段不要求修改 Renderer 新增 Mesh Pass。若后续确实需要引擎改动，应单独立项、备份，并说明为什么现有扩展点不足。

<a id="lab06"></a>
### 阶段 6：性能优化与可交付验证

Lab06 保留为后续性能深化任务。首次打包在 Lab03D，AO／滤波的基础性能与交付在 Lab04E，均不等待 Lab05/06；本阶段再选择一个压力测试或优化问题深入，不重复计算已经完成的成果。

**学习重点**

- CPU 提交瓶颈与 GPU 瓶颈、实例化与剔除、材质复杂度和透明过绘制。
- Shader 变体、首次资源加载、PSO 首次使用卡顿与稳态帧时间的区别。
- 画质档位、效果降级、运行时资源管理及打包行为。

**实践顺序**

1. 创建可调规模的压力测试关卡，分别改变对象数、材质数和效果成本。
2. 比较普通 Mesh Actor 与实例化方案；同时观察剔除、更新和 Gameplay 需求，不仅看 Draw Call 数。
3. 给已有渲染效果增加明确的画质档位和关闭路径。
4. 完成 Development 打包，验证冷启动、预热后运行和资源加载场景。

**验收**

- 固定相机、分辨率、画质、测试区间和预热条件，至少重复测量三次。
- 记录帧时间与关键 Pass 毫秒数，不只记录 FPS。
- 优化前后既有数字，也有等价画质或画质取舍的说明。
- 原始数据可追溯；未测量的平台和功能标为未验证。

**交付**：可运行程序、复现步骤、测试数据、性能报告和已知限制。  
**资源**：R06、R11、R13。

## 5. 职业方向分支

全年总计划已选择“渲染／引擎为主，保留客户端”。下表用于后续取舍，不是新增并行任务；神经渲染二选一和 ClientLab 的日历、范围由[全年路线](图形与游戏引擎开发_全年学习路线_2026-2027.md)维护。

| 方向 | 主线之后优先加深 | 建议专题 | 应体现的能力 |
| --- | --- | --- | --- |
| 渲染引擎 | Shader/RDG、Mesh Pipeline、线程与资源、RHI | SSAO 与引导滤波已纳入 Lab04；之后按全年计划选择神经专题或后续加深 | 算法实现、正确性边界、管线接入、GPU 调试与性能分析 |
| 客户端 | Gameplay 与渲染接口、资源生命周期、质量配置、打包与加载 | 可复用选中/扫描效果组件、材质状态系统、带预算的效果管理 | 需求拆解、组件接口、稳定性、资源管理与实际交付 |
| 暂未确定 | 完成阶段 1～4，再做阶段 6 的基础版本 | 同一效果的材质实现与插件实现对比 | 能解释方案取舍，再决定下一步 |

客户端方向按 ClientLab 阶段补充网络同步、UI、异步加载与 Gameplay 架构；动画等按实际需求扩展。不在当前 RenderLab 阶段额外叠加一套完整客户端课程。

专题拓展建议：

- **SSAO 加深**：在 Lab04 的已验证基线上研究采样或分辨率改进，不重复建立基础 AO 项目。
- **时域累积**：历史纹理、运动矢量、重投影、拒绝历史、相机切换与资源重建。
- **GPU 剔除**：包围体测试、可见索引输出、间接绘制参数与同步。
- **Ray Tracing**：在主线跑通后学习加速结构、可见性与材质路径，先做小范围实验，再看 Lumen/RT 集成。
- **RHI**：沿自己写出的 Pass 追踪资源创建、命令记录和同步，不从整个 D3D12RHI/VulkanRHI 模块第一行读起。

除已纳入 Lab04 的基础 AO／滤波外，以上均为后续备选，不要求全部完成，也不在本次文档整合时实施。

## 6. 工程组织与实验记录

### 6.1 现有目录与后续约定

保留已有 `Content/Levels/`、游戏源码和插件目录。下列新的实验资产子目录仅作后续建议，不要求迁移已有地图；原示例中的 `RenderExperiments` 统一对应现有 `RenderLabRendering`。

```text
Content/RenderLab/
  Shared/                         少量确实共享的资产
  Lab01_Materials/                 阶段 1 的关卡与材质
  Lab02_PostProcess/               阶段 2 的关卡与后处理资产
  Lab03_ShaderPass/                阶段 3 的测试内容
  Lab04_RDG/                      阶段 4 的模糊、AO 与滤波测试内容
  Lab05_MeshPipeline/             阶段 5 的测试内容
  Lab06_Performance/              阶段 6 的压力场景
Source/RenderLab/                  Gameplay Actor、Component 和控制入口
Plugins/RenderLabRendering/        已有 Runtime 渲染插件，继续复用
  Source/
  Shaders/
Docs/tasks/                       分任务学习与操作指导
Docs/Experiments/                 分阶段实验记录
Saved/RenderLabCaptures/           本机抓帧和临时性能数据
```

资产可使用 `L_`、`M_`、`MI_`、`MF_`、`BP_` 等一致前缀。不为每个小效果创建一个插件，也不为了单次实验提前抽象出通用渲染框架。

### 6.2 每个实验的记录模板

```markdown
# LabXX 实验名称

## 目标与范围
希望回答什么问题；本次不支持什么。

## 环境与复现
UE/GPU/驱动、关卡、入口、分辨率、屏幕百分比、画质与操作步骤。

## 实现
资产与代码入口、参数路径、线程边界、Pass 与资源关系。

## 验证
预期与实际结果、边界测试、抓帧中的关键事件。

## 性能
固定条件、测量方法、原始结果、前后对比与画质取舍。

## 结论
已理解的机制、失败尝试、已知限制和下一步。
```

抓帧文件可能很大，应按需保留；报告中保留关键截图与可复现步骤，不把全部临时输出直接纳入版本控制。

### 6.3 学习协作方式

- 每轮只推进一个明确实验，先说明预测结果，再编写最小实现。
- 代码生成后必须读懂参数、线程和资源路径，不以“能跑”为唯一标准。
- MCP 用于已支持的场景/资产查询和编辑辅助；新能力先验证，不能假设全部材质图操作都可用。
- MCP 调用成功不等于 GPU Pass 正确，不能替代编译日志、RDG 验证、抓帧和性能分析。
- 不自动批量保存所有资产，不覆盖用户实验；配置变更与关卡编辑分别记录。

## 7. UE 5.8 兼容性检查

参考资料沿用前期讨论整理的官方文档和作者仓库。本次主要整理路线，未重新下载、编译或验证第三方示例；链接可访问和 README 版本声明都不等于本机兼容性验证。

| 风险 | 实施时的处理方式 |
| --- | --- |
| Substrate 与旧材质教程不同 | 从简单材质开始；读取 GBuffer 前确认当前格式与访问接口，不复制旧通道布局假设。 |
| SceneViewExtension 回调变化 | 对照本机头文件；`SubscribeToPostProcessingPass` 的当前签名包含 `FSceneView`，旧重载已弃用。 |
| ScreenPass 输入与视图范围不同 | 核对纹理尺寸、ViewRect、输入切片和输出约定；测试窗口与屏幕百分比变化。 |
| 资源跨帧或跨线程失效 | 明确快照、所有权、导入/提取与销毁时机；不保存图内临时引用供下一帧使用。 |
| 示例依赖 Renderer 私有头文件 | 记录依赖及版本风险，能用公开接口时优先使用；升级后重新编译与验证。 |
| Shader 模块与加载时机 | 对照 5.8 示例检查路径映射、加载阶段和打包编译，首次模块接入不依赖 Live Coding。 |
| 光栅功能不自动覆盖所有渲染路径 | 单独标注主视图、阴影、Scene Capture、Nanite、RT、多视图的支持状态。 |
| 抓帧工具与复杂 RT 配置 | 先验证工具/驱动/功能组合；确需改设置时记录并保留原基线。 |

本机源码起点：

- [SceneViewExtension.h](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Public/SceneViewExtension.h)：扩展点和回调签名。
- [PostProcessBloomSetup.cpp](E:/ue/UE_5.8/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessBloomSetup.cpp)：从小功能学习 Shader、参数与 Pass 组织。
- [DeferredShadingRenderer.cpp](E:/ue/UE_5.8/Engine/Source/Runtime/Renderer/Private/DeferredShadingRenderer.cpp)：在已有局部理解后，查看 `FDeferredShadingSceneRenderer::Render` 的总体调度。
- [LensDistortionRendering.cpp](E:/ue/UE_5.8/Engine/Plugins/Compositing/LensDistortion/Source/LensDistortion/Private/LensDistortionRendering.cpp)：补充观察 Global Shader 与低层 RHI 的关系；其直接 RHI/旧式参数写法不是新功能的首选 RDG 模板。

源码阅读顺序：**实验问题 -> GPU 事件 -> C++ Pass 构建 -> Shader 参数和 HLSL -> 必要的下层接口**，不要求整目录通读。

## 8. 学习资源索引

官方文档中也可能保留旧示例。概念按需阅读，接口以本机 UE 5.8 为准；优先定位一段与实验相关的内容，不要求先读完全部文档。

| 编号 | 资源 | 使用方式与边界 |
| --- | --- | --- |
| R01 | [Material Instances](https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-materials-in-unreal-engine) | 材质实例、动态参数与 Static Parameter，是阶段 1 的主要入口。 |
| R02 | [Substrate Materials Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-substrate-materials-in-unreal-engine) | 先掌握简单材质表达与现有工作流，不从复杂分层开始。 |
| R03 | [Content Examples](https://dev.epicgames.com/documentation/en-us/unreal-engine/content-examples-sample-project-for-unreal-engine) | 独立参考工程，优先看材质、后处理、Blueprint Render To Target；下载时确认版本。 |
| R04 | [Post Process Materials](https://dev.epicgames.com/documentation/en-us/unreal-engine/post-process-materials-in-unreal-engine) | 输入、插入位置、深度与屏幕空间处理。 |
| R05 | [Graphics Programming Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/graphics-programming-overview-for-unreal-engine) | 建立场景表示与帧结构的概念，注意其中可能保留旧平台描述。 |
| R06 | [Using RenderDoc](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-renderdoc-with-unreal-engine) | 单帧事件和资源检查；工具兼容性与实时性能测量需分别验证。 |
| R07 | [Shader Development](https://dev.epicgames.com/documentation/en-us/unreal-engine/shader-development-in-unreal-engine) | Global/Material Shader、参数、编译与调试。 |
| R08 | [Threaded Rendering](https://dev.epicgames.com/documentation/en-us/unreal-engine/threaded-rendering-in-unreal-engine) | 渲染线程所有权、同步与生命周期；具体接口核对 5.8。 |
| R09 | [Render Dependency Graph](https://dev.epicgames.com/documentation/en-us/unreal-engine/render-dependency-graph-in-unreal-engine) | Pass、资源依赖、生命周期、验证和 RDG Insights 的主教材。 |
| R10 | [A57R4L/SceneViewExtensionTemplate](https://github.com/A57R4L/SceneViewExtensionTemplate) | 小型插件参考；前期检查 README 明确更新到 UE 5.6，未在本机验证 5.8，不能直接认定可用。 |
| R11 | [Unreal Insights](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-insights-in-unreal-engine) | 时间线、线程和性能分析，配合 RDG 与 GPU 数据使用。 |
| R12 | [Mesh Drawing Pipeline](https://dev.epicgames.com/documentation/en-us/unreal-engine/mesh-drawing-pipeline-in-unreal-engine) | 传统 Mesh 提交、缓存与绘制命令的结构。 |
| R13 | [PSO Precaching](https://dev.epicgames.com/documentation/en-us/unreal-engine/pso-precaching-for-unreal-engine) | 阶段 6 学习首次使用卡顿和验证方法，不在入门阶段盲目调参数。 |
| R14 | [GAMES202](https://sites.cs.ucsb.edu/~lingqi/teaching/games202.html) | 主线之后按阴影、GI、BRDF、时域和降噪专题补充，不是 UE 操作教程。 |
| R15 | [Temaran/UnrealEngineShaderPluginDemo](https://github.com/Temaran/UnrealEngineShaderPluginDemo) | 可选历史参考；前期检查默认分支为 5.0，不作为 5.8 开箱即用模板。 |
| R16 | [Programming with Substrate GBuffer Formats](https://dev.epicgames.com/documentation/unreal-engine/programming-with-substrate-gbuffer-formats) | 进入场景法线/GBuffer 读取前核对格式、访问方式和限制。 |

开始阶段 1 时只需 R01～R03；进入下一阶段后再扩展资源范围。已学过的 GAMES101、LearnOpenGL 按需查漏，不重新完整学习。

## 9. 第一个学习任务（历史起点）

**任务名：Lab01A - 最小材质实验台。**

详细指导：[Lab01A_Material_Workbench.md](E:/ue_project/pratice/RenderLab/Docs/tasks/Lab01A_Material_Workbench.md)，包含学习资料、分步实验、源码阅读和验收清单。下文保留最初任务设计；当前已有材质和后处理实验，旧清单尚未逐项同步，不代表需要重新开始。

目标：在不写渲染插件、不修改 Engine 的前提下，理解 UE 材质实例和 C++ 参数更新。

待执行清单：

- [ ] 完成阶段 0 的备份/版本管理与环境记录。
- [ ] 新建独立材质实验关卡，不替换当前默认地图。
- [ ] 放置基础几何体、固定相机和简单光照，建立可重复比较的曝光条件。
- [ ] 创建一个简单 Substrate 父材质，暴露颜色、粗糙度、金属度与发光参数。
- [ ] 创建若干材质实例，观察共享父材质与不同参数组合。
- [ ] 创建 C++ 实验 Actor，管理动态材质实例，并通过明确入口修改参数。
- [ ] 验证多个对象参数独立、重复 PIE 正常，不用 Tick 每帧重建 MID。
- [ ] 记录参数路径、截图和至少一个解释清楚的现象。

本任务不包含溶解、复杂 UI、描边、Compute Shader 或性能压力测试。Lab01A 验收后，再安排 Lab01B 溶解实验，随后进入阶段 2。

## 10. 进度维护

- 阶段状态使用：待开始、进行中、已验收；只有满足验收标准后才标记已验收。
- 具体能力使用：已验证、部分验证、未验证，并记录验证环境。
- 实现进度与验收分开：可以记录“核心功能已跑通”，但提交、编译通过或单次成功不等于完整验收。
- 每完成一个实验，在对应记录中写出结论，再更新本文的阶段状态和任务清单；全年文档只维护阶段目标与排期，不复制逐项进度。
- 因接口、时间或职业目标改变路线时，记录调整原因，不把尚未实施的设计描述成已完成能力。

<a id="next-step"></a>
### 10.1 当前下一步

1. 按 [Lab04 指南的准备与文件清单](tasks/Lab04_RDG_AO_Filtering_Workbench.md#prepare)建立独立实验条件，从 [Lab04A](tasks/Lab04_RDG_AO_Filtering_Workbench.md#lab04a)的横向模糊开始，再接纵向与合成。
2. A 验证通过后，按 B → C → D → E 推进输入验证、AO、引导滤波和 V1；不把 Compute、时域或半分辨率设为起步前置。
3. 同步将已完成的 Lab03C/D 结果归档到 `Docs/Experiments/Lab03_Results.md`，记录抓帧、打包路径与未验证范围；Lab04 的新结果逐步记录到 `Docs/Experiments/Lab04_Results.md`。不重做已有实验。

### 10.2 路线调整记录

2026-09-30：与全年计划整合，以作品 V1 为近期目标，将原本可选的基础 AO 与引导滤波纳入 Lab04；Mesh 动手任务保留并后移，基础性能和打包提前随实验验收。统一投入预算、插件命名与文档职责。本次只调整文档，没有创建 Lab04 代码、修改资产、执行 UE 编译或运行验收，也未提交推送。

2026-10-08：根据用户反馈将 Lab03C/D 标为已完成反馈、记录待归档；新增 Lab04 A～E 实践指南并将当前入口移到横向模糊。核对本机 UE5.8 接口，但没有修改渲染实现、资产或配置，也未编译、抓帧、打包、提交或推送。
