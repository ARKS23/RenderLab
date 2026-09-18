# Lab02 实验记录

## 环境

- 项目：RenderLab
- Unreal Engine：5.8.0 CL 55116800
- RHI：DX12
- 关卡：`/Game/Levels/Lab02_Level`
- 当前进度：Lab02A、Lab02B、Lab02C 已验收；Lab02D 待开始。

## Lab02A：可调灰度后处理

- 已验证 `PostProcessInput0` 原图直通。
- 已验证固定颜色接入、灰度计算、`0 / 0.5 / 1` 参数变化。
- 已验证材质实例与运行时 MID 的参数更新，以及重复 PIE 后的初始状态恢复。
- 后处理材质通过 `PostProcessComponent::AddOrUpdateBlendable` 接入。

## Lab02B：屏幕坐标与深度

- 已验证 `ScreenPosition.ViewportUV` 的屏幕空间颜色显示。
- 已验证 `SceneDepth` 的显示映射。
- 已观察相机距离和 `FarDisplay` 对深度显示结果的影响。
- 已区分屏幕 UV、模型 UV、场景深度和后处理输入颜色。

## Lab02C：指定物体描边

- 使用 `M_Lab02_VisibleOutline` / `MI_Lab02_VisibleOutline` 合成可见外轮廓。
- 使用 Custom Stencil 识别目标，后处理材质的 `TargetID` 为 `1`。
- 使用 `AOutlineSelector` 进行 Gameplay 视线选择：
  - 玩家视点向前执行 `ECC_Visibility` 单次射线检测；
  - 只有带有 Actor Tag `OutlineSelectable` 的对象可被选择；
  - 命中墙体或空处时清除选择，不穿透墙体寻找目标；
  - 选中组件时开启 Custom Depth 并写入 Stencil `1`；
  - 切换或取消选择时恢复组件原始 Custom Depth 和 Stencil 状态。
- 已验证：
  - 目标进入视线后显示轮廓；
  - 从对象 A 切换到对象 B；
  - 看向空处后轮廓消失；
  - 墙体遮挡后不显示被遮挡轮廓；
  - 重复进入 PIE 后没有残留选择状态。

## 当前限制

当前实现限定为单玩家、单选择器和命中的单个 Primitive Component。多 Mesh Actor 整体选择、点击选择、输入抽象、选择高亮接口和性能更新频率尚未实现。

## Lab02D：GPU 抓帧

尚未验证。下一步固定分辨率、屏幕百分比、抗锯齿、曝光和效果参数，使用 RenderDoc 捕获一帧，记录后处理材质事件、输入输出纹理和对应源码入口。

## 结论

Lab02A-C 已完成从后处理输入、屏幕空间数据、深度/Stencil 遮挡到 Gameplay 选择控制的完整实验链路。下一阶段重点是把这条链路映射到一次真实 GPU 帧，而不是继续增加表面效果。
