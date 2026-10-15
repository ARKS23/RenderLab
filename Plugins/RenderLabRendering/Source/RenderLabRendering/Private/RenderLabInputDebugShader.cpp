#include "RenderLabInputDebugShader.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "ScreenPass.h"
#include "ShaderParameterStruct.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "SceneTexturesConfig.h"
#include "RenderGraphBuilder.h"
#include "RHIStaticStates.h"
#include "SceneView.h"

class FRenderLabInputDebugPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRenderLabInputDebugPS);
	SHADER_USE_PARAMETER_STRUCT(FRenderLabInputDebugPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
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

IMPLEMENT_GLOBAL_SHADER(
	FRenderLabInputDebugPS,
	"/Plugin/RenderLabRendering/Private/RenderLabInputDebug.usf",
	"MainPS",
	SF_Pixel);

FScreenPassTexture AddRenderLabInputDebugPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& Inputs)
{
	// 获取当前后处理传入的颜色
	const FScreenPassTexture Input = FScreenPassTexture::CopyFromSlice(
		GraphBuilder, Inputs.GetInput(EPostProcessMaterialInput::SceneColor));
	check(Input.IsValid());

	// 使用Renderer指定的输出，未指定时创建工作纹理
	FScreenPassRenderTarget Output = Inputs.OverrideOutput;
	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget::CreateFromInput(
			GraphBuilder, Input, ERenderTargetLoadAction::ENoAction,
			TEXT("RenderLab.InputDebugOutput"));
	}

	if (!Inputs.SceneTextures.SceneTextures)
	{
		return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
	}

	const FSceneTextureUniformParameters* SceneParameters = Inputs.SceneTextures.SceneTextures->GetContents(); // 场景纹理的Uniform Buffer
	FRDGTextureRef DepthTexture = SceneParameters ? SceneParameters->SceneDepthTexture : nullptr;

	if (!DepthTexture)
	{
		return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
	}

	// 写入参数
	auto *Parameters = GraphBuilder.AllocParameters<FRenderLabInputDebugPS::FParameters>();
	Parameters->View = View.ViewUniformBuffer; // UE自带的Camera UBO
	Parameters->SceneDepthTexture = DepthTexture;
	Parameters->InputViewport = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Input));
	Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();

	// vs + ps
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.GetFeatureLevel());
	TShaderMapRef<FScreenPassVS> VertexShader(ShaderMap);
	TShaderMapRef<FRenderLabInputDebugPS> PixelShader(ShaderMap);

	// 绘制
	AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("RenderLab.InputDebug"), View,
		FScreenPassTextureViewport(Output), FScreenPassTextureViewport(Input),
		VertexShader, PixelShader, Parameters);

	// 输出交回后处理链
	return Output;
}
