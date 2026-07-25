#include "AI/BTNodes/BTService_CombatState.h"

#include "AIController.h"
#include "AI/Tokens/CombatTokenSubsystem.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "Characters/BaseCharacter.h"
#include "Characters/Components/HealthComponent.h"
#include "GameFramework/Pawn.h"

UBTService_CombatState::UBTService_CombatState()
{
	NodeName = TEXT("Update Combat State");

	Interval = 0.15f;
	RandomDeviation = 0.05f;

	bNotifyBecomeRelevant = true;
	bNotifyTick = true;

	// Constrain each selector to the right key type so the editor dropdowns only
	// offer valid keys and typos become compile-time-ish errors instead of silent
	// runtime no-ops.
	TargetActorKey.AddObjectFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTService_CombatState, TargetActorKey), AActor::StaticClass());

	DistanceToTargetKey.AddFloatFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTService_CombatState, DistanceToTargetKey));
	PressureKey.AddFloatFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTService_CombatState, PressureKey));
	WaitTimeKey.AddFloatFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTService_CombatState, WaitTimeKey));

	CanSeeTargetKey.AddBoolFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTService_CombatState, CanSeeTargetKey));
	InMeleeRangeKey.AddBoolFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTService_CombatState, InMeleeRangeKey));
	ShouldRepositionKey.AddBoolFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTService_CombatState, ShouldRepositionKey));
	HasValidVantageKey.AddBoolFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTService_CombatState, HasValidVantageKey));
	HasTokenKey.AddBoolFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTService_CombatState, HasTokenKey));

	LastKnownLocationKey.AddVectorFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTService_CombatState, LastKnownLocationKey));
	VantagePointKey.AddVectorFilter(this,
		GET_MEMBER_NAME_CHECKED(UBTService_CombatState, VantagePointKey));

	// Sensible defaults so a freshly-placed node mostly just works.
	TargetActorKey.SelectedKeyName = TEXT("TargetActor");
	DistanceToTargetKey.SelectedKeyName = TEXT("DistanceToTarget");
	CanSeeTargetKey.SelectedKeyName = TEXT("CanSeeTarget");
	LastKnownLocationKey.SelectedKeyName = TEXT("LastKnownLocation");
	InMeleeRangeKey.SelectedKeyName = TEXT("InMeleeRange");
	PressureKey.SelectedKeyName = TEXT("Pressure");
	ShouldRepositionKey.SelectedKeyName = TEXT("ShouldReposition");
	HasValidVantageKey.SelectedKeyName = TEXT("HasValidVantage");
	VantagePointKey.SelectedKeyName = TEXT("VantagePoint");
	HasTokenKey.SelectedKeyName = TEXT("HasToken");
	WaitTimeKey.SelectedKeyName = TEXT("WaitTime");
}

FString UBTService_CombatState::GetStaticDescription() const
{
	return FString::Printf(
		TEXT("Melee < %.0f | Panic < %.0f | Reposition after %.1fs"),
		MeleeRadius, PanicRadius, PressureThreshold);
}

void UBTService_CombatState::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* AICon = OwnerComp.GetAIOwner();
	APawn* Pawn = AICon ? AICon->GetPawn() : nullptr;

	if (!Blackboard || !Pawn)
	{
		return;
	}

	AActor* TargetActor = Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName));

	// Token state is worth mirroring even with no target, so the tree can still
	// make sensible decisions while searching.
	if (UCombatTokenSubsystem* Tokens = GetWorld()->GetSubsystem<UCombatTokenSubsystem>())
	{
		Blackboard->SetValueAsBool(HasTokenKey.SelectedKeyName, Tokens->HoldsToken(Pawn));
		Blackboard->SetValueAsFloat(WaitTimeKey.SelectedKeyName, Tokens->GetWaitTime(Pawn));
	}

	if (!IsValid(TargetActor))
	{
		Blackboard->SetValueAsBool(CanSeeTargetKey.SelectedKeyName, false);
		Blackboard->SetValueAsBool(InMeleeRangeKey.SelectedKeyName, false);
		return;
	}

	// A dead target should drop the enemy out of combat entirely rather than
	// leaving it beaming a corpse.
	if (bClearTargetOnDeath)
	{
		if (const ABaseCharacter* TargetCharacter = Cast<ABaseCharacter>(TargetActor))
		{
			const UHealthComponent* TargetHealth = TargetCharacter->GetHealthComponent();
			if (TargetHealth && TargetHealth->IsDead())
			{
				Blackboard->ClearValue(TargetActorKey.SelectedKeyName);
				Blackboard->SetValueAsBool(CanSeeTargetKey.SelectedKeyName, false);
				Blackboard->SetValueAsBool(InMeleeRangeKey.SelectedKeyName, false);
				return;
			}
		}
	}

	// ---------- Distance ----------

	const float Distance = FVector::Dist(Pawn->GetActorLocation(), TargetActor->GetActorLocation());
	Blackboard->SetValueAsFloat(DistanceToTargetKey.SelectedKeyName, Distance);
	Blackboard->SetValueAsBool(InMeleeRangeKey.SelectedKeyName, Distance < MeleeRadius);

	// ---------- Line of sight ----------

	const bool bCanSee = AICon->LineOfSightTo(TargetActor);
	Blackboard->SetValueAsBool(CanSeeTargetKey.SelectedKeyName, bCanSee);

	if (bCanSee)
	{
		Blackboard->SetValueAsVector(LastKnownLocationKey.SelectedKeyName, TargetActor->GetActorLocation());
	}

	// ---------- Pressure ----------

	// Pressure is an accumulator rather than a plain distance check so that a
	// player who dashes past briefly doesn't trigger a full reposition, but one
	// who commits to closing the gap does.
	float Pressure = Blackboard->GetValueAsFloat(PressureKey.SelectedKeyName);

	Pressure = (Distance < PanicRadius)
		? Pressure + DeltaSeconds
		: FMath::Max(0.f, Pressure - DeltaSeconds * PressureDecayRate);

	Blackboard->SetValueAsFloat(PressureKey.SelectedKeyName, Pressure);
	Blackboard->SetValueAsBool(ShouldRepositionKey.SelectedKeyName, Pressure >= PressureThreshold);

	// ---------- Vantage validity ----------

	// Validity must match what the LASER can actually do, not the lenient
	// LineOfSightTo (which traces from the eyes to several points and passes if any
	// connect). A spot where the eyes can see the player but the chest-height beam
	// hits a ramp is NOT a good vantage - checking it with the same kind of trace
	// the beam uses stops the enemy believing a blocked spot is fine.
	bool bLaserCanConnect = false;
	{
		const FVector Muzzle = Pawn->GetActorLocation() + FVector(0, 0, LaserTraceHeight);
		const FVector TargetChest = TargetActor->GetActorLocation() + FVector(0, 0, LaserTraceHeight);

		FCollisionQueryParams Params(SCENE_QUERY_STAT(VantageLOS), false, Pawn);
		Params.AddIgnoredActor(TargetActor);

		const bool bBlocked = GetWorld()->LineTraceTestByChannel(
			Muzzle, TargetChest, ECC_Visibility, Params);

		bLaserCanConnect = !bBlocked;
	}

	const bool bInRangeBand = Distance >= MinVantageDistance && Distance <= MaxVantageDistance;
	const bool bCurrentSpotIsGood = bLaserCanConnect && bInRangeBand;

	Blackboard->SetValueAsBool(HasValidVantageKey.SelectedKeyName, bCurrentSpotIsGood);
}
