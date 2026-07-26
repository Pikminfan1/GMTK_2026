// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ReloadZone.generated.h"

class UCapsuleComponent;
class AReloadPoint;
class APlayerCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReloadZoneActivated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReloadZoneRelocated);

/**
 * The single active reload zone. Sits at one AReloadPoint at a time; while the player
 * is inside its trigger, reloading is allowed (the player's bReloadRequiresZone gate
 * checks IsInReloadZone). After the player reloads here, the zone relocates to a
 * DIFFERENT reload point.
 *
 * Only ONE of these should exist. It discovers all AReloadPoint actors in the level at
 * BeginPlay and picks its starting point.
 *
 * Bring your own particle: this actor exposes OnZoneShown / OnZoneHidden Blueprint
 * events so a Blueprint subclass can toggle a Niagara/Cascade effect on and off as the
 * zone appears and relocates.
 */
UCLASS()
class GMTK_2026_API AReloadZone : public AActor
{
	GENERATED_BODY()

public:
	AReloadZone();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(BlueprintAssignable, Category = "ReloadZone")
	FOnReloadZoneActivated OnZoneActivated;

	UPROPERTY(BlueprintAssignable, Category = "ReloadZone")
	FOnReloadZoneRelocated OnZoneRelocated;

	/** Fires when the zone becomes visible at a point - toggle your particle ON here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "ReloadZone")
	void OnZoneShown();

	/** Fires just before the zone leaves a point - toggle your particle OFF here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "ReloadZone")
	void OnZoneHidden();

	/** Move the zone to a random reload point that isn't the current one. */
	UFUNCTION(BlueprintCallable, Category = "ReloadZone")
	void RelocateToNewPoint();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ReloadZone", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> TriggerVolume;

	/** How the zone detects the player reloaded inside it: it binds to the player's
	 *  OnReloadStarted while the player is overlapping, and relocates on that event. */
	UFUNCTION()
	void HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** Bound to the player's OnReloadStarted while they're in the zone. */
	UFUNCTION()
	void HandlePlayerReloaded();

private:
	void MoveToPoint(AReloadPoint* Point);

	/** Pick a random reload point that is neither the current one nor claimed by another
	 *  zone. Returns nullptr if none are free. */
	AReloadPoint* PickFreePoint() const;

	/** Claim/release a point in the shared registry so multiple zones never overlap. */
	void ClaimPoint(AReloadPoint* Point);
	void ReleasePoint(AReloadPoint* Point);

	/** Shared across all reload zones: which points are currently occupied. Static so
	 *  every zone sees every other zone's claim and avoids landing on the same point. */
	static TSet<TWeakObjectPtr<AReloadPoint>> ClaimedPoints;

	/** After a teleport, re-check whether the player is inside the trigger at the new
	 *  location and set their in-zone state + reload binding accordingly. */
	void SyncOverlappingPlayer();

	UPROPERTY()
	TArray<TObjectPtr<AReloadPoint>> ReloadPoints;

	UPROPERTY()
	TObjectPtr<AReloadPoint> CurrentPoint;

	UPROPERTY()
	TObjectPtr<APlayerCharacter> OverlappingPlayer;
};
