#pragma once

#include "ScreenPass.h"

class FRDGBuilder;
class FSceneView;
struct FPostProcessMaterialInputs;

FScreenPassTexture AddRenderLabAOPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& Inputs);
