// Fill out your copyright notice in the Description page of Project Settings.

#include "Pickups/HealthOrbSpawnerComponent.h"
#include "Pickups/HealthOrb.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Utility/LogChannels.h"

UHealthOrbSpawnerComponent::UHealthOrbSpawnerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthOrbSpawnerComponent::NotifyOwnerDied()
{
	if (bHasDropped)
	{
		return;
	}
	bHasDropped = true;

	if (AActor* Owner = GetOwner())
	{
		SpawnOrbBurst(Owner->GetActorLocation());
	}
}

void UHealthOrbSpawnerComponent::SpawnOrbBurst(const FVector& Location)
{
	if (!OrbClass)
	{
		UE_LOG(LogGMTKSpawn, Verbose, TEXT("HealthOrbSpawnerComponent on %s has no OrbClass - no drop."),
			GetOwner() ? *GetOwner()->GetName() : TEXT("?"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();

	// Actor location is at the capsule center (chest); drop to the feet so orbs spawn
	// at ground level outside the body.
	FVector SpawnBase = Location;
	if (const ACharacter* OwnerChar = Cast<ACharacter>(OwnerActor))
	{
		if (const UCapsuleComponent* Capsule = OwnerChar->GetCapsuleComponent())
		{
			SpawnBase.Z -= Capsule->GetScaledCapsuleHalfHeight();
		}
	}
	SpawnBase.Z += SpawnHeightOffset;

	const int32 Count = FMath::RandRange(MinOrbsPerKill, MaxOrbsPerKill);

	for (int32 i = 0; i < Count; i++)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// Small random origin jitter so orbs don't all start from one point (helps big
		// bursts read as a scatter rather than a fountain from a single spot).
		const FVector Jitter(
			FMath::FRandRange(-SpawnJitterRadius, SpawnJitterRadius),
			FMath::FRandRange(-SpawnJitterRadius, SpawnJitterRadius),
			0.f);

		AHealthOrb* Orb = World->SpawnActor<AHealthOrb>(OrbClass, SpawnBase + Jitter, FRotator::ZeroRotator, SpawnParams);
		if (!Orb)
		{
			continue;
		}

		// Random horizontal direction.
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);

		// Per-orb speed within the band. The band itself sets the distance range, and
		// because it's rolled independently per orb, orbs travel varying distances -
		// so they don't all land on one perfect circle.
		const float Speed = FMath::FRandRange(ScatterSpeedMin, ScatterSpeedMax);

		Orb->LaunchScatter(Dir, Speed);
	}

	UE_LOG(LogGMTKSpawn, Verbose, TEXT("%s dropped %d health orbs"),
		OwnerActor ? *OwnerActor->GetName() : TEXT("?"), Count);
}
