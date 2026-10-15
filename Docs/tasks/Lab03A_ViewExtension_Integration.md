# Lab03A 实操：把已有 Shader 接入 PIE

日期：2026-09-23

适用环境：RenderLab / UE 5.8.0 CL 55116800 / Windows / DX12。

文档定位（2026-09-30 更新）：这是 [Lab03 主指南](Lab03_GlobalShader_Workbench.md) 的历史接入步骤，保留 2026-09-23 的起点和示例用于复盘。当前 A/B 核心功能已跑通，进度见[主指南检查点](Lab03_GlobalShader_Workbench.md#progress)，下一步为 [Lab03C](Lab03_GlobalShader_Workbench.md#lab03c)。

下面的三参数 Pass 签名与固定启用设置属于 A 阶段；现有 Lab03B 已加入共享渲染状态和强度附加参数。不要用本文代码覆盖当前实现，也不需要重新创建文件。本文最初只提供指导，没有自动创建实现；本次同样仅更新文档说明。

## 1. 历史起点与本轮目标

2026-09-23 编写本文时检查确认：

- 插件的 Runtime 模块使用 `PostConfigInit`。
- Shader 虚拟路径映射已工作，`FRenderLabGrayscalePS` 此前已有成功编译记录。
- `AddRenderLabGrayscalePass` 已移到 Shader 类外，并在 Private 头文件声明。
- 最新 UBT 日志结果为 `Succeeded`，耗时 5.86 秒。
- `.usf` 当前只采样输入并原样输出，尚未计算灰度。

接下来要把函数接到实际帧中：

```text
进入 PIE
    -> 创建 PIE World 的 Subsystem
    -> Subsystem 创建并持有 World View Extension
    -> 该 World 的视图开始后处理
    -> 扩展向 Tonemap 后注册回调
    -> 引擎调用 AddRenderLabGrayscalePass
    -> RDG 执行全屏绘制
    -> 输出交回后处理链
```

本轮是**固定启用的接入实验**，不做强度、动态开关或跨线程参数更新。每个符合条件的 Game/PIE World 都会建立一个扩展，不是只对白名单地图生效；普通 Editor World 不参与。主指南的“默认关闭、显式启用”规格在 Lab03B 接入控制接口时落实。

第一轮只测试普通 Lit、单玩家、单视口、非 VR。代码排除 Scene Capture、Reflection Capture 和不满足 Shader 编译条件的平台；这不是其他视图模式的兼容性保证。

## 2. 改哪些文件

先保存需要保留的资产，正常关闭 Editor。首次加入反射类使用完整编译，不依赖 Live Coding。

插件源码目录：

`E:\ue_project\pratice\RenderLab\Plugins\RenderLabRendering\Source\RenderLabRendering`

| 操作 | 相对上述目录的文件 | 用途 |
| --- | --- | --- |
| 调整 | `RenderLabRendering.Build.cs` | 公共 Subsystem 头文件的模块依赖 |
| 新增 | `Private/RenderLabGrayscaleViewExtension.h` | 声明扩展 |
| 新增 | `Private/RenderLabGrayscaleViewExtension.cpp` | 筛选视图并订阅回调 |
| 新增 | `Public/RenderLabGrayscaleSubsystem.h` | 声明 World Subsystem 与持有的扩展 |
| 新增 | `Private/RenderLabGrayscaleSubsystem.cpp` | 创建、持有和释放扩展 |

已有的 Shader `.h/.cpp` 和 `.usf` 先保持不变。不修改游戏模块、Target.cs、`.uproject`、`.uplugin`、Config、MCP 或 Engine。所有新文件名、类名和 include 拼写按下面保持一致。

可以直接在 IDE 中创建普通头文件与 C++ 文件；不需要生成 Actor，也不需要创建 Blueprint。

## 3. 第一步：调整依赖的可见性

当前 `CoreUObject`、`Engine` 在插件的 Private 依赖中。接下来 Public 头文件会继承 `UWorldSubsystem`，把这两项**移动**到 Public 依赖，不要在两处重复保留。

将已有两个依赖数组调整成下面的内容，保留 Build.cs 的其他部分：

```csharp
PublicDependencyModuleNames.AddRange(
    new string[]
    {
        "Core",
        "CoreUObject",
        "Engine"
    });

PrivateDependencyModuleNames.AddRange(
    new string[]
    {
        "Slate",
        "SlateCore",
        "Projects",
        "RenderCore",
        "RHI",
        "Renderer"
    });
```

保留现有 Slate 项以避免本轮无关清理。渲染实现仍在 Private 目录，Subsystem 公共接口不暴露 `ScreenPass` 或 RDG 类型，所以不把所有渲染依赖都改成 Public。

## 4. 第二步：声明 View Extension

新建 `Private/RenderLabGrayscaleViewExtension.h`：

```cpp
#pragma once

#include "SceneViewExtension.h"

class UWorld;

class FRenderLabGrayscaleViewExtension final
    : public FWorldSceneViewExtension
{
public:
    FRenderLabGrayscaleViewExtension(
        const FAutoRegister& AutoRegister,
        UWorld* World);

    virtual void SubscribeToPostProcessingPass(
        EPostProcessingPass Pass,
        const FSceneView& View,
        FPostProcessingPassDelegateArray& InOutPassCallbacks,
        bool bIsPassEnabled) override;
};
```

这里是**普通 C++ 类**：

- 不写 `UCLASS()`，不写 `GENERATED_BODY()`，不用 `NewObject` 创建。
- 继承 `FWorldSceneViewExtension`，让引擎只在匹配 World 的视图中激活它。
- 保留基类的 `IsActiveThisFrame_Internal` 行为；本轮不用重新实现 World 判断。
- `FAutoRegister` 与 `FSceneViewExtensions::NewExtension` 配套，稍后会用到。
- UE5.8 的订阅方法有 `const FSceneView& View`，不要替换成旧教程中少一个参数的版本。

## 5. 第三步：订阅你的 Pass 函数

新建 `Private/RenderLabGrayscaleViewExtension.cpp`：

```cpp
#include "RenderLabGrayscaleViewExtension.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "RenderLabGrayscaleShader.h"
#include "SceneView.h"

FRenderLabGrayscaleViewExtension::FRenderLabGrayscaleViewExtension(
    const FAutoRegister& AutoRegister,
    UWorld* World)
    : FWorldSceneViewExtension(AutoRegister, World)
{
}

void FRenderLabGrayscaleViewExtension::SubscribeToPostProcessingPass(
    EPostProcessingPass Pass,
    const FSceneView& View,
    FPostProcessingPassDelegateArray& InOutPassCallbacks,
    bool bIsPassEnabled)
{
    if (Pass != EPostProcessingPass::Tonemap || !bIsPassEnabled)
    {
        return;
    }

    if (View.bIsSceneCapture || View.bIsReflectionCapture)
    {
        return;
    }

    if (!IsFeatureLevelSupported(
        View.GetShaderPlatform(), ERHIFeatureLevel::SM5))
    {
        return;
    }

    InOutPassCallbacks.Add(
        FPostProcessingPassDelegate::CreateStatic(
            &AddRenderLabGrayscalePass));
}
```

**最重要的是最后三行。**它们不是立刻调用绘制函数，而是将函数绑定到引擎提供的回调列表。引擎处理到对应位置时，才用本次 `GraphBuilder`、`View`、`Inputs` 调用它。

为什么能直接 `CreateStatic`？

你的独立函数签名与 Delegate 一致：

```cpp
FScreenPassTexture AddRenderLabGrayscalePass(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    const FPostProcessMaterialInputs& Inputs);
```

本轮没有渲染侧可变状态，不必为了转调这个函数额外创建成员回调。这个静态绑定也不捕获 Subsystem 或扩展的裸指针。到了 Lab03B 需要读取扩展的渲染参数时，再引入合适的成员回调与持有方式。

筛选 Feature Level 与 Shader 的 `ShouldCompilePermutation` 条件一致，避免在未编译这个 Shader 的平台上无条件获取它。

## 6. 第四步：声明 World Subsystem

新建 `Public/RenderLabGrayscaleSubsystem.h`：

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SharedPointer.h"
#include "RenderLabGrayscaleSubsystem.generated.h"

class FSceneViewExtensionBase;

UCLASS()
class RENDERLABRENDERING_API URenderLabGrayscaleSubsystem
    : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(
        FSubsystemCollectionBase& Collection) override;

    virtual void Deinitialize() override;

protected:
    virtual bool DoesSupportWorldType(
        const EWorldType::Type WorldType) const override;

private:
    TSharedPtr<FSceneViewExtensionBase, ESPMode::ThreadSafe> ViewExtension;
};
```

与上一步相比，这里才是 **UObject 反射类**：

- `.generated.h` 是 UHT 生成的，不手工创建；它必须是本头文件最后一个 include。
- `RENDERLABRENDERING_API` 对应当前模块名称。
- Subsystem 由引擎按照 World 生命周期创建，不往关卡拖一个 Subsystem，也不从模块 StartupModule 手动创建它。
- `ViewExtension` 是普通 C++ 对象的共享引用，不加 `UPROPERTY`。它不是 UObject，不由 GC 管理。
- Public 头文件只前置声明扩展基类，具体的渲染实现留在 Private。

## 7. 第五步：管理扩展的生命周期

新建 `Private/RenderLabGrayscaleSubsystem.cpp`：

```cpp
#include "RenderLabGrayscaleSubsystem.h"

