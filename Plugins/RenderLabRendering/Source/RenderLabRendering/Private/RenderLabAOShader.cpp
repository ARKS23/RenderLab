#include "RenderLabAOShader.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "ScreenPass.h"
#include "ShaderParameterStruct.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "SceneTexturesConfig.h"
#include "RenderGraphBuilder.h"
#include "Math/RandomStream.h"
#include "RHIStaticStates.h"
#include "SceneView.h"

// AO单通道纹理shader
class FRenderLabAOPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRenderLabAOPS);
	SHADER_USE_PARAMETER_STRUCT(FRenderLabAOPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_ARRAY(FVector4f, AOSamples, [32])
		SHADER_PARAMETER(int32, AOSampleCount)
		SHADER_PARAMETER(float, AORadius)
		SHADER_PARAMETER(float, AOBias)
		SHADER_PARAMETER(int32, AODebugMode)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View) // 让 GPU Shader 可以读取当前视图的相机矩阵、相机位置、深度转换参数等信息
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InputViewport)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// AO可视化灰度shader
class FRenderLabAOVisualizePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRenderLabAOVisualizePS);
	SHADER_USE_PARAMETER_STRUCT(FRenderLabAOVisualizePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RawAOTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, AOSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(
	FRenderLabAOPS,
	"/Plugin/RenderLabRendering/Private/RenderLabAO.usf",
	"MainAOPS",
	SF_Pixel);

IMPLEMENT_GLOBAL_SHADER(
	FRenderLabAOVisualizePS,
	"/Plugin/RenderLabRendering/Private/RenderLabAO.usf",
	"MainAOVisualizePS",
	SF_Pixel);

static const TArray<FVector4f>& GetRenderLabAOKernel()
{
	static const TArray<FVector4f> Kernel = []()
	{
		FRandomStream Random(1337);
		TArray<FVector4f> Result;
		Result.SetNumUninitialized(32);

		for (int32 Index = 0; Index < 32; ++Index)
		{
			// 局部半球上均匀生成方向, 随机值是Z和Phi方位角
			const float Z = Random.FRand();
			const float Phi = 2.0f * PI * Random.FRand();
			const float XY = FMath::Sqrt(FMath::Max(0.0f, 1.0f - Z * Z));

			// 样本长度偏向中心,利用平方项实现
			const float U = Random.FRand();
			const float Scale = 0.1f + 0.9f * U * U;

			Result[Index] = FVector4f(
				XY * FMath::Cos(Phi) * Scale,  // x
				XY * FMath::Sin(Phi) * Scale,	// y
				Z * Scale,	// z
				0.0f);
		}

		return Result;
	}();

	return Kernel;
}

FScreenPassTexture AddRenderLabAOPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& Inputs)
{
	// 获取输入纹理
	const FScreenPassTexture Input = FScreenPassTexture::CopyFromSlice(
		GraphBuilder, Inputs.GetInput(EPostProcessMaterialInput::SceneColor));
	check(Input.IsValid());

	const FSceneTextureUniformParameters* SceneParameters = Inputs.SceneTextures.SceneTextures ? Inputs.SceneTextures.SceneTextures->GetContents() : nullptr;
	FRDGTextureRef DepthTexture = SceneParameters ? SceneParameters->SceneDepthTexture : nullptr;
	if (!DepthTexture)
	{
		return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
	}

	// 类似VkImageCreateInfo
	const FRDGTextureDesc RawAODesc = FRDGTextureDesc::Create2D(Input.Texture->Desc.Extent,
		PF_R16F, FClearValueBinding::None,
		TexCreate_RenderTargetable | TexCreate_ShaderResource);

	const FScreenPassRenderTarget RawAO(
		GraphBuilder.CreateTexture(RawAODesc, TEXT("RenderLab.RawAO")),
		Input.ViewRect,
		ERenderTargetLoadAction::ENoAction);

	FScreenPassRenderTarget Output = Inputs.OverrideOutput;
	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget::CreateFromInput(
			GraphBuilder,
			Input,
			ERenderTargetLoadAction::ENoAction,
			TEXT("RenderLab.AOOutput"));
	}

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
	TShaderMapRef<FScreenPassVS> VertexShader(ShaderMap);

	// pass1 写入RawAO
	{
		auto *Parameters = GraphBuilder.AllocParameters<FRenderLabAOPS::FParameters>();
		const TArray<FVector4f>& Kernel = GetRenderLabAOKernel();
		for (int32 Index = 0; Index < 32; ++Index) Parameters->AOSamples[Index] = Kernel[Index];
		Parameters->AOSampleCount = 32;
		Parameters->AORadius = 20.0f;
		Parameters->AOBias = 2.0f;
		Parameters->AODebugMode = 0;
		Parameters->View = View.ViewUniformBuffer;
		Parameters->SceneDepthTexture = DepthTexture;
		Parameters->InputViewport = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(RawAO));
		Parameters->RenderTargets[0] = RawAO.GetRenderTargetBinding();

		TShaderMapRef<FRenderLabAOPS> PixelShader(ShaderMap);

		AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("RenderLab.AO"), View,
			FScreenPassTextureViewport(RawAO), FScreenPassTextureViewport(RawAO),
			VertexShader, PixelShader, Parameters);
	}

	// pass2 采样RawAO，输出灰度画面
	{
		auto *Parameters = GraphBuilder.AllocParameters<FRenderLabAOVisualizePS::FParameters>();
		Parameters->RawAOTexture = RawAO.Texture;
		Parameters->AOSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp>::GetRHI();
		Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();

		TShaderMapRef<FRenderLabAOVisualizePS> PixelShader(ShaderMap);

		AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("RenderLab.AOVisualize"), View,
			FScreenPassTextureViewport(Output), FScreenPassTextureViewport(RawAO),
			VertexShader, PixelShader, Parameters);
	}

	return Output;
}
