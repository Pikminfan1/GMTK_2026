#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_FireLaser.generated.h"

/**
 * Drives UEnemyLaserAttackComponent through a full attack and stays InProgress
 * until the component reports it's done.
 *
 * The task holds no attack logic itself - it only starts the component, faces the
 * target, and waits. That split means the same attack can be triggered from a
 * different tree, a state machine, or Blueprint without duplicating any of it.
 */
UCLASS()
class GMTK_2026_API UBTTask_FireLaser : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FireLaser();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	/** Stop moving while firing. The beam needs a stable origin to read clearly. */
	UPROPERTY(EditAnywhere, Category = "Laser")
	bool bStopMovementWhileFiring = true;

	/** Face the target for the duration of the attack. */
	UPROPERTY(EditAnywhere, Category = "Laser")
	bool bFocusTargetWhileFiring = true;
};
