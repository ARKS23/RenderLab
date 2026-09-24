# Lab03：Global Shader、Scene View Extension 与最小 RDG Pass

日期：2026-09-21

项目：RenderLab / Unreal Engine **5.8.0 CL 55116800** / Windows / DX12。

状态（2026-09-23）：**Lab03A 进行中。** 插件加载与 Shader 路径映射、Global Shader 编译已验证；独立 Pass 函数已通过 C++ 编译。View Extension 接入与画面验证待完成，Lab03B-D 尚未实施。下文是完整任务设计，其中尚未实施的片段不视为已编译验证。

- 上级路线：[UE_Rendering_Learning_Roadmap.md](../UE_Rendering_Learning_Roadmap.md)。
- 前置任务：[Lab02_PostProcess_Workbench.md](Lab02_PostProcess_Workbench.md)。
- 前置记录：[Lab02_Results.md](../Experiments/Lab02_Results.md)。
- Shader 思维参考：[ShaderCodeGuide.md](../ShaderCodeGuide.md)。
- 当前实操步骤：[Lab03A_ViewExtension_Integration.md](Lab03A_ViewExtension_Integration.md)，把已有 Shader/Pass 接入 PIE，包含逐文件代码与验证顺序。

你已反馈抓帧学习基本完成，可以进入本阶段。Lab02 文档仍保留“GPU 抓帧待开始”，开始 Lab03 时补上已有捕获的事件、输入输出和源码入口即可，不需要从头重做 Lab02。不要仅因存在 `.rdc` 文件就把全部分析条目标为已验收。

## 1. 本阶段要解决的问题

核心问题：

> 不使用后处理材质资产时，怎样通过项目插件，把自己编写的 HLSL 接入 UE 的一帧，并从 Gameplay 控制它？

最终做出一个可开关、可调强度的灰度后处理，沿用 Lab02 的灰度算法。熟悉的算法有利于把注意力放在输入资源、Shader 参数、执行位置、线程和生命周期上。

完成后应能解释：

1. 插件加载、Shader 类型注册、Shader 编译和逐帧执行分别发生在什么时候。
2. 一个 C++ Shader 类怎样对应到 `.usf` 文件、入口函数和 Shader 阶段。
3. Scene View Extension 怎样把回调接到当前视图的后处理流程。
4. RDG Pass 声明了哪些资源依赖，输出怎样被后续流程使用。
5. Gameplay 的 `Strength` 怎样经过线程边界进入 Shader 参数。
6. 效果关闭、窗口变化、PIE 结束和关卡切换时，资源与对象怎样保持有效。
7. 编辑器内成功的功能怎样进入 Cook 和打包程序。

建议用 5～7 次学习完成，每次约 1～2 小时；以阶段验收决定进度。第一次只完成 Lab03A 的插件、编译和输入直通，不要求同时完成线程控制与打包。

## 2. 实验规格与范围

### 2.1 交付物

| 交付物 | 具体要求 |
| --- | --- |
| 项目级 Runtime 插件 | `Plugins/RenderLabRendering/`，包含 C++ 和 `Shaders/`，不依赖 MCP Editor 插件 |
| 一个 Global Pixel Shader | 读取输入颜色，按强度混合灰度，保留输入 Alpha |
| 一个 View Extension | 接入主视图的后处理流程，明确所属 World 和执行位置 |
| Gameplay 控制入口 | `SetGrayscaleSettings(bool Enabled, float Strength)`，内部将强度限制到 `0～1` |
| 一个最小 RDG Raster Pass | 有可识别的 GPU 事件名，明确输入与输出 |
| 独立实验关卡 | 固定相机、曝光与测试物体，保留 Lab02 原资产 |
| 实验记录 | 调用链、抓帧资源表、边界测试和 Development 打包结果 |

最终默认值约定：`Enabled = false`、`Strength = 0`。测试关卡通过 BeginPlay 显式启用，避免插件自动改变所有游戏地图。

为便于理解控制语义，本轮规定：

- `Enabled = false`：不订阅本效果的后处理回调，不添加灰度 Pass。
- `Enabled = true, Strength = 0`：仍执行灰度 Pass，但输出原图。
- `Enabled = true, Strength = 1`：输出完全灰度。

这有意保留“视觉上无变化”和“没有效果 Pass”的区别。后续可以优化零强度路径，但要单独更新实验规格。

### 2.2 第一版支持范围

先验证 Windows / DX12、桌面主视图、单玩家、单视口、普通 Lit 视图、PIE 与独立游戏。效果在所属游戏 World 中生效；第一版不提供普通 Editor 视口预览。

本轮采用全屏 **Pixel Shader + RDG Raster Pass**。它与 Lab02 的全屏处理相近，便于理解 Shader 绑定和渲染目标。Compute Shader 的 UAV、线程组与越界处理留到后续练习。

暂不扩展多 Pass 模糊、跨帧历史、Async Compute、GPU Readback、GBuffer 解码、Nanite/Lumen 内部修改、VR、移动端、多玩家分屏和 Scene Capture。保持现有全局渲染配置；场景含有 Lumen/Nanite 内容不等于本轮已验证这些模块的所有路径。

## 3. 先建立整条链路的认识

```text
模块启动
    ├─ 注册 Global Shader 类型
    └─ 注册 Shader 虚拟路径映射
               ↓
引擎按目标平台编译 / 加载 Shader

游戏 World 初始化
    └─ WorldSubsystem 创建并持有该 World 的 View Extension

Gameplay 设置 Enabled / Strength                 Game Thread
    └─ 校验参数，复制纯数据快照
          └─ 排入 Render Command
                └─ 更新渲染侧状态                Render Thread

当前视图进入后处理
    └─ View Extension 订阅 Tonemap 后的回调
          ├─ 取得 SceneColor 输入及 ViewRect
          ├─ 确定输出 Render Target
          ├─ 填写 Shader 参数
          └─ 向当前 FRDGBuilder 添加 Raster Pass
                ↓ RDG 执行与 RHI 命令提交
          Global Pixel Shader                    GPU
                ↓
          输出颜色进入后续后处理链
```

