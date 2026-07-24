#include "AI/BTNodes/BTTask_FireLaser.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "Characters/Components/EnemyLaserAttackComponent.h"
#include "GameFramework/PawnMovementComponent.h"

UBTTask_FireLaser::UBTTask_FireLaser()
{
	NodeName = TEXT("Fire Laser");

	bNotifyTick = true;
	bNotifyTaskFinished = true;

	TargetActorKey.AddObjectFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTTask_FireLaser, TargetActorKey), AActor::StaticClass());
	TargetActorKey.SelectedKeyName = TEXT("TargetActor");
}

FString UBTTask_FireLaser::GetStaticDescription() const
{
	return FString::Printf(TEXT("Fire sustained beam at %s"), *TargetActorKey.SelectedKeyName.ToString());
}

EBTNodeResult::Type UBTTask_FireLaser::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	APawn* Pawn = AICon ? AICon->GetPawn() : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();

	if (!Pawn || !Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	UEnemyLaserAttackComponent* Laser = Pawn->FindComponentByClass<UEnemyLaserAttackComponent>();
	AActor* TargetActor = Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName));

	if (!Laser || !IsValid(TargetActor))
	{
		return EBTNodeResult::Failed;
	}

	if (bFocusTargetWhileFiring)
	{
		AICon->SetFocus(TargetActor, EAIFocusPriority::Gameplay);
	}

	if (bStopMovementWhileFiring)
	{
		AICon->StopMovement();

		if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
		}
	}

	if (!Laser->BeginAttack(TargetActor))
	{
		// Component was busy or on cooldown. Clean up the focus we just set.
		AICon->ClearFocus(EAIFocusPriority::Gameplay);
		return EBTNodeResult::Failed;
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_FireLaser::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AICon = OwnerComp.GetAIOwner();
	APawn* Pawn = AICon ? AICon->GetPawn() : nullptr;

	UEnemyLaserAttackComponent* Laser = Pawn
		? Pawn->FindComponentByClass<UEnemyLaserAttackComponent>()
		: nullptr;

	if (!Laser)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// The component owns the attack's lifetime; the task just waits for it to
	// leave the active states.
	const ELaserState LaserState = Laser->GetState();

	if (LaserState == ELaserState::Cooldown || LaserState == ELaserState::Idle)
	{
		if (AICon)
		{
			AICon->ClearFocus(EAIFocusPriority::Gameplay);
		}

		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_FireLaser::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	APawn* Pawn = AICon ? AICon->GetPawn() : nullptr;

	if (Pawn)
	{
		if (UEnemyLaserAttackComponent* Laser = Pawn->FindComponentByClass<UEnemyLaserAttackComponent>())
		{
			Laser->AbortAttack();
		}
	}

	if (AICon)
	{
		AICon->ClearFocus(EAIFocusPriority::Gameplay);
	}

	return EBTNodeResult::Aborted;
}
