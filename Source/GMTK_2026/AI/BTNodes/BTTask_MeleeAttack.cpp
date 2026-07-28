#include "AI/BTNodes/BTTask_MeleeAttack.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "Characters/Components/EnemyMeleeAttackComponent.h"
#include "GameFramework/PawnMovementComponent.h"

UBTTask_MeleeAttack::UBTTask_MeleeAttack()
{
	NodeName = TEXT("Melee Attack");

	bNotifyTick = true;
	bNotifyTaskFinished = true;

	TargetActorKey.AddObjectFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTTask_MeleeAttack, TargetActorKey), AActor::StaticClass());
	TargetActorKey.SelectedKeyName = TEXT("TargetActor");
}

FString UBTTask_MeleeAttack::GetStaticDescription() const
{
	return FString::Printf(TEXT("Swing at %s"), *TargetActorKey.SelectedKeyName.ToString());
}

EBTNodeResult::Type UBTTask_MeleeAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	APawn* Pawn = AICon ? AICon->GetPawn() : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();

	if (!Pawn || !Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	UEnemyMeleeAttackComponent* Melee = Pawn->FindComponentByClass<UEnemyMeleeAttackComponent>();
	AActor* TargetActor = Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName));

	if (!Melee || !IsValid(TargetActor))
	{
		return EBTNodeResult::Failed;
	}

	if (bFocusTargetWhileSwinging)
	{
		AICon->SetFocus(TargetActor, EAIFocusPriority::Gameplay);
	}

	if (bStopMovementWhileSwinging)
	{
		AICon->StopMovement();

		if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
		}
	}

	if (!Melee->BeginAttack(TargetActor))
	{
		// Component was busy or on cooldown. Clean up the focus we just set.
		AICon->ClearFocus(EAIFocusPriority::Gameplay);
		return EBTNodeResult::Failed;
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_MeleeAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AICon = OwnerComp.GetAIOwner();
	APawn* Pawn = AICon ? AICon->GetPawn() : nullptr;

	UEnemyMeleeAttackComponent* Melee = Pawn
		? Pawn->FindComponentByClass<UEnemyMeleeAttackComponent>()
		: nullptr;

	if (!Melee)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// The component owns the attack's lifetime; the task just waits for it to
	// leave the active states.
	const EMeleeAttackState MeleeState = Melee->GetState();

	if (MeleeState == EMeleeAttackState::Cooldown || MeleeState == EMeleeAttackState::Idle)
	{
		if (AICon)
		{
			AICon->ClearFocus(EAIFocusPriority::Gameplay);
		}

		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_MeleeAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	APawn* Pawn = AICon ? AICon->GetPawn() : nullptr;

	if (Pawn)
	{
		if (UEnemyMeleeAttackComponent* Melee = Pawn->FindComponentByClass<UEnemyMeleeAttackComponent>())
		{
			Melee->AbortAttack();
		}
	}

	if (AICon)
	{
		AICon->ClearFocus(EAIFocusPriority::Gameplay);
	}

	return EBTNodeResult::Aborted;
}
