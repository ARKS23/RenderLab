// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Lab0Material.generated.h"

UCLASS()
class RENDERLAB_API ALab0Material : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ALab0Material();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION(BlueprintCallable, Category="Material Lab")
	void PlayRoughnessPulse();
	
	UFUNCTION(BlueprintCallable, Category="Material Lab")
	void SetLabColor(FLinearColor Color);
	
	UFUNCTION(BlueprintCallable, Category="Material Lab")
	void SetLabEmissive(FLinearColor Color, float Strength);
	
	UFUNCTION(BlueprintCallable, Category="Material Lab")
	void ResetAppearance();
	
protected:
	UPROPERTY(VisibleAnywhere, Category="Material Lab")
	TObjectPtr<UStaticMeshComponent> Mesh;
	
	UPROPERTY(EditAnywhere, Category="Material Lab")
	float InitialRoughness = 0.15f;
	
	bool bPlayRoughnessPulse = false;
	
	UPROPERTY(EditAnywhere, Category="Material Lab")
	FLinearColor InitialBaseColor = FLinearColor(0.18f, 0.18f, 0.18f, 1.0f);
	
	// MID
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RuntimeMaterial;
};
