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

/**
 * Owned by ABaseGameMode. Holds the wave state machine: current wave index, spawning,
 * and remaining-enemy tracking. GameState/UI bind to the delegates below rather than
 * polling this component directly.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class GMTK_2026_API UWaveManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UWaveManagerComponent();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	TArray<TObjectPtr<UWaveDataAsset>> Waves;
	
	UPROPERTY(BlueprintAssignable, Category = "Wave")
	FOnWaveStarted OnWaveStarted;
	
	UPROPERTY(BlueprintAssignable, Category = "Wave")
	FOnWaveCompleted OnWaveCompleted;
	
	UPROPERTY(BlueprintAssignable, Category = "Waves")
	FOnAllWavesCompleted OnAllWavesCompleted;
	
	UFUNCTION(BlueprintCallable, Category = "Wave")
	void StartNextWave();
	
	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetCurrentWaveNumber() const { return CurrentWaveIndex + 1; }

	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetEnemiesRemaining() const { return EnemiesRemaining; }

protected:
	virtual void BeginPlay() override;

	void SpawnEnemyEntry(const FWaveEnemyEntry& Entry);

	UFUNCTION()
	void HandleEnemyDeath(AActor* DeadActor);

	UPROPERTY()
	TArray<TObjectPtr<AEnemySpawnPoint>> CachedSpawnPoints;

	UPROPERTY()
	int32 CurrentWaveIndex = -1;

	UPROPERTY()
	int32 EnemiesRemaining = 0;

public:	
	// Called every frame
	//virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
