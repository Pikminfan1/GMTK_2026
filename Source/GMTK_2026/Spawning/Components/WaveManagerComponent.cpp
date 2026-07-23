#include "Spawning/Components/WaveManagerComponent.h"
#include "Data/WaveDataAsset.h"
#include "Spawning/EnemySpawnPoint.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/Components/HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Utility/LogChannels.h"


// Sets default values for this component's properties
UWaveManagerComponent::UWaveManagerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


void UWaveManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	TArray<AActor*> FoundSpawnPoints;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemySpawnPoint::StaticClass(), FoundSpawnPoints);
	for (AActor* Actor : FoundSpawnPoints)
	{
		if (AEnemySpawnPoint* SpawnPoint = Cast<AEnemySpawnPoint>(Actor))
		{
			CachedSpawnPoints.Add(SpawnPoint);
		}
	}

	if (CachedSpawnPoints.Num() == 0)
	{
		UE_LOG(LogGMTKSpawn, Warning, TEXT("WaveManagerComponent found no EnemySpawnPoint actors in the level."));
	}
}

void UWaveManagerComponent::StartNextWave()
{
	CurrentWaveIndex++;

	if (!Waves.IsValidIndex(CurrentWaveIndex))
	{
		OnAllWavesCompleted.Broadcast();
		UE_LOG(LogGMTKSpawn, Log, TEXT("All waves completed."));
		return;
	}

	UWaveDataAsset* Wave = Waves[CurrentWaveIndex];
	if (!Wave)
	{
		UE_LOG(LogGMTKSpawn, Warning, TEXT("Wave entry %d is null, skipping."), CurrentWaveIndex);
		StartNextWave();
		return;
	}

	EnemiesRemaining = 0;
	for (const FWaveEnemyEntry& Entry : Wave->EnemyEntries)
	{
		EnemiesRemaining += Entry.Count;
		for (int32 i = 0; i < Entry.Count; i++)
		{
			SpawnEnemyEntry(Entry);
		}
	}

	OnWaveStarted.Broadcast(GetCurrentWaveNumber());
	UE_LOG(LogGMTKSpawn, Log, TEXT("Wave %d started with %d enemies"), GetCurrentWaveNumber(), EnemiesRemaining);

	// TODO: this spawns every enemy in the wave instantly. Entry.SpawnInterval is already
	// there for you to stagger spawns with a timer once you want that instead.
}

void UWaveManagerComponent::SpawnEnemyEntry(const FWaveEnemyEntry& Entry)
{
	if (!Entry.EnemyClass || CachedSpawnPoints.Num() == 0)
	{
		return;
	}
	
	AEnemySpawnPoint* SpawnPoint = CachedSpawnPoints[FMath::RandRange(0, CachedSpawnPoints.Num() - 1)];
	if (!SpawnPoint)
	{
		return;
	}
	
	TSubclassOf<AEnemyCharacter> ClassToSpawn = SpawnPoint->OverrideEnemyClass ? SpawnPoint->OverrideEnemyClass : Entry.EnemyClass;
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	
	//TODO: Before Passing the enemy transform, modify the rotation to be facing the player
	if (AEnemyCharacter* NewEnemy = GetWorld()->SpawnActor<AEnemyCharacter>(ClassToSpawn, SpawnPoint->GetActorTransform(), SpawnParams))
	{
		if (UHealthComponent* HealthComp = NewEnemy->GetHealthComponent())
		{
			HealthComp->OnDeath.AddDynamic(this, &UWaveManagerComponent::HandleEnemyDeath);
		}
		
	}
}

void UWaveManagerComponent::HandleEnemyDeath(AActor* DeadActor)
{
	EnemiesRemaining = FMath::Max(0, EnemiesRemaining - 1);
	
	if (EnemiesRemaining == 0)
	{
		OnWaveCompleted.Broadcast((GetCurrentWaveNumber()));
		UE_LOG(LogGMTKSpawn, Log, TEXT("Wave %d cleared"), GetCurrentWaveNumber());
		
		// TODO: decide whether the next wave starts automatically after a delay, or
		// waits for something else (a UI prompt, a pickup, a countdown) before calling StartNextWave().
		// This would be a good place for a shop system and upgrades to be inserted
	}
}




