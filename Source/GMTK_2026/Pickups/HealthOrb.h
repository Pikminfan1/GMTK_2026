// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HealthOrb.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class APlayerCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOrbCollected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOrbAttractStarted);

/**
 * A scattered health pickup that magnetizes to the player. Lifecycle:
 *   1. Spawns and is flung outward + up, then arcs and falls. Movement is NON-swept
 *      so orbs fan out freely without colliding into pools; a downward ground trace
 *      lands them on the floor.
 *   2. Sits until the player comes within AttractRadius.
 *   3. Once attracting, homes in on the player - accelerating - and is GUARANTEED to
 *      be collected once latched, unless the player dies.
 *   4. On reaching the player, heals them and destroys itself.
 *
 * Scatter is decoupled: the incoming direction drives horizontal spread (how far
 * OUT), LaunchUpwardSpeed drives the vertical pop (how HIGH), and ScatterGravity
 * drives the fall speed - all tunable independently.
 *
 * Two self-destruct safety timers:
 *   - Lifespan: an orb that is never collected despawns after this (idle cleanup).
 *   - MaxAttractTime: an orb that latched but can't reach the player gives up.
 */
UCLASS(Blueprintable)
class GMTK_2026_API AHealthOrb : public AActor
{
	GENERATED_BODY()

public:
	AHealthOrb();

	virtual void Tick(float DeltaSeconds) override;

	/** Give the orb an initial outward toss. Direction is used for HORIZONTAL spread
	 *  only; vertical pop comes from LaunchUpwardSpeed. */
	UFUNCTION(BlueprintCallable, Category = "Orb")
	void LaunchScatter(const FVector& Direction, float Speed);

	UPROPERTY(BlueprintAssignable, Category = "Orb")
	FOnOrbCollected OnCollected;

	UPROPERTY(BlueprintAssignable, Category = "Orb")
	FOnOrbAttractStarted OnAttractStarted;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orb", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orb", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> OrbMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Orb")
	float HealAmount = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Orb")
	float AttractRadius = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Orb")
	float CollectRadius = 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Orb")
	float AttractInitialSpeed = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Orb")
	float AttractAcceleration = 1200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Orb")
	float ArmDelay = 0.35f;

	/** Horizontal spread decay (higher = orbs slow their outward travel quicker, so
	 *  they don't fly as far). Keep low (~0.5-1) for a wide spread. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Orb")
	float ScatterDamping = 1.f;

	/** Downward acceleration during scatter (uu/s^2). Governs FALL SPEED. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Orb")
	float ScatterGravity = 980.f;

	/** Upward launch speed (uu/s) added on top of horizontal scatter. Controls pop
	 *  HEIGHT independently of spread and fall speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Orb")
	float LaunchUpwardSpeed = 450.f;

	/** Minimum airborne time before the orb may land, so it can't settle on frame one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Orb")
	float MinAirborneTime = 0.15f;

	/** How far down to trace for the ground each tick while falling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Orb")
	float GroundTraceDistance = 300.f;

	/** How high above the ground impact the orb rests. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Orb")
	float GroundRestHeight = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Orb")
	float Lifespan = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Orb")
	float MaxAttractTime = 3.f;

private:
	void Collect();

	UPROPERTY()
	TObjectPtr<APlayerCharacter> CachedPlayer;

	FVector ScatterVelocity = FVector::ZeroVector;
	bool bHasLanded = false;

	bool bIsAttracting = false;
	float CurrentAttractSpeed = 0.f;
	float AttractElapsed = 0.f;

	float TimeAlive = 0.f;
};
