// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Damageable.h"
#include "WaveStartTrigger.generated.h"

class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWaveStartTriggered);

/**
 * A shootable object that starts the next wave when hit. Implements IDamageable so
 * the player's existing weapon (which only talks to IDamageable) can activate it with
 * no special-casing - shooting it is the same code path as shooting an enemy.
 *
 * Fires OnWaveStartTriggered so a team member can hook VFX/SFX to the activation
 * without touching this class.
 */
UCLASS()
class GMTK_2026_API AWaveStartTrigger : public AActor, public IDamageable
{
	GENERATED_BODY()

public:
	AWaveStartTrigger();

	//~ IDamageable
	virtual bool ApplyDamage_Implementation(float DamageAmount, AController* EventInstigator, AActor* DamageCauser) override;

	/** Fires when the trigger is successfully activated (before the wave starts), for FX. */
	UPROPERTY(BlueprintAssignable, Category = "Wave")
	FOnWaveStartTriggered OnWaveStartTriggered;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** If true, the trigger only works while no wave is active (between waves). Prevents
	 *  the player skipping ahead mid-fight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	bool bOnlyBetweenWaves = true;

	/** If true, re-arms after each use so it can start every subsequent wave. If false,
	 *  it fires once and then ignores further hits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	bool bReusable = true;

	/** Blueprint hook to react to activation (change material, play anim, etc.). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Wave")
	void OnActivatedVisual();

private:
	bool bHasFired = false;
};
