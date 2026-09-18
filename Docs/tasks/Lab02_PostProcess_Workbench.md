# Lab02：屏幕空间后处理与一帧分析

日期：2026-09-15  
目标环境：RenderLab / UE 5.8.0 CL 55116800 / Windows / DX12。  
起始 Git 快照：`eab9b3b`，包含此前材质、MID、溶解及 MF/HLSL 实验。

**状态：Lab02A-C 已验收，Lab02D 待开始。** 本文同时作为学习指导与阶段记录；Lab02A-C 已在 UE 5.8.0 中完成实际实验，下一步进入 GPU 抓帧与后处理 Pass 分析。

上级路线：[UE_Rendering_Learning_Roadmap.md](E:/ue_project/pratice/RenderLab/Docs/UE_Rendering_Learning_Roadmap.md)。  
前置指导：[Lab01A_Material_Workbench.md](E:/ue_project/pratice/RenderLab/Docs/tasks/Lab01A_Material_Workbench.md)。

## 1. 从当前进度出发

此前已经围绕材质实例、MID 参数控制、UV、溶解遮罩、发光边缘和 MF + Custom HLSL 做过学习实验。接下来不必不断增加相似的表面效果，而应开始回答：

> UE 已经渲染出一张场景图像后，我如何读取它、处理它，并确认这段计算发生在哪个阶段？

本阶段需要建立的链路：

```text
前一阶段输出的场景图像
    -> 后处理材质的场景纹理输入
    -> 屏幕空间采样与局部 HLSL 计算
    -> 后处理输出
    -> 后续处理与显示
```

进入 Lab02 前，建议给溶解实验做一次小型收尾，尚未验证的项目不要直接标记完成：

- [x] 相同输入下对比节点版与 HLSL 版溶解。
- [x] 用 MID 更新 `DissolveThreshold`，让物体溶解后恢复。
- [x] 两个对象使用独立 MID，修改 A 不影响 B。
- [x] 再次进入 PIE，初始状态可恢复。

不需要为收尾创建通用动画框架，也不需要继续扩展溶解效果库。， 

## 2. 分阶段目标

| 子任务 | 要做出的结果 | 主要学习问题 | 状态 |
| --- | --- | --- | --- |
| Lab02A | 原图直通、可调灰度、MID 控制 | 输入来自哪里？计算在何处执行？参数如何进入后处理？ | 已验收 |
| Lab02B | 屏幕 UV 与场景深度可视化 | 视口坐标、纹理尺寸与深度数值分别代表什么？ | 已验收 |
| Lab02C | 指定物体的屏幕空间描边 | 如何识别对象、采样邻域，并定义遮挡规则？ | 已验收 |
| Lab02D | 一次可解释的 GPU 抓帧 | 如何找到效果事件、输入输出资源和对应源码？ | 待开始 |

**当前进度为 A-C 已完成。** 已先建立后处理输入、屏幕坐标、深度和 Custom Depth/Stencil 的正确认知，再完成邻域描边与 Gameplay 选取。下一步进行 D；不在还未抓帧前把 GPU Pass 分析标记为完成。

本阶段不做：Global Shader 插件、自定义 RDG Pass、Engine 修改、复杂 GBuffer 解码、完整光照模型或多平台渲染框架。这些不是完成后处理入门的前提。

## 3. 先看哪些参考资料

资料编号仅适用于本文，不与 Lab01 的编号混用。

