// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ReloadPoint.generated.h"

class UBillboardComponent;

/**
 * A predetermined location the active reload zone can occupy. Place several of these
 * around the level; the single AReloadZone hops between them. This actor is just a
 * marker - it has no logic and no runtime cost beyond existing to be found.
 */
UCLASS()
class GMTK_2026_API AReloadPoint : public AActor
{
	GENERATED_BODY()

public:
	AReloadPoint();

#if WITH_EDITORONLY_DATA
	/** Editor-only sprite so the point is visible when placing it. */
	UPROPERTY()
	TObjectPtr<UBillboardComponent> EditorSprite;
#endif
};
