// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Components/ComboComponent.h"
#include "Core/BaseGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Utility/LogChannels.h"

UComboComponent::UComboComponent()
{
	// Ticks only to count down the combo window. Cheap - one float subtract and a
	// comparison - and we could disable tick when no combo is active, but leaving it
	// on keeps the logic simple and the cost is negligible.
	PrimaryComponentTick.bCanEverTick = true;
}

void UComboComponent::BeginPlay()
{
	Super::BeginPlay();

	// Self-wire to the central kill event. If the GameState isn't the expected type,
	// the combo simply never advances - RegisterKill can still be called manually.
	CachedGameState = GetWorld() ? GetWorld()->GetGameState<ABaseGameState>() : nullptr;
	if (CachedGameState)
	{
		CachedGameState->OnEnemyKilled.AddDynamic(this, &UComboComponent::HandleEnemyKilled);
	}
	else
	{
		UE_LOG(LogGMTKCombat, Warning,
			TEXT("ComboComponent could not find a BaseGameState; combo will only advance via manual RegisterKill()."));
	}
}

void UComboComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CachedGameState)
	{
		CachedGameState->OnEnemyKilled.RemoveDynamic(this, &UComboComponent::HandleEnemyKilled);
	}
	Super::EndPlay(EndPlayReason);
}

void UComboComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ComboCount <= 0)
	{
		return; // no active combo to time out
	}

	TimeRemaining -= DeltaTime;

	if (TimeRemaining <= 0.f)
	{
		// Window elapsed with no kill - the streak ends.
		const int32 FinalCombo = ComboCount;

		ComboCount = 0;
		TimeRemaining = 0.f;

		UE_LOG(LogGMTKCombat, Verbose, TEXT("Combo ended at %d"), FinalCombo);

		OnComboEnded.Broadcast(FinalCombo);
		OnComboChanged.Broadcast(ComboCount);

		// Stacks drop to 0 when the streak ends, so the reward layer strips buffs.
		if (LastBroadcastStacks != 0)
		{
			LastBroadcastStacks = 0;
			OnComboStacksChanged.Broadcast(0);
		}
	}
}

void UComboComponent::HandleEnemyKilled(AActor* Killer, AActor* Victim)
{
	// The central kill event is global (any enemy, any killer). For a single-player
	// jam every kill is the player's, so we count them all. If you later want combo
	// to only count the local player's kills, gate on Killer == GetOwner() here.
	RegisterKill();
	UE_LOG(LogGMTKCombat, Log, TEXT("Killed enemy, combo meter saw it"));
}

void UComboComponent::RegisterKill()
{
	ComboCount++;

	// Every kill refreshes the full window - this is what lets a fast streak run
	// indefinitely as long as the player keeps killing inside the window.
	TimeRemaining = ComboWindow;

	UE_LOG(LogGMTKCombat, Verbose, TEXT("Combo -> %d (window refreshed to %.1fs)"), ComboCount, ComboWindow);

	OnComboIncremented.Broadcast(ComboCount);
	OnComboChanged.Broadcast(ComboCount);

	// Fire the stack event only when the tier actually changes (every KillsPerStack).
	const int32 Stacks = GetComboStacks();
	if (Stacks != LastBroadcastStacks)
	{
		LastBroadcastStacks = Stacks;
		OnComboStacksChanged.Broadcast(Stacks);
	}
}

void UComboComponent::ResetCombo()
{
	if (ComboCount <= 0)
	{
		return;
	}

	const int32 FinalCombo = ComboCount;

	ComboCount = 0;
	TimeRemaining = 0.f;

	// A forced reset still counts as the combo ending, so reward/UI logic bound to
	// OnComboEnded fires consistently whether it timed out or was cleared.
	OnComboEnded.Broadcast(FinalCombo);
	OnComboChanged.Broadcast(ComboCount);

	if (LastBroadcastStacks != 0)
	{
		LastBroadcastStacks = 0;
		OnComboStacksChanged.Broadcast(0);
	}
}
