# Lab01A：最小材质实验台

日期：2026-09-12

目标环境：RenderLab / UE 5.8.0 / Windows。适合已学习 GAMES101、LearnOpenGL，熟悉 C++ 和 UE 基础 Gameplay 的学习者。

**状态：待开始。本文是操作与学习指导，不是实验完成记录。** 本次仅创建文档，没有创建材质、Actor、关卡或 C++ 实现，也没有编译运行文中的示意代码。

上级路线：[UE_Rendering_Learning_Roadmap.md](E:/ue_project/pratice/RenderLab/Docs/UE_Rendering_Learning_Roadmap.md)。

## 1. 这个任务要解决什么问题

核心问题：

> 一个 C++ Actor 修改粗糙度，为什么能够改变屏幕上的高光，却不需要重新编写或编译整份 Shader？

你已经理解 BRDF、纹理采样和图形 API，本次不重学这些基础。重点是把已有知识映射到 UE 的材质资产、参数继承、UObject 生命周期和渲染线程更新机制。

完成后应能解释：

1. Material、Material Instance Constant 和 Material Instance Dynamic 分别负责什么。
2. 父材质中的参数、材质实例覆盖值、运行时覆盖值之间的关系。
3. 两个对象共享父材质，为什么可以有不同外观；什么时候会意外一起变化。
4. Substrate 的简单表面表达如何承接熟悉的金属度工作流。
5. 材质参数更新如何从游戏线程送到渲染线程。
6. 如何用受控实验区分材质、光照、曝光和后处理造成的变化。

建议安排 3～4 次学习，每次 1.5～2.5 小时。第一次只做到材质实例，不必一次完成全部步骤。

## 2. 最终成果与范围

### 2.1 需要做出来

- 一个独立的实验关卡，不替换项目默认地图。
- 一个简单不透明 Substrate 父材质，暴露颜色、粗糙度、金属度和发光参数。
- 两个材质实例预设，展示电介质与金属参数组合。
- 一个可重复放置的 C++ Actor，每个 Actor 只管理一个球体和自己的 MID。
- 一个两行三列的展示阵列，以及一个可重复触发的运行时参数修改实验。
- 一份简短实验记录，包含截图、参数表、源码调用链和验收结果。

### 2.2 本次不做

不做溶解、贴图资产导入、复杂 UI、材质函数库、Custom HLSL、后处理描边、渲染插件、RDG、RT 效果或完整 GPU 性能报告。材质函数和贴图工作流留给后续任务。

不安装第三方依赖，不修改 Engine，也不为了匹配旧教程关闭 Substrate。

## 3. 先读哪些资料

以下 Epic 页面于 2026-09-12 直接读取并确认返回 UE 5.8 文档正文。本机源码路径也已核对存在。页面可访问不等于示例已在本项目运行；本文尚未验证实际编辑器菜单、资产或编译结果。

