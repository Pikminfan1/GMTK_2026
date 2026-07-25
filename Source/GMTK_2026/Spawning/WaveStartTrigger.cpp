// Fill out your copyright notice in the Description page of Project Settings.

#include "Spawning/WaveStartTrigger.h"
#include "Components/StaticMeshComponent.h"
#include "Core/BaseGameMode.h"
#include "Spawning/Components/WaveManagerComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Utility/LogChannels.h"

AWaveStartTrigger::AWaveStartTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// Block the weapon's Pawn-channel trace so it can be shot like an enemy.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
}

void AWaveStartTrigger::BeginPlay()
{
	Super::BeginPlay();
}

bool AWaveStartTrigger::ApplyDamage_Implementation(float DamageAmount, AController* EventInstigator, AActor* DamageCauser)
{
	if (bHasFired && !bReusable)
	{
		return false;
	}

	ABaseGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABaseGameMode>() : nullptr;
	UWaveManagerComponent* WaveManager = GameMode ? GameMode->GetWaveManager() : nullptr;
	if (!WaveManager)
	{
		UE_LOG(LogGMTKSpawn, Warning, TEXT("WaveStartTrigger hit but no WaveManager found."));
		return false;
	}

	// Don't let the player skip ahead while enemies are still alive, if configured so.
	if (bOnlyBetweenWaves && WaveManager->IsWaveActive())
	{
		UE_LOG(LogGMTKSpawn, Verbose, TEXT("WaveStartTrigger hit but a wave is still active - ignoring."));
		return false;
	}

	bHasFired = true;

	// FX hooks fire before the spawn so effects line up with the "press".
	OnWaveStartTriggered.Broadcast();
	OnActivatedVisual();

	WaveManager->StartNextWave();

	UE_LOG(LogGMTKSpawn, Log, TEXT("WaveStartTrigger activated - starting next wave."));
	return true;
}
