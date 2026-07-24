#include "AI/BTNodes/BTTask_ReleaseToken.h"

#include "AIController.h"
#include "AI/Tokens/CombatTokenSubsystem.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"

UBTTask_ReleaseToken::UBTTask_ReleaseToken()
{
	NodeName = TEXT("Release Combat Token");

	HasTokenKey.AddBoolFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTTask_ReleaseToken, HasTokenKey));
	HasTokenKey.SelectedKeyName = TEXT("HasToken");
}

FString UBTTask_ReleaseToken::GetStaticDescription() const
{
	return TEXT("Return token to the combat director");
}

EBTNodeResult::Type UBTTask_ReleaseToken::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	APawn* Pawn = AICon ? AICon->GetPawn() : nullptr;

	if (Pawn && GetWorld())
	{
		if (UCombatTokenSubsystem* Tokens = GetWorld()->GetSubsystem<UCombatTokenSubsystem>())
		{
			Tokens->ReleaseToken(Pawn);
		}
	}

	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent())
	{
		Blackboard->SetValueAsBool(HasTokenKey.SelectedKeyName, false);
	}

	// Never fails - a release that does nothing is still a valid outcome, and
	// failing here would break every Sequence this sits in.
	return EBTNodeResult::Succeeded;
}