这张图表达职责和数据方向，不表示各线程逐项同步等待。调用 setter 不意味着 GPU 当场完成更新；添加 RDG Pass 也不意味着 GPU 已执行。

| 概念 | 在本实验中的职责 | 需要澄清的边界 |
| --- | --- | --- |
| Runtime 模块 | 提供运行时 C++ 类型，接入启动过程 | `Runtime` 不代表可以在任意加载阶段才注册 Shader |
| Global Shader | 独立于具体材质资产注册的一类 Shader | “Global”不表示每帧自动执行，也不表示只能有一个实例或一个变体 |
| View Extension | 提供当前视图的渲染扩展回调 | 回调发生不代表已经添加 GPU 工作 |
| `FRDGBuilder` | 组织资源与 Pass 依赖，管理图内执行 | 在回调中使用传入的图，不自行执行引擎正在构建的图 |
| `FScreenPassTexture` | 一张纹理及其有效视图矩形 | 纹理总尺寸不一定等于有效区域 |
| RHI | 更低层的资源与命令抽象 | 本轮通过 RDG/ScreenPass 使用它，无需直接编写 D3D12 调用 |

## 4. 开始前的准备

### 4.1 保留项目当前状态

先运行：

```powershell
git status --short --branch
git log -3 --oneline
```

2026-09-21 编写本文时，工作区已有 `Config/DefaultEngine.ini` 修改，以及新增的法线实验材质和 Lab02 External Actor 资产。这些属于已有工作，不能为开始 Lab03 而覆盖、回退或清理。

当前配置含 RenderDoc 集成、Shader 调试输出及编译进程数量调整，其中注释提到启动内存不足后的恢复设置。第一次编译保持这些值；需要额外 Shader 诊断时，再按一个明确问题逐项启用并记录。不要把所有 Shader 的调试符号和输出一次性全打开。

已有提交是可恢复基线。需要建立实验分支时，先保存与梳理现有工作，再按 [Git_Workflow.md](../Git_Workflow.md) 操作；本指南没有创建分支、提交或新插件。

### 4.2 记录实验条件

在未来的 `Docs/Experiments/Lab03_Results.md` 中记录：

- UE 完整版本、当前 Git 提交、GPU/驱动、RHI 和 Shader 平台。
- 地图、相机、分辨率、屏幕百分比、抗锯齿方式。
- SDR/HDR、曝光、Local Exposure，以及参与比较的后处理效果。
- 编辑器视口、PIE 新窗口、独立运行或打包程序中的哪一种模式。

建议独立新建简单关卡，包含几种明显颜色和细边缘。若复制 Lab02 地图，先识别并只在副本中关闭已有的灰度、描边和随机材质变化，防止两套效果叠加或画面持续变化干扰比较。不要改写 Lab02 原地图。

### 4.3 建议目录与职责

以下目录尚未创建：

```text
Plugins/RenderLabRendering/
  RenderLabRendering.uplugin
  Source/RenderLabRendering/
    RenderLabRendering.Build.cs
    Public/
      RenderLabGrayscaleSubsystem.h
    Private/
      RenderLabRenderingModule.cpp
      RenderLabGrayscaleSubsystem.cpp
      RenderLabGrayscaleViewExtension.h
      RenderLabGrayscaleViewExtension.cpp
      RenderLabGrayscaleShader.h
      RenderLabGrayscaleShader.cpp
  Shaders/Private/
    RenderLabGrayscale.usf

Content/RenderLab/Lab03_ShaderPass/
  Maps/L_Lab03_Grayscale.umap

Docs/Experiments/
  Lab03_Results.md
  Images/Lab03/

Saved/RenderLabCaptures/Lab03/             本机捕获，不提交完整大文件
```

职责保持简单：

- Module 负责 Shader 路径映射；不在早期启动函数中访问当前关卡或创建游戏对象。
- `URenderLabGrayscaleSubsystem` 负责 World 生命周期与 Gameplay 参数入口。
- `FRenderLabGrayscaleViewExtension` 负责视图筛选、回调订阅与效果调用。
- Shader 文件与 Pass 构建代码负责 GPU 参数和绘制；不依赖 Actor。
- 薄 Level Blueprint 调用 World Subsystem 进行实验，不承担 Shader 逻辑。

## 5. 任务拆分

| 子任务 | 要完成什么 | 主要知识 | 完成条件 |
| --- | --- | --- | --- |
| Lab03A | 自定义 Shader 接入画面 | 模块、Shader、View Extension、单 Pass RDG | 原图直通、诊断颜色、固定灰度均符合预期 |
| Lab03B | 参数控制与关闭路径 | GT/RT 数据快照、所有权、World 范围 | 参数可控，关闭时没有灰度 Pass，不影响其他 World |
| Lab03C | 抓帧与 Lab02 对照 | 输入输出资源、插入位置、颜色与坐标 | 能在捕获中解释整个效果链路 |
| Lab03D | 生命周期与首次打包 | 初始化/退出、Cook、运行时依赖 | 重复 PIE、切换地图、缩放窗口和 Development 包通过 |

每次先写预测，再动手，然后用日志、画面或抓帧检查。不要用“编译通过”替代输入输出和生命周期验收。

## 6. Lab03A：从插件到第一个画面

### A1. 创建 Runtime 模块

阅读本机 OpenColorIO 的 `.uplugin` 和 `OpenColorIOModule.cpp`，只观察模块类型、加载阶段及路径映射，不复制整个色彩管理系统。

插件描述符需要包含的关键模块片段：

```json
{
  "Modules": [
    {
      "Name": "RenderLabRendering",
      "Type": "Runtime",
      "LoadingPhase": "PostConfigInit"
    }
  ]
}
```

