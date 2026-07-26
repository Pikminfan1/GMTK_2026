// Fill out your copyright notice in the Description page of Project Settings.

#include "World/ReloadZone.h"
#include "World/ReloadPoint.h"
#include "Characters/PlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Utility/LogChannels.h"

// Shared registry of points occupied by any zone, so zones never stack on one point.
TSet<TWeakObjectPtr<AReloadPoint>> AReloadZone::ClaimedPoints;

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

void AReloadZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Free our point so a future/other zone can use it, and don't leave a stale claim.
	ReleasePoint(CurrentPoint);
	CurrentPoint = nullptr;
	Super::EndPlay(EndPlayReason);
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

	// Start at a free point (one no other zone has claimed) so multiple zones spread out.
	AReloadPoint* Start = PickFreePoint();
	if (!Start)
	{
		// More zones than points, or all claimed - fall back to any point so this zone
		// still functions (it may share, but that's better than sitting nowhere).
		Start = ReloadPoints[FMath::RandRange(0, ReloadPoints.Num() - 1)];
		UE_LOG(LogGMTKCombat, Warning, TEXT("ReloadZone: no free reload point at start - more zones than points?"));
	}
	MoveToPoint(Start);
}

void AReloadZone::MoveToPoint(AReloadPoint* Point)
{
	if (!Point)
	{
		return;
	}

	// Update the shared claim registry: free the old point, claim the new one.
	ReleasePoint(CurrentPoint);
	CurrentPoint = Point;
	ClaimPoint(CurrentPoint);

	SetActorLocation(Point->GetActorLocation());

	// Teleporting a trigger does NOT automatically re-evaluate overlaps, so force it.
	// Without this, the engine keeps stale overlap state from the old location and the
	// player won't get a fresh BeginOverlap at the new spot - which broke reloads after
	// the first relocation.
	TriggerVolume->UpdateOverlaps();

	// Re-sync the player's in-zone state against the NEW location: if the player is
	// standing inside the trigger now, mark them in-zone and (re)bind the reload hook;
	// otherwise they're outside until they walk in.
	SyncOverlappingPlayer();

	OnZoneShown();
	OnZoneActivated.Broadcast();

	UE_LOG(LogGMTKCombat, Verbose, TEXT("ReloadZone now at %s"), *Point->GetName());
}

void AReloadZone::SyncOverlappingPlayer()
{
	// Find the player and test whether they're currently within the trigger volume.
	APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (!Player)
	{
		return;
	}

	TArray<AActor*> Overlapping;
	TriggerVolume->GetOverlappingActors(Overlapping, APlayerCharacter::StaticClass());
	const bool bPlayerInside = Overlapping.Contains(Player);

	if (bPlayerInside)
	{
		OverlappingPlayer = Player;
		Player->SetInReloadZone(true);
		Player->OnReloadStopped.AddUniqueDynamic(this, &AReloadZone::HandlePlayerReloaded);
	}
	else
	{
		if (OverlappingPlayer == Player)
		{
			OverlappingPlayer = nullptr;
		}
		Player->SetInReloadZone(false);
		Player->OnReloadStopped.RemoveDynamic(this, &AReloadZone::HandlePlayerReloaded);
	}
}

void AReloadZone::RelocateToNewPoint()
{
	if (ReloadPoints.Num() == 0)
	{
		return;
	}

	// Signal the old spot is going away (particle off) before moving.
	OnZoneHidden();

	// Pick a point that isn't the current one AND isn't claimed by another zone.
	AReloadPoint* NewPoint = PickFreePoint();
	if (!NewPoint)
	{
		// Nowhere free to move (e.g. only one point, or all others claimed). Stay put
		// rather than stacking onto another zone's point.
		UE_LOG(LogGMTKCombat, Verbose, TEXT("ReloadZone: no free point to relocate to - staying."));
		return;
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
	Player->OnReloadStopped.AddUniqueDynamic(this, &AReloadZone::HandlePlayerReloaded);
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
	Player->OnReloadStopped.RemoveDynamic(this, &AReloadZone::HandlePlayerReloaded);

	if (OverlappingPlayer == Player)
	{
		OverlappingPlayer = nullptr;
	}
}

void AReloadZone::HandlePlayerReloaded()
{
	// Bound to the player's OnReloadStopped (reload RELEASE). Only relocate if a real
	// reload actually loaded rounds - a tap-and-release on full mags shouldn't move the
	// zone.
	if (OverlappingPlayer && !OverlappingPlayer->DidLastReloadLoadRounds())
	{
		return;
	}

	// RelocateToNewPoint -> MoveToPoint -> SyncOverlappingPlayer re-evaluates the
	// player's in-zone state at the new location, so we don't manually clear it here.
	RelocateToNewPoint();
}

AReloadPoint* AReloadZone::PickFreePoint() const
{
	// Collect points that aren't the current one and aren't claimed by another zone.
	TArray<AReloadPoint*> Candidates;
	for (AReloadPoint* Point : ReloadPoints)
	{
		if (!Point || Point == CurrentPoint)
		{
			continue;
		}
		if (ClaimedPoints.Contains(Point))
		{
			continue; // another zone is here
		}
		Candidates.Add(Point);
	}

	if (Candidates.Num() == 0)
	{
		return nullptr;
	}
	return Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
}

void AReloadZone::ClaimPoint(AReloadPoint* Point)
{
	if (Point)
	{
		ClaimedPoints.Add(Point);
	}
}

void AReloadZone::ReleasePoint(AReloadPoint* Point)
{
	if (Point)
	{
		ClaimedPoints.Remove(Point);
	}
}
