#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_ReleaseToken.generated.h"

/**
 * Hands the combat token back to the director. Always succeeds, including when
 * this enemy wasn't holding one - so it's safe to drop at the top of any branch
 * that should guarantee the token is free before proceeding.
 */
UCLASS()
class GMTK_2026_API UBTTask_ReleaseToken : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ReleaseToken();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector HasTokenKey;
};