这只是描述符片段，不是完整插件文件。`PostConfigInit` 用于让 Shader 类型和路径在引擎初始化 Shader 系统所需的时机可用；不要照搬普通 Gameplay 模块的 `Default` 然后依赖偶然的加载顺序。

规划模块依赖：

| 模块 | 使用原因 | 建议位置 |
| --- | --- | --- |
| `Core`、`CoreUObject`、`Engine` | 基础类型、反射、WorldSubsystem、View Extension | 公共头文件实际使用时放 Public |
| `Projects` | `IPluginManager` 查询插件目录 | Private |
| `RenderCore` | Global Shader、Shader 参数、RDG、渲染命令 | Private |
| `RHI` | 采样器、渲染状态与资源类型 | Private |
| `Renderer` | `ScreenPass.h`、后处理输入等 Renderer 公共接口 | Private |

先保持渲染类型在 Private 目录，公共接口只暴露 Gameplay 需要的参数方法。不要给 `Source/RenderLab/RenderLab.Build.cs` 一次性添加所有渲染模块。

如果后续由游戏 C++ 直接调用该 Subsystem，再为游戏模块添加 `RenderLabRendering` 依赖。仅从 Blueprint 调用已加载插件暴露的函数，不要求给游戏模块增加相同的 C++ 依赖。

`Renderer` 的公共头文件不等于 `Renderer/Private`。第一版不额外添加 Engine 私有目录，也不为了获取私有 `FViewInfo` 而强制转换 `FSceneView`。

### A2. 配置 Shader 虚拟路径

在模块 `StartupModule()` 中：

1. 用 `IPluginManager::Get().FindPlugin(TEXT("RenderLabRendering"))` 找到自身插件。
2. 校验插件指针，取实际 `Shaders` 目录。
3. 调用 `AddShaderSourceDirectoryMapping` 注册：

```text
/Plugin/RenderLabRendering
    -> <实际工程路径>/Plugins/RenderLabRendering/Shaders

/Plugin/RenderLabRendering/Private/RenderLabGrayscale.usf
    -> Shaders/Private/RenderLabGrayscale.usf
```

虚拟路径用于 Shader 编译器，不能用硬编码的 `E:\...` 路径替代。升级或移动项目后，物理目录通过插件位置重新求得。

不要在关闭自己的插件时调用 `ResetAllShaderSourceDirectoryMappings()`；它的范围是全局映射，会波及其他模块。第一版采用正常启动/退出生命周期，不要求支持 Shader 模块的动态卸载和热重载。

首次接入新模块、反射类和 Shader 类型时，正常关闭 Editor，完整编译后重新启动。已有 `.uproject` 中的 MCP 和 ModelingTools 条目应保留，只增量添加新的 Runtime 插件；它不应带仅允许 Editor 的限制。

可参考的本机编译命令，实施时在插件文件准备好后运行：

```powershell
& 'E:\ue\UE_5.8\Engine\Build\BatchFiles\Build.bat' `
  RenderLabEditor Win64 Development `
  '-Project=E:\ue_project\pratice\RenderLab\RenderLab.uproject' `
  -WaitMutex
```

**停下来检查：**模块是否加载、路径是否映射、Shader 编译日志是否能定位到自己的文件。此时没有画面效果是正常的，还没有创建并接入渲染回调。

### A3. 注册 Global Pixel Shader 与参数接口

建议类型：`FRenderLabGrayscalePS`，继承 `FGlobalShader`。

需要理解四个入口：

- `DECLARE_GLOBAL_SHADER`：声明 Shader 类型。
- `SHADER_USE_PARAMETER_STRUCT`：使用结构化参数绑定。
- `BEGIN_SHADER_PARAMETER_STRUCT`：声明传给 Shader 与 RDG 的参数。
- `IMPLEMENT_GLOBAL_SHADER`：关联 C++ 类型、虚拟路径、HLSL 入口和 Shader 阶段。

第一版参数示意：

```cpp
BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
    SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
    SHADER_PARAMETER(float, Strength)
    RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()
```

注册示意：

```cpp
IMPLEMENT_GLOBAL_SHADER(
    FRenderLabGrayscalePS,
    "/Plugin/RenderLabRendering/Private/RenderLabGrayscale.usf",
    "MainPS",
    SF_Pixel);
```

这些片段不是完整可编译类。需要自行补全头文件、类声明和编译平台条件，并与本机 `GlobalShader.h`、`ShaderParameterStruct.h` 核对。

输入纹理、Sampler 和 `Strength` 的名字与类型需要和 HLSL 对应；`RENDER_TARGET_BINDING_SLOTS()` 用于描述本次绘制的输出绑定，不是在 HLSL 中声明一个同名普通变量。

第一版只有一套算法，不设计多维 Permutation。若通过 `ShouldCompilePermutation` 限制桌面 Feature Level，运行时取 Shader 的条件必须与之匹配；不要在 Cook 时排除了某个平台，又在运行时无条件获取该 Shader。项目使用 SM6，示例中的 SM5 Feature Level 下限也不能被理解成强制项目切回 SM5。

### A4. 编写最小 HLSL

从“输入直通”开始，确认可用后再加入灰度。下面展示与 `FScreenPassVS` 配套的单视图 Pixel Shader 写法：

```hlsl
#include "/Engine/Private/Common.ush"

Texture2D InputTexture;
SamplerState InputSampler;
float Strength;

