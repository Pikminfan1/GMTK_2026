// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BTNodes/BTTask_TeleportToLocation.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "Characters/EnemyCharacter.h"
#include "TimerManager.h"

UBTTask_TeleportToLocation::UBTTask_TeleportToLocation()
{
	NodeName = TEXT("Teleport To Location");
}

EBTNodeResult::Type UBTTask_TeleportToLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	APawn* Pawn = AICon ? AICon->GetPawn() : nullptr;
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!Pawn || !BB)
	{
		return EBTNodeResult::Failed;
	}

	FVector Destination = BB->GetValueAsVector(DestinationKey);
	if (Destination.IsNearlyZero())
	{
		// No valid destination in the key - let the tree fall through to another branch.
		return EBTNodeResult::Failed;
	}

	// Optionally snap onto the NavMesh so a slightly-off EQS point lands cleanly. This
	// only validates the point IS navmesh - it does NOT pathfind, so the teleporter can
	// reach navmesh spots with no walkable route.
	if (bProjectToNavMesh)
	{
		if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			FNavLocation NavLoc;
			if (Nav->ProjectPointToNavigation(Destination, NavLoc, FVector(NavProjectExtent)))
			{
				Destination = NavLoc.Location;
			}
		}
	}

	// Telegraph: hide + disable collision now, reappear at the destination after the delay.
	PendingDestination = Destination;
	CachedOwnerComp = &OwnerComp;
	CachedPawn = Pawn;

	Pawn->SetActorHiddenInGame(true);
	Pawn->SetActorEnableCollision(false);

	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Pawn))
	{
		Enemy->OnTeleportVanish();
	}

	if (TelegraphDelay <= 0.f)
	{
		CompleteTeleport();
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(
			TelegraphTimer, this, &UBTTask_TeleportToLocation::CompleteTeleport, TelegraphDelay, false);
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_TeleportToLocation::CompleteTeleport()
{
	APawn* Pawn = CachedPawn.Get();
	UBehaviorTreeComponent* OwnerComp = CachedOwnerComp.Get();

	if (!Pawn || !OwnerComp)
	{
		if (OwnerComp)
		{
			FinishLatentTask(*OwnerComp, EBTNodeResult::Failed);
		}
		return;
	}

	Pawn->SetActorLocation(PendingDestination, false, nullptr, ETeleportType::TeleportPhysics);
	Pawn->SetActorHiddenInGame(false);
	Pawn->SetActorEnableCollision(true);

	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Pawn))
	{
		Enemy->OnTeleportAppear();
	}

	FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
}
