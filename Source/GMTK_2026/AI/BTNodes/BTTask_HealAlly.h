// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_HealAlly.generated.h"

/**
 * Finds the most-wounded ally (lowest health fraction, not full, alive, within heal
 * range) and drives the healer's UEnemyHealBeamComponent to heal it, staying InProgress
 * until the beam finishes.
 *
 * Fails immediately if no ally needs healing, so the tree can fall through to normal
 * pursuit (the healer then just advances on the player like any other enemy).
 *
 * Mirrors BTTask_FireLaser: the task holds no heal logic itself, it just selects the
 * target, starts the component, and waits.
 */
UCLASS()
class GMTK_2026_API UBTTask_HealAlly : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_HealAlly();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	/** Optional: where to store the chosen ally (so other nodes / focus can read it). */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector HealTargetKey;

	/** Max distance to consider an ally for healing. */
	UPROPERTY(EditAnywhere, Category = "Heal")
	float HealSearchRange = 2000.f;

	/** An ally is only a heal candidate below this health fraction (1.0 = any non-full). */
	UPROPERTY(EditAnywhere, Category = "Heal")
	float WoundedThreshold = 0.99f;

	UPROPERTY(EditAnywhere, Category = "Heal")
	bool bStopMovementWhileHealing = true;

	UPROPERTY(EditAnywhere, Category = "Heal")
	bool bFocusTargetWhileHealing = true;

private:
	/** Find the most-wounded valid ally within range of Healer. Returns nullptr if none. */
	AActor* FindMostWoundedAlly(APawn* Healer) const;
};