void MainPS(
    noperspective float4 UVAndScreenPos : TEXCOORD0,
    float4 SvPosition : SV_POSITION,
    out float4 OutColor : SV_Target0)
{
    float4 source = Texture2DSample(
        InputTexture, InputSampler, UVAndScreenPos.xy);

    // A4 第一步先使用：OutColor = source;
    float gray = dot(source.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    OutColor = float4(
        lerp(source.rgb, float3(gray, gray, gray), saturate(Strength)),
        source.a);
}
```

这是待编译的实现参考，限定于本文单视图路径，不包含立体渲染接口。`UVAndScreenPos.xy` 应由正确配置输入/输出 Viewport 的 ScreenPass 绘制提供；不是可以任意替换成 `SV_POSITION.xy / 窗口尺寸` 的通用坐标。

实验顺序：

1. 原图直通：检查输入、输出与采样区域。
2. 临时交换 R/B 通道：让红蓝测试物体明显交换颜色，证明自己的 Shader 在运行。
3. 恢复采样，固定 `Strength = 1`：验证灰度。
4. 加入 `0 / 0.5 / 1` 对照，再进入运行时控制。

保持输入 Alpha，不无条件写 `1`。本轮计算的是**当前输入编码上的加权灰度**；色调映射后不等于固定 sRGB，也不能把这里的数值称为场景物理亮度。不要凭经验额外加入 `pow(x, 2.2)` 或 PreExposure 修正。

### A5. 创建并持有 View Extension

推荐用一个轻量 `UWorldSubsystem` 持有 `FWorldSceneViewExtension` 派生对象：

1. Subsystem 只支持 `EWorldType::Game` 和 `EWorldType::PIE`。
2. 初始化时，通过 `FSceneViewExtensions::NewExtension<...>(World, ...)` 创建扩展。
3. Subsystem 用线程安全共享指针持有扩展；不要只创建一个局部变量后立即丢弃。
4. 沿用 `FWorldSceneViewExtension` 的 World 匹配逻辑。若覆盖 `IsActiveThisFrame_Internal`，保留基类条件。
5. 按视图排除 `bIsSceneCapture`、`bIsReflectionCapture`；多视图、VR 等仍保持未验证。
6. Deinitialize 中撤销本 World 的控制并释放自身持有的引用，具体顺序见 Lab03B。

这让生命周期与 World 对应，也避免普通材质预览窗口和 Editor World 自动套上灰度。初期在 PIE 或独立游戏窗口观察；Editor 视口不生效符合本轮规格。

A 阶段尚未接入动态控制时，可以在扩展创建时传入固定的实验设置 `Enabled=true, Strength=1`；这些值发布后不再修改。完成固定效果后，B 阶段再替换为线程安全的参数更新，并恢复第 2 节约定的关闭默认值。不要在扩展已经参与渲染后，从游戏线程直接改它的普通成员。

不要在 `PostConfigInit` 阶段假设游戏 World 已存在。模块加载和 World 创建是两个不同阶段。

本机 UE5.8 的订阅方法签名包括 `const FSceneView&`：

```cpp
void SubscribeToPostProcessingPass(
    EPostProcessingPass Pass,
    const FSceneView& View,
    FPostProcessingPassDelegateArray& InOutPassCallbacks,
    bool bIsPassEnabled) override;
```

回调本身返回 `FScreenPassTexture`，参数为：

```cpp
FScreenPassTexture AfterTonemap_RenderThread(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    const FPostProcessMaterialInputs& Inputs);
```

订阅限定到 `EPostProcessingPass::Tonemap`、对应引擎 Pass 启用、当前视图符合范围、渲染侧效果启用的情况。使用与扩展生命周期匹配的 Delegate 绑定方式；不要绑定短命栈对象。

旧教程不带 `FSceneView` 的重载在本机头文件中已弃用，并归入“不再调用”的旧 API。`FAfterPassCallbackDelegateArray` 是旧名别名，本指南采用当前的 `FPostProcessingPassDelegateArray`。

### A6. 在回调中建立一个 RDG Pass

回调内依次完成：

1. 从 `Inputs.GetInput(EPostProcessMaterialInput::SceneColor)` 取得当前输入。
2. 注意返回类型是 **`FScreenPassTextureSlice`**；第一版使用 `FScreenPassTexture::CopyFromSlice(GraphBuilder, ...)` 获得适合普通 Texture2D 采样的输入。
3. 优先使用 `Inputs.OverrideOutput`。没有有效 Override 时，用 `FScreenPassRenderTarget::CreateFromInput` 创建自己的输出。
4. 分别构造 `FScreenPassTextureViewport InputViewport(Input)` 与 `OutputViewport(Output)`。
5. 使用 `GraphBuilder.AllocParameters<...>()` 分配参数，绑定输入、Sampler、当前强度和 `Output.GetRenderTargetBinding()`。
6. 从对应 View 的 Feature Level 获取 Global Shader Map，取得 `FScreenPassVS` 与 `FRenderLabGrayscalePS`。
7. 用 `AddDrawScreenPass` 添加绘制，事件名建议为 `RenderLab.Grayscale`。
8. 返回包含**实际写入纹理及有效 ViewRect**的结果，交给后续流程。

以下为流程伪代码，需结合类型与错误分支完成实现：

```text
Input  = 从 Inputs 的 SceneColor slice 转换得到
Output = Inputs.OverrideOutput
if Output 无效:
    Output = 基于 Input 创建 RDG Render Target

PassParameters.InputTexture = Input.Texture
PassParameters.InputSampler = 选定的采样器
PassParameters.Strength = 本次渲染侧快照中的强度
PassParameters.RenderTargets[0] = Output 的绑定

AddDrawScreenPass(
    当前 GraphBuilder, "RenderLab.Grayscale", 当前 View,
    OutputViewport, InputViewport,
    ScreenPassVS, GrayscalePS, PassParameters)

return Output
```

需要明确的约定：

- **`OverrideOutput` 有效时必须写入它。**它可能对应后处理最终输出；另写一张纹理然后返回不能替代此契约。
- 输入 Slice 转换不保证免费；若涉及 Texture2DArray 或指定输出，可能产生 Copy Pass。抓帧时把它与灰度 Draw 区分。
- 第一版让输入与输出使用不同子资源；不要一边采样同一纹理子资源，一边把它作为 Render Target 写入。若路径出现别名，先复制到独立输入再处理。
- `AddDrawScreenPass` 内部已经使用 `GraphBuilder.AddPass`。不要在它外面再包一层 `AddPass`，也不要在回调里调用 `GraphBuilder.Execute()`。
- Pass 参数通过 RDG 分配；延迟执行的 Lambda 不能持有已经退出作用域的栈引用。
- `FRDGTextureRef` 只在当前图的有效期内使用，不存到 Subsystem 或扩展成员供下一帧复用。
- 不需要手动创建长期 RHI 纹理或手写资源 Barrier。RDG 必须能从参数结构看到实际读写资源。

如果已进入回调，但随后需要原图回退，核对并使用本机 `Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder)`；它会处理必要的输出复制。正常关闭效果应在订阅前跳过，不以“回调返回了原图”宣称没有任何 GPU 工作。

### A7. 坐标与输出范围

最容易混淆的三个量：

| 量 | 含义 |
| --- | --- |
| Texture Extent | 纹理实际分配的总尺寸 |
| ViewRect | 当前视图在该纹理中使用的矩形，可以有非零起点 |
| ViewportUV | 当前视口范围内的归一化位置 |

若将某个有效区域内的归一化坐标映射到源纹理，概念上需要：

```text
SourcePixelPosition = InputViewRect.Min + ViewportUV * InputViewRect.Size
SourceTextureUV     = SourcePixelPosition / InputTextureExtent
```

这里用于解释范围，不是让你手写一套忽略像素中心、切片和采样边界的转换函数。第一版通过 ScreenPass 的输入/输出 Viewport 生成采样坐标。

Sampler 的 Clamp 通常约束整张纹理的边界，不能自动保证不采到 ViewRect 外的有效分配区域。若后续加入邻域采样、缩放或边缘滤波，应额外核对有效矩形边界；本轮先保持一对一颜色变换。

### A8. 验收

- [ ] 插件和 Shader 编译成功，日志能确认模块与 Shader 文件。
- [ ] PIE 中原图直通没有拉伸、黑边、区域偏移。
- [ ] 红蓝交换诊断效果明确，恢复后固定灰度正确。
- [ ] 能指出输入、输出、Sampler、Strength 和 GPU 事件名。
- [ ] 能解释输入 Slice 转换、ViewRect 与 OverrideOutput 的处理。
- [ ] 源码未加入 Engine 私有头文件路径，未修改 Engine。

## 7. Lab03B：Gameplay 参数、线程与生命周期

### B1. 设计一个小接口

在 Subsystem 暴露 `BlueprintCallable`：

```cpp
void SetGrayscaleSettings(bool bEnabled, float Strength);
```

在 Game Thread 校验强度是否有限并限制到 `0～1`。将开关和强度一起提交，避免一次效果设置被拆成两个不同步的控制入口。蓝图先按以下顺序驱动：

```text
BeginPlay
  -> Enabled=true, Strength=0
  -> 延迟观察 -> true, 0.5
  -> 延迟观察 -> true, 1
  -> 延迟观察 -> false, 1
