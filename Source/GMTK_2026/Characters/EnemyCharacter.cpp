// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/EnemyCharacter.h"
#include "AI/EnemyAIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Pickups/HealthOrbSpawnerComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BrainComponent.h"
#include "TimerManager.h"

AEnemyCharacter::AEnemyCharacter()
{
	// Intentionally minimal - mesh, stats, and Behavior Tree assignment happen per
	// enemy-archetype Blueprint, not here.

	// Every enemy drops health orbs on death by default. Blueprint archetypes tune the
	// amount (or set count to 0 to drop nothing) on this component.
	OrbSpawner = CreateDefaultSubobject<UHealthOrbSpawnerComponent>(TEXT("OrbSpawner"));
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	// The spawn sequence is kicked off by the AI controller's OnPossess (after it runs
	// the Behavior Tree), so there's nothing to do here. See BeginSpawnSequence.
}

void AEnemyCharacter::BeginSpawnSequence()
{
	// Guard: only run once, and only if enabled.
	if (!bUseSpawnSequence || bIsSpawning || bSpawnFinished)
	{
		return;
	}

	bIsSpawning = true;
	bSpawnFinished = false;

	// Pause the AI so the enemy holds still during the spawn-in. The controller runs
	// the Behavior Tree on possession; stopping the brain here freezes it until
	// FinishSpawn() resumes it. Movement is also disabled so nothing nudges it.
	if (AEnemyAIController* AIController = GetEnemyAIController())
	{
		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Spawning"));
		}
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->DisableMovement();
	}

	// Let the Blueprint play the spawn animation + particle effect. It should call
	// FinishSpawn() when the animation completes.
	OnSpawnSequenceStarted();

	// Failsafe: if the Blueprint never calls FinishSpawn (no anim wired, or a mistake),
	// auto-finish so the enemy can't stay frozen forever.
	if (SpawnFailsafeTime > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			SpawnFailsafeTimer, this, &AEnemyCharacter::FinishSpawn, SpawnFailsafeTime, false);
	}
}

void AEnemyCharacter::FinishSpawn()
{
	// Idempotent - only the first call does anything.
	if (bSpawnFinished || !bIsSpawning)
	{
		return;
	}
	bSpawnFinished = true;
	bIsSpawning = false;

	GetWorldTimerManager().ClearTimer(SpawnFailsafeTimer);

	// Restore movement and resume the AI so the enemy starts acting.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}

	if (AEnemyAIController* AIController = GetEnemyAIController())
	{
		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			Brain->RestartLogic();
		}
	}

	OnSpawnSequenceFinished();
}

AEnemyAIController* AEnemyCharacter::GetEnemyAIController() const
{
	return Cast<AEnemyAIController>(GetController());
}

void AEnemyCharacter::HandleDeath_Implementation(AActor* DeadActor)
{
	// Drop the loot before the base tears the enemy down (disables collision/movement).
	if (OrbSpawner)
	{
		OrbSpawner->NotifyOwnerDied();
	}

	Super::HandleDeath_Implementation(DeadActor);
}

float AEnemyCharacter::SetMovementSpeed(EEnemyMovementSpeed SpeedType)
{
	if (GetCharacterMovement())
	{
		float NewSpeed = 0.f;
		switch (SpeedType)
			{
			case EEnemyMovementSpeed::Idle:
				NewSpeed = 0.f;
				break;
			case EEnemyMovementSpeed::Walking:
				NewSpeed = 200.f;
				break;
			case EEnemyMovementSpeed::Jogging:
				NewSpeed = 400.f;
				break;
			case EEnemyMovementSpeed::Sprinting:
				NewSpeed = 600.f;
				break;
			default:
				UE_LOG(LogTemp, Warning, TEXT("Unknown EEnemyMovementSpeed value: %d"), static_cast<int32>(SpeedType));
				break;
			}
		GetCharacterMovement()->MaxWalkSpeed = NewSpeed;
		return NewSpeed;
	}
	return 0.f;
}
