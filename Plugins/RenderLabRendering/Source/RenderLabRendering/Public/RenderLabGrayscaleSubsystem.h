#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SharedPointer.h"
#include "RenderLabGrayscaleSubsystem.generated.h"

class FSceneViewExtensionBase;


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

protected:
	// 仅在 Game 和 PIE World 中启用
	virtual bool DoesSupportWorldType(
		const EWorldType::Type WorldType) const override;

private:
	// 持有 Scene View Extension，保证其生命周期
	TSharedPtr<FSceneViewExtensionBase, ESPMode::ThreadSafe> ViewExtension;
};