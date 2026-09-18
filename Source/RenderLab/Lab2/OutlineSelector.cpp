// Fill out your copyright notice in the Description page of Project Settings.


#include "OutlineSelector.h"

// Sets default values
AOutlineSelector::AOutlineSelector()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AOutlineSelector::BeginPlay()
{
	Super::BeginPlay();
	
}

void AOutlineSelector::SetSelectedComponent(UPrimitiveComponent* New)
{
	// 安全性检查
	if (!IsValid((New))) New = nullptr;
	if (Selected.Get() == New) return;
	
	// 恢复旧值
	if (UPrimitiveComponent* Old = Selected.Get())
	{
		Old->SetCustomDepthStencilValue(SavedID);
		Old->SetRenderCustomDepth(bSavedDepth);
	}
	
	// 更新新值
	Selected = New;
	if (!New) return;
	bSavedDepth = New->bRenderCustomDepth;
	SavedID = New->CustomDepthStencilValue;
	New->SetCustomDepthStencilValue(1);
	New->SetRenderCustomDepth(true);
}

// Called every frame
void AOutlineSelector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	APlayerController *PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		SetSelectedComponent(nullptr);
		return;
	}
	
	// 获取摄像机位置和方向
	FVector Start;
	FRotator Rotation;
	PC->GetPlayerViewPoint(Start, Rotation);
	
	// 碰撞参数: 统计名字，简单碰撞，忽略自己
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OutlineSelect), false, this);
	if (APawn* Pawn = PC->GetPawn()) Params.AddIgnoredActor(Pawn);
	
	// 碰撞检测
	FHitResult Hit;
	GetWorld()->LineTraceSingleByChannel(
		Hit, Start, Start + Rotation.Vector() * 3000.0f, ECC_Visibility, Params);
	AActor* HitActor = Hit.GetActor();
	const bool bSelectable = IsValid(HitActor) && HitActor->ActorHasTag("OutlineSelectable");
	SetSelectedComponent(bSelectable ? Hit.GetComponent() : nullptr);
}