```

延迟仅便于观察，不是线程同步机制。第一版不需要 UI、输入框架，也不需要每帧寻找或重建 Subsystem/View Extension。

### B2. 将游戏侧状态与渲染侧状态分开

建议维护：

- 游戏侧状态：Subsystem 的当前设置，仅 Game Thread 读写。
- 渲染侧状态：独立的纯 C++ 小结构，包含 `Enabled` 和 `Strength`，发布后仅 Render Thread 读写。
- Subsystem 和 View Extension 持有渲染侧状态的共享所有权；它不包含 Actor、World 或 UObject 指针。

更新步骤：

1. setter 在 Game Thread 生成纯值快照。
2. `ENQUEUE_RENDER_COMMAND` 按值捕获快照，以及保证状态对象存活的共享引用。
3. 命令在 Render Thread 把快照复制进渲染侧状态。
4. Render Thread 上的订阅和 Pass 构建读取该状态，将本次值写入 RDG 参数结构。

示意：

```text
GT: Snapshot = { Enabled, ClampedStrength }
GT: Enqueue command [State 的共享引用, Snapshot 的值副本]
RT: State = Snapshot
RT: 订阅检查 State.Enabled
RT: PassParameters.Strength = State.Strength
```

不要在渲染回调里通过 `Subsystem->GetWorld()` 查 Actor，也不要让两个线程同时读写一份普通 `float` 或 `bool`。

**线程安全共享指针保障引用计数和寿命，不自动保障成员数据并发访问。**这个方案依靠明确的读写线程与命令顺序保证数据安全。即使标记为 `volatile`，也不能替代同步设计。

`IsActiveThisFrame_Internal` 中可以保留基类 World 判断，不读取仅归渲染线程所有的设置；开关检查放在渲染侧订阅逻辑。不要因为两个方法属于同一个 C++ 对象，就假定它们在同一线程调用。

第一次只传 bool/float，先理解值的所有权。纹理 UObject、异步加载、跨帧历史和 GPU 缓冲区的生命周期留作独立任务。

### B3. 退出时谁还可能持有对象

Subsystem 结束时：

1. 在 Game Thread 停止接受该 World 的新设置，更新自己的关闭状态。
2. 排入设置渲染侧 `Enabled = false` 的命令；命令捕获状态对象的共享引用，不能捕获即将销毁的 UObject 裸指针。
3. 释放 Subsystem 持有的 View Extension 引用。
4. 已经开始构建的 View Family、回调或排队命令可能仍然持有必要的引用；允许在途工作结束，再自然释放。

扩展析构不能依赖仍存在的游戏 Actor，也不应在未知线程中操作 UObject。正常 World 退出与模块代码热卸载是不同问题，本轮不测试运行中卸载 Shader 模块。

不在每次 setter 或每帧插入 `FlushRenderingCommands` 来“确保同步”。发生退出问题时，先追踪持有者与命令捕获；确有资源释放需求时再研究恰当的同步点。

### B4. 验收

- [ ] `0 / 0.5 / 1` 的输出符合预期。
- [ ] 相同值重复设置不会创建第二个扩展或第二份效果。
- [ ] 关闭时不再添加 `RenderLab.Grayscale` Pass，其他后处理保持工作。
- [ ] 强度为零但启用时，仍能找到效果事件，输出保持原图。
- [ ] 能画出 GT 设置、命令快照、RT 状态、Shader 参数的四个位置。
- [ ] 渲染侧没有读取 Subsystem/Actor 的可变属性。
- [ ] 普通 Editor World 与资产预览不受游戏 World 的控制影响。

## 8. Lab03C：抓帧、位置核对与 Lab02 对照

### C1. 先确认真实插入位置

本机 UE5.8 桌面后处理代码中的相关顺序可以概括为：

```text
Tonemap
  -> Tonemap 的 View Extension 回调（本次拟接入位置）
  -> FXAA（若启用）及其扩展回调
  -> SMAA（若启用）及其扩展回调
  -> Scene Color After Tonemapping 的后处理材质链
