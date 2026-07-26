// Fill out your copyright notice in the Description page of Project Settings.

#include "World/ReloadZone.h"
#include "World/ReloadPoint.h"
#include "Characters/PlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Utility/LogChannels.h"

AReloadZone::AReloadZone()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerVolume = CreateDefaultSubobject<UCapsuleComponent>(TEXT("TriggerVolume"));
	SetRootComponent(TriggerVolume);
	TriggerVolume->InitCapsuleSize(150.f, 150.f);
	TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AReloadZone::BeginPlay()
{
	Super::BeginPlay();

	TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &AReloadZone::HandleTriggerBeginOverlap);
	TriggerVolume->OnComponentEndOverlap.AddDynamic(this, &AReloadZone::HandleTriggerEndOverlap);

	// Discover all reload points in the level.
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AReloadPoint::StaticClass(), Found);
	for (AActor* Actor : Found)
	{
		if (AReloadPoint* Point = Cast<AReloadPoint>(Actor))
		{
			ReloadPoints.Add(Point);
		}
	}

	if (ReloadPoints.Num() == 0)
	{
		UE_LOG(LogGMTKCombat, Warning, TEXT("ReloadZone found no AReloadPoint actors - it has nowhere to go."));
		return;
	}

	// Start at a random point.
	AReloadPoint* Start = ReloadPoints[FMath::RandRange(0, ReloadPoints.Num() - 1)];
	MoveToPoint(Start);
}

void AReloadZone::MoveToPoint(AReloadPoint* Point)
{
	if (!Point)
	{
		return;
	}

	CurrentPoint = Point;
	SetActorLocation(Point->GetActorLocation());

	OnZoneShown();
	OnZoneActivated.Broadcast();

	UE_LOG(LogGMTKCombat, Verbose, TEXT("ReloadZone now at %s"), *Point->GetName());
}

void AReloadZone::RelocateToNewPoint()
{
	if (ReloadPoints.Num() == 0)
	{
		return;
	}

	// Signal the old spot is going away (particle off) before moving.
	OnZoneHidden();

	// If there's more than one point, pick any point that isn't the current one so the
	// zone always visibly moves. With a single point it just stays put.
	AReloadPoint* NewPoint = CurrentPoint;
	if (ReloadPoints.Num() == 1)
	{
		NewPoint = ReloadPoints[0];
	}
	else
	{
		while (NewPoint == CurrentPoint)
		{
			NewPoint = ReloadPoints[FMath::RandRange(0, ReloadPoints.Num() - 1)];
		}
	}

	MoveToPoint(NewPoint);
	OnZoneRelocated.Broadcast();
}

void AReloadZone::HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!Player)
	{
		return;
	}

	OverlappingPlayer = Player;

	// Tell the player they may reload now.
	Player->SetInReloadZone(true);

	// Listen for the player's reload so we can relocate once they use the zone. Bound
	// only while overlapping; unbound on exit.
	Player->OnReloadStarted.AddUniqueDynamic(this, &AReloadZone::HandlePlayerReloaded);
}

void AReloadZone::HandleTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!Player)
	{
		return;
	}

	Player->SetInReloadZone(false);
	Player->OnReloadStarted.RemoveDynamic(this, &AReloadZone::HandlePlayerReloaded);

	if (OverlappingPlayer == Player)
	{
		OverlappingPlayer = nullptr;
	}
}

void AReloadZone::HandlePlayerReloaded()
{
	// The player reloaded while standing in the zone - relocate to a new point. Clear
	// the current player's in-zone flag and unbind first (they're about to be "outside"
	// the zone once it moves away from them).
	if (OverlappingPlayer)
	{
		OverlappingPlayer->SetInReloadZone(false);
		OverlappingPlayer->OnReloadStarted.RemoveDynamic(this, &AReloadZone::HandlePlayerReloaded);
		OverlappingPlayer = nullptr;
	}

	RelocateToNewPoint();
}
