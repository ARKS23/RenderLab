// Fill out your copyright notice in the Description page of Project Settings.


#include "Lab2PostProcessMaterial.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

// Sets default values
ALab2PostProcessMaterial::ALab2PostProcessMaterial()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	PostProcess = CreateDefaultSubobject<UPostProcessComponent>("PostProcess");
	SetRootComponent(PostProcess);
	PostProcess->bUnbound = true;
	PostProcess->bEnabled = true;
	PostProcess->BlendWeight = 1.0f;
}

// Called when the game starts or when spawned
void ALab2PostProcessMaterial::BeginPlay()
{
	Super::BeginPlay();
	
	if (!ensureMsgf(IsValid(SourceMaterial.Get()), TEXT("Assign a post-process material"))) return;
	
	RuntimeMaterial = UMaterialInstanceDynamic::Create(SourceMaterial.Get(), this);
	if (!RuntimeMaterial) return;
	
	ResetEffect();
	PostProcess->AddOrUpdateBlendable(RuntimeMaterial.Get(), 1.0f);
}

void ALab2PostProcessMaterial::SetGrayscaleAmount(float Amount)
{
	if (RuntimeMaterial)
	{
		RuntimeMaterial->SetScalarParameterValue(TEXT("GrayscaleAmount"), FMath::Clamp(Amount, 0.0f, 1.0f));
	}
}

// Called every frame
void ALab2PostProcessMaterial::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	
}