| 编号 | 参考资料 | 阅读时机与范围 |
| --- | --- | --- |
| R1，必读局部 | [Post Process Materials](https://dev.epicgames.com/documentation/en-us/unreal-engine/post-process-materials-in-unreal-engine) | 先看创建与接入、Blendable Location、SceneTexture/PostProcessInput0；B/C 阶段再看 UV、深度与 Stencil。 |
| R2，遇到设置问题时查 | [Post Process Effects](https://dev.epicgames.com/documentation/en-us/unreal-engine/post-process-effects-in-unreal-engine) | Volume、作用范围、权重和场景后处理设置。暂时不逐项学习所有效果。 |
| R3，边写边查 | [Custom Material Expressions](https://dev.epicgames.com/documentation/en-us/unreal-engine/custom-material-expressions-in-unreal-engine) | 输入命名、类型、函数体写法与优化限制。不从私有场景采样函数开始。 |
| R4，复习局部 | [Material Functions Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/material-functions-overview-in-unreal-engine) | Function Input/Output、调用接口与复用。已掌握的内容可跳过。 |
| R5，Lab02D 再读 | [Using RenderDoc](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-renderdoc-with-unreal-engine) | 集成、捕获、事件与资源检查；使用前验证本机 DX12、驱动和 RT 配置兼容性。 |
| R6，Lab02D 补充 | [Unreal Insights](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-insights-in-unreal-engine) | CPU/GPU 时间线与测量方法，不把单帧调试器中的时间当作稳态性能。 |
| R7，亮度异常时查 | [Auto Exposure](https://dev.epicgames.com/documentation/en-us/unreal-engine/auto-exposure-in-unreal-engine) | 固定曝光、相机覆盖与编辑器视口曝光设置。 |

**第一次阅读控制在 20～30 分钟：R1 的接入部分 + R2 的 Volume 部分，然后立即做原图直通。** 已熟悉 MF/HLSL，不需要先重读完整 R3/R4，更不要先看完 RenderDoc 教程才动手。

在线链接是概念参考入口，本次未取得可用于核验的网页正文，不宣称在线示例已按 UE 5.8 验证。本文涉及版本敏感的名称、深度转换和运行时接口，已直接核对本机源码，第 10 节给出入口；运行结果仍需逐项实验确认。

## 4. 准备实验环境

### 4.1 目录与资产

以下是后续操作时创建的建议路径，本文没有创建这些资产：

```text
Content/RenderLab/Lab02_PostProcess/
  Maps/L_Lab02_PostProcess.umap
  Materials/M_Lab02_Grayscale.uasset
  Materials/MI_Lab02_Grayscale.uasset
  Functions/MF_Lab02_Grayscale.uasset
  Materials/M_Lab02_UVDebug.uasset          Lab02B 再创建
  Materials/M_Lab02_DepthDebug.uasset       Lab02B 再创建
  Materials/M_Lab02_Outline.uasset          Lab02C 再创建
Source/RenderLab/Lab02/
  Lab02PostProcessController.h/.cpp        Lab02A 运行时控制时再创建
Docs/Experiments/
  Lab02_Results.md                         实际实验记录
  Images/Lab02/                            关键截图
```

不移动或重命名已有 Lab0/Lab01 资产，不替换项目默认地图。不复制整套大型场景，只需基础几何体、地面、简单光照和有区别的颜色。

### 4.2 显示与比较条件

1. 新建独立关卡，放置红、绿、蓝及中性灰物体，方便观察颜色变化。
2. 固定相机和光照，记录分辨率、屏幕百分比、抗锯齿方式、SDR/HDR 输出状态。
3. 使用一个明确负责实验的 Post Process Volume，避免多个 Volume/相机同时提供同名效果。
4. 在实验 Volume 中固定曝光；记录 Local Exposure，比较时不要让它或光照一起变化。
5. 首轮保持项目现有 DX12、Substrate、Lumen/RT 配置，不为了对齐旧教程关闭全局功能。
6. 固定条件下等待 Shader 编译与画面稳定，分别记录 Editor 与 PIE 结果。

**本阶段先限定：单主视口、普通透视相机、桌面渲染路径。** 不把 Scene Capture、多视口、VR、移动端或 Path Tracing 的表现视为已验证。

## 5. Lab02A：可调灰度后处理

### A1. 先让原图通过

**操作**

1. 新建 `M_Lab02_Grayscale`，点击材质图空白处设置材质属性。
2. 设置 `Material Domain = Post Process`。
3. 设置 `Blendable Location = Scene Color After Tonemapping`，`Is Blendable` 保持启用。
4. 新建 `SceneTexture` 节点，将 `Scene Texture Id` 设为 `PostProcessInput0`。
5. 第一轮让它的 UV 输入保持未连接；让引擎处理当前输入纹理的默认坐标映射。
6. 将其 Color 输出经过 `ComponentMask(RGB)` 接到材质根节点 `Emissive Color`，Apply 并保存。
7. 创建 `MI_Lab02_Grayscale`，作为 Volume 使用的实例。
8. 实验 Volume 开启 `Infinite Extent (Unbound)`，保持启用，Volume 的 `Blend Weight = 1`。
9. 在 Volume 的 `Post Process Materials` 列表中添加这个 MI，条目 Weight 设为 `1`。

```text
SceneTexture: PostProcessInput0
    -> ComponentMask: RGB
    -> Emissive Color
```

这里的 Emissive 是后处理材质的颜色输出接口，不是在给球体增加发光。该材质接入 Volume，不放进 Mesh 的材质槽，也不用连接 Substrate Slab。

**为什么先不改变颜色**

先确认输入、输出和接入位置都正确，再增加算法。`PostProcessInput0` 表示当前后处理材质接收到的输入 0，在本次位置对应前一阶段场景颜色，不是任意时候都含义相同的“最终屏幕图”。

UE 5.8 已将旧名称 `After Tonemapping` 对应到 `Scene Color After Tonemapping`；以本机 [BlendableInterface.h](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Classes/Engine/BlendableInterface.h:66) 为准。

**验证**

- 开关这个灰度材质条目的权重，原图直通时视觉上应保持一致；暂时不要开关整个 Volume，否则曝光等设置也可能改变。
- 为确认效果确实接入，可临时输出固定红色；确认全屏改变后立即恢复原图直通。
- 不应出现黑屏、明显拉伸、取样区域偏移或仅屏幕一角有效。
- 不用材质球预览替代实际场景视口中的验证。

只有原图直通和临时颜色验证都通过，才继续下一步。画面看起来没变，不足以单独证明材质已经生效。

### A2. 用 MF + HLSL 实现灰度

**接口**

创建 `MF_Lab02_Grayscale`，用 Function Input 接收数据，参数仍留在父材质中：

| Function Input 名称 | Input Type | 来源 |
| --- | --- | --- |
| `InputColor` | Vector3 | SceneTexture 的 RGB |
| `GrayscaleAmount` | Scalar | 父材质的同名 Scalar Parameter |

在 MF 内添加一个 Custom，主输出类型设为 `CMOT Float3`，添加同名的两个 Custom Inputs。将两个 Function Input 分别连接进去。

Custom Code：

```hlsl
// A weighted grayscale effect in the current input color encoding.
float gray = dot(InputColor, float3(0.2126f, 0.7152f, 0.0722f));
float amount = saturate(GrayscaleAmount);
return lerp(InputColor, float3(gray, gray, gray), amount);
```

将 Custom 主输出接到 `FunctionOutput`，输出名称设为 `Color`。FunctionOutput 的结果类型由连接决定，不需要在那里额外设置 Custom 的 Output Type。本实验只有一个输出，不需要 Additional Outputs。

在父材质中新建 `GrayscaleAmount` 参数，默认值设为 `0`，Slider 范围为 `0～1`：

```text
SceneTexture(PostProcessInput0).Color -> ComponentMask(RGB)
    -> MF_Lab02_Grayscale.InputColor

Scalar Parameter: GrayscaleAmount
    -> MF_Lab02_Grayscale.GrayscaleAmount

MF_Lab02_Grayscale.Color
    -> Emissive Color
```

**算法含义**

- `dot` 将 RGB 按同一组权重组合为单个灰度值。
- `float3(gray, gray, gray)` 让三个通道相同。
- `lerp` 用 Amount 在原图与灰度之间混合。
- `saturate` 将本次算法使用的 Amount 限制在 `0～1`。
- 参数默认值为 `0`，为效果提供不改变原图的中性状态。

**颜色空间边界**

本实验刻意选择色调映射后的输入，做的是当前输入编码上的加权灰度风格效果，不能因此声称计算了物理亮度或色调映射前的场景线性亮度。即使权重形式熟悉，也必须先知道输入编码。

本机源码明确说明这个位置的颜色空间随输出设置变化，可涉及 SDR、HDR 或线性输出。不要擅自增加 `pow(Color, 2.2)`，也不要同时修改 HDR 或工作色域去“修正”实验。严格的线性空间处理留作后续独立对照。

### A3. 实例调参与对照

在 `MI_Lab02_Grayscale` 中勾选覆盖 `GrayscaleAmount`，依次观察：

| Amount | 预测结果 |
| --- | --- |
| `0` | 保持原来的颜色 |
| `0.5` | 原图与灰度之间的中间状态 |
| `1` | RGB 三通道相同的灰度结果 |

比较时保持 Volume 和材质条目的权重都为 `1`。**算法参数、材质条目权重、Volume 权重是不同层级的控制量，不要同时调。**

学习性对照：在父材质旁边用相同权重的 `DotProduct + Lerp` 搭一版原生节点，用同一输入和参数分别切换根节点连接，比较结果。不要让两个完整效果同时参与 Volume。

观察一致可作为首轮视觉证据，不等于已经测得逐像素完全相同。Custom 的代码更短也不代表更快；`Amount = 0` 代表算法输出原图，不保证引擎已省略这次后处理 Pass。

### A4. 接入运行时 MID

先完成 A1～A3，再写控制代码。不要把新行为混入已有的粗糙度实验类。

建议用一个轻量 `ALab02PostProcessController`：

1. 持有 `UPostProcessComponent`，设置为无界并启用，组件权重为 `1`。
2. 使用 `UPROPERTY` 保存指定的灰度 MIC；用 `UPROPERTY(Transient)` 保存运行时 MID。
3. `BeginPlay` 校验 MIC，使用 `UMaterialInstanceDynamic::Create` 创建 MID。
4. 将 MID 作为组件的 Blendable，权重设为 `1`。本机提供 `UPostProcessComponent::AddOrUpdateBlendable`，具体参数类型见第 10 节源码。
5. 从原实验 Volume 中移除刚才用于预览的那一个灰度 MI 条目；保留曝光等其他设置，避免两个来源重复或相互混合。
6. 提供 `SetGrayscaleAmount(float)`、`ResetEffect()` 两个可调用入口；无需先实现输入系统或通用效果管理器。

setter 核心调用示意，不是完整 Actor：

```cpp
if (RuntimeMaterial)
{
    RuntimeMaterial->SetScalarParameterValue(
        TEXT("GrayscaleAmount"),
        FMath::Clamp(Amount, 0.0f, 1.0f));
}
```

创建了 MID 不等于场景已经使用它；需要核对后处理组件实际引用的是该 MID。不要用 Mesh 的 `CreateDynamicMaterialInstance` 给后处理创建材质槽绑定，也不要在 Tick 中反复创建 MID 或添加 Blendable。

第一次可以用很薄的 Blueprint 触发：进入 PIE 后调用 `0 -> 1 -> 0`，中间留出观察时间。C++ 负责参数更新，Blueprint 只负责实验触发。重置参数不等于移除 Pass，两个目标应分别验证。

如果使用共享 Volume 而不是自有组件，必须记录自己添加的 MID 条目，并在结束时只移除该条目，不清空整个后处理列表。第一版优先用自有组件简化生命周期。

### A5. Lab02A 验收

- [ ] 原图直通与临时固定颜色测试均成功。
- [ ] 能指出当前输入节点、MF 接口、Custom 计算和根节点输出。
- [ ] `0 / 0.5 / 1` 的效果符合预测。
- [ ] 原生节点与 HLSL 版本使用相同输入和权重，完成视觉对照。
- [ ] 运行时修改的是实际绑定的 MID，不是磁盘上的 MIC。
- [ ] 连续三次 PIE 能回到约定初值，不产生重复效果或异常日志。
- [ ] 调整窗口长宽比后，没有明显黑边、拉伸或画面偏移。
- [ ] 能解释为什么“Amount 为 0”与“该 Pass 没有执行”不是一回事。

首次会话可以停在 A3，不必为了赶进度同时完成运行时控制和抓帧。

## 6. Lab02B：屏幕坐标与深度

以下是下一轮实验设计。先做可视化，不立即做多邻域采样。

### B1. 屏幕 UV

在单独的调试后处理材质中，使用 `ScreenPosition` 的 `ViewportUV`，观察 U、V 灰度及 `(U, V, 0)` 颜色图。移动相机、改变窗口长宽比，对比它与之前的模型 UV。

应理解：ViewportUV 描述当前视口归一化位置，不是模型表面的 UV；视口有效区域不一定填满底层纹理。后续不能不经确认就把 ViewportUV 当成所有 SceneTexture 的采样 UV。

原图采样继续保留原生 SceneTexture 的默认 UV。之后研究邻域时，再明确“源纹理像素”“输出像素”与视口区域的转换及边界处理。改变屏幕百分比时，这个区别尤其需要验证。

### B2. 场景深度显示

1. 放置距离相机不同的几个不透明物体。
2. 从 `SceneTexture: SceneDepth` 读取深度，取 R 通道作为标量。
3. 将数据交给只负责显示映射的 MF/Custom，而不是直接输出大数值。
4. 提供 `NearDisplay` 和 `FarDisplay`，满足 `FarDisplay > NearDisplay`。

```hlsl
float range = max(FarDisplay - NearDisplay, 0.001f);
float depth01 = saturate((Depth - NearDisplay) / range);
return float3(depth01, depth01, depth01);
```

默认世界单位设置下，可先尝试 `NearDisplay = 0`、`FarDisplay = 3000`，按场景规模调整。这两个量是显示映射范围，不是在修改相机近远裁剪面。

**关键区别：**本实验读取的桌面 SceneDepth 路径已经调用 `ConvertFromDeviceZ`；它不是未经处理的硬件 Device Z，不要再套一遍网上的深度线性化公式。普通透视相机下应按视空间 Z 方向深度理解，不直接当成相机到表面的欧氏距离。依据见第 10 节。

**验收：**能预测近物体/远物体的亮暗，调整 FarDisplay 只改变显示映射，不改变几何或相机；天空/无几何区域单独说明，不用它作为几何距离的标定点。

## 7. Lab02C：指定物体描边

先明确效果规格，再动手：

- 第一版只处理选定的不透明静态网格，不声称自动覆盖透明、Nanite、RT 或所有材质路径。
- 能选中一个物体，取消选择后撤销标记；不修改其他对象的原始标记状态。
- 先做单色描边，定义宽度是源纹理像素还是最终输出像素。
- 先验证可见对象轮廓，再单独增加“被遮挡时隐藏”或“透视显示”的策略。

**实验顺序**

1. 检查项目和实际运行时 Custom Depth/Stencil 状态，不从配置文件缺少某行推断它关闭。
2. 到这一步才按需启用项目的 `Custom Depth-Stencil Pass`；需要 Stencil 时使用 `Enabled with Stencil`，记录改动并遵循编辑器重启提示。
3. 让一个测试 Mesh 参与 Custom Depth，并设置明确的 Stencil ID，例如 `1`。
4. 先显示目标 ID 遮罩，确认写入和采样，再比较相邻采样点形成边缘。
5. 加入场景颜色合成；再用同一采样位置、同一深度含义的 SceneDepth/CustomDepth 处理遮挡。
6. 最后接入 Gameplay 射线选取，并保留对象原有标记以便恢复。

本机 `EnabledWithStencil = 3` 的定义见 [RendererSettings.h](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Classes/Engine/RendererSettings.h:56)。Lab02A/B 不为此提前更改项目设置。

**Stencil 采样不等于硬件 Stencil Test。**本轮讨论的是在 Shader 中采样 CustomStencil 得到遮罩。UE 5.8 的后处理材质硬件 Stencil Test 路径在 After Tonemapping 位置有明确限制，不要顺手勾选该功能代替遮罩计算。见 [PostProcessMaterial.cpp](E:/ue/UE_5.8/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessMaterial.cpp:90)。

**验收：**取消选择可恢复；遮挡行为符合预先定义；改变窗口尺寸、屏幕百分比并移动相机时，能解释轮廓宽度和抖动变化。使用 TSR/TAA 时，不预设深度与色调映射后图像天然逐像素对齐。必要时另做插入位置对照，不盲目关闭抗锯齿。

### 7.1 Lab02C 已完成记录

验证日期：2026-09-18。

- 使用 `M_Lab02_VisibleOutline` / `MI_Lab02_VisibleOutline` 完成可见物体轮廓合成，后处理材质的 `TargetID` 为 `1`。
- 使用 `AOutlineSelector` 通过玩家视点调用 `LineTraceSingleByChannel`，以 `ECC_Visibility` 获取第一个阻挡命中。
- 只有带有 Actor Tag `OutlineSelectable` 的对象会被选中；命中墙或空处时清除选择，不穿透遮挡物继续寻找目标。
- 选中组件时开启 Custom Depth 并写入 Stencil `1`；切换或取消选择时恢复该组件原来的 Custom Depth 和 Stencil 值。
- 已验证目标在视线中显示轮廓、目标切换、看向空处取消轮廓、墙体遮挡后不显示被遮挡轮廓，以及重复进入 PIE 后状态恢复。

本次版本的范围仍限定为单玩家、单选择器和命中的单个 Primitive Component。多 Mesh Actor 的整体选择、点击选择、输入抽象、选择高亮接口和性能更新频率尚未实现；这些不属于本轮后处理入门的验收条件。

## 8. Lab02D：找到自己的后处理 Pass

现在 A-C 已完成，可以选择已经稳定的灰度或可见描边效果抓一帧。第一帧建议使用灰度效果，便于先确认后处理材质事件和输入输出资源。

1. 固定相机、画质、分辨率和效果参数，等待编译完成并预热。
2. 核对本机 RenderDoc 集成和 DX12/驱动/RT 组合是否可用；打不开时先记录日志和版本，不直接修改一整套渲染设置。
3. 捕获一帧，寻找包含 `PostProcessMaterial` 和材质名称的事件。本机事件标签定义见第 10 节，不保证所有工具以相同层级显示。
4. 查看该绘制实际绑定的输入图像、输出目标、纹理格式、尺寸与 Viewport/Scissor。
5. 对比输入和输出，找到哪一步把颜色变成灰度，并记录它相对色调映射的位置。
6. 找到对应的 C++ Pass 构建位置，只读局部代码；此时不要求自己编写 RDG Pass。

| 工具 | 本轮用途 | 不应用来证明什么 |
| --- | --- | --- |
| RenderDoc | 单帧事件、资源绑定、输入输出与绘制状态 | 仅靠捕获帧的时间推断稳定性能 |
| `stat unit` | 观察帧与线程/GPU 时间概览 | 精确定位某个 Shader 的纹理输入 |
| GPU 计时 / Unreal Insights | 在可用 Trace 和稳定条件下观察事件耗时与时间线 | 用一次波动或 FPS 代替可重复的性能数据 |

**验收：**保留一张事件位置截图、一张输入输出对照图，以及能说明插入位置和资源范围的简短记录。未成功捕获就写“未验证”，不以 MCP 查询成功代替 GPU 验证。

## 9. 常见问题与检查顺序

| 现象 | 先检查 |
| --- | --- |
| 完全没变化 | Post Process 域、Volume/组件作用范围、两个权重、是否使用正确实例；用临时纯色验证接入。 |
| 黑屏或报 SceneColor 相关错误 | SceneTexture ID 是否为本轮的 PostProcessInput0；输入和输出是否真的连接；不要直接粘贴旧教程私有采样函数。 |
| 只有一部分屏幕有效或画面拉伸 | UV 是否被错误接入、视口与纹理尺寸是否混淆；先恢复默认 UV 的原图直通版本。 |
| 参数变化了但结果没变 | 参数覆盖、MF 输入、Custom 输入名及实际绑定的 MID；不能仅凭 setter 不报错认定有效。 |
| 灰度强度与预期不一致 | 是否同时调了算法 Amount、材质条目 Weight、Volume Weight；是否有重复来源或相机覆盖。 |
| 亮度比较不稳定 | 曝光、Local Exposure、相机、光照、SDR/HDR 与插入位置是否固定。 |
| 深度几乎全白 | 深度直接显示发生饱和、显示范围过小或重复线性化，不先反转整张图猜测。 |
| 描边随分辨率变粗或错位 | 宽度单位、源纹理尺寸、有效视口、TSR/TAA 与插入位置。 |
| PIE 重开后效果叠加或状态不对 | 是否重复添加 Blendable、保存了旧引用或没有明确初始化参数。 |
| Custom 版本代码更短 | 这是可读性变化，不是性能结论；先验证等价，再测量。 |

## 10. UE 5.8 源码阅读入口

以下路径和相关符号在本次建档时已核对。只在实验遇到对应问题后读取，不要求现在通读：

| 入口 | 只回答这个问题 |
| --- | --- |
| [BlendableInterface.h](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Classes/Engine/BlendableInterface.h:14) | 当前插入位置有哪些？Input0、颜色空间和执行分辨率有哪些约定？ |
| [PostProcessVolume.h](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Classes/Engine/PostProcessVolume.h:26) | Volume 的 Settings、权重和 Unbound 如何表达？ |
| [PostProcessComponent.h](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Classes/Components/PostProcessComponent.h:88) | 如何把 MID 作为 Blendable 添加到组件？ |
| [Scene.h](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Classes/Engine/Scene.h:2636) | AddBlendable 如何更新已有条目？RemoveBlendable 的范围是什么？ |
| [MaterialExpressionSceneTexture.h](E:/ue/UE_5.8/Engine/Source/Runtime/Engine/Public/Materials/MaterialExpressionSceneTexture.h:21) | SceneTexture 的 ID、坐标和过滤选项是什么？ |
| [MaterialTemplate.ush](E:/ue/UE_5.8/Engine/Shaders/Private/MaterialTemplate.ush:1330) | 默认采样坐标如何从当前视口转换到目标场景纹理？ |
| [MaterialTemplate.ush 的采样分派](E:/ue/UE_5.8/Engine/Shaders/Private/MaterialTemplate.ush:3312) | SceneDepth、CustomDepth、PostProcessInput0 分别调用什么？ |
| [SceneTexturesCommon.ush](E:/ue/UE_5.8/Engine/Shaders/Private/SceneTexturesCommon.ush:57) | SceneDepth 是否已经从 Device Z 转换？ |
| [PostProcessMaterial.cpp](E:/ue/UE_5.8/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessMaterial.cpp:733) | 后处理材质如何建立 Pass？输入、输出和事件名在哪里？ |
| [后处理 GPU 事件标签](E:/ue/UE_5.8/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessMaterial.cpp:881) | 抓帧时怎样关联材质名称与 GPU 事件？ |

这些是理解引擎的入口，不表示应从 Custom 里直接调用每个私有函数。第一版保持“原生节点负责引擎资源访问，MF/Custom 负责纯数学”，避免无谓耦合内部采样签名、PreExposure 和材质编译条件。

## 11. 学习指导与验收记录

### 11.1 建议的学习方式

1. **先证明数据通路，再证明算法。**原图直通与临时纯色测试是排查后处理接入的重要工具。
2. **每个数值都问一句单位与空间。**是模型 UV、ViewportUV、纹理 UV、线性深度，还是编码后的颜色？
3. **让 MF 保持接口清楚。**简单灰度函数只接收颜色和强度，不承担整个场景资源查询。
4. **每次只改变一个层级。**实验 Amount 时固定 Volume；实验插入位置时固定算法与参数。
5. **将维护性与性能分开评价。**HLSL 清晰是好事，但不能从节点数量推断 GPU 时间。
6. **把结果和边界一起记录。**写出已验证的视口、相机、分辨率与路径，不把一次成功泛化为所有场景。

### 11.2 实验报告

完成实际操作后再创建 `Docs/Experiments/Lab02_Results.md`，至少记录：

```markdown
# Lab02 实验记录

## 环境
UE/Git 提交、GPU/驱动、RHI、关卡、分辨率、屏幕百分比、AA、SDR/HDR、曝光。

## Lab02A
资产与控制入口、插入位置、MF 参数、原图/灰度截图、MID 与重复 PIE 验证。

## Lab02B
坐标和深度来源、显示映射范围、窗口与相机变化、已确认的数值含义。

## Lab02C
选择与恢复、Stencil ID、轮廓宽度单位、遮挡规则、边缘异常。

## Lab02D
捕获环境、GPU 事件、输入输出格式/尺寸、源码入口、已知限制。

## 结论
已验证 / 部分验证 / 未验证；预测错误及修正过程；下一步。
```

当前 Lab02A-C 已验收，仍需完成 Lab02D 的 GPU 抓帧后，才进入 Global Shader 与项目渲染插件。

### 11.3 提交与回退

每个子任务完成后单独检查并提交，避免等到整阶段结束才保存版本：

- 保存本次需要的 UE 资产，检查 Git 与 LFS；保留关卡依赖的 External Actors/Objects。
- 只提交实验资产、源码、必要配置与记录，不提交 Saved 缓存、完整 GPU 捕获或 IDE 状态。
- 默认不修改 Lab01 代码；若修正前置问题，说明原因并单独检查。
- 回退优先考虑明确提交的 `git revert`，不要用硬重置清空未提交学习成果。
- 只有实际验收过的子任务才标为完成，不因文档已经写好就修改学习状态。

**当前下一步：固定实验条件，使用 RenderDoc 捕获一帧 Lab02 后处理，并记录事件、输入纹理、输出纹理与源码入口。**
