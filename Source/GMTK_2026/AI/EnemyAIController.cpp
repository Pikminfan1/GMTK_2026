#include "AI/EnemyAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Utility/LogChannels.h"

AEnemyAIController::AEnemyAIController()
{
	UAIPerceptionComponent* NewPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 1500.f;
	SightConfig->LoseSightRadius = 1800.f;
	SightConfig->PeripheralVisionAngleDegrees = 90.f;
	SightConfig->SetMaxAge(5.f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	NewPerceptionComponent->ConfigureSense(*SightConfig);
	NewPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
	NewPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AEnemyAIController::OnTargetPerceptionUpdated);

	SetPerceptionComponent(*NewPerceptionComponent);
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (BehaviorTreeAsset)
	{
		if (BehaviorTreeAsset->BlackboardAsset)
		{
			UBlackboardComponent* BlackboardComp = nullptr;
			if (UseBlackboard(BehaviorTreeAsset->BlackboardAsset, BlackboardComp))
			{
				Blackboard = BlackboardComp;
			}
		}
		RunBehaviorTree(BehaviorTreeAsset);
	}
	else
	{
		UE_LOG(LogGMTKAI, Warning, TEXT("%s possessed %s with no BehaviorTreeAsset assigned"), *GetName(), *InPawn->GetName());
	}
}

void AEnemyAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Blackboard)
	{
		return;
	}

	// TODO: swap "TargetActor"/"CanSeeTarget" for whatever Blackboard keys you actually
	// set up once the tree (and the token system it'll coordinate with) exists.
	if (Stimulus.WasSuccessfullySensed())
	{
		Blackboard->SetValueAsObject(TEXT("TargetActor"), Actor);
		Blackboard->SetValueAsBool(TEXT("CanSeeTarget"), true);
	}
	else
	{
		Blackboard->SetValueAsBool(TEXT("CanSeeTarget"), false);
	}
}