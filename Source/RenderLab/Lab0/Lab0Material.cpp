// Fill out your copyright notice in the Description page of Project Settings.


#include "Lab0Material.h"

// Sets default values
ALab0Material::ALab0Material()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
}

// Called when the game starts or when spawned
void ALab0Material::BeginPlay()
{
	Super::BeginPlay();
	
	if (!Mesh->GetStaticMesh() || !Mesh->GetMaterial(0))
	{
		UE_LOG(LogTemp, Warning, TEXT("Assign a mesh and material first."));
		return;
	}
	
	// 在对应的材质索引创建材质
	RuntimeMaterial = Mesh->CreateDynamicMaterialInstance(0);
	if (RuntimeMaterial)
	{
		RuntimeMaterial->SetScalarParameterValue(TEXT("Roughness"), FMath::Clamp(InitialRoughness, 0.f, 1.f));
	}
}

void ALab0Material::PlayRoughnessPulse()
{
	if (!RuntimeMaterial || bPlayRoughnessPulse)
	{
		return;
	}
	
	bPlayRoughnessPulse = true;
}

void ALab0Material::SetLabColor(FLinearColor Color)
{
	if (!RuntimeMaterial) return;
	RuntimeMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
}

void ALab0Material::SetLabEmissive(FLinearColor Color, float Strength)
{
	if (!RuntimeMaterial) return;
	RuntimeMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
	RuntimeMaterial->SetScalarParameterValue(
		TEXT("EmissiveStrength"), FMath::Max(0.0f, Strength));
}

void ALab0Material::ResetAppearance()
{
	bPlayRoughnessPulse = true;
	if (!RuntimeMaterial) return;
	SetLabColor(InitialBaseColor);
	SetLabEmissive(FLinearColor::White, 0.0f);
	RuntimeMaterial->SetScalarParameterValue(
		TEXT("Roughness"), FMath::Clamp(InitialRoughness, 0.0f, 1.0f));
}

// Called every frame
void ALab0Material::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (!RuntimeMaterial) return;
	
	if (!bPlayRoughnessPulse)
	{
		float Value = FMath::Cos(InitialRoughness + GetWorld()->GetTimeSeconds());
		Value = FMath::Abs(Value);
		RuntimeMaterial->SetScalarParameterValue(TEXT("Roughness"), Value);
	}
}

