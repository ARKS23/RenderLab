// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "OutlineSelector.generated.h"

UCLASS()
class RENDERLAB_API AOutlineSelector : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AOutlineSelector();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	void SetSelectedComponent(UPrimitiveComponent* New);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	TWeakObjectPtr<UPrimitiveComponent> Selected;
	bool bSavedDepth = false;
	int32 SavedID = 0;
};
