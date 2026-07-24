#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTService_CombatState.generated.h"

/**
 * The single source of truth for combat blackboard state. Rather than scattering
 * distance checks and LOS traces across a dozen decorators that each re-derive the
 * same facts, everything is computed once here and written to the blackboard for
 * the rest of the tree to read.
 *
 * Put this on the root Selector of the combat tree so it runs for the whole fight.
 */
UCLASS()
class GMTK_2026_API UBTService_CombatState : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_CombatState();

	virtual FString GetStaticDescription() const override;

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	// ---------- Blackboard keys ----------

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector DistanceToTargetKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector CanSeeTargetKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector LastKnownLocationKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector InMeleeRangeKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector PressureKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector ShouldRepositionKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector HasValidVantageKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector VantagePointKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector HasTokenKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector WaitTimeKey;

	// ---------- Tuning ----------

	/** Inside this distance the enemy considers switching to melee. */
	UPROPERTY(EditAnywhere, Category = "Combat|Ranges")
	float MeleeRadius = 250.f;

	/** Inside this distance, pressure builds and the enemy eventually flees. */
	UPROPERTY(EditAnywhere, Category = "Combat|Ranges")
	float PanicRadius = 600.f;

	/** A vantage point this close to the player is no longer usable. */
	UPROPERTY(EditAnywhere, Category = "Combat|Ranges")
	float MinVantageDistance = 700.f;

	/**
	 * Seconds the player must stay inside PanicRadius before the enemy repositions.
	 * This delay is deliberate - it's what gives the player a window to close the
	 * distance and land a hit instead of the enemy teleporting away instantly.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat|Pressure")
	float PressureThreshold = 1.5f;

	/** How fast pressure bleeds off once the player backs away again. */
	UPROPERTY(EditAnywhere, Category = "Combat|Pressure")
	float PressureDecayRate = 2.0f;

	/** Clear TargetActor if the target dies, so the tree drops out of combat. */
	UPROPERTY(EditAnywhere, Category = "Combat")
	bool bClearTargetOnDeath = true;
};
