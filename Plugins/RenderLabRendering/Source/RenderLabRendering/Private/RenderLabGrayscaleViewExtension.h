#pragma once

#include "SceneViewExtension.h"

class UWorld;

// 绑定到指定 UWorld 的 Scene View Extension
// 用于把自定义灰度后处理接入 UE 的后处理流程
class FRenderLabGrayscaleViewExtension final : public  FWorldSceneViewExtension
{
public:
	// 创建并注册 View Extension，同时绑定目标 World
	FRenderLabGrayscaleViewExtension(const FAutoRegister& AutoRegister, UWorld* World);
	
	// 订阅指定后处理阶段，并向该阶段注册自定义回调
	virtual void SubscribeToPostProcessingPass(
		EPostProcessingPass Pass,
		const FSceneView& View,
		FPostProcessingPassDelegateArray& InOutPassCallbacks,
		bool bIsPassEnabled) override;
};