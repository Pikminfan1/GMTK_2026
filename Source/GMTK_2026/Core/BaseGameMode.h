// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BaseGameMode.generated.h"

class UWaveManagerComponent;
class ABaseGameState;

/**
 * Owns overall match flow. Creates the WaveManagerComponent, starts the survival
 * timer on the GameState, wires wave events into the GameState's UI-facing data,
 * and routes enemy/player deaths into the central kill and game-over events.
 *
 * GameState holds the data and events; GameMode is the authority that drives the
 * transitions between them.
 */
UCLASS()
class GMTK_2026_API ABaseGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABaseGameMode();

	UFUNCTION(BlueprintPure, Category = "Waves")
	UWaveManagerComponent* GetWaveManager() const { return WaveManagerComponent; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waves", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWaveManagerComponent> WaveManagerComponent;

	/** How long the player must survive to win, in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	float SurvivalTimeSeconds = 300.f;

	/** Points awarded per enemy killed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	int32 ScorePerKill = 100;

	/** Score multiplier bonus per point of combo. Final points for a kill are
	 *  ScorePerKill * (1 + ComboCount * ComboScoreBonusPerKill). 0.1 = +10% per combo,
	 *  so a 10-combo kill is worth double. 0 = combo doesn't affect score. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	float ComboScoreBonusPerKill = 0.1f;

	// ---------- Event handlers ----------

	UFUNCTION()
	void HandleWaveStarted(int32 WaveNumber);

	UFUNCTION()
	void HandleWaveCompleted(int32 WaveNumber);

	UFUNCTION()
	void HandleAllWavesCompleted();

	/** An enemy died (relayed from the wave manager). Routes into the central kill
	 *  event + awards score. */
	UFUNCTION()
	void HandleEnemyDied(AActor* DeadEnemy);

	/** The player died. Triggers game over. */
	UFUNCTION()
	void HandlePlayerDeath(AActor* DeadPlayer);

private:
	UPROPERTY()
	TObjectPtr<ABaseGameState> CachedGameState;
};
