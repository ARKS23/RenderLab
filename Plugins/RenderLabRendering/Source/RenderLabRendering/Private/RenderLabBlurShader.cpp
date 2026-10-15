#include "RenderLabBlurShader.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "ScreenPass.h"
#include "ShaderParameterStruct.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RenderGraphBuilder.h"
#include "RHIStaticStates.h"
#include "SceneView.h"

// Blur shader
class FRenderLabBlurPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRenderLabBlurPS);
	SHADER_USE_PARAMETER_STRUCT(FRenderLabBlurPS, FGlobalShader);

	// C++ -> HLSL的接口
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture) // 输入纹理
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)  // 采样器
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InputViewport) // 打包并绑定整个“输入视口”的所有物理尺寸和坐标信息，然后无缝传递给着色器中使用
		SHADER_PARAMETER(FVector2f, BlurDirection) // 采样方向
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	// 控制Shader编译条件
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// Composite Shader
class FRenderLabBlurCompositePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRenderLabBlurCompositePS);
	SHADER_USE_PARAMETER_STRUCT(FRenderLabBlurCompositePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OriginalTexture) // 原图纹理
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BlurredTexture) // 模糊纹理
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)  // 采样器
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InputViewport)
		SHADER_PARAMETER(float, Mix)					  // 模糊强度
		RENDER_TARGET_BINDING_SLOTS()						  // 输出到哪里
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// 注册Shader
IMPLEMENT_GLOBAL_SHADER(
	FRenderLabBlurPS,
	"/Plugin/RenderLabRendering/Private/RenderLabBlur.usf",
	"MainPS",
	SF_Pixel);

IMPLEMENT_GLOBAL_SHADER(
	FRenderLabBlurCompositePS,
	"/Plugin/RenderLabRendering/Private/RenderLabComposite.usf",
	"MainPS",
	SF_Pixel);

// 模糊单向pass
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

	// 分配并填写 Pixel Shader 参数
	auto *Parameters = GraphBuilder.AllocParameters<FRenderLabBlurPS::FParameters>();
	Parameters->InputTexture = Input.Texture;
	Parameters->InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters->InputViewport = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Input)); // 获取输入viewport再拿到screenViewport信息
	Parameters->BlurDirection = Direction;
	Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
	TShaderMapRef<FScreenPassVS> VertexShader(ShaderMap);
	TShaderMapRef<FRenderLabBlurPS> PixelShader(ShaderMap);

	AddDrawScreenPass(
		GraphBuilder, RDG_EVENT_NAME("%s", PassName), View,
		FScreenPassTextureViewport(Output), FScreenPassTextureViewport(Input),
		VertexShader, PixelShader, Parameters);
}

// 原图和模糊合成Pass
static void AddRenderLabBlurCompositePass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FScreenPassTexture OriginalTexture,
	FScreenPassTexture BlurredTexture,
	FScreenPassRenderTarget Output,
	float Mix)
{
	check(OriginalTexture.IsValid() && BlurredTexture.IsValid() && Output.IsValid());

	// 两张输入纹理需要同一个采样UV，坐标布局一致
	check(OriginalTexture.Texture->Desc.Extent == BlurredTexture.Texture->Desc.Extent);
	check(OriginalTexture.ViewRect == BlurredTexture.ViewRect);

	// 绘制读取的纹理与写入目标分离
	check(OriginalTexture.Texture != Output.Texture);
	check(BlurredTexture.Texture != Output.Texture);

	auto *Parameters = GraphBuilder.AllocParameters<FRenderLabBlurCompositePS::FParameters>();
	Parameters->OriginalTexture = OriginalTexture.Texture;
	Parameters->BlurredTexture = BlurredTexture.Texture;
	Parameters->InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	Parameters->InputViewport = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(OriginalTexture));
	Parameters->Mix = Mix;
	Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
	TShaderMapRef<FScreenPassVS> VertexShader(ShaderMap);
	TShaderMapRef<FRenderLabBlurCompositePS> PixelShader(ShaderMap);

	AddDrawScreenPass(
		GraphBuilder, RDG_EVENT_NAME("RenderLab.BlurComposite"), View,
		FScreenPassTextureViewport(Output), FScreenPassTextureViewport(OriginalTexture),
		VertexShader, PixelShader, Parameters);
}

FScreenPassTexture AddRenderLabBlurPasses(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& Inputs,
	float Mix)
{
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

	// 横向模糊输出纹理
	const FScreenPassRenderTarget TempX = FScreenPassRenderTarget::CreateFromInput(
		GraphBuilder, Input, ERenderTargetLoadAction::ENoAction, TEXT("RenderLab.BlurXTexture"));
	// 纵向模糊输出纹理
	const FScreenPassRenderTarget TempY = FScreenPassRenderTarget::CreateFromInput(
		GraphBuilder, Input, ERenderTargetLoadAction::ENoAction, TEXT("RenderLab.BlurYTexture"));

	// 横向模糊
	AddRenderLabBlurDirectionPass(GraphBuilder, View,
		Input, TempX, FVector2f(1.0f, 0.0f),
		TEXT("RenderLab.BlurX"));
	// 纵向模糊
	AddRenderLabBlurDirectionPass(GraphBuilder, View,
		TempX, TempY, FVector2f(0.0f, 1.0f),
		TEXT("RenderLab.BlurY"));
	// 合成模糊
	AddRenderLabBlurCompositePass(GraphBuilder, View, Input, TempY, Output, Mix);
	return Output;
}
