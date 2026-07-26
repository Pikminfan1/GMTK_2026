// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_TeleportToVantage.generated.h"

/**
 * Teleports the pawn to an ideal laser vantage around the current target instead of
 * pathing there. A vantage is a NavMesh point within a distance band of the target
 * where a chest-height line of sight to the target is clear (same rule the base
 * enemy's reposition logic uses), favouring longer distances.
 *
 * The teleport is telegraphed: the pawn hides (vanish), waits TelegraphDelay, then
 * reappears at the destination. Blueprint hooks are provided via the pawn (see the
 * teleporter enemy) for VFX at both ends.
 *
 * Used by the teleporter enemy variant's Behavior Tree in place of a Move To node.
 */
UCLASS()
class GMTK_2026_API UBTTask_TeleportToVantage : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_TeleportToVantage();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	/** Blackboard key holding the target actor (the player). */
	UPROPERTY(EditAnywhere, Category = "Teleport")
	FName TargetActorKey = TEXT("TargetActor");

	/** Closest a vantage may be to the target. */
	UPROPERTY(EditAnywhere, Category = "Teleport")
	float MinVantageDistance = 800.f;

	/** Farthest a vantage may be from the target. */
	UPROPERTY(EditAnywhere, Category = "Teleport")
	float MaxVantageDistance = 2000.f;

	/** How many candidate directions to sample around the target. */
	UPROPERTY(EditAnywhere, Category = "Teleport", meta = (ClampMin = "4"))
	int32 SampleCount = 12;

	/** Height offset for the LOS trace, matching the laser's chest-height beam. */
	UPROPERTY(EditAnywhere, Category = "Teleport")
	float LaserTraceHeight = 60.f;

	/** How far, vertically, to project a sampled point onto the NavMesh. */
	UPROPERTY(EditAnywhere, Category = "Teleport")
	float NavProjectExtent = 300.f;

	/** Seconds the pawn stays hidden between vanishing and reappearing (telegraph). */
	UPROPERTY(EditAnywhere, Category = "Teleport", meta = (ClampMin = "0.0"))
	float TelegraphDelay = 0.6f;

private:
	/** Finish the teleport: move the pawn, unhide, re-enable, and end the task. Runs
	 *  after TelegraphDelay via a timer. */
	void CompleteTeleport();

	// Cached across the delay.
	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;
	TWeakObjectPtr<APawn> CachedPawn;
	FVector PendingDestination = FVector::ZeroVector;
	FTimerHandle TelegraphTimer;
};
