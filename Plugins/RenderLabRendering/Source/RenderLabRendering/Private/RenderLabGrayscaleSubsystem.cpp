#include "../Public/RenderLabGrayscaleSubsystem.h"

#include "Engine/World.h"
#include "RenderLabGrayscaleViewExtension.h"

bool URenderLabGrayscaleSubsystem::DoesSupportWorldType(
	const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE;
}

void URenderLabGrayscaleSubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ViewExtension = FSceneViewExtensions::NewExtension<FRenderLabGrayscaleViewExtension>(GetWorld());

	UE_LOG(LogTemp, Display,
		TEXT("[RenderLabRendering] Created view extension for %s"),
		*GetNameSafe(GetWorld()));
}

void URenderLabGrayscaleSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Display,
		TEXT("[RenderLabRendering] Releasing view extension for %s"),
		*GetNameSafe(GetWorld()));

	// 释放对 View Extension 的持有
	ViewExtension.Reset();
	Super::Deinitialize();
}