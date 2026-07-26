// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_TeleportToLocation.generated.h"

/**
 * Teleports the pawn to a location stored in a blackboard Vector key (e.g. VantagePoint,
 * produced by the same EQS queries the base enemy walks to). This is a drop-in
 * replacement for a "Move To VantagePoint" node in the teleporter's combat tree: it
 * reuses the EQS-computed destination but blinks there instead of pathing.
 *
 * The teleport is telegraphed: hide (vanish) -> wait TelegraphDelay -> reappear at the
 * destination. The pawn's OnTeleportVanish/OnTeleportAppear Blueprint events fire for VFX.
 *
 * The destination is optionally re-projected onto the NavMesh so an EQS point that's
 * slightly off a surface still lands cleanly. Because it teleports, reachability is
 * irrelevant - it can blink to a valid navmesh spot with no walkable path to it.
 */
UCLASS()
class GMTK_2026_API UBTTask_TeleportToLocation : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_TeleportToLocation();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	/** Blackboard Vector key holding the teleport destination (e.g. VantagePoint). */
	UPROPERTY(EditAnywhere, Category = "Teleport")
	FName DestinationKey = TEXT("VantagePoint");

	/** Seconds hidden between vanishing and reappearing (the telegraph window). */
	UPROPERTY(EditAnywhere, Category = "Teleport", meta = (ClampMin = "0.0"))
	float TelegraphDelay = 0.6f;

	/** If true, re-project the destination onto the NavMesh before teleporting. */
	UPROPERTY(EditAnywhere, Category = "Teleport")
	bool bProjectToNavMesh = true;

	/** Vertical extent used when projecting onto the NavMesh. */
	UPROPERTY(EditAnywhere, Category = "Teleport")
	float NavProjectExtent = 200.f;

private:
	void CompleteTeleport();

	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;
	TWeakObjectPtr<APawn> CachedPawn;
	FVector PendingDestination = FVector::ZeroVector;
	FTimerHandle TelegraphTimer;
};