```

因此 Lab02 材质的 `Scene Color After Tonemapping` 与本次 `EPostProcessingPass::Tonemap` 回调**不是严格相同的插入点**。不能只因名字里都有 Tonemap，就要求两者总是逐像素相同。

使用当前 AA 设置先抓一帧，记录中间实际存在的 Pass。做严格 A/B 时，确保影响输入的中间处理一致；若不一致，先说明差异，必要时再单独设计插入点对照。不要为强行得到相同截图而同时关闭 AA、Lumen、曝光等一组功能。

本轮只订阅后续处理回调，不使用 `ReplacingTonemapper` 替换 UE 的色调映射。

### C2. 捕获与资源表

固定相机、效果强度与实验条件，等 Shader 编译完成和画面稳定。捕获以下三种状态：

| 编号 | 设置 | 预期 |
| --- | --- | --- |
| G0 | `Enabled=false` | 不存在本效果事件 |
| G1 | `Enabled=true, Strength=0` | 存在事件，输出原图 |
| G2 | `Enabled=true, Strength=1` | 存在事件，输出灰度 |

在 G2 中找到 `RenderLab.Grayscale`，填写：

| 项目 | 要记录的内容 |
| --- | --- |
| 调用位置 | 前后各一两个 GPU 事件；是否在 Tonemap 后 |
| 输入 | 资源名称、维度、格式、Extent、实际绑定的 Slice/Mip |
| 输出 | 资源名称、格式、Extent；是自建输出还是 OverrideOutput |
| 绘制区域 | Viewport 与 Scissor；与代码中的 ViewRect 是否一致 |
| Shader | Vertex/Pixel Shader、可识别的入口与源文件信息 |
| 参数 | Strength、Sampler，实际绑定的输入资源 |
| 图像 | 输入颜色和输出灰度的对照，同一位置的像素示例 |
| 前置复制 | 是否因输入切片等原因多出 Copy Pass |

调试符号不足时，先用事件、资源和实际输出建立链路，再针对自己的 Shader 补充诊断。看不到 HLSL 源码不意味着 Pass 没执行。

### C3. 与材质版分开运行

创建两个可明确切换的测试条件：

- 条件 M：只启用 Lab02 灰度材质，插件效果关闭。
- 条件 G：只启用插件灰度，移除本测试场景中灰度材质的参与。

参数、颜色编码、曝光和实际插入位置应有记录。两版同样使用 `0.2126 / 0.7152 / 0.0722` 权重；不要一版保留颜色空间转换，一版省略，再把差异归因于 Global Shader。

比较问题：

1. 两者使用的输入是否是同一处理阶段的图像？
2. 执行分辨率、资源格式和有效矩形是否相同？
3. 哪些步骤由材质系统提供，哪些在插件中由你显式指定？
4. 插件版是否增加了必要的复制？关闭效果的行为有什么不同？

不要预设 Global Shader 一定更快。两种实现都可能执行一次相似的全屏绘制；资源复制、带宽、执行位置和调度也影响成本。需要性能结论时使用相同条件、预热和重复测量；RenderDoc 单次捕获计时不作为稳态性能证明。

### C4. 验收

- [ ] 三种开关/强度状态与实际事件一致。
- [ ] 有事件位置截图、输入输出对照及资源表。
- [ ] 能从 GPU 事件找到自己的 C++ Pass 构建函数和 HLSL。
- [ ] 材质与插件版没有同时叠加。
- [ ] 插入位置差异得到解释，不把视觉接近当成逐像素等价证明。
- [ ] 没有以代码更短、节点更少或单次捕获时间推断性能优劣。

## 9. Lab03D：边界验证与首次 Development 打包

### D1. 测试矩阵

| 条件 | 操作 | 期望 |
| --- | --- | --- |
| 重复 PIE | 连续进入/退出至少三次 | 初始状态一致，无重复扩展和退出异常 |
| 参数范围 | 输入负数、超过 1 的数和正常值 | 接口按规格处理，不出现不可解释输出 |
| 窗口形状 | 宽屏、接近正方形、非整数缩放后的尺寸 | 无黑边、拉伸或局部覆盖 |
| 非整齐尺寸 | 例如 `1279×719`，记录实际有效视图尺寸 | 处理完整区域，不假定尺寸整除 |
| 屏幕百分比 | 固定 AA，对照 100 与 75 | 采样区域正确；记录实际后处理输入尺寸 |
| 地图切换 | 从实验图进入不调用控制接口的简单地图 | 新 World 使用关闭默认值，旧设置不泄漏 |
| 开关 | 连续开启、调整、关闭 | 后续渲染正常，关闭路径无灰度事件 |
| 独立运行 | 普通游戏窗口与 Development 包 | 不依赖编辑器或 MCP |

改变屏幕百分比后，Tonemap 后纹理可能仍保持输出分辨率，因为前面已有上采样。以实际捕获为准，不把 CVar 数值直接当成 Pass 的纹理大小。

对输入矩形有偏移、输出最终落到 OverrideOutput 的情况要留意。单个窗口一次成功不能覆盖所有路径；未遇到的路径在记录中标为未验证。

### D2. 首次打包流程

1. 确认 Runtime 插件已启用，所有正常游戏路径只依赖 Runtime 模块。
2. 在实验关卡显式放置运行时控制入口，避免只有 Editor 手工调参时才启用效果。
3. 在打包设置中确认实验地图会进入 Cook；记录地图包含方式。不要为了方便无意替换项目原默认地图。
4. 完成 Win64 Development 的 Build、Cook、Stage、Package，记录日志位置与失败阶段。
5. 打开生成的可执行程序并显式加载实验地图；确认窗口中确实是该地图。
6. 测试开关、强度、窗口变化和退出。读取打包日志，检查 Shader 缺失、插件未加载或资产路径错误。

首次打包关注四类问题：

- C++ 编译与链接是否通过。
- Shader 是否为实际目标平台 Cook 出所需版本。
- 地图和运行时入口是否包含在包中。
- 插件是否在所需时间加载并有正确的依赖关系。

仅编译 `RenderLabEditor`、仅运行 Standalone PIE 或仅在本机 DDC 有缓存都不构成打包验收。也不要用手工复制某个缓存 Shader 文件来掩盖 Cook 配置问题。

## 10. UE5.8 常见问题与排查顺序

| 现象 | 先检查什么 |
| --- | --- |
| Shader 文件找不到 | 插件实际目录、虚拟前缀、文件名大小写、StartupModule 和加载阶段 |
| Shader 类型加载太晚 | `.uplugin` 是否在所需阶段加载，是否试图靠 Live Coding 首次加入 Shader 类型 |
| C++ 编译通过但没有回调 | 扩展是否被持有、World 类型与视图筛选、是否实现了包含 `FSceneView` 的新签名 |
| Editor 视口没效果，PIE 有 | 本指南第一版只支持 Game/PIE World，这是预期 |
| Shader Map 中没有该 Shader | 平台/Feature Level、ShouldCompilePermutation、首次完整重启和 Cook 日志 |
| 只在部分区域显示或发生拉伸 | Input/Output Viewport、ViewRect 起点、Slice 转换和实际纹理尺寸 |
| 最后画面黑屏或像没有写回 | 是否忽略有效 OverrideOutput、输出绑定与返回纹理是否一致 |
| RDG 资源验证失败 | 参数结构是否声明实际读写、是否同子资源边采边写、是否跨帧保存图内资源 |
| 改参数后随机闪烁 | 普通成员被 GT/RT 共同访问、捕获了栈引用、多个入口提交不一致状态 |
| 退出 PIE 偶发崩溃 | Lambda 的 UObject/裸 this 捕获、引用持有者、Subsystem 结束后的在途命令 |
| 出现多个灰度事件 | 重复创建扩展、多个 World/视图、同时运行材质版与插件版 |
| `Strength=0` 仍有 GPU 事件 | 符合本轮控制规格；零强度不是关闭开关 |
| 关闭后仍有复制 | 是否仍订阅回调并走原图返回辅助函数，或复制属于其他引擎流程 |
| 材质版与插件版颜色不同 | AA/执行位置、颜色编码、Alpha、Sampler、曝光与其他效果 |
| 打包程序失败，Editor 正常 | Runtime 依赖、地图 Cook、目标平台 Shader、插件加载阶段 |
| Shader 编译再次耗尽内存 | 当前配置、后台负载、是否批量打开诊断或改变全局 Shader 配置；先看日志，不清空整个 DDC |

排查时一次只改变一个原因。不要通过永久跳过 RDG 验证、每帧 Flush 或无限扩大头文件依赖来隐藏问题。

## 11. 源码阅读路线与参考资料

### 11.1 本机 UE5.8 源码入口

以下文件和关键符号已于 2026-09-21 在本机 **5.8.0 / CL 55116800** 源码中读取核对。表中是参考入口，不能据此宣称本文尚未实现的插件已经编译或运行通过。行号对应本机快照，升级后按符号搜索。

| 顺序 | 源码入口 | 本次只回答这个问题 |
| --- | --- | --- |
| S01 | [OpenColorIO.uplugin](E:/ue/UE_5.8/Engine/Plugins/Compositing/OpenColorIO/OpenColorIO.uplugin)；[OpenColorIOModule.cpp](E:/ue/UE_5.8/Engine/Plugins/Compositing/OpenColorIO/Source/OpenColorIO/Private/OpenColorIOModule.cpp:24) | Shader 模块怎样加载并映射目录？ |
| S02 | [GlobalShader.h](E:/ue/UE_5.8/Engine/Source/Runtime/RenderCore/Public/GlobalShader.h:282)；[PostProcessBloomSetup.cpp](E:/ue/UE_5.8/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessBloomSetup.cpp:65) | Shader 类、参数结构、编译条件与注册怎样对应？ |
| S03 | [SceneViewExtension.h](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Public/SceneViewExtension.h:117) | 当前有哪些接入位置，订阅方法和返回值是什么？ |
| S04 | [WorldSubsystem.h](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Public/Subsystems/WorldSubsystem.h)；[SceneViewExtension.cpp](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Private/SceneViewExtension.cpp:43) | World 生命周期和扩展的 World 筛选如何配合？ |
| S05 | [OpenColorIODisplayExtension.cpp](E:/ue/UE_5.8/Engine/Plugins/Compositing/OpenColorIO/Source/OpenColorIO/Private/OpenColorIODisplayExtension.cpp:66) | 官方本机插件怎样订阅 Tonemap 并处理 Slice/OverrideOutput？ |
| S06 | [PostProcessMaterialInputs.h](E:/ue/UE_5.8/Engine/Source/Runtime/Renderer/Public/PostProcess/PostProcessMaterialInputs.h:49) | GetInput 的真实类型、OverrideOutput 和原图回退有什么约定？ |
| S07 | [ScreenPass.h](E:/ue/UE_5.8/Engine/Source/Runtime/Renderer/Public/ScreenPass.h:45) | Texture、Slice、Viewport 和 AddDrawScreenPass 怎样组织？ |
| S08 | [OpenColorIORendering.cpp](E:/ue/UE_5.8/Engine/Plugins/Compositing/OpenColorIO/Source/OpenColorIO/Private/OpenColorIORendering.cpp:24) | 参数、采样器、Render Target 和绘制如何绑定？只看骨架 |
| S09 | [PostProcessVisualizeBuffer.usf](E:/ue/UE_5.8/Engine/Shaders/Private/PostProcessVisualizeBuffer.usf)；[ScreenPass.usf](E:/ue/UE_5.8/Engine/Shaders/Private/ScreenPass.usf) | ScreenPass 顶点输出怎样进入 Pixel Shader？ |
| S10 | [PostProcessing.cpp](E:/ue/UE_5.8/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessing.cpp:723)；[Tonemap 后的调用位置](E:/ue/UE_5.8/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessing.cpp:1637) | 回调返回的纹理怎样传给后续链，材质版在哪个位置？ |

OpenColorIO 用于阅读本机已存在的接入方式，不需要为了 Lab03 启用它或复制其动态 Shader 参数系统。引擎内部 `.cpp` 可以用于学习，但不能因此把其中的所有私有 include 直接搬入项目插件。

建议分三次阅读，每次 20～40 分钟：

1. **A 阶段：**S01、S02、S03、S05、S06，看初始化与最小通路。
2. **B 阶段：**S04 和 S05 中 `SetupView` 的命令捕获，看对象与线程。注意参考中的弱指针方案与本指南的独立共享状态方案有不同的持有方式。
3. **C 阶段：**S07～S10，结合自己的 GPU 事件读调用链。

不要从 `DeferredShadingRenderer.cpp` 第一行开始通读，也不要求在写第一个 Pass 前掌握整个 RDG 编译器。

### 11.2 官方概念资料

以下为官方文档入口，按当前问题阅读局部。本文的 UE5.8 接口结论依据上面的本机源码；本轮在线检索未取得可核验的页面正文，因此不把网上示例标为已验证适配 5.8。

```text
Shaders in Plugins
https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-shaders-in-plugins-unreal-engine

