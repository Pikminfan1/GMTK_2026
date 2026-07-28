#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTService_EscalateSpeedByWaitTime.generated.h"

/**
 * Maps token-wait frustration onto movement speed, so denied enemies visibly get
 * more aggressive the longer the director keeps them waiting:
 *
 *   Holding a token            -> Sprint (closing to use it)
 *   Waiting < JogThreshold     -> Walk   (patient orbiting)
 *   Waiting < SprintThreshold  -> Jog    (getting antsy)
 *   Waiting beyond that        -> Sprint (barely restrained)
 *
 * This is the payoff of the subsystem tracking wait time at all: a crowd of
 * enemies who can't attack reads as a restless, tightening pack instead of a
 * passive queue.
 *
 * Reads the WaitTime/HasToken keys that BTService_CombatState mirrors onto the
 * blackboard, and drives AEnemyCharacter::SetMovementSpeed. Put it on the root
 * of the melee grunt's tree, alongside the combat-state service.
 */
UCLASS()
class GMTK_2026_API UBTService_EscalateSpeedByWaitTime : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_EscalateSpeedByWaitTime();

	virtual FString GetStaticDescription() const override;

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector WaitTimeKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector HasTokenKey;

	/** Waits shorter than this stay at a walk. */
	UPROPERTY(EditAnywhere, Category = "Escalation")
	float JogWaitThreshold = 2.f;

	/** Waits shorter than this (but past JogWaitThreshold) jog; beyond it, sprint. */
	UPROPERTY(EditAnywhere, Category = "Escalation")
	float SprintWaitThreshold = 5.f;
};
