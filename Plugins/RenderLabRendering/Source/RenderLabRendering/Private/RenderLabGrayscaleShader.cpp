// Fill out your copyright notice in the Description page of Project Settings.

#include "RenderLabGrayscaleShader.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "ScreenPass.h"
#include "ShaderParameterStruct.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RenderGraphBuilder.h"
#include "RHIStaticStates.h"
#include "SceneView.h"

class FRenderLabGrayscalePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRenderLabGrayscalePS); // FRenderLabGrayscalePS 是一个 Global Shader 类型。
	SHADER_USE_PARAMETER_STRUCT(FRenderLabGrayscalePS, FGlobalShader); // 使用参数结构体机制
	
	// C++ -> HLSL的接口
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture) // 输入纹理
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)  // 采样器
		SHADER_PARAMETER(float, Strength)					  // Lab03B强度参数
		RENDER_TARGET_BINDING_SLOTS()						  // 输出到哪里
	END_SHADER_PARAMETER_STRUCT()
	
	// 控制Shader编译条件
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// 注册Shader
IMPLEMENT_GLOBAL_SHADER(
    FRenderLabGrayscalePS,
    "/Plugin/RenderLabRendering/Private/RenderLabGrayscale.usf",
    "MainPS",
    SF_Pixel);

FScreenPassTexture AddRenderLabGrayscalePass(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, float Strength)
{
	// 获取当前后处理阶段的 SceneColor 输入
	const FScreenPassTexture Input = FScreenPassTexture::CopyFromSlice(
		GraphBuilder, Inputs.GetInput(EPostProcessMaterialInput::SceneColor)	
	);
	check(Input.IsValid());
		
	// 优先使用 Renderer 指定的输出；没有则创建新的输出纹理
	FScreenPassRenderTarget Output = Inputs.OverrideOutput;
	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget::CreateFromInput(
			GraphBuilder, Input, View.GetOverwriteLoadAction(), TEXT("GrayscaleOutput")
		);
	}
		
	// 分配并填写 Pixel Shader 参数
	auto *Parameters = GraphBuilder.AllocParameters<FRenderLabGrayscalePS::FParameters>();
	Parameters->InputTexture = Input.Texture;
	Parameters->InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters->Strength = Strength;  // 可传入调整的参数
	Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();
		
	// 获取当前 Feature Level 对应的 Global Shader
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
	TShaderMapRef<FScreenPassVS> VertexShader(ShaderMap);
	TShaderMapRef<FRenderLabGrayscalePS> PixelShader(ShaderMap);
		
	// 向 RDG 添加一个全屏 Raster Pass
	AddDrawScreenPass(
		GraphBuilder, RDG_EVENT_NAME("RenderLab.Grayscale"),
		View, FScreenPassTextureViewport(Output), FScreenPassTextureViewport(Input),
		VertexShader, PixelShader, Parameters
	);
		
	// 返回处理后的屏幕纹理，供后续后处理继续使用
	return Output;
}

