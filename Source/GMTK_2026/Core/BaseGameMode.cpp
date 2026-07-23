#include "Core/BaseGameMode.h"
#include "Spawning/Components/WaveManagerComponent.h"

ABaseGameMode::ABaseGameMode()
{
	WaveManagerComponent = CreateDefaultSubobject<UWaveManagerComponent>(TEXT("WaveManagerComponent"));
}

void ABaseGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (WaveManagerComponent)
	{
		WaveManagerComponent->StartNextWave();
	}
}