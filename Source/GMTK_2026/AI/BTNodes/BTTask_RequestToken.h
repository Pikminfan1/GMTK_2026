#pragma once

#include "CoreMinimal.h"
#include "AI/Tokens/CombatTokenSubsystem.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_RequestToken.generated.h"

/**
 * Asks the combat director for permission to attack. Succeeds if granted, fails
 * immediately if not - so placing this in a Sequence naturally causes the whole
 * attack branch to bail out and fall through to the fallback behaviour.
 */
UCLASS()
class GMTK_2026_API UBTTask_RequestToken : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_RequestToken();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	/** Which attack type to request. Determines how much of the budget it costs. */
	UPROPERTY(EditAnywhere, Category = "Combat Tokens")
	ETokenRequestType TokenType = ETokenRequestType::RangedLaser;

	/** Mirrored to the blackboard so decorators can observe the result. */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector HasTokenKey;
};
