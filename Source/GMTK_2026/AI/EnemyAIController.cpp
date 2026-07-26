#include "AI/EnemyAIController.h"
#include "Kismet/GameplayStatics.h"
#include "Characters/PlayerCharacter.h"
#include "AI/Tokens/CombatTokenSubsystem.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/BaseCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/Components/HealthComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Utility/CombatTeams.h"
#include "Utility/LogChannels.h"

AEnemyAIController::AEnemyAIController()
{
	UAIPerceptionComponent* NewPerceptionComponent =
		CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 2200.f;
	SightConfig->LoseSightRadius = 2600.f;
	SightConfig->PeripheralVisionAngleDegrees = 90.f;
	SightConfig->SetMaxAge(5.f);

	// Only react to hostiles. GetTeamAttitudeTowards() below is what actually
	// classifies actors, so enemies no longer register each other as targets.
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

	NewPerceptionComponent->ConfigureSense(*SightConfig);
	NewPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
	NewPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(
		this, &AEnemyAIController::OnTargetPerceptionUpdated);

	SetPerceptionComponent(*NewPerceptionComponent);

	// Sight radius needs to comfortably exceed the laser's max engagement range,
	// otherwise the enemy loses its target mid-attack and the beam cuts out.
	SetGenericTeamId(CombatTeams::Enemy);
}

FGenericTeamId AEnemyAIController::GetGenericTeamId() const
{
	return FGenericTeamId(TeamId);
}

ETeamAttitude::Type AEnemyAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	// Ask the other actor (or its controller) which team it's on. Pawns don't
	// implement the interface themselves, so fall through to their controller.
	const IGenericTeamAgentInterface* OtherTeamAgent = Cast<const IGenericTeamAgentInterface>(&Other);

	if (!OtherTeamAgent)
	{
		if (const APawn* OtherPawn = Cast<const APawn>(&Other))
		{
			OtherTeamAgent = Cast<const IGenericTeamAgentInterface>(OtherPawn->GetController());
		}
	}

	if (!OtherTeamAgent)
	{
		return ETeamAttitude::Neutral;
	}

	return OtherTeamAgent->GetGenericTeamId() == GetGenericTeamId()
		? ETeamAttitude::Friendly
		: ETeamAttitude::Hostile;
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!BehaviorTreeAsset)
	{
		UE_LOG(LogGMTKAI, Warning, TEXT("%s possessed %s with no BehaviorTreeAsset assigned"),
			*GetName(), InPawn ? *InPawn->GetName() : TEXT("nullptr"));
		return;
	}

	if (BehaviorTreeAsset->BlackboardAsset)
	{
		UBlackboardComponent* BlackboardComp = nullptr;
		if (UseBlackboard(BehaviorTreeAsset->BlackboardAsset, BlackboardComp))
		{
			Blackboard = BlackboardComp;
		}
	}

	RunBehaviorTree(BehaviorTreeAsset);

	if (Blackboard)
	{
		// SelfActor lets EQS tests and BT nodes reference the querying pawn without
		// each one having to walk the controller chain.
		Blackboard->SetValueAsObject(SelfActorKey, InPawn);
		Blackboard->SetValueAsBool(IsDeadKey, false);
	}

	// Constant pursuit: seed the player as the target immediately so the enemy chases
	// from spawn without needing line of sight first. Perception still refines the
	// last-known location while the player is visible.
	if (bConstantPursuit && Blackboard)
	{
		if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			Blackboard->SetValueAsObject(TargetActorKey, PlayerPawn);
			Blackboard->SetValueAsVector(LastKnownLocationKey, PlayerPawn->GetActorLocation());
		}
	}

	// Death has to release the combat token, or the budget leaks every time an
	// enemy dies mid-attack and eventually nobody can attack at all. Damage forces
	// aggro and attempts a token steal so the shot enemy fights back.
	if (const ABaseCharacter* PossessedCharacter = Cast<ABaseCharacter>(InPawn))
	{
		if (UHealthComponent* Health = PossessedCharacter->GetHealthComponent())
		{
			Health->OnDeath.AddUniqueDynamic(this, &AEnemyAIController::HandlePawnDeath);
			Health->OnDamaged.AddUniqueDynamic(this, &AEnemyAIController::HandlePawnDamaged);
		}
	}

	// Kick off the spawn-in sequence now that the Behavior Tree is running - this
	// pauses the brain again until the spawn animation finishes. Done here (after
	// RunBehaviorTree) rather than in the pawn's BeginPlay/PossessedBy so the pause
	// isn't immediately undone by the tree starting.
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(InPawn))
	{
		if (Enemy->UsesSpawnSequence())
		{
			Enemy->BeginSpawnSequence();
		}
	}
}

void AEnemyAIController::OnUnPossess()
{
	ReleaseCombatToken();
	Super::OnUnPossess();
}

void AEnemyAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseCombatToken();
	Super::EndPlay(EndPlayReason);
}

void AEnemyAIController::HandlePawnDeath(AActor* DeadActor)
{
	ReleaseCombatToken();

	if (Blackboard)
	{
		Blackboard->SetValueAsBool(IsDeadKey, true);
	}

	// Stop the tree so a half-finished attack branch can't keep ticking on a corpse.
	if (UBrainComponent* Brain = GetBrainComponent())
	{
		Brain->StopLogic(TEXT("Pawn died"));
	}
}

void AEnemyAIController::ReleaseCombatToken()
{
	if (const UWorld* World = GetWorld())
	{
		if (UCombatTokenSubsystem* Tokens = World->GetSubsystem<UCombatTokenSubsystem>())
		{
			Tokens->ReleaseToken(GetPawn());
		}
	}
}

void AEnemyAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Blackboard || !Actor)
	{
		return;
	}

	// Belt-and-braces: the affiliation filter should already have excluded friendlies,
	// but an explicit check here means a misconfigured team ID can't cause enemies to
	// start lasering each other.
	if (GetTeamAttitudeTowards(*Actor) != ETeamAttitude::Hostile)
	{
		return;
	}
	
	if (!Actor->IsA<APlayerCharacter>())
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		Blackboard->SetValueAsObject(TargetActorKey, Actor);
		Blackboard->SetValueAsVector(LastKnownLocationKey, Actor->GetActorLocation());
	}
	else
	{
		// Keep TargetActor set so the tree can run its search behaviour toward the
		// last known position. BTService_CombatState clears it if the target goes
		// stale or dies.
		Blackboard->SetValueAsVector(LastKnownLocationKey, Stimulus.StimulusLocation);
	}
}

void AEnemyAIController::HandlePawnDamaged(float DamageAmount, AController* EventInstigator)
{
	// Force aggro: lock onto the player the instant this enemy is hurt (covers the case
	// where constant pursuit is off, or the target was somehow cleared).
	if (Blackboard)
	{
		if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			Blackboard->SetValueAsObject(TargetActorKey, PlayerPawn);
			Blackboard->SetValueAsVector(LastKnownLocationKey, PlayerPawn->GetActorLocation());
		}
	}

	// Try to steal a token from an idle holder so the enemy being shot can fight back.
	if (UWorld* World = GetWorld())
	{
		if (UCombatTokenSubsystem* Tokens = World->GetSubsystem<UCombatTokenSubsystem>())
		{
			Tokens->TryStealTokenFor(GetPawn(), AggroTokenType);
		}
	}
}
