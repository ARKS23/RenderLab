#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SharedPointer.h"
#include "RenderLabGrayscaleSubsystem.generated.h"

class FSceneViewExtensionBase;
struct FRenderLabGrayscaleRenderState;


/**
 * 管理灰度 Scene View Extension 的 World Subsystem。
 * 随 UWorld 创建/销毁，负责 View Extension 的生命周期。
 */
UCLASS()
class RENDERLABRENDERING_API URenderLabGrayscaleSubsystem
	: public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// World Subsystem 初始化时创建并注册 View Extension
	virtual void Initialize(
		FSubsystemCollectionBase& Collection) override;

	// World Subsystem 销毁时释放 View Extension
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable)
	void SetGrayscaleSettings(bool bEnabled = false, float Strength = 0.0f);

	UFUNCTION(BlueprintCallable)
	void SetBlurSettings(bool bEnable = false, float Mix = 1.0f);

	UFUNCTION(BlueprintCallable)
	void SetInputDebugSettings(bool bEnable = false);

protected:
	// 仅在 Game 和 PIE World 中启用
	virtual bool DoesSupportWorldType(
		const EWorldType::Type WorldType) const override;

private:
	// 持有 Scene View Extension，保证其生命周期
	TSharedPtr<FSceneViewExtensionBase, ESPMode::ThreadSafe> ViewExtension;
	TSharedPtr<FRenderLabGrayscaleRenderState, ESPMode::ThreadSafe> RenderState; // State根资源

	// Game Thread记录
	bool bAcceptingSettings = false;
	bool bGameEnabled = false;
	float GameStrength = 0.0f;
	float BlurStrength = 1.0f;
};
