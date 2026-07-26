// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AI/Tokens/CombatTokenSubsystem.h"
#include "DetourCrowdAIController.h"
#include "GenericTeamAgentInterface.h"
#include "Perception/AIPerceptionTypes.h"
#include "EnemyAIController.generated.h"

class UBehaviorTree;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;

UENUM(BlueprintType)
enum class EEnemyState : uint8
{
	Idle        UMETA(DisplayName = "Idle"),
	Patrolling  UMETA(DisplayName = "Patrolling"),
	Chasing     UMETA(DisplayName = "Chasing"),
	Attacking   UMETA(DisplayName = "Attacking"),
	Dead        UMETA(DisplayName = "Dead")
};

UENUM(BlueprintType)
enum class EEnemyMovementSpeed : uint8
{
	Idle        UMETA(DisplayName = "Idle"),
	Walking		UMETA(DisplayName = "Walking"),
	Jogging     UMETA(DisplayName = "Jogging"),
	Sprinting   UMETA(DisplayName = "Sprinting"),
};

/**
 * Runs the Behavior Tree and owns perception for each enemy. Derives from
 * ADetourCrowdAIController (not plain AAIController) so multiple enemies converging
 * on the player locally avoid each other via Detour Crowd instead of just clumping.
 *
 * Implements IGenericTeamAgentInterface so perception can tell the player apart from
 * other enemies - without that, enemies perceive each other and end up writing a
 * fellow enemy into TargetActor.
 */
UCLASS()
class GMTK_2026_API AEnemyAIController : public ADetourCrowdAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController();

	//~ Begin IGenericTeamAgentInterface
	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	//~ End IGenericTeamAgentInterface

	/** Blackboard key names. Exposed so a Blueprint subclass can rename them if needed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Blackboard")
	FName TargetActorKey = TEXT("TargetActor");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Blackboard")
	FName LastKnownLocationKey = TEXT("LastKnownLocation");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Blackboard")
	FName SelfActorKey = TEXT("SelfActor");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Blackboard")
	FName IsDeadKey = TEXT("IsDead");

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	/** Which team this controller belongs to. Set to Enemy by default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Team")
	uint8 TeamId = 1;

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/** Bound to the possessed pawn's HealthComponent so death releases the token. */
	UFUNCTION()
	void HandlePawnDeath(AActor* DeadActor);

	/** Bound to the possessed pawn's HealthComponent. On damage, forces aggro onto the
	 *  player and tries to steal a token so the shot enemy fights back. */
	UFUNCTION()
	void HandlePawnDamaged(float DamageAmount, AController* EventInstigator);

	/** If true, the enemy always targets the player from possession on (constant
	 *  pursuit), instead of only acquiring via line of sight. Perception still updates
	 *  the last-known location for nicer movement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI|Aggro")
	bool bConstantPursuit = true;

	/** The token type this enemy uses, for steal-on-damage grants. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI|Aggro")
	ETokenRequestType AggroTokenType = ETokenRequestType::RangedLaser;

private:
	/** Hands the combat token back, if this pawn is holding one. Safe to call repeatedly. */
	void ReleaseCombatToken();
};
