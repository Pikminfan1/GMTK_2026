#include "AI/BTNodes/BTTask_RequestToken.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"

UBTTask_RequestToken::UBTTask_RequestToken()
{
	NodeName = TEXT("Request Combat Token");

	HasTokenKey.AddBoolFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTTask_RequestToken, HasTokenKey));
	HasTokenKey.SelectedKeyName = TEXT("HasToken");
}

FString UBTTask_RequestToken::GetStaticDescription() const
{
	const UEnum* EnumPtr = StaticEnum<ETokenRequestType>();
	const FString TypeName = EnumPtr
		? EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(TokenType)).ToString()
		: TEXT("Unknown");

	return FString::Printf(TEXT("Request: %s"), *TypeName);
}

EBTNodeResult::Type UBTTask_RequestToken::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// Tokens are held by the CONTROLLER - the thing that decides to attack - so the
	// controller is what gets registered with the director.
	AAIController* AICon = OwnerComp.GetAIOwner();

	UCombatTokenSubsystem* Tokens = GetWorld()
		? GetWorld()->GetSubsystem<UCombatTokenSubsystem>()
		: nullptr;

	if (!AICon || !Tokens)
	{
		return EBTNodeResult::Failed;
	}

	const bool bGranted = Tokens->RequestToken(AICon, TokenType);

	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent())
	{
		Blackboard->SetValueAsBool(HasTokenKey.SelectedKeyName, bGranted);
	}

	// Failing here is the normal, expected path when the budget is full - it's how
	// denied enemies get pushed into their fallback behaviour.
	return bGranted ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}
