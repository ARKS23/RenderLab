#include "RenderLabGrayscaleViewExtension.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "RenderLabGrayscaleShader.h"
#include "SceneView.h"

FRenderLabGrayscaleViewExtension::FRenderLabGrayscaleViewExtension(
	const FAutoRegister& AutoRegister, UWorld* World)
	: FWorldSceneViewExtension(AutoRegister, World){}

void FRenderLabGrayscaleViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& View, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	// 只挂接到启用的 Tonemap 后处理阶段
	if (Pass != EPostProcessingPass::Tonemap || !bIsPassEnabled)
	{
		return;
	}

	// 不影响 SceneCapture 和 ReflectionCapture
	if (View.bIsSceneCapture || View.bIsReflectionCapture)
	{
		return;
	}

	// 当前 View 至少需要 SM5
	if (View.GetFeatureLevel() < ERHIFeatureLevel::SM5)
	{
		return;
	}

	// 注册灰度后处理回调
	InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateStatic(&AddRenderLabGrayscalePass));
}

