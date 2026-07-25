// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthOrbSpawnerComponent.generated.h"

class AHealthOrb;

/**
 * Drops a scatter of health orbs when its OWNER dies. Lives on an enemy (baked into
 * AEnemyCharacter by default), so each enemy archetype controls its own drop via this
 * component's settings.
 *
 * The owner calls NotifyOwnerDied() from its death handler.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GMTK_2026_API UHealthOrbSpawnerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthOrbSpawnerComponent();

	/** Call from the owner's death handler to drop the burst. Guards against double-drop. */
	UFUNCTION(BlueprintCallable, Category = "Orbs")
	void NotifyOwnerDied();

	/** Spawn a burst at an explicit location. Exposed so bosses/crates can reuse it. */
	UFUNCTION(BlueprintCallable, Category = "Orbs")
	void SpawnOrbBurst(const FVector& Location);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbs")
	TSubclassOf<AHealthOrb> OrbClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbs", meta = (ClampMin = "0"))
	int32 MinOrbsPerKill = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbs", meta = (ClampMin = "0"))
	int32 MaxOrbsPerKill = 6;

	/** Outward toss speed range. A WIDE gap here (e.g. 200-700) is what makes orbs
	 *  travel varying distances so they don't land on a perfect circle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbs")
	float ScatterSpeedMin = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbs")
	float ScatterSpeedMax = 700.f;

	/** Random horizontal offset applied to each orb's spawn point, so a big burst
	 *  doesn't all originate from one spot. 0 = all from the same point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbs", meta = (ClampMin = "0"))
	float SpawnJitterRadius = 40.f;

	/** Height above the death location to spawn, so orbs don't start inside the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbs")
	float SpawnHeightOffset = 50.f;

private:
	bool bHasDropped = false;
};
