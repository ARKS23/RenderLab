#pragma once

enum class ERenderLabExperiment : uint8
{
	Grayscale,
	Blur,
	InputDebug,
	AO
};

// 灰度设置
struct FRenderLabGrayscaleRenderState
{
	bool bEnabled = false;
	ERenderLabExperiment Experiment = ERenderLabExperiment::Grayscale;
	float Strength = 0.0f;
	float BlurMix = 1.0f;
};
