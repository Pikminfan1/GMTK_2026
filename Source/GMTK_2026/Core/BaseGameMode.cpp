// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/BaseGameMode.h"
#include "Core/BaseGameState.h"
#include "Spawning/Components/WaveManagerComponent.h"
#include "Characters/PlayerCharacter.h"
#include "Characters/Components/ComboComponent.h"
#include "Characters/Components/HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Utility/LogChannels.h"

ABaseGameMode::ABaseGameMode()
{
	WaveManagerComponent = CreateDefaultSubobject<UWaveManagerComponent>(TEXT("WaveManagerComponent"));
}

void ABaseGameMode::BeginPlay()
{
	Super::BeginPlay();

	CachedGameState = GetGameState<ABaseGameState>();
	if (!CachedGameState)
	{
		UE_LOG(LogGMTKCore, Error,
			TEXT("BaseGameMode expects a BaseGameState. Set GameState Class to BaseGameState (or a subclass)."));
	}

	if (WaveManagerComponent)
	{
		WaveManagerComponent->OnWaveStarted.AddDynamic(this, &ABaseGameMode::HandleWaveStarted);
		WaveManagerComponent->OnWaveCompleted.AddDynamic(this, &ABaseGameMode::HandleWaveCompleted);
		WaveManagerComponent->OnAllWavesCompleted.AddDynamic(this, &ABaseGameMode::HandleAllWavesCompleted);
		WaveManagerComponent->OnEnemyDied.AddDynamic(this, &ABaseGameMode::HandleEnemyDied);
	}

	// Start the survival match on the GameState.
	if (CachedGameState)
	{
		CachedGameState->StartMatch(SurvivalTimeSeconds);
	}

	// Player death triggers game over.
	if (APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		if (UHealthComponent* PlayerHealth = Player->GetHealthComponent())
		{
			PlayerHealth->OnDeath.AddDynamic(this, &ABaseGameMode::HandlePlayerDeath);
		}
	}
	else
	{
		UE_LOG(LogGMTKCore, Warning,
			TEXT("BaseGameMode could not find the player character at BeginPlay to bind death. "
				 "If the player spawns later, bind HandlePlayerDeath then."));
	}

	// Kick off the first wave.
	if (WaveManagerComponent)
	{
		WaveManagerComponent->StartNextWave();
	}
}

void ABaseGameMode::HandleWaveStarted(int32 WaveNumber)
{
	if (CachedGameState && WaveManagerComponent)
	{
		CachedGameState->CurrentWaveNumber = WaveNumber;
		CachedGameState->EnemiesRemaining = WaveManagerComponent->GetEnemiesRemaining();
	}
}

void ABaseGameMode::HandleWaveCompleted(int32 WaveNumber)
{
	if (CachedGameState)
	{
		CachedGameState->EnemiesRemaining = 0;
	}
	// Next wave starts externally (the shootable start-wave actor calls StartNextWave).
}

void ABaseGameMode::HandleAllWavesCompleted()
{
	// Win is the survival timer, not clearing waves, so the match continues. If you
	// later want "clear all waves = win", declare victory on the GameState here.
	UE_LOG(LogGMTKCore, Log, TEXT("All defined waves cleared; match continues until the timer expires."));
}

void ABaseGameMode::HandleEnemyDied(AActor* DeadEnemy)
{
	if (!CachedGameState)
	{
		return;
	}

	// Resolve the killer. For now the player is the only damage source that matters,
	// so attribute kills to the player pawn. When enemies can kill each other or the
	// environment can kill them, resolve this from damage instigator instead.
	AActor* Killer = UGameplayStatics::GetPlayerPawn(this, 0);

	// Report the kill FIRST so the combo component (which listens to OnEnemyKilled)
	// increments for this kill before we read it - this kill's points should reflect
	// the combo it just contributed to.
	CachedGameState->ReportEnemyKilled(Killer, DeadEnemy);

	// Score scales with the current combo: base * (1 + combo * bonus). A longer streak
	// makes each kill worth progressively more.
	int32 Points = ScorePerKill;
	if (APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		if (const UComboComponent* Combo = Player->GetComboComponent())
		{
			const float Multiplier = 1.f + (Combo->GetComboCount() * ComboScoreBonusPerKill);
			Points = FMath::RoundToInt(ScorePerKill * Multiplier);
		}
	}

	CachedGameState->AddScore(Points);
}

void ABaseGameMode::HandlePlayerDeath(AActor* DeadPlayer)
{
	if (CachedGameState)
	{
		CachedGameState->TriggerGameOver();
	}
}
