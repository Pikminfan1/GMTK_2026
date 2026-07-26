// Fill out your copyright notice in the Description page of Project Settings.

#include "World/ReloadPoint.h"
#include "Components/BillboardComponent.h"

AReloadPoint::AReloadPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

#if WITH_EDITORONLY_DATA
	EditorSprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("EditorSprite"));
	if (EditorSprite)
	{
		EditorSprite->SetupAttachment(Root);
	}
#endif
}