Render Dependency Graph
https://dev.epicgames.com/documentation/en-us/unreal-engine/render-dependency-graph-in-unreal-engine

Threaded Rendering
https://dev.epicgames.com/documentation/en-us/unreal-engine/threaded-rendering-in-unreal-engine

ISceneViewExtension API
https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/ISceneViewExtension
```

RDG 资料先看资源声明、Pass 参数、图的构建/执行和资源生命周期。资源提取、异步计算及历史纹理在 Lab04 再系统展开。

## 12. 实验记录、验收与提交

### 12.1 报告模板

实施时新建 `Docs/Experiments/Lab03_Results.md`：

```markdown
# Lab03 实验记录

## 环境与范围
UE/CL、Git 提交、GPU/驱动、RHI/Shader 平台、地图、相机、
分辨率、屏幕百分比、AA、SDR/HDR、曝光、运行方式。

## Lab03A：接入
模块与 Shader 文件、虚拟路径、编译日志、直通/诊断/灰度结果。
输入 Slice、输出和 OverrideOutput 处理。

## Lab03B：参数与生命周期
GT/RT 数据路径、对象持有关系、开关和强度测试。
退出顺序、已发现问题和修正。

## Lab03C：GPU 帧
事件截图、输入输出资源表、Shader 与参数。
G0/G1/G2 对照，材质版与插件版的位置差异。

