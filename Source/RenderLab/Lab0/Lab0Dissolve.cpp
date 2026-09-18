// Fill out your copyright notice in the Description page of Project Settings.


#include "Lab0Dissolve.h"

// Sets default values
ALab0Dissolve::ALab0Dissolve()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
}

// Called when the game starts or when spawned
void ALab0Dissolve::BeginPlay()
{
	Super::BeginPlay();
	
	if (!Mesh->GetStaticMesh() || !Mesh->GetMaterial(0))
	{
		UE_LOG(LogTemp, Warning, TEXT("Assign a mesh and material first."));
		return;
	}
	
	// 在对应的材质索引创建材质
	RuntimeMaterial = Mesh->CreateDynamicMaterialInstance(0);
	
	// 候选Threshold
	for (int i = 0; i < 30; ++i)
	{
		float RandomFloat = FMath::FRandRange(0.001f, 0.3f);
		ThresholdArray.Add(RandomFloat);
	}
}

// Called every frame
void ALab0Dissolve::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	AccumulatedTime += DeltaTime;
	if (AccumulatedTime < 1.f) return;
	AccumulatedTime -= 1.f;
	
	float Index = FMath::RandRange(0, 29);
	RuntimeMaterial->SetScalarParameterValue(TEXT("DissolveThreshold"), ThresholdArray[Index]);
}

