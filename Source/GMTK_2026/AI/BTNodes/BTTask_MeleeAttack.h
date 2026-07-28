#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_MeleeAttack.generated.h"

/**
 * Drives UEnemyMeleeAttackComponent through a full swing and stays InProgress
 * until the component reports it's done.
 *
 * The task holds no attack logic itself - it starts the component against the
 * blackboard target, optionally pins the pawn in place, and waits. If the branch
 * aborts mid-swing (token stolen, target died), AbortTask tells the component to
 * bail so the pawn never finishes an attack it lost permission for.
 */
UCLASS()
class GMTK_2026_API UBTTask_MeleeAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MeleeAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	/** Stop moving for the duration of the attack so the swing lands from where
	 *  the telegraph showed it starting. */
	UPROPERTY(EditAnywhere, Category = "Melee")
	bool bStopMovementWhileSwinging = true;

	/** Face the target for the duration of the attack. */
	UPROPERTY(EditAnywhere, Category = "Melee")
	bool bFocusTargetWhileSwinging = true;
};
