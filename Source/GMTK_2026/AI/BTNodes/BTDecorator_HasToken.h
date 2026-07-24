#pragma once

#include "CoreMinimal.h"
#include "AI/Tokens/CombatTokenSubsystem.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_HasToken.generated.h"

/**
 * Gates a branch on combat token availability.
 *
 * Two modes, and the difference matters:
 *  - bRequireHeldToken = false (default): passes if this enemy could *afford* the
 *    token. Use this to guard an attack branch that will request one itself.
 *  - bRequireHeldToken = true: passes only if the enemy already holds one. Use this
 *    to abort a branch the moment the token is taken away.
 *
 * Set Observer Aborts to Self on attack branches. Using Both causes a thrash loop:
 * releasing the token re-evaluates the branch, which requests it again, which
 * re-evaluates again.
 */
UCLASS()
class GMTK_2026_API UBTDecorator_HasToken : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_HasToken();

	virtual FString GetStaticDescription() const override;

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	/** Which attack type's cost to check affordability against. */
	UPROPERTY(EditAnywhere, Category = "Combat Tokens")
	ETokenRequestType RequiredType = ETokenRequestType::RangedLaser;

	/** If true, only an already-held token passes. If false, affordability passes too. */
	UPROPERTY(EditAnywhere, Category = "Combat Tokens")
	bool bRequireHeldToken = false;
};
