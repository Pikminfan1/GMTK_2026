// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BaseGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScoreChanged, int32, NewScore);

/** Central kill signal. Killer may be null (environmental death). Combo, score,
 *  and pickup-spawning all hook this one event rather than each watching deaths. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyKilled, AActor*, Killer, AActor*, Victim);

/** Fired once when the survival timer expires - the player has won. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVictory);

/** Fired once when the player dies - the match is lost. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameOver);

/** Fired each time the remaining match time changes meaningfully (once per second),
 *  so UI can update a countdown without polling. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchTimeChanged, float, SecondsRemaining);

UENUM(BlueprintType)
enum class EMatchState : uint8
{
	WaitingToStart	UMETA(DisplayName = "Waiting To Start"),
	InProgress		UMETA(DisplayName = "In Progress"),
	Victory			UMETA(DisplayName = "Victory"),
	GameOver		UMETA(DisplayName = "Game Over")
};

/**
 * Holds the match data the UI actually reads - wave number, enemies remaining, score,
 * survival timer, and win/lose state. Kept separate from GameMode so UI widgets bind
 * here instead of reaching into GameMode directly.
 *
 * This is also the central hub for the OnEnemyKilled event: GameMode routes every
 * enemy death here, and combo/score/pickup systems subscribe to it.
 */
UCLASS()
class GMTK_2026_API ABaseGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ABaseGameState();

	virtual void Tick(float DeltaSeconds) override;

	// ---------- Match data (UI-facing) ----------

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	int32 CurrentWaveNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	int32 EnemiesRemaining = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	int32 Score = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Match")
	EMatchState MatchState = EMatchState::WaitingToStart;

	/** Seconds left before the player wins by survival. Counts down while InProgress. */
	UPROPERTY(BlueprintReadOnly, Category = "Match")
	float TimeRemaining = 0.f;

	// ---------- Events ----------

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnScoreChanged OnScoreChanged;

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnEnemyKilled OnEnemyKilled;

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnVictory OnVictory;

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnGameOver OnGameOver;

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnMatchTimeChanged OnMatchTimeChanged;

	// ---------- API (called by GameMode) ----------

	UFUNCTION(BlueprintCallable, Category = "Match")
	void AddScore(int32 Amount);

	/** Begin the survival countdown. Sets state to InProgress. */
	UFUNCTION(BlueprintCallable, Category = "Match")
	void StartMatch(float SurvivalSeconds);

	/** Broadcast a kill through the central hub. GameMode calls this. */
	UFUNCTION(BlueprintCallable, Category = "Match")
	void ReportEnemyKilled(AActor* Killer, AActor* Victim);

	/** End the match as a loss. Idempotent - only fires once. */
	UFUNCTION(BlueprintCallable, Category = "Match")
	void TriggerGameOver();

	UFUNCTION(BlueprintPure, Category = "Match")
	bool IsMatchOver() const { return MatchState == EMatchState::Victory || MatchState == EMatchState::GameOver; }

private:
	/** Whole seconds last broadcast, so OnMatchTimeChanged only fires on a tick change. */
	int32 LastBroadcastSecond = -1;
};
