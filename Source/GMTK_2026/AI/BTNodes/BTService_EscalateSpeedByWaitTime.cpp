#include "AI/BTNodes/BTService_EscalateSpeedByWaitTime.h"

#include "AIController.h"
#include "AI/EnemyAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "Characters/EnemyCharacter.h"

UBTService_EscalateSpeedByWaitTime::UBTService_EscalateSpeedByWaitTime()
{
	NodeName = TEXT("Escalate Speed By Wait Time");

	Interval = 0.25f;
	RandomDeviation = 0.05f;

	WaitTimeKey.AddFloatFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTService_EscalateSpeedByWaitTime, WaitTimeKey));
	WaitTimeKey.SelectedKeyName = TEXT("WaitTime");

	HasTokenKey.AddBoolFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTService_EscalateSpeedByWaitTime, HasTokenKey));
	HasTokenKey.SelectedKeyName = TEXT("HasToken");
}

FString UBTService_EscalateSpeedByWaitTime::GetStaticDescription() const
{
	return FString::Printf(
		TEXT("Walk < %.1fs wait | Jog < %.1fs | Sprint beyond (or holding a token)"),
		JogWaitThreshold, SprintWaitThreshold);
}

void UBTService_EscalateSpeedByWaitTime::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	const AAIController* AICon = OwnerComp.GetAIOwner();
	AEnemyCharacter* Enemy = AICon ? Cast<AEnemyCharacter>(AICon->GetPawn()) : nullptr;

	if (!Blackboard || !Enemy)
	{
		return;
	}

	// An armed enemy always closes at full speed - it has permission to attack and
	// should spend it before the hold-time reclaim fires.
	if (Blackboard->GetValueAsBool(HasTokenKey.SelectedKeyName))
	{
		Enemy->SetMovementSpeed(EEnemyMovementSpeed::Sprinting);
		return;
	}

	const float WaitTime = Blackboard->GetValueAsFloat(WaitTimeKey.SelectedKeyName);

	if (WaitTime < JogWaitThreshold)
	{
		Enemy->SetMovementSpeed(EEnemyMovementSpeed::Walking);
	}
	else if (WaitTime < SprintWaitThreshold)
	{
		Enemy->SetMovementSpeed(EEnemyMovementSpeed::Jogging);
	}
	else
	{
		Enemy->SetMovementSpeed(EEnemyMovementSpeed::Sprinting);
	}
}
