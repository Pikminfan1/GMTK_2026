#include "AI/BTNodes/BTDecorator_HasToken.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UBTDecorator_HasToken::UBTDecorator_HasToken()
{
	NodeName = TEXT("Has Combat Token");

	// Ticking is what makes this an *observing* decorator - without it the
	// condition is only evaluated once on entry and losing the token mid-attack
	// would never abort the branch.
	bNotifyTick = true;

	bAllowAbortLowerPri = true;
	bAllowAbortChildNodes = true;
}

FString UBTDecorator_HasToken::GetStaticDescription() const
{
	const UEnum* EnumPtr = StaticEnum<ETokenRequestType>();
	const FString TypeName = EnumPtr
		? EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(RequiredType)).ToString()
		: TEXT("Unknown");

	return bRequireHeldToken
		? FString::Printf(TEXT("Currently holds a %s token"), *TypeName)
		: FString::Printf(TEXT("Holds or can afford a %s token"), *TypeName);
}

bool UBTDecorator_HasToken::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const AAIController* AICon = OwnerComp.GetAIOwner();

	UCombatTokenSubsystem* Tokens = GetWorld()
		? GetWorld()->GetSubsystem<UCombatTokenSubsystem>()
		: nullptr;

	if (!AICon || !Tokens)
	{
		return false;
	}

	// Already holding always passes, in both modes.
	if (Tokens->HoldsToken(AICon))
	{
		return true;
	}

	if (bRequireHeldToken)
	{
		return false;
	}

	return Tokens->GetAvailableBudget() >= Tokens->GetCostForType(RequiredType);
}

void UBTDecorator_HasToken::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	// Re-evaluating each tick is what drives the abort when the condition flips.
	ConditionalFlowAbort(OwnerComp, EBTDecoratorAbortRequest::ConditionResultChanged);
}
