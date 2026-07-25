// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/BaseGameState.h"
#include "Utility/LogChannels.h"

ABaseGameState::ABaseGameState()
{
	// The survival timer needs a tick. It's cheap - one float decrement and a
	// whole-second comparison - so no need for a separate timer handle.
	PrimaryActorTick.bCanEverTick = true;
}

void ABaseGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (MatchState != EMatchState::InProgress)
	{
		return;
	}

	TimeRemaining = FMath::Max(0.f, TimeRemaining - DeltaSeconds);

	// Only tell the UI when the whole-second value changes, not every frame.
	const int32 WholeSecond = FMath::CeilToInt(TimeRemaining);
	if (WholeSecond != LastBroadcastSecond)
	{
		LastBroadcastSecond = WholeSecond;
		OnMatchTimeChanged.Broadcast(TimeRemaining);
	}

	if (TimeRemaining <= 0.f)
	{
		// Survived to the end - win.
		MatchState = EMatchState::Victory;
		UE_LOG(LogGMTKCore, Log, TEXT("Match won - survival timer expired."));
		OnVictory.Broadcast();
	}
}

void ABaseGameState::AddScore(int32 Amount)
{
	Score += Amount;
	OnScoreChanged.Broadcast(Score);
}

void ABaseGameState::StartMatch(float SurvivalSeconds)
{
	TimeRemaining = SurvivalSeconds;
	MatchState = EMatchState::InProgress;
	LastBroadcastSecond = -1;

	UE_LOG(LogGMTKCore, Log, TEXT("Match started - survive %.0f seconds."), SurvivalSeconds);

	// Prime the UI with the starting value immediately.
	OnMatchTimeChanged.Broadcast(TimeRemaining);
}

void ABaseGameState::ReportEnemyKilled(AActor* Killer, AActor* Victim)
{
	// Central fan-out. Score is bumped here as the one guaranteed consequence;
	// combo and pickups subscribe to the event rather than being called directly.
	OnEnemyKilled.Broadcast(Killer, Victim);
}

void ABaseGameState::TriggerGameOver()
{
	if (IsMatchOver())
	{
		return; // already resolved, don't fire twice
	}

	MatchState = EMatchState::GameOver;
	UE_LOG(LogGMTKCore, Log, TEXT("Game over - player died."));
	OnGameOver.Broadcast();
}
