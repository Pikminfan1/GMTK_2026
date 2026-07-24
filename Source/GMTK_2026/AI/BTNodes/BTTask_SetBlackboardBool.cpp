#include "AI/BTNodes/BTTask_SetBlackboardBool.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"

UBTTask_SetBlackboardBool::UBTTask_SetBlackboardBool()
{
	NodeName = TEXT("Set Blackboard Bool");

	TargetKey.AddBoolFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTTask_SetBlackboardBool, TargetKey));
}

FString UBTTask_SetBlackboardBool::GetStaticDescription() const
{
	return FString::Printf(TEXT("Set %s = %s"),
		*TargetKey.SelectedKeyName.ToString(),
		bValue ? TEXT("true") : TEXT("false"));
}

EBTNodeResult::Type UBTTask_SetBlackboardBool::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();

	if (!Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsBool(TargetKey.SelectedKeyName, bValue);
	return EBTNodeResult::Succeeded;
}