#include "Engine/World.h"
#include "RenderLabGrayscaleViewExtension.h"

bool URenderLabGrayscaleSubsystem::DoesSupportWorldType(
    const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game
        || WorldType == EWorldType::PIE;
}

void URenderLabGrayscaleSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    ViewExtension =
        FSceneViewExtensions::NewExtension<
            FRenderLabGrayscaleViewExtension>(GetWorld());

    UE_LOG(LogTemp, Display,
        TEXT("[RenderLabRendering] Created view extension for %s"),
        *GetNameSafe(GetWorld()));
}

void URenderLabGrayscaleSubsystem::Deinitialize()
{
    UE_LOG(LogTemp, Display,
        TEXT("[RenderLabRendering] Releasing view extension for %s"),
        *GetNameSafe(GetWorld()));

    ViewExtension.Reset();
    Super::Deinitialize();
}
```

为什么用 Subsystem 持有？

引擎的扩展注册表保存弱引用。如果创建扩展之后只用一个函数内局部共享指针持有，离开作用域就可能失去有效实例。这里把强引用保留到对应 World 结束。

本机 `FSceneViewExtensions::NewExtension` 同时完成创建与注册，不再手动重复注册。

`Reset()` 只释放 Subsystem 持有的这一份引用，不代表立即清空所有在途渲染工作。已经构建的 View Family 可能仍持有扩展；本轮回调仅调用静态函数，没有访问被销毁的 Subsystem，也没有长期保存 RDG 资源。

这里不每帧 Flush，不 `delete` 扩展，不调用全局清理函数。本轮不支持运行中卸载 Shader 模块；需要改反射类或模块时使用正常的关闭、编译和重启流程。

## 8. 完整编译并启动

四个新文件和依赖调整全部完成后，Editor 保持关闭，运行：

```powershell
& 'E:\ue\UE_5.8\Engine\Build\BatchFiles\Build.bat' `
  RenderLabEditor Win64 Development `
  '-Project=E:\ue_project\pratice\RenderLab\RenderLab.uproject' `
  -WaitMutex
```

