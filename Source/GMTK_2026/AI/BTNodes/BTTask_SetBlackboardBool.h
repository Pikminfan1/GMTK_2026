#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_SetBlackboardBool.generated.h"

/**
 * Sets a bool blackboard key to a fixed value and succeeds.
 *
 * The engine ships Set Blackboard Value nodes for some types, but not a clean bool
 * setter with an editable value - and the tree needs to flip flags like
 * HasValidVantage and reset Pressure at specific points in a Sequence.
 */
UCLASS()
class GMTK_2026_API UBTTask_SetBlackboardBool : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SetBlackboardBool();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	bool bValue = true;
};
