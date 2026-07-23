// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawnPoint.generated.h"

class AEnemyCharacter;
class UBillboardComponent;

/**
 * Simple placeable marker for where enemies can spawn. Passive by design - it just
 * marks a location (and optionally overrides which enemy class spawns there);
 * UWaveManagerComponent does the actual spawning.
 */
UCLASS()
class GMTK_2026_API AEnemySpawnPoint : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEnemySpawnPoint();
	
	/** Optional - if set, this spawn point only spawns this specific enemy class, overriding whatever the wave entry says. */
	/* Double check code in WaveManager to make sure that it is pulling enemy classes, and not converting them to the override class */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
	TSubclassOf<AEnemyCharacter> OverrideEnemyClass;

protected:
#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "Spawning")
	TObjectPtr<UBillboardComponent> EditorIcon;
#endif
	

};
