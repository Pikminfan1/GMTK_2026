// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DetourCrowdAIController.h"
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
 */
UCLASS()
class GMTK_2026_API AEnemyAIController : public ADetourCrowdAIController
{
	GENERATED_BODY()
	
public:
	AEnemyAIController();
	
	
protected:
	virtual void OnPossess(APawn* InPawn) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	//UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI", meta = (AllowPrivateAccess = "true"))
	//TObjectPtr<UAIPerceptionComponent> PerceptionComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FName AttackTargetKeyName = TEXT("AttackTarget");
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
};
