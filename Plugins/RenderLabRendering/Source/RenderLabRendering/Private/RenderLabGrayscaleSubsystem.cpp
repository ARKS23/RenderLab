#include "../Public/RenderLabGrayscaleSubsystem.h"

#include "AudioMixerBlueprintLibrary.h"
#include "Engine/World.h"
#include "RenderLabGrayscaleViewExtension.h"
#include "RenderLabGrayscaleRenderState.h"

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

	RenderState = MakeShared<FRenderLabGrayscaleRenderState, ESPMode::ThreadSafe>();
	ViewExtension = FSceneViewExtensions::NewExtension<FRenderLabGrayscaleViewExtension>(GetWorld(), RenderState.ToSharedRef());
	bAcceptingSettings = true;

	UE_LOG(LogTemp, Display,
		TEXT("[RenderLabRendering] Created view extension for %s"),
		*GetNameSafe(GetWorld()));
}

void URenderLabGrayscaleSubsystem::Deinitialize()
{
	check(IsInGameThread());
	bAcceptingSettings = false;
	bGameEnabled = false;
	GameStrength = 0.0f;
	if (RenderState.IsValid())
	{
		const TSharedRef<FRenderLabGrayscaleRenderState> State = RenderState.ToSharedRef();
		ENQUEUE_RENDER_COMMAND(DisableRenderLabGrayscale)(
			[State](FRHICommandListImmediate&)
			{
				check(IsInRenderingThread());
				State->bEnabled = false;
				State->Strength = 0.0f;
			}
		);
	}

	UE_LOG(LogTemp, Display,
		TEXT("[RenderLabRendering] Releasing view extension for %s"),
		*GetNameSafe(GetWorld()));

	// 释放对 View Extension 的持有
	ViewExtension.Reset();
	RenderState.Reset();
	Super::Deinitialize();
}

void URenderLabGrayscaleSubsystem::SetGrayscaleSettings(bool bEnabled, float Strength)
{
	check(IsInGameThread());
	if (!bAcceptingSettings || !RenderState.IsValid()) return;

	const float SafeStrength = FMath::IsFinite(Strength) ? FMath::Clamp(Strength, 0.0f, 1.0f) : 0.0f;
	bGameEnabled = bEnabled;
	GameStrength = SafeStrength;

	// 给渲染线程，需要mirror
	const FRenderLabGrayscaleRenderState Snapshot(bEnabled, ERenderLabExperiment::Grayscale, SafeStrength, 1.0f);
	const TSharedRef<FRenderLabGrayscaleRenderState> State = RenderState.ToSharedRef();
	ENQUEUE_RENDER_COMMAND(UpdateRenderLabGrayscaleSettings)(
		[State, Snapshot](FRHICommandListImmediate&)
		{
			*State = Snapshot;
		}
	);
}

void URenderLabGrayscaleSubsystem::SetBlurSettings(bool bEnable, float Mix)
{
	check(IsInGameThread());
	if (!bAcceptingSettings || !RenderState.IsValid()) return;

	const float SafeMix = FMath::IsFinite(Mix) ? FMath::Clamp(Mix, 0.0f, 1.0f) : 0.0f;
	bGameEnabled = bEnable;
	GameStrength = SafeMix;
	const FRenderLabGrayscaleRenderState Snapshot(bEnable, ERenderLabExperiment::Blur, GameStrength, SafeMix);
	const TSharedRef<FRenderLabGrayscaleRenderState> State = RenderState.ToSharedRef();
	ENQUEUE_RENDER_COMMAND(UpdateRenderLabBlurSettings)(
		[State, Snapshot](FRHICommandListImmediate&)
		{
			check(IsInRenderingThread());
			*State = Snapshot;
		}
	);
}

void URenderLabGrayscaleSubsystem::SetInputDebugSettings(bool bEnable)
{
	check(IsInGameThread());
	if (!bAcceptingSettings || !RenderState.IsValid()) return;

	const FRenderLabGrayscaleRenderState Snapshot(bEnable, ERenderLabExperiment::InputDebug, GameStrength, BlurStrength);
	const TSharedRef<FRenderLabGrayscaleRenderState> State = RenderState.ToSharedRef();
	ENQUEUE_RENDER_COMMAND(UpdateRenderLabInputDebugSettings)(
		[State, Snapshot](FRHICommandListImmediate&)
		{
			*State = Snapshot;
		}
	);
}

void URenderLabGrayscaleSubsystem::SetAOSettings(bool bEnable)
{
	check(IsInGameThread());
	if (!bAcceptingSettings || !RenderState.IsValid()) return;

	const FRenderLabGrayscaleRenderState Snapshot(bEnable, ERenderLabExperiment::AO, GameStrength, BlurStrength);
	const TSharedRef<FRenderLabGrayscaleRenderState> State = RenderState.ToSharedRef();
	ENQUEUE_RENDER_COMMAND(UpdateRenderLabAOSettings)(
		[State, Snapshot](FRHICommandListImmediate&)
		{
			*State = Snapshot;
		}
	);
}
