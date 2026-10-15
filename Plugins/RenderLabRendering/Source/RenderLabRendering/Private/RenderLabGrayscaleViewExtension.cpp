#include "RenderLabGrayscaleViewExtension.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "RenderLabGrayscaleRenderState.h"
#include "RenderLabInputDebugShader.h"
#include "RenderLabGrayscaleShader.h"
#include "RenderLabBlurShader.h"
#include "SceneView.h"

FRenderLabGrayscaleViewExtension::FRenderLabGrayscaleViewExtension(
	const FAutoRegister& AutoRegister, UWorld* World, TSharedRef<FRenderLabGrayscaleRenderState, ESPMode::ThreadSafe> InRenderState)
	: FWorldSceneViewExtension(AutoRegister, World), RenderState(InRenderState){}

void FRenderLabGrayscaleViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass Pass, const FSceneView& View, FPostProcessingPassDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	// 只挂接到启用的 Tonemap 后处理阶段
	if (Pass != EPostProcessingPass::Tonemap || !bIsPassEnabled)
	{
		return;
	}

	if (!RenderState->bEnabled)
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

	// 注册处理回调
	if (RenderState->Experiment == ERenderLabExperiment::Grayscale)
	{
		const float StrengthSnapshot = FMath::Clamp(RenderState->Strength, 0.f, 1.f);
		InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateStatic(&AddRenderLabGrayscalePass, StrengthSnapshot));
	}
	else if (RenderState->Experiment == ERenderLabExperiment::Blur)
	{
		const float MixSnapshot = FMath::Clamp(RenderState->BlurMix, 0.f, 1.f);
		InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateStatic(&AddRenderLabBlurPasses, MixSnapshot));
	}
	else if (RenderState->Experiment == ERenderLabExperiment::InputDebug)
	{
		InOutPassCallbacks.Add(FPostProcessingPassDelegate::CreateStatic(&AddRenderLabInputDebugPass));
	}
}