检查 UHT、C++ 编译和链接结果。如果 IDE 文件列表没有刷新，可以重新生成项目文件；不要把“IDE 尚未显示新文件”当成 UnrealBuildTool 一定看不到文件。

若有错误，先看第一条实际编译错误，不只看最后的退出码。常见原因：

| 报错或现象 | 优先检查 |
| --- | --- |
| 找不到 `.generated.h` | UCLASS 文件名、最后一个 include、UHT 是否运行 |
| 基类或依赖类型不可见 | include、Build.cs 的 CoreUObject/Engine 依赖 |
| Override 签名不匹配 | 是否漏掉 `const FSceneView&` 或 `const` |
| 未解析 `AddRenderLabGrayscalePass` | 函数仍在类内、定义加了 static/匿名命名空间、声明定义不一致 |
| 未解析 Renderer 接口 | 原有 `Renderer` Private 依赖是否保留 |

本轮代码按本机接口核对，尚未在你的插件中编译。以你完成文件后的 UBT/UHT 结果为准。

## 9. 实际验证：先直通，再证明它执行

### 9.1 验证对象创建与销毁

重新打开 Editor，进入一个普通 Lit 测试关卡：

1. 还没进入 PIE 时，不要求出现 `Created view extension`，因为普通 Editor World 被排除了。
2. 进入 PIE，搜索 `[RenderLabRendering] Created view extension for`。
3. 退出 PIE，搜索 `[RenderLabRendering] Releasing view extension for`。
4. 重复三次，确认每轮都出现对应记录，没有崩溃或异常残留。

暂时只开一个 PIE 客户端。若出现多个 World 的记录，先核对日志里的 World 名称和运行设置，不立刻认定是重复注册。

### 9.2 验证原图直通

