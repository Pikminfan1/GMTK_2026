// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BTNodes/BTTask_HealAlly.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "Characters/Components/EnemyHealBeamComponent.h"
#include "Characters/Components/HealthComponent.h"
#include "Characters/EnemyCharacter.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"

UBTTask_HealAlly::UBTTask_HealAlly()
{
	NodeName = TEXT("Heal Ally");

	bNotifyTick = true;
	bNotifyTaskFinished = true;

	HealTargetKey.AddObjectFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTTask_HealAlly, HealTargetKey), AActor::StaticClass());
	HealTargetKey.SelectedKeyName = TEXT("HealTarget");
}

FString UBTTask_HealAlly::GetStaticDescription() const
{
	return FString::Printf(TEXT("Heal the most-wounded ally within %.0f"), HealSearchRange);
}

AActor* UBTTask_HealAlly::FindMostWoundedAlly(APawn* Healer) const
{
	if (!Healer)
	{
		return nullptr;
	}

	// All enemies are AEnemyCharacter subclasses, so that's the ally set. (The healer
	// itself and the player are excluded below.)
	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(Healer, AEnemyCharacter::StaticClass(), Enemies);

	const FVector HealerLoc = Healer->GetActorLocation();
	const float RangeSq = HealSearchRange * HealSearchRange;

	AActor* Best = nullptr;
	float BestFraction = WoundedThreshold; // must be strictly below this to qualify

	for (AActor* Ally : Enemies)
	{
		if (!IsValid(Ally) || Ally == Healer)
		{
			continue;
		}

		// In range?
		if (FVector::DistSquared(HealerLoc, Ally->GetActorLocation()) > RangeSq)
		{
			continue;
		}

		const UHealthComponent* Health = Ally->FindComponentByClass<UHealthComponent>();
		if (!Health || Health->IsDead())
		{
			continue;
		}

		const float Fraction = Health->GetHealthPercent();
		if (Fraction < BestFraction)
		{
			// Lower fraction = more wounded = higher priority.
			BestFraction = Fraction;
			Best = Ally;
		}
	}

	return Best;
}

EBTNodeResult::Type UBTTask_HealAlly::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	APawn* Pawn = AICon ? AICon->GetPawn() : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();

	if (!Pawn || !Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	UEnemyHealBeamComponent* HealBeam = Pawn->FindComponentByClass<UEnemyHealBeamComponent>();
	if (!HealBeam)
	{
		return EBTNodeResult::Failed;
	}

	AActor* Ally = FindMostWoundedAlly(Pawn);
	if (!Ally)
	{
		// No one needs healing - fail so the tree falls through to normal pursuit.
		return EBTNodeResult::Failed;
	}

	// Record the chosen ally so focus / other nodes can use it.
	Blackboard->SetValueAsObject(HealTargetKey.SelectedKeyName, Ally);

	if (bFocusTargetWhileHealing)
	{
		AICon->SetFocus(Ally, EAIFocusPriority::Gameplay);
	}

	if (bStopMovementWhileHealing)
	{
		AICon->StopMovement();
		if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
		}
	}

	if (!HealBeam->BeginAttack(Ally))
	{
		AICon->ClearFocus(EAIFocusPriority::Gameplay);
		return EBTNodeResult::Failed;
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_HealAlly::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AICon = OwnerComp.GetAIOwner();
	APawn* Pawn = AICon ? AICon->GetPawn() : nullptr;

	UEnemyHealBeamComponent* HealBeam = Pawn
		? Pawn->FindComponentByClass<UEnemyHealBeamComponent>()
		: nullptr;

	if (!HealBeam)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	const EHealBeamState BeamState = HealBeam->GetState();
	if (BeamState == EHealBeamState::Cooldown || BeamState == EHealBeamState::Idle)
	{
		if (AICon)
		{
			AICon->ClearFocus(EAIFocusPriority::Gameplay);
		}
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_HealAlly::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	APawn* Pawn = AICon ? AICon->GetPawn() : nullptr;

	if (Pawn)
	{
		if (UEnemyHealBeamComponent* HealBeam = Pawn->FindComponentByClass<UEnemyHealBeamComponent>())
		{
			HealBeam->AbortAttack();
		}
	}

	if (AICon)
	{
		AICon->ClearFocus(EAIFocusPriority::Gameplay);
	}

	return EBTNodeResult::Aborted;
}
