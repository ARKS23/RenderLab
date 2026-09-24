#pragma once

#include "ScreenPass.h"

class FRDGBuilder;
class FSceneView;
struct FPostProcessMaterialInputs;

FScreenPassTexture AddRenderLabGrayscalePass(
	FRDGBuilder& GraphBuilder, 
	const FSceneView& View, 
	const FPostProcessMaterialInputs& Inputs);