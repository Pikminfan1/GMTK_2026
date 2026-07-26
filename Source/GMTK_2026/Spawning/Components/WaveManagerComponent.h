// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WaveManagerComponent.generated.h"

class UWaveDataAsset;
class AEnemySpawnPoint;
class AEnemyCharacter;
struct FWaveEnemyEntry;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveStarted, int32, WaveNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveCompleted, int32, WaveNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllWavesCompleted);

/** Relays which enemy died, so the GameMode can route it into the central kill
 *  event with a killer. The wave manager itself doesn't know the killer - it only
 *  knows the victim - so killer resolution happens upstream. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveEnemyDied, AActor*, DeadEnemy);

/** Fires when the between-wave countdown to the next auto-advance begins, passing how
 *  many seconds until the next wave. UI can show a "next wave in N" timer. Does not
 *  fire if auto-advance is off. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNextWaveCountdownStarted, float, SecondsUntilNextWave);

/**
 * Owned by ABaseGameMode. Holds the wave state machine: current wave index, spawning,
 * and remaining-enemy tracking. GameState/UI bind to the delegates below rather than
 * polling this component directly.
 *
 * Waves auto-advance: when a wave is cleared, the next starts automatically after
 * TimeBetweenWaves. The shootable WaveStartTrigger can still call StartNextWave() to
 * begin the next wave EARLY, which cancels the pending auto-advance so there's no
 * double-start.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class GMTK_2026_API UWaveManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWaveManagerComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	TArray<TObjectPtr<UWaveDataAsset>> Waves;

	/** When true, clearing a wave auto-starts the next after TimeBetweenWaves. The
	 *  trigger still works as an optional early-start. When false, waves ONLY advance
	 *  via StartNextWave (the trigger), like the old behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	bool bAutoAdvanceWaves = true;

	/** Delay between clearing a wave and the next auto-starting. Also the window during
	 *  which the player can shoot the trigger to start early. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.0"))
	float TimeBetweenWaves = 5.f;

	UPROPERTY(BlueprintAssignable, Category = "Wave")
	FOnWaveStarted OnWaveStarted;

	UPROPERTY(BlueprintAssignable, Category = "Wave")
	FOnWaveCompleted OnWaveCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Waves")
	FOnAllWavesCompleted OnAllWavesCompleted;

	/** Fires for every enemy death with the victim actor. */
	UPROPERTY(BlueprintAssignable, Category = "Wave")
	FOnWaveEnemyDied OnEnemyDied;

	/** Fires when the between-wave countdown begins (auto-advance only). */
	UPROPERTY(BlueprintAssignable, Category = "Wave")
	FOnNextWaveCountdownStarted OnNextWaveCountdownStarted;

	/** Start the next wave. Safe to call from a shootable "start wave" actor. Does
	 *  nothing if a wave is already active (enemies still alive), unless bForce.
	 *  Cancels any pending auto-advance timer so it can't double-start. */
	UFUNCTION(BlueprintCallable, Category = "Wave")
	void StartNextWave(bool bForce = false);

	/** True if a wave is currently active with enemies still alive. */
	UFUNCTION(BlueprintPure, Category = "Waves")
	bool IsWaveActive() const { return EnemiesRemaining > 0; }

	/** True during the between-wave gap (wave cleared, next not yet started). */
	UFUNCTION(BlueprintPure, Category = "Waves")
	bool IsBetweenWaves() const;

	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetCurrentWaveNumber() const { return CurrentWaveIndex + 1; }

	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetEnemiesRemaining() const { return EnemiesRemaining; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void SpawnEnemyEntry(const FWaveEnemyEntry& Entry);

	UFUNCTION()
	void HandleEnemyDeath(AActor* DeadActor);

	UPROPERTY()
	TArray<TObjectPtr<AEnemySpawnPoint>> CachedSpawnPoints;

	UPROPERTY()
	int32 CurrentWaveIndex = -1;

	UPROPERTY()
	int32 EnemiesRemaining = 0;

private:
	FTimerHandle NextWaveTimer;
};
