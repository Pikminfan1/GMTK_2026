// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BTNodes/BTTask_TeleportToVantage.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "Characters/EnemyCharacter.h"
#include "TimerManager.h"

UBTTask_TeleportToVantage::UBTTask_TeleportToVantage()
{
	NodeName = TEXT("Teleport To Vantage");

	// A Behavior Tree normally shares ONE node object between every AI running the
	// same tree asset. This task keeps latent state across the telegraph delay
	// (pawn, destination, timer), so each AI needs its own copy of the node -
	// otherwise two teleporters teleporting at the same time overwrite each
	// other's state.
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_TeleportToVantage::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	APawn* Pawn = AICon ? AICon->GetPawn() : nullptr;
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!Pawn || !BB)
	{
		return EBTNodeResult::Failed;
	}

	AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKey));
	if (!Target)
	{
		return EBTNodeResult::Failed;
	}

	UWorld* World = GetWorld();
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Nav)
	{
		return EBTNodeResult::Failed;
	}

	const FVector TargetLoc = Target->GetActorLocation();
	const FVector TargetChest = TargetLoc + FVector(0, 0, LaserTraceHeight);

	// Sample directions around the target; for each, place a candidate at a random
	// distance in the band (biased toward the far end so the teleporter likes to keep
	// its distance), project to the NavMesh, and keep it only if the laser could
	// actually connect from there. Track the farthest valid candidate.
	FVector BestDest = FVector::ZeroVector;
	float BestDistance = -1.f;

	const float AngleStep = 360.f / FMath::Max(1, SampleCount);
	const float StartAngleOffset = FMath::FRandRange(0.f, AngleStep); // rotate the ring so it isn't identical each time

	for (int32 i = 0; i < SampleCount; ++i)
	{
		const float AngleDeg = StartAngleOffset + AngleStep * i;
		const float AngleRad = FMath::DegreesToRadians(AngleDeg);
		const FVector Dir(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.f);

		// Bias toward the far end of the band: sqrt-weight pushes samples outward.
		const float T = FMath::Sqrt(FMath::FRand());
		const float Dist = FMath::Lerp(MinVantageDistance, MaxVantageDistance, T);

		const FVector RawPoint = TargetLoc + Dir * Dist;

		// Project onto the NavMesh so we don't teleport into geometry / off the floor.
		FNavLocation NavLoc;
		if (!Nav->ProjectPointToNavigation(RawPoint, NavLoc, FVector(NavProjectExtent)))
		{
			continue;
		}

		// Laser LOS check from this candidate's chest height to the target's chest.
		const FVector CandChest = NavLoc.Location + FVector(0, 0, LaserTraceHeight);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TeleportVantageLOS), false, Pawn);
		Params.AddIgnoredActor(Target);
		const bool bBlocked = World->LineTraceTestByChannel(CandChest, TargetChest, ECC_Visibility, Params);
		if (bBlocked)
		{
			continue;
		}

		const float CandDistance = FVector::Dist(NavLoc.Location, TargetLoc);
		if (CandDistance > BestDistance)
		{
			BestDistance = CandDistance;
			BestDest = NavLoc.Location;
		}
	}

	if (BestDistance < 0.f)
	{
		// No valid vantage found this attempt - fail so the tree can fall back (e.g.
		// try again next tick, or run a different branch).
		return EBTNodeResult::Failed;
	}

	// Telegraph: hide + disable collision now, reappear after the delay.
	PendingDestination = BestDest;
	CachedOwnerComp = &OwnerComp;
	CachedPawn = Pawn;

	Pawn->SetActorHiddenInGame(true);
	Pawn->SetActorEnableCollision(false);

	// Let the enemy Blueprint play a vanish VFX.
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
		World->GetTimerManager().SetTimer(
			TelegraphTimer, this, &UBTTask_TeleportToVantage::CompleteTeleport, TelegraphDelay, false);
	}

	// The task stays in progress until CompleteTeleport finishes it.
	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_TeleportToVantage::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// The branch aborted mid-telegraph (token lost, target died, a higher-priority
	// branch fired). The pawn must never be left hidden and collisionless, so
	// restore it in place and cancel the pending reappear.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TelegraphTimer);
	}

	if (APawn* Pawn = CachedPawn.Get())
	{
		Pawn->SetActorHiddenInGame(false);
		Pawn->SetActorEnableCollision(true);
	}

	CachedPawn = nullptr;
	CachedOwnerComp = nullptr;

	return EBTNodeResult::Aborted;
}

void UBTTask_TeleportToVantage::CompleteTeleport()
{
	APawn* Pawn = CachedPawn.Get();
	UBehaviorTreeComponent* OwnerComp = CachedOwnerComp.Get();

	if (!Pawn || !OwnerComp)
	{
		// Pawn or tree went away during the telegraph (e.g. died) - nothing to finish.
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
