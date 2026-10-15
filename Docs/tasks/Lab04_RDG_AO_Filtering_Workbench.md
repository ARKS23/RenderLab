# Lab04：多 Pass RDG、屏幕空间 AO 与引导滤波

编写日期：2026-10-08。

项目：RenderLab / Unreal Engine **5.8.0 CL 55116800** / Windows / DX12 / SM6。

状态：**实践指南已建立，Lab04 实现与运行验证尚未开始。**

上级路线：[项目执行路线](../UE_Rendering_Learning_Roadmap.md#lab04)。

年度目标与时间预算：[全年学习路线](../图形与游戏引擎开发_全年学习路线_2026-2027.md)。

前置任务：[Lab03 指南](Lab03_GlobalShader_Workbench.md)。

用户已反馈 Lab03C/D 完成，可以沿用已有成果进入本阶段。Lab03 的完整抓帧、边界测试与打包记录仍需归档；本文不代表重新执行过这些测试。

**阅读与使用方式：**先完成准备和 Lab04A，再按 B → C → D → E 推进。代码块是供你逐步实现的参考，不是已经落入插件并通过编译的功能。不要一次复制全部阶段，也不要把未做的测试提前勾选。

## 导航

- [1. 目标与范围](#scope)
- [2. 实施前准备与文件清单](#prepare)
- [3. 阅读资料与本机源码](#reading)
- [4. Lab04A：横向模糊、纵向模糊与合成](#lab04a)
- [5. Lab04B：深度、法线与位置重建](#lab04b)
- [6. Lab04C：最小屏幕空间 AO](#lab04c)
- [7. Lab04D：深度／法线引导滤波](#lab04d)
- [8. Lab04E：V1 验证与交付](#lab04e)
- [9. 可选加深与排错](#troubleshooting)
- [10. 实验记录与第一天任务](#records)

<a id="scope"></a>
## 1. 目标与范围

### 1.1 本阶段解决什么问题

Lab03 已经回答“一个 Shader 怎样进入 UE 的一帧”。Lab04 要回答：

1. 多个 Pass 如何通过中间纹理组成可解释的资源依赖图？
2. 怎样证明深度、法线、坐标和有效视图范围正确？
3. 怎样把一个图形算法接入现有插件，而不把整个引擎重写一遍？
4. 怎样区分算法误差、输入错误、采样错误和资源绑定错误？
5. 怎样交付有质量对照、性能记录和复现步骤的作品？

最终成果是 **RenderLab V1**，不只是“屏幕变暗了”的 AO 演示。

| 阶段 | 最小成果 | 进入下一步的条件 |
| --- | --- | --- |
| A | 全分辨率横向模糊、纵向模糊、原图合成 | 三个 Pass 和中间纹理可追踪，边缘与视图范围正确 |
| B | 深度、法线、位置调试显示 | 空间、单位、背景、坐标映射均能解释 |
| C | 独立 AO 灰度输出 | 接触、凹角、平面等简单场景符合预期 |
| D | 引导滤波及低／高采样对照 | 降噪收益和边缘损失有证据 |
| E | 三场景、性能、边界、打包与演示 | 其他人能按记录重现结果 |

### 1.2 第一版支持范围

- Windows / DX12 / 桌面延迟渲染 / 单玩家主视图 / 非 VR。
- 使用现有 Runtime 插件 `RenderLabRendering`，不创建第二套渲染框架。
- 保留 Lab03 灰度代码、蓝图入口及原实验关卡。
- A 阶段沿用 Tonemap 后回调，只做当前颜色编码上的图像滤波，不称为物理正确的 HDR 光照滤波。
- B/C/D 首先验证静态、不透明、简单几何；暂不承诺透明表面、毛发、水、多层材质、多视图和全部 Nanite 路径。
- 首先使用 Raster + Global Pixel Shader；Compute 作为后续受控对照。
- 不先加入半分辨率、时域历史、Async Compute 或 GPU Readback。

**AO 边界：**第一版在后处理中展示自己计算的遮蔽，不替换 UE SSAO/Lumen，也不接入完整光照。将 AO 乘到 Tonemap 后的颜色仅是可选展示，不等于物理正确地只衰减环境光。

### 1.3 学习节奏

每次约 60～120 分钟，只解决一个问题：

```text
预测结果 → 最小实现 → 编译／运行 → 抓帧或数值检查
        → 改一个条件 → 记录 → 留下可恢复版本
```

全年计划将 2026-10-05 至 2026-10-11 作为 A/B 的目标窗口，但日期不是验收标准。B 不正确就继续 B，不为赶进度跳到 AO。高采样参考也是屏幕空间近似，不称为真实 Ground Truth。

<a id="prepare"></a>
## 2. 实施前准备与文件清单

### 2.1 已核对的本机基线

本文编写时读取了项目和本机引擎：

| 项目 | 当前情况 |
| --- | --- |
| 引擎 | `E:/ue/UE_5.8`，`Engine/Build/Build.version` 为 5.8.0 CL 55116800 |
| 插件 | Runtime 模块，`PostConfigInit` 加载 |
| 虚拟 Shader 路径 | `/Plugin/RenderLabRendering` |
| 渲染依赖 | 已有 `RenderCore`、`RHI`、`Renderer`、`Projects` 等 |
| 接入 | World Subsystem → World View Extension → Tonemap 回调 |
| 线程 | GT 生成值快照，渲染命令更新 RT 状态，委托按值携带参数 |
| 配置 | DX12 / SM6；Substrate 已开启 |
| 调试 | Shader 符号关闭，配置保留此前内存不足后的限制 |

不要为开始 Lab04 重建插件、修改 Engine、重新配置 MCP，或一次开启所有 Shader 调试信息。

### 2.2 先保护实验条件

1. 检查 Git 状态。当前已有路线文档改动，不覆盖或回退。
2. 确认已完成的 Lab03 打包程序和必要截图仍可找到，记录其本机路径。
3. 新建独立实验关卡，例如 `/Game/RenderLab/Lab04_RDG/L_Lab04_Workbench`，不替换原默认地图。
4. 场景先用地面、墙角、球体、方块和彩色条纹；固定相机、曝光和光照。
5. 关闭本测试场景中 Lab02 灰度／描边等干扰效果，Lab03 灰度也先关闭。记录关闭了什么，不批量改整个项目的画质设置。
6. 每个实验记录实际分辨率、Screen Percentage、AA、RHI、Substrate、运行方式和当前 Git 提交。
7. RenderDoc 的临时 Shader 替换先撤销，再捕获新的基线。

固定数值先使用 `1280×720`；后续加入 `1279×719`、近方形窗口和屏幕百分比变化。

### 2.3 A 阶段逐文件清单

下面是**你实践时要修改／新增的文件**，不是本文已经替你实现的内容。

| 文件，均位于 `Plugins/RenderLabRendering/` 下 | 操作 | 职责 |
| --- | --- | --- |
| `Shaders/Private/RenderLabBlur.usf` | 新增 | 单方向模糊，横／纵共用同一 Shader |
| `Shaders/Private/RenderLabComposite.usf` | 新增 | 原图与模糊图合成 |
| `Source/RenderLabRendering/Private/RenderLabBlurShader.h` | 新增 | 暴露 `AddRenderLabBlurPasses` |
| `Source/RenderLabRendering/Private/RenderLabBlurShader.cpp` | 新增 | Shader 注册、参数、三个 Pass 和资源串联 |
| `Source/RenderLabRendering/Private/RenderLabGrayscaleRenderState.h` | 增量修改 | 实验模式与 `BlurMix` |
| `Source/RenderLabRendering/Public/RenderLabGrayscaleSubsystem.h` | 增量修改 | 增加蓝图入口 `SetBlurSettings` |
| `Source/RenderLabRendering/Private/RenderLabGrayscaleSubsystem.cpp` | 增量修改 | 完整快照提交与默认状态处理 |
| `Source/RenderLabRendering/Private/RenderLabGrayscaleViewExtension.cpp` | 增量修改 | 在同一扩展内选择灰度或模糊回调 |

**暂不重命名**现有 `GrayscaleSubsystem` / `GrayscaleViewExtension`，避免破坏蓝图引用。它们当前承担实验宿主职责；等多个实验都稳定后再讨论命名整理。

`.uproject`、`.uplugin`、`*.Target.cs` 和 `*.Build.cs` 目前不需要为 A 阶段新增依赖；`Config/*.ini` 和 MCP 配置也不需要修改。新增头文件使用正常模块包含路径，不加 `Renderer/Private` 搜索路径。

B/C/D 按阶段再新增 `RenderLabInputDebugShader.*`、`RenderLabAOShader.*`、`RenderLabAODenoiseShader.*` 及对应 `.usf`，不要现在创建一堆空类。

<a id="reading"></a>
## 3. 阅读资料与本机源码

### 3.1 每次只读本阶段需要的内容

| 时机 | 阅读内容 | 阅读后要能回答 |
| --- | --- | --- |
| A 前 | 项目路线 R09 / 全年路线 R04：官方 RDG 文档 | 参数声明怎样表达读写？资源何时可用？什么 Pass 会被裁剪？ |
| A 实践时 | `ScreenPass.h/.cpp`、`ScreenPass.ush/.usf` | Extent、ViewRect、采样 UV 和最终输出有什么区别？ |
| B 前 | 全年路线的后处理资料；项目路线 R16：Substrate GBuffer | 当前法线来自哪里？深度是什么表示？ |
| B 实践时 | `Common.ush` 与场景纹理参数声明 | 怎样从 DeviceZ 得到线性深度和位置？ |
| C 前 | 全年路线 R19：LearnOpenGL SSAO | 半球样本如何投影和判断遮蔽？屏幕空间缺什么信息？ |
| D 前 | 全年路线 R20：GAMES202 滤波／去噪相关部分 | 空间、深度、法线权重分别解决什么问题？ |
| E 前 | 项目路线 R11：Unreal Insights | 怎样做正常运行中的重复测量，而不是只读单帧数字？ |

资料链接集中沿用[项目资源索引](../UE_Rendering_Learning_Roadmap.md)和[全年资源索引](../图形与游戏引擎开发_全年学习路线_2026-2027.md#resources)。两份文档的 R 编号不同，不混用。

### 3.2 本机源码入口

下列符号／文件已做存在性与相关内容检查，不代表本文示例已通过编译。

| 本机文件 | 本次重点 |
| --- | --- |
| [ScreenPass.h](E:/ue/UE_5.8/Engine/Source/Runtime/Renderer/Public/ScreenPass.h) | `FScreenPassTexture`、`CopyFromSlice`、`FScreenPassRenderTarget`、`AddDrawScreenPass` |
| [ScreenPass.cpp](E:/ue/UE_5.8/Engine/Source/Runtime/Renderer/Private/ScreenPass.cpp) | `CreateFromInput`、`GetScreenPassTextureViewportParameters` |
| [ScreenPass.ush](E:/ue/UE_5.8/Engine/Shaders/Private/ScreenPass.ush) | `SCREEN_PASS_TEXTURE_VIEWPORT` 与 UV 边界 |
| [ScreenPass.usf](E:/ue/UE_5.8/Engine/Shaders/Private/ScreenPass.usf) | `ScreenPassVS` 生成的 `UVAndScreenPos` |
| [PostProcessMaterialInputs.h](E:/ue/UE_5.8/Engine/Source/Runtime/Renderer/Public/PostProcess/PostProcessMaterialInputs.h) | `SceneColor`、`OverrideOutput`、`SceneTextures`、直通辅助函数 |
| [SceneViewExtension.h](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Public/SceneViewExtension.h) | 带 `FSceneView` 的订阅签名与输出约定 |
| [RenderGraphBuilder.h](E:/ue/UE_5.8/Engine/Source/Runtime/RenderCore/Public/RenderGraphBuilder.h) | `CreateTexture`、`AllocParameters`、Pass 构建 |
| [SceneTexturesConfig.h](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Public/SceneTexturesConfig.h) | 公开的 `FSceneTextureUniformParameters` |
| [Common.ush](E:/ue/UE_5.8/Engine/Shaders/Private/Common.ush) | `ConvertFromDeviceZ`、`SvPositionToTranslatedWorld` |
| [SceneTexturesCommon.ush](E:/ue/UE_5.8/Engine/Shaders/Private/SceneTexturesCommon.ush) | `LookupDeviceZ` 与 `CalcSceneDepth` 的不同含义 |
| [SubstrateRead.ush](E:/ue/UE_5.8/Engine/Shaders/Private/Substrate/SubstrateRead.ush) | Substrate 的材质读取路径，按实际格式继续追踪 |
| [PostProcessBloomSetup.cpp](E:/ue/UE_5.8/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessBloomSetup.cpp) | 一个功能怎样绑定参数、创建输出，以及区分 PS/CS |

`SceneTextureParameters.h` 在本机位于 `Renderer/Private`，不是可随意依赖的公开头文件；`GetGBufferData` 等旧入口在本机 Shader 中也有弃用提示。不要为消除编译错误直接扩大私有依赖或手写旧通道解码。

<a id="lab04a"></a>
## 4. Lab04A：横向模糊、纵向模糊与合成

### A0. 先画资源图

```text
Original ──→ BlurX ──→ TempX ──→ BlurY ──→ TempY ──┐
    └───────────────────────────────────────────────┤
                                                   ↓
                                               Composite
                                                   ↓
                                             FinalOutput
```

| Pass | 读取 | 写入 | GPU 事件名 |
| --- | --- | --- | --- |
| 横向模糊 | Original | TempX | `RenderLab.BlurX` |
| 纵向模糊 | TempX | TempY | `RenderLab.BlurY` |
| 合成 | Original、TempY | FinalOutput | `RenderLab.BlurComposite` |

先记住三个约定：

- `TempX`、`TempY` 是当前 RDG 图内资源；不存入 Subsystem 或 View Extension 的成员。
- 每个模糊 Pass 的输入和输出是不同纹理，不同一子资源边读边写。
- `Inputs.OverrideOutput` 只留给最终写回阶段，中间纹理不能拿它反复覆盖。

### A1. 添加入口，不破坏灰度基线

在 `RenderLabGrayscaleRenderState.h` 中增量扩展纯数据状态，例如：

```cpp
#pragma once
#include "CoreMinimal.h"

enum class ERenderLabExperiment : uint8
{
    Grayscale,
    Blur
};

struct FRenderLabGrayscaleRenderState
{
    bool bEnabled = false;
    float Strength = 0.0f;
    ERenderLabExperiment Experiment = ERenderLabExperiment::Grayscale;
    float BlurMix = 1.0f;
};
```

在现有 Subsystem 的 `public` 部分增加声明：

```cpp
UFUNCTION(BlueprintCallable, Category = "RenderLab")
void SetBlurSettings(bool bEnabled = false, float Mix = 1.0f);
```

对应实现沿用 Lab03B 的线程模式：

```cpp
void URenderLabGrayscaleSubsystem::SetBlurSettings(bool bEnabled, float Mix)
{
    check(IsInGameThread());
    if (!bAcceptingSettings || !RenderState.IsValid())
    {
        return;
    }

    FRenderLabGrayscaleRenderState Snapshot;
    Snapshot.bEnabled = bEnabled;
    Snapshot.Experiment = ERenderLabExperiment::Blur;
    Snapshot.BlurMix = FMath::IsFinite(Mix)
        ? FMath::Clamp(Mix, 0.0f, 1.0f) : 0.0f;

    // Existing game-thread mirrors; Strength is meaningful only for grayscale.
    bGameEnabled = bEnabled;
    GameStrength = 0.0f;

    const TSharedRef<FRenderLabGrayscaleRenderState, ESPMode::ThreadSafe>
        State = RenderState.ToSharedRef();
    ENQUEUE_RENDER_COMMAND(UpdateRenderLabBlurSettings)(
        [State, Snapshot](FRHICommandListImmediate&)
        {
            check(IsInRenderingThread());
            *State = Snapshot;
        });
}
```

同时检查原 `SetGrayscaleSettings`：

1. 保留原入口、有限值检查与范围限制。
2. 将构造快照改成显式逐字段赋值，设置 `Experiment = Grayscale`，避免新增字段后仍依赖旧的位置初始化。
3. 不从 GT 读取 `*RenderState` 来生成下一份快照；RT 状态只在渲染线程读写。
4. `Deinitialize` 仍先阻止新设置，再排入关闭命令、释放引用。可令关闭命令赋值为默认状态，确保新增字段也被重置。

在 `RenderLabGrayscaleViewExtension.cpp` 中添加 `#include "RenderLabBlurShader.h"`，保留现有 Tonemap、启用、SceneCapture、Feature Level 检查，然后用模式选择替换原来的单一订阅语句：

```cpp
if (RenderState->Experiment == ERenderLabExperiment::Blur)
{
    const float MixSnapshot = RenderState->BlurMix;
    InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateStatic(
        &AddRenderLabBlurPasses, MixSnapshot));
}
else
{
    const float StrengthSnapshot = RenderState->Strength;
    InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateStatic(
        &AddRenderLabGrayscalePass, StrengthSnapshot));
}
```

这不是新增一个并行 View Extension，也不会自动叠加灰度和模糊；**同一个 World 当前只选择一个实验模式**。`SetBlurSettings(false, ...)` 会关闭这个共享实验入口，不表示“只关闭模糊但保留灰度”。

新头文件 `RenderLabBlurShader.h`：

```cpp
#pragma once
#include "ScreenPass.h"

class FRDGBuilder;
class FSceneView;
struct FPostProcessMaterialInputs;

FScreenPassTexture AddRenderLabBlurPasses(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    const FPostProcessMaterialInputs& Inputs,
    float Mix);
```

**编译顺序：**先建立声明、定义和 Shader 文件，再构建。不要在缺少 `AddRenderLabBlurPasses` 定义时启动。新增 UFUNCTION 和 Global Shader 类型后进行正常 Editor 构建并重开编辑器，不把 Live Coding 成功当作新类型注册成功。

### A2. 编写单方向模糊 Shader

新建 `Shaders/Private/RenderLabBlur.usf`：

```hlsl
#include "/Engine/Private/Common.ush"
#include "/Engine/Private/ScreenPass.ush"

Texture2D InputTexture;
SamplerState InputSampler;
SCREEN_PASS_TEXTURE_VIEWPORT(InputViewport)
float2 BlurDirection;

float4 MainPS(noperspective float4 UVAndScreenPos : TEXCOORD0) : SV_Target0
{
    const float Weights[5] = { 1.0, 4.0, 6.0, 4.0, 1.0 };
    float2 UV = UVAndScreenPos.xy;
    float2 StepUV = BlurDirection * InputViewport_ExtentInverse;
    float3 Sum = 0.0;

    [unroll]
    for (int i = -2; i <= 2; ++i)
    {
        float2 SampleUV = clamp(
            UV + float(i) * StepUV,
            InputViewport_UVViewportBilinearMin,
            InputViewport_UVViewportBilinearMax);
        Sum += InputTexture.SampleLevel(InputSampler, SampleUV, 0).rgb
            * Weights[i + 2];
    }

    float2 CenterUV = clamp(
        UV,
        InputViewport_UVViewportBilinearMin,
        InputViewport_UVViewportBilinearMax);
    float Alpha = InputTexture.SampleLevel(InputSampler, CenterUV, 0).a;
    return float4(Sum / 16.0, Alpha);
}
```

第一版用固定五点二项式核，作为可分离高斯近似。`BlurDirection=(1,0)` 是横向，`(0,1)` 是纵向；此时采样半径固定为两个输入纹素，不先设计可变核。

必须解释：

- 权重和是 16，归一化使纯色区域不因滤波整体变亮／变暗。
- 一纹素的 UV 距离取 **输入纹理 Extent 的倒数**，不是窗口大小或输出 ViewRect 大小的倒数。
- Sampler 的 Clamp 约束整张纹理；额外的 UV Clamp 约束本视图的有效区域，防止读到未写入区域。
- `RGB` 参与模糊，Alpha 保留中心值；最终合成仍从原图保留 Alpha。
- Shader 操作当前输入编码中的颜色值，不默认追加 Gamma 或曝光转换。

### A3. C++ 参数、注册与单 Pass 构建

在 `RenderLabBlurShader.cpp` 中先建立以下部分。文件还需接上 A4/A5 的函数，才能构成完整实现。

```cpp
#include "RenderLabBlurShader.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RenderGraphBuilder.h"
#include "RHIStaticStates.h"
#include "SceneView.h"
#include "ShaderParameterStruct.h"

class FRenderLabBlurPS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FRenderLabBlurPS);
    SHADER_USE_PARAMETER_STRUCT(FRenderLabBlurPS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
        SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
        SHADER_PARAMETER_STRUCT(
            FScreenPassTextureViewportParameters, InputViewport)
        SHADER_PARAMETER(FVector2f, BlurDirection)
        RENDER_TARGET_BINDING_SLOTS()
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(
        const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

IMPLEMENT_GLOBAL_SHADER(
    FRenderLabBlurPS,
    "/Plugin/RenderLabRendering/Private/RenderLabBlur.usf",
    "MainPS", SF_Pixel);

static void AddRenderLabBlurDirectionPass(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    FScreenPassTexture Input,
    FScreenPassRenderTarget Output,
    FVector2f Direction,
    const TCHAR* PassName)
{
    check(Input.IsValid() && Output.IsValid());
    check(Input.Texture != Output.Texture);

    auto* Parameters = GraphBuilder.AllocParameters<FRenderLabBlurPS::FParameters>();
    Parameters->InputTexture = Input.Texture;
    Parameters->InputSampler =
        TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    Parameters->InputViewport = GetScreenPassTextureViewportParameters(
        FScreenPassTextureViewport(Input));
    Parameters->BlurDirection = Direction;
    Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();

    FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
    TShaderMapRef<FScreenPassVS> VertexShader(ShaderMap);
    TShaderMapRef<FRenderLabBlurPS> PixelShader(ShaderMap);

    AddDrawScreenPass(
        GraphBuilder, RDG_EVENT_NAME("%s", PassName), View,
        FScreenPassTextureViewport(Output),
        FScreenPassTextureViewport(Input),
        VertexShader, PixelShader, Parameters);
}
```

`SHADER_PARAMETER_STRUCT(..., InputViewport)` 与 HLSL 的 `SCREEN_PASS_TEXTURE_VIEWPORT(InputViewport)` 是一对；成员在 Shader 中表现为 `InputViewport_...`，不要写成 C++ 风格的点访问。

### A4. 第一个可运行检查点：只有横向模糊

在同一 `.cpp` 中暂时实现这个版本的公开入口：

```cpp
FScreenPassTexture AddRenderLabBlurPasses(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    const FPostProcessMaterialInputs& Inputs,
    float Mix)
{
    (void)Mix; // Composite is added at checkpoint A6.
    const FScreenPassTexture Input = FScreenPassTexture::CopyFromSlice(
        GraphBuilder, Inputs.GetInput(EPostProcessMaterialInput::SceneColor));
    check(Input.IsValid());

    FScreenPassRenderTarget Output = Inputs.OverrideOutput;
    if (!Output.IsValid())
    {
        Output = FScreenPassRenderTarget::CreateFromInput(
            GraphBuilder, Input, ERenderTargetLoadAction::ENoAction,
            TEXT("RenderLab.BlurOutput"));
    }

    AddRenderLabBlurDirectionPass(
        GraphBuilder, View, Input, Output, FVector2f(1.0f, 0.0f),
        TEXT("RenderLab.BlurX"));
    return Output;
}
```

此版本有意不使用 `Mix`，不要此时测试混合强度。新建输出完全覆盖当前 ViewRect，且采样被约束在有效区域，所以使用 `ENoAction`；有效矩形外的像素不作保证。若使用引擎提供的 `OverrideOutput`，保留其自身 LoadAction。

关卡蓝图：`BeginPlay → Get World Subsystem（RenderLabGrayscaleSubsystem）→ SetBlurSettings(true, 1)`。本阶段先保持固定设置，不加延时切换，不在 Tick 重建 Subsystem。

**验收：**

- [ ] 正常构建，编辑器可重开；旧灰度入口仍能独立运行。
- [ ] 竖直黑白边缘向左右扩散，水平边缘没有被横向核额外上下扩散。
- [ ] RenderDoc 中出现 `RenderLab.BlurX`，输入是当前场景颜色，输出是新纹理或实际 OverrideOutput。
- [ ] 图像不移位、无整屏黑边；纯色区域大体保持原值，允许格式量化误差。
- [ ] 关闭后没有本实验的模糊事件。

如果所有方向都明显模糊，先排除场景已有 DOF、动态模糊、时域 AA 等因素，再判断自己的核。不要同时改变多项设置。

### A5. 第二个检查点：增加纵向 Pass

只替换 A4 入口中“调用一次横向模糊”的部分：

```cpp
const FScreenPassRenderTarget TempX = FScreenPassRenderTarget::CreateFromInput(
    GraphBuilder, Input, ERenderTargetLoadAction::ENoAction,
    TEXT("RenderLab.BlurXTexture"));

AddRenderLabBlurDirectionPass(
    GraphBuilder, View, Input, TempX, FVector2f(1.0f, 0.0f),
    TEXT("RenderLab.BlurX"));
AddRenderLabBlurDirectionPass(
    GraphBuilder, View, TempX, Output, FVector2f(0.0f, 1.0f),
    TEXT("RenderLab.BlurY"));
```

原来的 Input/Output 创建和 `return Output` 保留。只有最后的纵向 Pass 写最终输出；`TempX` 始终是独立中间资源。

抓帧检查：

1. X 的输出资源，是否与 Y 的输入资源相同？
2. X 中间图是否只做横向滤波？Y 输出是否同时产生两个方向的滤波？
3. Input、TempX 与最终输出各自的 Extent、ViewRect、格式是什么？
4. 缩放窗口后，五点核仍以输入纹素为单位吗？

二维可分离核等于两个一维核的外积。核心 RGB 累加采样从直接 5×5 的 25 次变成两次 5 点的 10 次，但本教学 Shader 还单独读取中心 Alpha，并增加中间读写与 Pass 成本；不能仅凭采样数声称实际 GPU 时间一定更低。

### A6. 第三个检查点：合成原图与模糊图

新建 `RenderLabComposite.usf`：

```hlsl
#include "/Engine/Private/Common.ush"
#include "/Engine/Private/ScreenPass.ush"

Texture2D OriginalTexture;
Texture2D BlurredTexture;
SamplerState InputSampler;
SCREEN_PASS_TEXTURE_VIEWPORT(InputViewport)
float Mix;

float4 MainPS(noperspective float4 UVAndScreenPos : TEXCOORD0) : SV_Target0
{
    float2 UV = clamp(
        UVAndScreenPos.xy,
        InputViewport_UVViewportBilinearMin,
        InputViewport_UVViewportBilinearMax);
    float4 Original = OriginalTexture.SampleLevel(InputSampler, UV, 0);
    float3 Blurred = BlurredTexture.SampleLevel(InputSampler, UV, 0).rgb;
    return float4(lerp(Original.rgb, Blurred, saturate(Mix)), Original.a);
}
```

在同一 `.cpp` 中新增 `FRenderLabBlurCompositePS`，沿用 A3 的 Shader 类结构，但参数替换为：

```cpp
BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OriginalTexture)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BlurredTexture)
    SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
    SHADER_PARAMETER_STRUCT(
        FScreenPassTextureViewportParameters, InputViewport)
    SHADER_PARAMETER(float, Mix)
    RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()
```

注册使用 `RenderLabComposite.usf`、`MainPS`、`SF_Pixel`。保留自己的类名，不与 Lab03 或 BlurPS 重复。

增加一个局部 `AddRenderLabBlurCompositePass`，参数是 `GraphBuilder, View, Original, Blurred, Output, Mix`。按 A3 填写参数、获取该 Shader 并调用 `AddDrawScreenPass`，事件名为 `RenderLab.BlurComposite`。这里必须：

- 检查 `Original.Texture != Output.Texture`、`Blurred.Texture != Output.Texture`。
- 检查 `Original.Texture->Desc.Extent == Blurred.Texture->Desc.Extent`。
- 检查 `Original.ViewRect == Blurred.ViewRect`。
- `InputViewport` 和 Draw 的输入 Viewport 都来自 `Original`。
- 输出 Viewport 来自 `Output`，不能因为它是最终输出就假设坐标起点为零。

同一 UV 采样两张输入，**只在这两张输入具有相同 Extent 和 ViewRect 时成立**。本阶段用 `CreateFromInput` 保持这项约定；半分辨率版本必须分别映射。

最终将公开入口的资源组织改成以下顺序，不能同时保留 A4/A5 的旧定义：

```text
Input = CopyFromSlice(SceneColor)
TempX = CreateFromInput(Input, ENoAction, "RenderLab.BlurXTexture")
TempY = CreateFromInput(Input, ENoAction, "RenderLab.BlurYTexture")
Output = Inputs.OverrideOutput
if Output 无效:
    Output = CreateFromInput(Input, ENoAction, "RenderLab.BlurOutput")

AddBlurDirection(Input, TempX, (1,0), "RenderLab.BlurX")
AddBlurDirection(TempX, TempY, (0,1), "RenderLab.BlurY")
AddComposite(Input, TempY, Output, Mix, "RenderLab.BlurComposite")
return Output
```

这是资源组织伪代码；上述局部函数的声明与参数由你按 A3/A6 补齐。先解释每项参数，再实现，不需要加入通用 Pass 管理器。

若观察到最终 Output 与某个输入相同，不删除检查。先用独立工作纹理计算，再通过合适的 ScreenPass 拷贝／写回步骤满足 OverrideOutput，单独抓帧确认没有读写反馈。

### A7. 参数与抓帧验收

第一版规定 `Mix=0` 时仍执行三个 Pass，输出原图；`Enabled=false` 时不订阅模糊回调。这样保留 Lab03 学过的“零强度”和“关闭”区别，之后优化再改变规格。

| 状态 | 预期事件 | 预期画面 |
| --- | --- | --- |
| 关闭 | 无 `RenderLab.Blur*` | 原图 |
| 开启，Mix=0 | X、Y、Composite | 原图 |
| 开启，Mix=0.5 | X、Y、Composite | 原图与模糊混合 |
| 开启，Mix=1 | X、Y、Composite | 两方向模糊 |

记录每个 Pass 的读写资源、格式、Extent、Mip/Slice、Viewport、Scissor 和参数。不能仅按纹理名字认定依赖；同名资源可能是不同对象，RDG 的物理内存也可能复用。

对纯色区、黑白边缘各选一个像素，手算五点权重与合成；允许输出格式的量化误差。当前输入可能是 `R10G10B10A2_UNORM`，两个中间 Pass 都会发生存储量化，不能按无限精度要求逐位相等。

解释两个 RDG 现象：

- 没有被最终输出消费的中间 Pass 可能被裁剪。不要用 `NeverCull` 掩盖断开的资源依赖。
- `AddDrawScreenPass` 是图构建，不是立刻执行 GPU 绘制；正常依赖下不需要自己插入 Flush 或手写 RHI Barrier。

**A 完成条件：**四状态符合约定，资源图可解释，边缘／窗口回归通过，灰度基线仍有效。到此再进入 B。

<a id="lab04b"></a>
## 5. Lab04B：深度、法线与位置重建

### B0. 先制定输入合同

新增 `RenderLabInputDebugShader.h/.cpp` 与 `RenderLabInputDebug.usf`。扩展现有模式选择，先增加一个输入调试模式，不让模糊同时参与。

按顺序提供 `DeviceZ`、`LinearDepth`、`Position`、`Normal` 四种调试输出。这里是建议命名，不是项目已有的枚举或蓝图接口。

| 输入 | 必须明确的合同 |
| --- | --- |
| 场景颜色 | 所处阶段、编码、Extent、ViewRect |
| 深度 | 实际资源、DeviceZ／线性深度、远平面／背景值、单位 |
| 法线 | 几何／着色法线、坐标空间、编码、归一化与有效性 |
| 位置 | View／Translated World 空间、矩阵、坐标范围 |
| 像素坐标 | 输出 ViewportUV、深度纹理坐标、有效矩形与抖动 |

不要把 `.uproject` 使用 UE5.8、Shader 能编译或指针非空当作“场景资源正确”的证明。引擎可能绑定占位纹理，必须检查尺寸和内容。

### B1. 从公开参数入口读取深度

当前 Tonemap 回调里的 `FPostProcessMaterialInputs` 含 `SceneTextures`。本机公开头文件可提供场景纹理 Uniform Buffer；可以从中取出 RDG 深度资源，再显式声明为本 Pass 的输入：

```cpp
// Inside the current render-graph callback; not persistent state.
if (!Inputs.SceneTextures.SceneTextures)
{
    return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
}
const FSceneTextureUniformParameters* SceneParameters =
    Inputs.SceneTextures.SceneTextures->GetContents();
FRDGTextureRef DepthTexture =
    SceneParameters ? SceneParameters->SceneDepthTexture : nullptr;
if (!DepthTexture)
{
    return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
}
```

需要包含 `SceneTexturesConfig.h` 和 RDG 相关公开头文件。具体 Pass 参数示意：

```cpp
SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
```

填写 `Parameters->View = View.ViewUniformBuffer`、`Parameters->SceneDepthTexture = DepthTexture`；RDG 才能看到本 Pass 的深度读取。不要只把指针藏进 lambda，也不要在 GT 查询这些资源。

此入口仅针对本文桌面路径；如果 Uniform Buffer 无效或内容不符合预期，保持原图并记录不支持条件，不强行走 Mobile 参数，也不把 `FSceneView` 强转成私有 `FViewInfo`。

### B2. 先校准坐标，再读取数值

初始验证使用固定相机和 100% 屏幕百分比。记录现有 AA；为隔离重建问题可在独立实验中暂时关闭时域 AA，但必须记录并恢复，不把它写成整个项目的永久前提。

先输出 UV 渐变／棋盘，确认上下左右和有效区域。不能认为 Tonemap 后颜色的像素坐标天然等于原始深度坐标。

坐标关系示意：

```text
OutputPixel
  → OutputViewportUV
  → DepthViewRect.Min + OutputViewportUV * DepthViewRect.Size
  → 深度纹理中的像素位置与纹理 UV
```

深度 ViewRect 要与产生深度的视图和 View Uniform 一致，不能仅用深度 Extent 推断。`View.ViewRectMin`、`View.ViewSizeAndInvSize`、`View.BufferSizeAndInvSize` 是核对入口；它们在当前回调中的实际值仍需验证。C++ 的公开 `FSceneView` 不能被假定具有私有 `FViewInfo::ViewRect`。

使用 `Load` 或 Point 采样读取第一版深度。若落到整数深度像素 `(x,y)`，重建用该像素的中心 `(x+0.5,y+0.5)`，不要把一个像素的深度和另一个连续位置的射线混在一起。

暂不支持的缩放／抖动条件应显示未验证状态，不能悄悄按窗口大小凑 UV。

### B3. DeviceZ 与线性深度

1. 用原始 `DeviceZ` 显示，分别观察近物体、远物体和天空。
2. 查本机 `ConvertFromDeviceZ`，用 View Uniform 转成线性深度。
3. 显示用 `saturate(LinearDepth / DisplayRange)`，但**只在显示时归一化**，算法输入仍保留实际单位。
4. 测试已知距离的平面。明确该深度沿投影／视线方向的定义，不把它自动当成到相机的欧氏距离。
5. 确认当前路径的 reversed-Z 约定和背景值；不要使用网上示例的固定 OpenGL 深度公式。

背景首先生成有效性判断。不要把远平面值直接反演成巨大位置，再让它参与 AO 和滤波。

### B4. 重建位置

查 `Common.ush` 的 `SvPositionToTranslatedWorld`。其输入需要匹配矩阵约定的**像素位置和 DeviceZ**；不能把线性深度塞入要求 DeviceZ 的 `.z`。

验证顺序：

1. 输出重建位置的分量或世界单位网格，不只输出看起来漂亮的颜色。
2. 对轴对齐平面，沿平面移动像素时对应固定坐标应保持稳定。
3. 将重建位置用匹配的投影重新投影，应回到原深度像素中心附近，并能重现原始深度。
4. 旋转／平移相机，检查平面、角点、球体轮廓。
5. 在局部原点附近通过后，移动场景／相机检查大坐标敏感性。

优先在 View 或 Translated World 空间做局部计算，避免把大世界绝对坐标强转为普通 float 后求很小的差。调试用 `frac(Position / GridSize)` 只能辅助观察，不能替代重投影误差。

建议记录静态、不透明、非边缘区域的重投影误差；先以小于约半个深度像素为诊断目标，实际容差和量化来源写入记录。

### B5. 法线：先明确来源

当前项目开启 Substrate，不能假定 `GBufferA.rgb * 2 - 1` 就是最终表面法线。

第一条可控路径是**从深度重建几何法线**：

1. 重建中心及左右上下相邻的有效位置。
2. 选择深度连续的一侧构造横／纵切向量，避免把前景与背景跨边界相减。
3. 叉乘并归一化，处理接近零长度的退化情况。
4. 用已知平面和球体确认朝向；必要时按视线统一朝向，记录其只是可见表面的约定。
5. 以 `0.5 * N + 0.5` 显示，同时明确 N 位于哪个空间。

这种结果是几何法线，不含法线贴图细节，也不能被记录为“已读取 Substrate 着色法线”。它可以作为 C 的明确限定输入。

第二条路径是按当前 Substrate GBuffer 格式取得**着色法线**：阅读项目路线 R16 和本机读取辅助代码，再确认所需 Shader 宏、材质数据和绑定是否能从当前公开扩展入口提供。若依赖尚不可用，记录限制，先使用验证过的深度法线；不直接扩大 Renderer 私有依赖或关闭 Substrate 规避问题。

验证时：世界空间法线不应仅因相机旋转而改变，视图空间法线会改变。转动模型，两者都应按各自坐标系变化。不同法线源对边缘和法线贴图的差异要写清。

### B6. 屏幕百分比与时域设置

基本输入验证后，再逐项恢复正常 AA，比较 100% 与 75% 屏幕百分比：

- 记录颜色与深度的实际尺寸，不把 `75` 直接当作所有纹理的尺寸比例。
- 对照颜色轮廓与深度／法线轮廓。时域上采样后的颜色与当前帧低分辨率深度不一定逐像素一致。
- 重新检查深度像素到视口、投影矩阵和抖动约定。
- 必要时让 AO 在深度原生网格计算，再明确映射到展示输出，而不是随意放大深度。

**B 完成条件：**输入合同、背景、空间、重投影、法线来源、缩放支持范围均已记录。只看到一张灰度深度图，不等于 B 完成。

<a id="lab04c"></a>
## 6. Lab04C：最小屏幕空间 AO

### C1. 明确算法与输出语义

第一版采用可解释的半球采样 SSAO 教学基线，不直接移植 GTAO。

新增 `RenderLabAOShader.h/.cpp` 和 `RenderLabAO.usf`，通过现有实验宿主选择 AO。沿用 GT/RT 快照机制传递参数，不增加每帧同步等待。

统一语义：

```text
AO = 1：本算法未检测到遮蔽，显示白色
AO = 0：遮蔽强，显示黑色
```

最小输入为验证过的深度、法线、View 参数、采样核。最小参数：

| 参数 | 初始实验值／含义 |
| --- | --- |
| `Radius` | 例如 50，单位是当前工程世界单位，默认厘米；不是屏幕像素 |
| `Bias` | 例如 1，抑制自遮蔽，与 Radius 使用同一距离单位 |
| `SampleCount` | 先 8，再 16／32；范围检查且不超过核容量 |
| `Intensity` | 第一轮固定 1，先不要靠强度掩盖错误 |
| `DebugMode` | 原始 AO、有效样本比例、半径／法线诊断 |

数值只是起点；物体尺寸不同需要调整。`Radius <= 0` 应走明确的无遮蔽行为，不能让权重计算出现除零。有限值与参数限制在 GT 完成，Shader 中仍保留必要保护。

### C2. 先准备确定性的采样

1. 构造固定种子的半球样本，局部 Z 非负；样本分布与权重约定写入记录。
2. 将法线方向与半球 Z 对齐，构造稳定切线基；参考轴接近平行时切换参考轴。
3. 样本半径可以更多分布在中心附近，但记录这种分布，不能把它称作无偏真实 AO。
4. 第一版固定空间噪声旋转，不逐帧更换随机种子；否则后续静态对照会混入时间变化。
5. 可用有固定上限的 Shader 参数数组，暂不为少量样本引入独立上传／Readback 系统。

高采样对照必须沿用同一算法、Radius、Bias、分布和输入，不能同时改一组设置。

### C3. 单像素算法

下面是算法伪代码，不是可直接粘贴的 UE Shader。投影／反投影沿用 B 阶段已验证的实现：

```text
若中心为背景：AO = 1
P = 重建的中心位置
N = 同一空间的法线
TBN = 以 N 为半球方向的基
OcclusionSum = 0

对每个半球样本 K：
    Q = P + Radius * (TBN 变换 K)
    Project(Q) → 深度纹理坐标
    若 Q 在相机后、超出有效视图、或采样到背景：
        本样本贡献 0；不把坐标 clamp 到屏幕边缘
        继续
    S = 在投影位置读深度并重建的表面位置
    Zq = Q 的正向线性视图深度
    Zs = S 的正向线性视图深度
    若 Zs < Zq - Bias：
        OcclusionSum += 距离衰减权重(P, S, Radius)

AO = saturate(1 - OcclusionSum / SampleCount)
```

这里的比较以**沿相机前向递增的线性深度**为前提。UE 实际矩阵、轴向和深度表示必须由 B 确定，不能将这条不等式直接用于 DeviceZ 或随意负号翻转。

本版将屏幕外／未知样本当作未检测到遮蔽，并仍除以总样本数，所以屏幕边缘可能偏亮。另输出有效样本比例，让这项限制可观察；不要偷偷改成有效样本归一化又沿用旧对照。

距离权重应抑制很远的表面造成虚假遮蔽，例如使用与 `Radius` 同单位的平滑距离窗。先画出权重曲线，确认中心、边界和超范围的值。

### C4. RDG 与输出

第一版在当前帧构建 RawAO，再通过单独的显示 Pass 输出 `float4(AO, AO, AO, 1)`，并按约定写 OverrideOutput。

建议 RawAO 使用支持渲染和采样的单通道浮点格式，如 `PF_R16F`；按目标 RHI 验证支持。**不要沿用颜色输出的 `R10G10B10A2_UNORM` 自动保存位置／法线／深度。**

```text
SceneDepth + Normal + View + Kernel
                  ↓
             RenderLab.AO
                  ↓
                RawAO
                  ↓
        RenderLab.AOVisualize
                  ↓
          后处理链的最终输出
```

如 B 使用深度重建法线，先复用正确结果／算法；是否增加专用法线中间纹理由实际重复计算决定，不一开始缓存所有缓冲。

Tonemap 后回调可作为当前独立实验的宿主，但不意味着这是正式 AO 参与光照的插入位置。换接入点时必须重新验证资源和坐标，不把 Lab03 的颜色接入结论直接套到所有算法。

### C5. 验证顺序

| 场景／操作 | 预测 | 重点排错 |
| --- | --- | --- |
| 单独平面 | 内部接近白，不能普遍自遮蔽 | Bias、法线方向、重建和比较符号 |
| 球接触地面 | 接触附近更暗 | Radius 单位与取样方向 |
| 两墙凹角 | 凹角附近遮蔽增强 | 半球朝向、坐标空间 |
| 物体离地 | 接触暗区随距离减弱 | 距离窗是否真正生效 |
| 天空与几何边缘 | 天空保持白，边缘异常可解释 | 背景、越界、深度不连续 |
| 相机远近与旋转 | 世界半径约定可解释，无整屏反转 | 投影、法线空间、像素／世界半径混用 |

先用极少样本，能解释一个像素再加样本。RenderDoc 检查 RawAO，不以 UE 原有 AO/Lumen 导致的最终暗部证明你的算法有效。

**C 完成条件：**至少上述基础几何验证通过，8/16/32 样本对照、参数单位、屏幕外限制、插入位置和输出语义有记录。噪声大但行为正确时才进入 D；滤波不能修复错误的坐标或比较符号。

<a id="lab04d"></a>
## 7. Lab04D：深度／法线引导滤波

### D1. 先做可对照的错误基线

在同一 RawAO 上先运行普通空间模糊，观察前景／背景边缘的遮蔽串色。复用 A 的 Pass 组织经验，但 AO 单通道格式、背景和有效区域独立处理。

然后新增 `RenderLabAODenoiseShader.h/.cpp` 与 `.usf`。第一版使用小型 5×5 **joint bilateral（联合双边）滤波**，不要默认把它称为严格的 “Guided Filter” 线性模型算法；本文“引导滤波”是用深度／法线引导边缘保持的上位描述。

### D2. 写出权重再编码

对中心 p 和邻域 q：

```text
ws = exp(-pixelDistanceSquared(p,q) / (2 * SigmaSpatial^2))
wz = exp(-(LinearDepth(p)-LinearDepth(q))^2 / (2 * SigmaDepth^2))
wn = pow(max(dot(Np,Nq), 0), NormalPower)   // 第二轮才加入
w  = ws * wz * wn
FilteredAO(p) = sum(w * RawAO(q)) / max(sum(w), Epsilon)
```

- 第一轮 `wn=1`，只验证深度引导；第二轮再加入法线。
- `SigmaSpatial` 是 AO 网格的像素单位；`SigmaDepth` 是线性深度的距离单位。
- 两个 Sigma 必须大于零且有限，`NormalPower` 必须有限且不小于零；零半径／关闭滤波走显式直通路径，不让参数进入除零计算。
- N 必须同空间且归一化。几何法线与着色法线可产生不同滤波行为，比较时注明。
- 中心是背景则输出 1；邻域背景、无效位置或越界像素不参与。
- 第一版用 Point 读取 RawAO、深度和法线，避免尚未计算引导权重就先双线性混合两个表面。
- 含中心样本，检查权重和与 NaN；不要用随意增大 Epsilon 隐藏输入问题。

空间高斯是可分离的，但加入依赖场景的深度／法线权重后，横纵两遍**不再自动等于完整二维联合双边核**。先用二维小核建立基线，再把分离近似当作明确优化对照。

### D3. RDG 组织

```text
Depth / Normal ───────────┐
                         ↓
AO Pass → RawAO → GuidedFilter → FilteredAO
            └───────────┬───────────┘
                        ↓
             对照显示／选择显示
                        ↓
                    FinalOutput
```

滤波输出必须是另一张纹理。算法工作纹理没有意义上的颜色 Gamma；显示 Shader 将它转换成可见灰度，不要因显示对比不同而篡改原始数据。

对照模式至少包括：

1. 低采样 RawAO。
2. 同一低采样 RawAO + 普通模糊。
3. 同一低采样 RawAO + 深度引导。
4. 同一低采样 RawAO + 深度／法线引导。
5. 同算法高采样参考，例如 64 样本，仍明确它不是 Ground Truth。

### D4. 验收

- [ ] 平坦区域的噪声降低。
- [ ] 前景／背景边缘的串色比普通模糊减少。
- [ ] 凹角和接触阴影没有被无解释地抹掉。
- [ ] 逐项调整空间、深度、法线权重，能解释现象。
- [ ] 相机缓慢移动时记录残余闪烁；没有时域算法就不宣称时域稳定。
- [ ] 确认 RawAO 没被滤波 Pass 原地覆盖。
- [ ] 记录代价与质量取舍，不用“画面更干净”替代性能检查。

若 AO 仍不可靠，按全年缩减规则先交付已验证的输入调试和引导滤波实验，明确 AO 未完成，不把滤波后的视觉改善当作算法通过。

<a id="lab04e"></a>
## 8. Lab04E：V1 验证与交付

### E1. 三个固定测试场景

| 场景 | 主要用途 |
| --- | --- |
| 简单几何：平面、球、凹角 | 算法方向、距离和接触验证 |
| 薄物体、前后景交叉、屏幕边缘 | 遮蔽缺失、深度不连续、滤波串色 |
| 较密集静态几何与相机路径 | 性能、运动变化和稳定性 |

固定相机位置／运动路径、曝光、光照和实际生效的渲染设置。边界场景不是只选“好看”的角度。

### E2. 性能记录

RenderDoc 用于确认事件和资源，最终耗时在正常运行中用 GPU 分析工具／Unreal Insights 重复测量，不把捕获回放的单次时间当作稳态性能结论。

最小测量方案：

1. 固定供电、设备性能模式、分辨率、画质、AA、屏幕百分比和帧率／VSync 条件。
2. 等 Shader 编译、资源加载和温度／频率稳定，说明预热条件。
3. 每种方案至少三次独立测量窗口，记录时长、样本数、平均值和波动。
4. 分别记录 AO、滤波、显示／合成、必要输入准备和总增量成本。
5. 高采样参考单独测，报告正式运行成本时不能偷偷包含或排除某些 Pass。

| 方案 | 采样数 | AO ms | 输入准备 ms | 滤波 ms | 显示／合成 ms | 总增量 ms | 质量观察 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 关闭 | — | — | — | — | — | 基线 | 原图 |
| RawAO | 待填 | 待填 | 待填 | — | 待填 | 待填 | 待填 |
| RawAO + 引导滤波 | 待填 | 待填 | 待填 | 待填 | 待填 | 待填 | 待填 |
| 高采样参考 | 待填 | 待填 | 待填 | 按实际 | 待填 | 待填 | 待填 |

纹理内存可以按 Extent×格式字节数估算逻辑大小，但不能把所有 RDG 临时纹理简单相加就称为 GPU 峰值显存；资源生命周期与别名复用会影响实际占用。

### E3. 回归矩阵

- [ ] 连续进入／退出 PIE 至少三次，无重复扩展与退出异常。
- [ ] 关闭、零强度、正常强度、模式切换符合各自约定。
- [ ] 非有限／越界参数按接口约定处理。
- [ ] `1280×720`、`1279×719`、近方形及窗口变化无错误区域。
- [ ] 100%／75% 屏幕百分比与 AA 组合有明确的通过／未验证状态。
- [ ] 切入不调用接口的新地图后默认关闭，旧 World 设置不泄漏。
- [ ] Lab03 灰度仍可单独运行。
- [ ] SceneCapture／非目标 World 不被无意影响。
- [ ] Shader／RDG 验证无错误，没有用每帧 Flush 掩盖资源问题。

### E4. 打包与交付

Lab03D 已完成的是旧功能基线，新增 Shader 和新地图后仍需打包回归：

1. 检查 Runtime 插件依赖不包含 MCP／Editor 模块。
2. 明确新实验地图的 Cook 包含方式，不无意改掉原默认地图。
3. 正常构建 Win64 Development，执行 Cook、Stage、Package。
4. 在打包程序中显式加载实验地图，验证新 Shader、模式切换和窗口变化。
5. 检查无 Shader 缺失、资产路径错误和编辑器专用依赖。

V1 最小交付：

- 可恢复的源码提交、必要资产与构建／运行说明。
- Pass 依赖图、抓帧资源表与至少一处数值验证。
- RawAO／滤波／高采样参考的固定条件对照。
- 三场景测试、一份排错案例、性能原始数据位置与摘要。
- Development 运行记录，以及约两分钟演示视频。
- 已支持／未验证／已知失败条件，不能只写“UE5.8 支持”。

完整 `.rdc`、Binaries、Intermediate、DDC、临时符号输出不作为普通源码提交；必要截图和报告可以提交。提交前检查既有未提交改动，不把不相关文件混入。

<a id="troubleshooting"></a>
## 9. 可选加深与排错

### 9.1 先完成 V1，再选择一个扩展

**Compute 对照：**只把一个已验证 Pass 改成 CS。检查纹理 UAV 创建标志、SRV/UAV 参数、线程组上取整、线程范围保护及 ViewRect.Min 偏移；非整齐尺寸必须通过。先在普通计算队列路径验证，不顺带启用 Async Compute。

**半分辨率：**重新定义每张输入的 Extent、ViewRect 和采样映射，添加深度／法线引导上采样。不能只把 CreateTexture 尺寸除以二。

**时域：**历史纹理需要外部资源所有权及导入／提取，加入重投影、拒绝历史、相机切换和分辨率变化重置。不能跨帧保存 `FRDGTextureRef`。

第一轮半分辨率与时域至多选一个；Async Compute、Readback 按实际问题展开，不作为 AO 起步前置。

### 9.2 症状到检查位置

| 症状 | 优先检查 |
| --- | --- |
| 找不到 Shader 类型／入口 | 模块加载阶段、虚拟路径、注册类名、USF 入口、是否正常重启构建 |
| 所有 Pass 都没有 | World、启用与模式、订阅阶段、是否被裁剪 |
| Y 只有纵向模糊 | Y 是否错误读取 Original 而不是 TempX |
| Mix=0 仍有三个事件 | 符合本轮规格；零混合不等于关闭 |
| 边缘有黑线／旧内容 | ViewRect Clamp、未写区域、LoadAction 与有效像素中心 |
| 输出黑屏／没有接回 | 最终 OverrideOutput、返回纹理、读写同一资源 |
| 深度几乎全黑／全白 | DeviceZ 与线性深度混淆、显示尺度、占位纹理 |
| 重建位置随相机异常漂移 | 投影矩阵、像素中心、抖动、深度表示与 PreViewTranslation |
| 法线随相机变化“不符合预期” | 先确认法线空间，再判断错误 |
| 平面全黑、天空参与遮蔽 | 深度比较符号、背景、法线朝向、Bias |
| 物体隔很远仍产生暗边 | 缺少距离窗、半径单位错误 |
| 滤波跨边缘串色 | 深度采样、法线空间、SigmaDepth、边界有效性 |
| 静止有噪声，移动闪烁 | 采样分布／种子和无历史状态的限制，不立即堆时域 |
| 调试图正确，合成不正确 | 多输入坐标映射、颜色编码、额外后处理 |
| 编辑器正常，包里无效果 | 地图 Cook、Shader Cook、Runtime 加载与运行入口 |

<a id="records"></a>
## 10. 实验记录与第一天任务

### 10.1 实践时建立的记录

在 `Docs/Experiments/Lab04_Results.md` 中逐步填写，尚未完成的项目明确标注，不能直接复制本文的预期作为实际结果：

```markdown
# Lab04 实验记录

## 环境与复现
日期、UE/CL、Git、GPU/驱动、RHI、地图、相机、分辨率、
AA、屏幕百分比、Substrate、曝光、运行入口、打包路径。

## A：多 Pass
X/Y/Composite 的资源表、ViewRect、格式、四状态结果、数值验证。

## B：输入合同
深度来源和表示、法线来源和空间、重建方法、背景、
重投影误差、缩放／抖动限制。

## C：AO
算法和输入空间、半径单位、Bias、采样核／种子、
屏幕外策略、简单几何预测与实测。

## D：滤波
权重、单位、参数、普通模糊对照、引导滤波与参考、残余问题。

## E：验证与交付
三场景、性能条件与原始数据、回归表、打包与演示。

## 失败案例
现象 → 假设 → 只改一个条件 → 证据 → 结论。

## 状态
已验证／部分验证／未验证；下一次只推进的一件事。
```

### 10.2 不看代码时的复述题

1. 为什么模糊算法很短，却需要明确三个 Pass 的资源和输出约定？
2. 为什么使用了 Clamp Sampler，仍可能读到当前视图外的像素？
3. 为什么输出尺寸相同不等于两个资源的坐标完全一致？
4. 为什么不把临时 RDG 纹理留给下一帧使用？
5. 为什么深度必须先明确表示和单位，才能比较遮蔽？
6. 几何法线和着色法线的不同会怎样影响 AO 与滤波？
7. 为什么普通高斯可以分离，而深度引导滤波不能无条件这样推导？
8. 为什么后处理颜色乘 AO 不等于正式参与引擎光照？
9. 为什么采样更少或 Pass 更少不能单独证明性能更好？
10. 怎样让另一个人复现你宣称已经通过的验证？

### 10.3 现在只做这一件事

**第一轮停在 A4：横向模糊。**

1. 阅读 A0，自己画一次输入输出图。
2. 检查现有状态和入口，完成 A1 的增量修改。
3. 新建 Blur Shader 的三个文件，完成 A2/A3/A4。
4. 正常构建并重开编辑器，在独立关卡调用 `SetBlurSettings(true, 1)`。
5. 观察竖直条纹并抓一帧，确认 `RenderLab.BlurX`。
6. 写下输入／输出、尺寸／格式、方向参数和一个失败／成功现象。

横向 Pass 通过后再加纵向。**本指南写完不意味着任何 Lab04 功能已实现、编译、运行或验收。**