保持当前 `.usf` 不变。预期画面不变，因为它只是采样并返回输入颜色。

但“看起来没变”不能证明 Shader 已执行。捕获 **PIE 游戏视口**的一帧，搜索 `RenderLab.Grayscale`；不要捕获普通 Editor 视口后期待它存在。

在事件中核对：

- 是否是一次全屏绘制。
- 输入与输出资源，Viewport/Scissor 与实际区域。
- Pixel Shader 是否对应当前实现。
- 是否还有输入切片转换带来的复制。

`Created view extension` 证明对象创建，不单独证明订阅、绘制或 GPU 执行。当前代码不打印逐帧日志，避免刷屏。

### 9.3 用红蓝交换做可见性验证

直通没有黑边或错位后，退出 PIE、正常关闭 Editor，只修改：

`E:\ue_project\pratice\RenderLab\Plugins\RenderLabRendering\Shaders\Private\RenderLabGrayscale.usf`

保留声明和函数签名，将 `MainPS` 的函数体改为：

```hlsl
float4 color = Texture2DSample(
    InputTexture, InputSampler, UVAndScreenPos.xy);
return float4(color.bgr, color.a);
```

重新打开 Editor，等待必要的 Shader 编译结束，再进入 PIE。C++ 没变时无需为了 `.usf` 修改而重新编译整个 C++ 模块；本轮用关闭/重启来简化 Shader 更新验证。

预期：

- 有明显红色与蓝色的测试物体会交换颜色。
- 普通 Editor 视口保持原样，PIE 视图改变。
- 灰色物体不适合证明 R/B 交换；它们原本通道相同。
- 保留 Alpha，不无条件写成 1。

测试时避免在同一视图后面又叠加 Lab02 灰度或其他强烈颜色处理。只在独立测试场景或副本中调整参与效果，保留已有实验资产。

验证后可以恢复直通，或再独立验证固定灰度：

```hlsl
float4 color = Texture2DSample(
    InputTexture, InputSampler, UVAndScreenPos.xy);
float gray = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));
return float4(gray, gray, gray, color.a);
```

这仍是输入编码上的灰度风格处理，不代表计算物理亮度。每次只保留一种函数体，并记录当前使用的是直通、通道交换还是灰度。

## 10. 阅读时要理解的四个边界

| 边界 | 对应代码 | 需要理解的事情 |
| --- | --- | --- |
| UObject 与普通 C++ 对象 | Subsystem 与 View Extension | 两者创建方式和生命周期管理不同 |
| World 与 View | World 匹配及视图筛选 | 属于同一 World，不代表所有 Capture/预览都要参与 |
| 注册回调与执行回调 | `InOutPassCallbacks.Add` 与实际 Pass 函数 | 订阅阶段不是 GPU 绘制 |
| 添加 RDG 工作与执行 GPU | `AddDrawScreenPass` 与抓帧中的 Draw | CPU 构图成功不等于画面验证通过 |

源码依据：

- [SceneViewExtension.h](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Public/SceneViewExtension.h)：Delegate 签名、订阅方法与 `NewExtension`。
- [SceneViewExtension.cpp](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Private/SceneViewExtension.cpp)：弱引用注册表与 World 匹配。
- [WorldSubsystem.h](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Public/Subsystems/WorldSubsystem.h)：World 生命周期及类型筛选接口。
- [WorldSubsystem.cpp](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Private/Subsystems/WorldSubsystem.cpp)：`ShouldCreateSubsystem` 如何调用 `DoesSupportWorldType`。
- [OpenColorIODisplayExtension.cpp](E:/ue/UE_5.8/Engine/Plugins/Compositing/OpenColorIO/Source/OpenColorIO/Private/OpenColorIODisplayExtension.cpp)：本机 Tonemap 后订阅示例。

本轮完成标准：

- [ ] 新增文件通过 UHT、C++ 编译与链接。
- [ ] PIE 开始/结束有对应的创建/释放记录。
- [ ] 普通 Editor 视口不参与效果。
- [ ] 原图直通没有黑边、拉伸或偏移。
- [ ] 在正确视口的 GPU 捕获中找到 `RenderLab.Grayscale`。
- [ ] 红蓝交换或固定灰度能证明 Shader 真正影响 PIE 画面。
- [ ] 能按顺序说明 Subsystem、View Extension、Pass 函数与 HLSL 的关系。

完成后进入 Lab03B，再增加 `Enabled/Strength`、参数快照与渲染侧状态。不要在本轮还没验证画面时同时加入线程参数更新。