## Lab03D：稳定性与打包
测试矩阵：条件 | 预期 | 实际 | 证据 | 状态。
Build/Cook/Package 与独立运行记录。

## 结论与未验证范围
已理解的链路、失败尝试、已验证/未验证项目、下一步。
```

### 12.2 阶段总验收

- [ ] 能画出从 Gameplay setter 到输出纹理的完整链路。
- [ ] 能区分 Shader 注册、编译、Pass 添加与 GPU 执行。
- [ ] 能说明 Global Shader 与 Lab02 Material Shader 的职责差异。
- [ ] 输入、输出、ViewRect、纹理格式与颜色编码有记录。
- [ ] 开关路径和零强度路径符合约定，抓帧可证明。
- [ ] GT/RT 没有无约定共享可变状态，图内资源不跨帧滥用。
- [ ] 窗口、屏幕百分比、重复 PIE、切换地图验证通过。
- [ ] Development 打包并独立运行通过，MCP 不参与运行。
- [ ] 未验证的平台和渲染路径明确标注。

代码能运行后，还要用自己的话回答：

1. 为什么 `IMPLEMENT_GLOBAL_SHADER` 不会自动让效果出现在每一帧？
2. 为什么窗口大小不能直接代替输入纹理 Extent 和 ViewRect？
3. 为什么已经获得输入纹理后，还必须处理 OverrideOutput？
4. 为什么共享指针线程安全，不代表它持有的参数可以随意跨线程访问？
5. 为什么本轮不保存 RDG Texture 到下一帧？
6. 为什么 `Strength=0` 与 `Enabled=false` 的画面一样，GPU 工作却不同？
7. 为什么 Shader 在 Editor 可用，打包程序仍可能失败？

### 12.3 提交与后续

每个子任务形成一个可解释的检查点。保存本次需要的资产后，检查 Git/LFS，明确列出并暂存属于该任务的代码、必要配置和记录；不要把已有的其他实验改动顺手一起提交。

记录中的截图和小型说明可提交；插件 `Binaries/Intermediate`、完整 `.rdc`、Shader 调试输出及 DDC 不作为源码提交。不会因本文存在就自动创建、编译或提交实验实现。

完成 Lab03 后再进入 **Lab04：横向模糊 → 纵向模糊 → 合成**，届时深入多 Pass 依赖、全/半分辨率、资源生命周期，并按需要引入 Compute Shader。

**第一次学习的起点：建立 Lab03 实验基线，完成 A1～A2，再逐步做到原图直通。**
