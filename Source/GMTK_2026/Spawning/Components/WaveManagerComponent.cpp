#include "Spawning/Components/WaveManagerComponent.h"
#include "Data/WaveDataAsset.h"
#include "Spawning/EnemySpawnPoint.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/Components/HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Utility/LogChannels.h"
#include "TimerManager.h"

UWaveManagerComponent::UWaveManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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

void UWaveManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Don't leave the between-wave timer pointing at a torn-down component.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NextWaveTimer);
	}
	Super::EndPlay(EndPlayReason);
}

bool UWaveManagerComponent::IsBetweenWaves() const
{
	// A pending next-wave timer means a wave was cleared and the next hasn't begun.
	return GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(NextWaveTimer);
}

void UWaveManagerComponent::StartNextWave(bool bForce)
{
	// Any manual/early start cancels a pending auto-advance so the two can't both fire
	// and double-spawn. This is what lets the trigger act as an early-start: shooting
	// it during the countdown starts the wave now and clears the timer.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NextWaveTimer);
	}

	// Guard against starting a new wave while the current one is still being fought,
	// so a stray call (or an over-eager player shooting the start actor twice) can't
	// double-spawn. bForce bypasses this for debug or scripted starts.
	if (IsWaveActive() && !bForce)
	{
		UE_LOG(LogGMTKSpawn, Log, TEXT("StartNextWave ignored - wave %d still active (%d enemies left)."),
			GetCurrentWaveNumber(), EnemiesRemaining);
		return;
	}

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
		StartNextWave(bForce);
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

	// Relay the victim upward so the GameMode can report it into the central kill
	// event (with a resolved killer) and drive score/combo/pickups from one place.
	OnEnemyDied.Broadcast(DeadActor);

	if (EnemiesRemaining == 0)
	{
		OnWaveCompleted.Broadcast(GetCurrentWaveNumber());
		UE_LOG(LogGMTKSpawn, Log, TEXT("Wave %d cleared"), GetCurrentWaveNumber());

		if (bAutoAdvanceWaves)
		{
			// Auto-advance: schedule the next wave after the between-wave delay. The
			// shootable trigger can still call StartNextWave() during this window to
			// begin early - that path clears this timer, so there's no double-start.
			if (UWorld* World = GetWorld())
			{
				if (TimeBetweenWaves > 0.f)
				{
					World->GetTimerManager().SetTimer(
						NextWaveTimer,
						[this]() { StartNextWave(); },
						TimeBetweenWaves,
						false);

					OnNextWaveCountdownStarted.Broadcast(TimeBetweenWaves);
					UE_LOG(LogGMTKSpawn, Log, TEXT("Next wave in %.1fs (or shoot the trigger to start early)."), TimeBetweenWaves);
				}
				else
				{
					// No delay configured - go straight to the next wave.
					StartNextWave();
				}
			}
		}
		else
		{
			// Auto-advance disabled: the next wave begins only when something external
			// calls StartNextWave() - e.g. the shootable "start wave" actor.
			UE_LOG(LogGMTKSpawn, Log, TEXT("Auto-advance off - waiting for the trigger to start the next wave."));
		}
	}
}
