// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Lab0Dissolve.generated.h"

UCLASS()
class RENDERLAB_API ALab0Dissolve : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ALab0Dissolve();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere, Category="Material Lab")
	TObjectPtr<UStaticMeshComponent> Mesh;
	
	// MID
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RuntimeMaterial;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	TArray<float> ThresholdArray;
	float AccumulatedTime = 0;
};
