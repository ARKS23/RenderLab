// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Lab2PostProcessMaterial.generated.h"

UCLASS()
class RENDERLAB_API ALab2PostProcessMaterial : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ALab2PostProcessMaterial();
	
	UFUNCTION(BlueprintCallable, Category="Post Process Lab")
	void SetGrayscaleAmount(float Amount);
	
	UFUNCTION(BlueprintCallable, Category = "Post Process Lab")
	void ResetEffect() { SetGrayscaleAmount(0.0f); }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
protected:
	UPROPERTY(VisibleAnywhere, Category="Post Process Lab")
	TObjectPtr<class UPostProcessComponent> PostProcess;
	
	UPROPERTY(EditAnywhere, Category = "Post Process Lab")
	TObjectPtr<class UMaterialInterface> SourceMaterial;
	
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> RuntimeMaterial;
};