| 编号 | 参考资料 | 阅读范围与要回答的问题 |
| --- | --- | --- |
| R1，必读 | [Instanced Materials](https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-materials-in-unreal-engine) | 阅读继承、参数、MIC/MID 和 Static Parameters。哪些变化属于数据更新，哪些涉及 Shader 变体？ |
| R2，边做边查 | [Creating and Using Material Instances](https://dev.epicgames.com/documentation/en-us/unreal-engine/creating-and-using-material-instances-in-unreal-engine) | 参数节点、实例创建、覆盖勾选。学习实例工作流，不照搬旧版根节点截图。 |
| R3，必读局部 | [Substrate Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-substrate-materials-in-unreal-engine) | 阅读新工程、材质转换、Slab 与简单参数化。暂不钻研分层、SSS、Glints。 |
| R4，遇到显示问题先看 | [Auto Exposure](https://dev.epicgames.com/documentation/en-us/unreal-engine/auto-exposure-in-unreal-engine) | Manual、Apply Physical Camera Exposure、曝光补偿。为何同一材质可能在不同视图中亮度不同？ |
| R5，概念复习 | [Physically Based Materials](https://dev.epicgames.com/documentation/en-us/unreal-engine/physically-based-materials-in-unreal-engine) | Base Color、Roughness、Metallic、Specular 的含义。区分传统参数化和 Substrate 节点接口。 |
| R6，可选 | [Content Examples](https://dev.epicgames.com/documentation/en-us/unreal-engine/content-examples-sample-project-for-unreal-engine) | 参考材质实例展台的组织方式。确认下载版本，独立打开；本任务不要求下载或迁移整个示例工程。 |

**阅读节奏建议：**先用 30～45 分钟读 R1、R3 的相关部分，马上动手；遇到实例操作查 R2，遇到亮度异常查 R4。不要先连续看几小时资料，再凭记忆搭图。

## 4. 用已有图形知识建立 UE 心智模型

| UE 概念 | 可以怎样理解 | 不要产生的误解 |
| --- | --- | --- |
| Material | 表面表达图，以及编译所需的材质设置 | 不等于一份独立、固定的 Pixel Shader；实际还涉及平台、用途和渲染 Pass。 |
| MIC | 可保存的实例资产，继承父材质并覆盖参数 | “Constant”不代表所有值必须烘焙成编译期常量；资产内也可有普通标量和向量参数。 |
| MID | 运行时实例对象，供 Gameplay 更新动态参数 | 通常不是要保存进 Content 的资产，也不是为每个对象复制整份 Shader 源码。 |
| Scalar/Vector Parameter | 运行时可变化的材质数据，可类比 uniform 数据 | 调用 setter 不等于直接同步调用一次 Vulkan/D3D 参数绑定，也不表示 GPU 已立即消费。 |
| Static Switch Parameter | 影响编译选择的静态参数 | 不能通过 MID 的普通 scalar setter 把它当作运行时布尔值切换。 |
| Material Slot | Mesh Component 上的材质绑定位置 | 创建 MID 和让组件使用该 MID 是两个需要区分的概念；组件辅助 API 可以同时完成。 |

上述实例和编译参数概念参考 R1、R2；实际更新路径在第 9 节用本机源码核对。

本任务使用的继承关系：

```text
M_Lab01_Surface
    -> MI_Lab01_Dielectric
        -> Actor A 的 MID
        -> Actor B 的 MID
    -> MI_Lab01_Metal
        -> Actor C 的 MID
```

A、B 可以共享同一个 MIC 作为父级，但需要独立的 MID 才能在本任务中独立修改参数。共享父材质不自动保证合批，多个 MID 也不等于一定产生多个 Shader 变体；此时先理解所有权，不急着下性能结论。

## 5. 准备实验环境

### 5.1 版本与目录

初始基线提交为 `dda247a`。开始前在工程根目录检查：

```powershell
git status --short --branch
git log -1 --oneline
```

确认未提交内容属于谁、是否需要保留；不要为了得到干净状态直接回退或删除它们。可以在开始实施时建立 `lab/01-materials` 分支，若已存在则先检查，不重复创建。本次文档编写没有创建该分支或提交。

未来建议创建：

```text
Content/RenderLab/Lab01_Materials/
  Maps/L_Lab01_MaterialWorkbench.umap
  Materials/M_Lab01_Surface.uasset
  Materials/MI_Lab01_Dielectric.uasset
  Materials/MI_Lab01_Metal.uasset
  Blueprints/BP_MaterialLabSample.uasset        可选，只配置默认资产
Source/RenderLab/
  MaterialLabSample.h
  MaterialLabSample.cpp
Docs/Experiments/
  Lab01A_Results.md
  Images/Lab01A/
```

`Docs/tasks/` 保存任务指导；`Docs/Experiments/` 保存你实际执行后的证据和结论。上面的实验文件现在尚未创建。

### 5.2 场景与显示条件

1. 新建独立、简单的实验关卡，使用引擎自带基础球体和地面，不在 OpenWorld 模板上叠加本次实验。
2. 使用一盏可移动主光和稳定的环境光。可用 Sky Atmosphere 配合 Sky Light 建立环境；固定方向和强度，等环境捕获与 Shader 编译稳定后再比较。
3. 放置固定相机，记录位置、朝向、视场角。对比截图使用同一相机与视图模式；PIE 也要明确使用该相机，而不是另一台默认玩家相机。
4. 放置覆盖全关卡的 Post Process Volume，设置 Manual 曝光。入门实验建议关闭 Apply Physical Camera Exposure，选定曝光补偿值后固定，不同时使用第二套相机曝光覆盖。参考 R4。
5. 编辑器视口应使用游戏的曝光设置，避免视口独立曝光覆盖。显示名可能随界面语言变化，以 5.8 实际界面为准。
6. 记录 Local Exposure。当前项目的 Highlight/Shadow Contrast 默认值为 0.8；为减少本次变量，可在实验 Volume 使用 Bilateral 方法，把两者设为 1，并保持其他相关设置固定。不要只固定全局曝光而遗漏局部曝光。
7. 首轮关闭 Bloom 并保持其他后处理条件不变；发光专项实验时再单独对比 Bloom。不要把“发光表面”“光晕”和“照亮旁边物体”当成同一件事。

第 6 项的字段及含义已核对本机 [Scene.h](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Classes/Engine/Scene.h:2024)。本机 [项目配置](E:/ue_project/pratice/RenderLab/Config/DefaultEngine.ini) 已启用 Substrate、Lumen 相关方法和 RT 支持；本任务不切换这些全局设置，也不要求把它们都学完。

**停下来检查：**固定相机观察数秒，画面是否仍自动变亮/变暗？换到 PIE 后是否明显不同？若是，先统一显示条件，再调材质。

## 6. 实践一：创建父材质与两个实例

### 6.1 定义参数接口

创建 Surface 域、Opaque 的简单材质，使用以下接口。名称是本任务约定，应在材质图和 C++ 中保持一致。

| 参数 | 类型 | 建议初值 | 本次实验范围 |
| --- | --- | --- | --- |
| `BaseColor` | Vector | 线性 RGB `(0.18, 0.18, 0.18)` | 先保持不变，之后单独换色 |
| `Roughness` | Scalar | `0.5` | `0.15 / 0.5 / 0.85` |
| `Metallic` | Scalar | `0` | `0` 与 `1` |
| `EmissiveColor` | Vector | 线性 RGB `(1, 1, 1)` | 发光测试时单独改色 |
| `EmissiveStrength` | Scalar | `0` | `0 / 1 / 5`，只是实验值 |

将 Scalar/Vector Parameter 分组，便于实例编辑器扫描。UI 的 Slider Min/Max 用于编辑便利，不等于 C++ 运行时数值验证。

这些颜色值是线性数值，不是显示器上的 sRGB 字节值。使用颜色选择器时分清线性数值与 sRGB/Hex 输入；不要认为显示器上“18% 灰”就应填入 `0.18 * 255`。先使用中性灰减少色彩变量，纹理的 sRGB 标记留给下一任务。

### 6.2 UE 5.8 的节点连接

R3 明确说明：开启 Substrate 后，现有材质或新材质仍可能显示传统根节点输入，并在编译时转换。这不代表项目没启用 Substrate。

本任务采用显式的简单 Slab 表达，避免同时学习复杂转换和层叠：

```text
BaseColor ---------> Substrate Metalness-To-DiffuseAlbedo-F0.BaseColor
Metallic ----------> 同一节点的 Metallic
Constant 0.5 ------> 同一节点的 Specular

该节点的 DiffuseAlbedo ---> Substrate Slab 的 DiffuseAlbedo
该节点的 F0 -------------> Substrate Slab 的 F0
Roughness ---------------> Substrate Slab 的 Roughness
EmissiveColor * EmissiveStrength ---> Substrate Slab 的 EmissiveColor

Substrate Slab 输出 ------> 材质根节点的 Front Material
```

节点搜索名可能显示为 `Substrate Slab` 或 `Substrate Slab BSDF`。不要假设 Slab 上有 `Metallic` 输入；这里通过转换节点承接传统金属度参数化。其本机定义见 [MaterialExpressionSubstrate.h](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Public/Materials/MaterialExpressionSubstrate.h:272)，转换实现见 [MaterialExpressionSubstrate.cpp](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Private/Materials/MaterialExpressionSubstrate.cpp:3604)。

暂不连接 Normal、SSS、各向异性和第二高光瓣等输入。保留默认值，先把一个表面做正确。若编辑器自动生成了其他节点，先识别它们，不删除任何共享资产；只在本次新建材质中组织图。

如果实际界面没有上述节点或输入，记录截图和编译错误，再核对设置、R3 与本机定义；不要通过随机启用插件或修改 Engine 来继续。

### 6.3 创建实例并验证继承

从父材质创建：

- `MI_Lab01_Dielectric`：覆盖 `Metallic = 0`、`Roughness = 0.5`。
- `MI_Lab01_Metal`：覆盖 `Metallic = 1`、`Roughness = 0.5`。

让两个实例的 `BaseColor` 保持相同，`EmissiveStrength = 0`。在实例编辑器中确认覆盖勾选，参考 R2。

做三次观察：

1. 修改父材质的一个未被实例覆盖的参数，观察继承。
2. 在其中一个实例中覆盖该参数，确认另一个实例不随该覆盖变化。
3. 取消覆盖，确认重新继承。最后恢复约定初值并保存本次资产。

**应能解释：**为什么不复制两份材质图？为什么一个实例的改动会影响所有使用该实例资产的球体？MIC 中的“覆盖值”存在哪里？

## 7. 实践二：用 C++ 创建并更新 MID

### 7.1 类的职责

建议类名 `AMaterialLabSample`，一个 Actor 只负责一个球体。

- 一个 `UStaticMeshComponent`，作为根组件，使用球体 Mesh 和材质槽 0。
- 一个可编辑的 `UMaterialInterface` 引用 `SourceMaterial`，从 Details 或薄 Blueprint 默认值指定 MIC。
- 一个 `UPROPERTY(Transient)` 的 `TObjectPtr<UMaterialInstanceDynamic>`，保存当前运行时实例。
- 若干可编辑的初始实验参数。
- 一个 `BlueprintCallable` 更新入口，例如 `SetLabRoughness(float)`；后续再加其他 setter 和恢复初值入口。

`SourceMaterial` 也应由合适的 `UPROPERTY` 引用管理。不要用 `std::shared_ptr` 管理 UObject，不手动 `delete` MID，不为了避免 GC 而滥用 `AddToRoot`。

资产引用可以在 `BP_MaterialLabSample` 的默认值中配置；这个 Blueprint 不承担材质更新逻辑。这样不用为本次练习建立资源加载系统或硬编码整套资产路径。

### 7.2 初始化顺序

建议先只在 `BeginPlay` 初始化：

```text
构造函数：创建 Mesh Component，关闭不需要的 Tick
BeginPlay：校验 Mesh、SourceMaterial 和槽位
    -> 创建并绑定 MID
    -> 保存引用
    -> 应用本 Actor 的初值
后续事件：只更新现有 MID 参数
```

材质预览用 MIC，运行时用 MID。第一版不实现 `OnConstruction` 动态预览，避免拖动 Actor 就重复初始化，也不必现在解决编辑器重建行为。

以下只是方法体中的关键调用示意，不是完整可编译 Actor。头文件、反射声明、成员和错误处理需要自己补全：

```cpp
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

// In BeginPlay, after validating Mesh, SourceMaterial and material slot 0:
RuntimeMaterial = Mesh->CreateDynamicMaterialInstance(0, SourceMaterial);
if (!RuntimeMaterial)
{
    return;
}

RuntimeMaterial->SetVectorParameterValue(TEXT("BaseColor"), InitialBaseColor);
RuntimeMaterial->SetScalarParameterValue(
    TEXT("Roughness"), FMath::Clamp(InitialRoughness, 0.0f, 1.0f));
```

入口已经核对本机 `UPrimitiveComponent::CreateDynamicMaterialInstance`，见 [PrimitiveComponent.cpp](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Private/Components/PrimitiveComponent.cpp:2620)。它可同时创建并绑定 MID；直接调用 `UMaterialInstanceDynamic::Create` 则还需自己处理组件绑定。

注意：组件 API 在当前槽位已经是 MID 时可能直接返回它。不能笼统认为“每调用一次必定创建一个独立对象”。本实验从 MIC 初始化每个 Actor，并检查实际对象引用；不要把一个 MID 再当成所有对象的共同 SourceMaterial。

本机 [RenderLab.Build.cs](E:/ue_project/pratice/RenderLab/Source/RenderLab/RenderLab.Build.cs) 已依赖 `Engine`，本任务不应为了 MID API 添加 `Renderer`、`RenderCore` 或 `RHI` 模块。新增反射类首次编译时，建议正常关闭 Editor 后完整编译再打开，不把 Live Coding 成功当作首次接入验证。

### 7.3 让修改真正发生在运行时

推荐最简单、可重复的驱动方式：

1. 让一只球以 `Roughness = 0.15` 进入 PIE。
2. 用很薄的 Level Blueprint：`BeginPlay -> Delay 2 秒 -> 调用目标 Actor 的 SetLabRoughness(0.85)`。
3. 再延迟 2 秒恢复 `0.15`。延迟只用于肉眼观察，不是线程同步机制。
4. setter 内只更新当前 MID，不重新创建材质对象，也不修改 MIC 资产。

这一小段 Blueprint 只是实验触发器，材质更新仍在 C++。不需要先写输入系统或 UI；也不要以“只在编辑器中改材质参数”替代运行时验收。

对 `Roughness`、`Metallic` 做运行时范围限制；对本次 `EmissiveStrength` 至少处理负值。记录无效引用和槽位，不吞掉失败后继续宣称有效。

**覆盖顺序必须清楚：**如果 `BeginPlay` 主动设置某个值，它就会覆盖从 MIC 继承的同名值。初始参数先与对应 MIC 保持一致，再逐项修改，避免不知道是哪一层决定最终结果。

## 8. 实践三：受控对照实验

两行三列使用相同球体、尺寸和 BaseColor，第一行 `Metallic = 0`，第二行 `Metallic = 1`；三列分别为 `Roughness = 0.15 / 0.5 / 0.85`。每只球使用自己的 MID。

建议使用近似均匀的主光。不同位置仍可能有不同间接光和反射，因此阵列用于快速观察；正式 A/B 截图要让同一只球留在同一位置，只改变一个参数。

| 实验 | 只改变什么 | 要观察和记录什么 |
| --- | --- | --- |
| E1 粗糙度 | 同一球的 `0.15 -> 0.85` | 高光形状、峰值、反射清晰度；不要把粗糙金属解释成变成电介质。 |
| E2 金属度 | 同一球的 `0 -> 1` | 在相同颜色与粗糙度下，漫反射和镜面贡献如何变化；没有丰富环境反射时不要直接判断“金属坏了”。 |
| E3 实例独立性 | 只改 Actor A 的 MID | Actor B 的参数和外观是否保持原样；检查两个 MID 引用确实不同。 |
| E4 发光 | 固定曝光，强度 `0 -> 1 -> 5` | 区分表面亮度、Bloom 光晕和周围照明；Lumen 可能引入间接影响，不能归因于曝光或直接光变化。 |
| E5 生命周期 | 退出并重新进入 PIE 三次 | 初始状态能否恢复，MIC 资产是否被意外改写，是否有异常日志。 |
| E6 无效输入 | 一个无效 SourceMaterial 或缺失 Mesh 的新测试实例 | 清晰报错或安全跳过，不崩溃；只使用本实验对象测试，不改现有共享对象。 |

先写预测，再运行并截图，最后解释。预测错误不是失败，无法分清变量、复现不了结果才需要重做。

运行时字段可以回读，组件槽位也可以核对，但 setter/getter 返回或日志中的数值不等于该参数一定参与最终着色。参数必须存在于有效图路径中，且当前 Mesh 真正在使用该 MID。

## 9. 源码阅读：只追一条参数路径

在运行时实验成功后再读，首轮控制在 30～45 分钟：

| 顺序 | 本机 UE 5.8 入口 | 本次只回答这个问题 |
| --- | --- | --- |
| 1 | [PrimitiveComponent.cpp:2620](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Private/Components/PrimitiveComponent.cpp:2620) | MID 的父级和 Outer 从哪里来？何时创建、复用、绑定？ |
| 2 | [MaterialInstanceDynamic.h](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Public/Materials/MaterialInstanceDynamic.h:21) | 对外暴露哪些动态参数接口？哪些返回 void，意味着不能靠返回值确认参数有效？ |
| 3 | [MaterialInstanceDynamic.cpp:196](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Private/Materials/MaterialInstanceDynamic.cpp:196) | 参数名称如何包装后传给内部实现？ |
| 4 | [MaterialInstance.cpp:4224](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Private/Materials/MaterialInstance.cpp:4224) | 参数覆盖如何存储？值不变时是否仍排入渲染更新？ |
| 5 | [MaterialInstance.cpp:608](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Private/Materials/MaterialInstance.cpp:608) | 什么数据被复制到渲染命令，Render Proxy 收到什么更新？ |

本机实现可概括为：

```text
Gameplay 事件，Game Thread
    -> UMaterialInstanceDynamic::SetScalarParameterValue
    -> UMaterialInstance::SetScalarParameterValueInternal
    -> 参数覆盖更新
    -> GameThread_UpdateMIParameter
    -> ENQUEUE_RENDER_COMMAND
    -> RenderThread_UpdateParameter / CacheUniformExpressions
    -> 后续渲染使用更新的数据
```

这条链路说明为什么不应把 setter 理解为同步 GPU 调用。本实验不要手动插入 `FlushRenderingCommands` 以“确保生效”，也不要自行把 UObject 访问搬到渲染线程。

理解到 Render Proxy 与数据更新即可，不继续深挖整个材质编译器、PSO 或 D3D12RHI。行号对应本机当前源码，升级后按函数名搜索。

## 10. 常见问题与排查顺序

| 现象 | 优先检查 |
| --- | --- |
| 整个场景过亮、发白 | Manual 曝光是否真正生效；物理相机曝光、补偿、Local Exposure、Bloom 与灯光是否同时叠加。不要先把 BaseColor 改成接近黑色掩盖问题。 |
| 金属看起来很黑 | 是否有足够的可反射环境；Sky Light/场景捕获是否稳定；Metallic/F0 是否正确连接。金属不是“塑料加亮”。 |
| 编辑器视口和 PIE 不同 | 相机、曝光覆盖、视图模式、画质以及运行时初值是否一致。 |
| 参数更新日志正常，画面不变 | 参数名、类型、有效连接、材质槽、实际绑定对象；拼错的参数名不能靠 setter 无报错证明正确。 |
| 两个 Actor 一起变化 | 是否共享同一个 MID，是否修改了 MIC 资产，或误用了全局参数集合。 |
| 粗糙度变化不明显 | 光源/反射环境能否显示高光；是否同时修改曝光；是否比较同一位置的同一球。 |
| 改材质后出现编译 | 是否修改了父图、Static Switch 或用途；先等背景编译稳定，再测试普通 scalar/vector 更新。不能认定任何编译都是 MID setter 导致。 |
| 再次 PIE 状态混乱 | MID 是否每次运行正确初始化，是否在 Construction/Tick 重建，是否保留了不正确的 UObject 引用。 |
| 找不到旧教程中的 Material header 路径 | 本机 5.8 的相关头文件位于 `Engine/Public/Materials`；正常 include 仍用 `Materials/MaterialInstanceDynamic.h`，不要复制磁盘绝对路径到 C++ include。 |

## 11. 我的学习指导建议

1. **把材质当作有类型的数据接口，不当作连线手艺。**每个参数都应有名称、范围、调用方和可观察的结果；这与你设计 C++ 模块接口是同一类工程习惯。
2. **先建立证据，再解释机制。**先让一只球变化并确认绑定，再看六只球；先观察更新，再读调用链。不要一开始同时调十个设置。
3. **用渲染方程思考，但不要过度推导。**判断正在改变的是表面散射参数、入射光，还是曝光/显示变换；本任务不需要重新手写 GGX。
4. **不要把 CPU 对象、Shader 程序和 GPU 参数混为一谈。**MID 是 UObject，普通参数更新是数据变化；独立 MID、独立 Shader 与独立 Draw Call 并不是一一对应关系。
5. **先把生命周期做对，再谈优化。**六个球不需要材质参数索引缓存或复杂批量系统；避免重复创建、错误共享和每帧无意义调用更重要。
6. **不要急着把所有视觉工作移到 C++。**材质图表达表面，C++ 表达行为，薄 Blueprint 负责触发和资产配置。本任务要理解职责边界，而不是消灭其中一种工具。
7. **保留“异常实验”。**一次参数拼错、一次共享 MID、一次曝光覆盖的排查记录，通常比多做几个漂亮材质更能训练工程判断；只在可恢复的实验范围内操作。
8. **按职业方向分别多问一句。**引擎方向问“数据何时跨线程、谁持有资源”；客户端方向问“谁调用、何时恢复、对象销毁后是否安全”。本任务两种问题都能练到。

本任务先不要进行无条件的“材质实例一定更快”“粗糙度高就更便宜”“打开 RT 就更真实”等泛化。说清测试条件与实际观测，比提前背性能结论更重要。

## 12. 验收与提交

### 12.1 必做验收

- [ ] 能在 UE 5.8 编译工程并打开独立实验关卡。
- [ ] 父材质图结构清楚，参数与本文接口一致，没有编译错误。
- [ ] 两个 MIC 能验证覆盖与继承，不通过复制父图制造预设。
- [ ] 六只球的粗糙度/金属度排列正确，显示条件有记录。
- [ ] 至少一个参数在 PIE 期间通过 C++ 入口改变并恢复，而非只在编辑器改资产。
- [ ] Actor A 的变化不影响 Actor B，MID 引用与组件绑定得到核对。
- [ ] 不在 Tick 中创建 MID，也不依赖每帧重建实例。
- [ ] PIE 重复三次正常，处理无效引用，不意外修改 MIC 资产。
- [ ] 读懂并记录一条从 setter 到渲染线程更新的调用链。
- [ ] 回答：Static Switch 为什么不适合这个运行时控制？为什么画面可能因曝光变化而误导判断？
- [ ] 报告明确哪些结果已验证，哪些只做了推测；保留待解决的问题。

### 12.2 最小实验报告

在未来的 `Docs/Experiments/Lab01A_Results.md` 中记录：

```markdown
# Lab01A 实验记录

## 环境
UE/提交、GPU/驱动、RHI、关卡、相机、分辨率、画质、曝光和局部曝光。

## 实现入口
父材质与实例路径、C++ 类、触发方法、参数及初值。

## 对照实验
编号 | 控制变量 | 预测 | 实际结果 | 截图 | 解释

## 源码理解
参数更新调用链，以及对象/线程边界。

## 验收
已验证项目、未验证项目、异常日志与已知限制。

## 复盘
最初判断错了什么，怎样确认原因，下一任务要解决什么。
```

最少保留：材质图截图、两行三列阵列截图、同一球更新前后截图。截图不是性能数据，本任务不要求给出严格 GPU 性能结论。

实际完成后，保存需要保留的资产，检查 `git diff`、`git status` 和 `git lfs ls-files`，再提交与本任务相关的代码、资产和记录。不要提交缓存、IDE 状态和全部抓帧文件。参考 [Git_Workflow.md](E:/ue_project/pratice/RenderLab/Docs/Git_Workflow.md)。

本文档创建后仍保持任务待开始；只有完成实际验收，才更新上级路线。下一步是 Lab01B 溶解实验，不提前扩展到后处理或 RDG。
