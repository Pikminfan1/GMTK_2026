// Fill out your copyright notice in the Description page of Project Settings.

#include "Pickups/HealthOrb.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Characters/PlayerCharacter.h"
#include "Characters/Components/HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Utility/LogChannels.h"

AHealthOrb::AHealthOrb()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	SetRootComponent(CollisionSphere);
	CollisionSphere->InitSphereRadius(20.f);
	// Overlap-only, no blocking. Scatter movement is non-swept and we land via an
	// explicit ground trace, so the sphere never needs to physically collide - which
	// also means orbs can't pool against geometry or each other.
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Overlap);

	OrbMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrbMesh"));
	OrbMesh->SetupAttachment(CollisionSphere);
	OrbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHealthOrb::BeginPlay()
{
	Super::BeginPlay();

	CachedPlayer = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));

	if (Lifespan > 0.f)
	{
		SetLifeSpan(Lifespan);
	}
}

void AHealthOrb::LaunchScatter(const FVector& Direction, float Speed)
{
	// Horizontal spread (Direction * Speed) and vertical pop (LaunchUpwardSpeed) are
	// independent, so distance-out and height can be tuned separately.
	FVector Horizontal = Direction.GetSafeNormal();
	Horizontal.Z = 0.f;
	Horizontal = Horizontal.GetSafeNormal() * Speed;

	ScatterVelocity = Horizontal + FVector(0.f, 0.f, LaunchUpwardSpeed);
	bHasLanded = false;
}

void AHealthOrb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TimeAlive += DeltaSeconds;

	if (!CachedPlayer)
	{
		CachedPlayer = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	}

	const bool bPlayerValid = CachedPlayer != nullptr;
	bool bPlayerDead = false;
	if (bPlayerValid)
	{
		if (const UHealthComponent* Health = CachedPlayer->GetHealthComponent())
		{
			bPlayerDead = Health->IsDead();
		}
	}

	// ---------- Scatter phase ----------
	if (!bIsAttracting)
	{
		if (!bHasLanded)
		{
			// Gravity governs fall speed; horizontal spread decays via damping.
			ScatterVelocity.Z -= ScatterGravity * DeltaSeconds;

			FVector HorizVel(ScatterVelocity.X, ScatterVelocity.Y, 0.f);
			HorizVel = FMath::VInterpTo(HorizVel, FVector::ZeroVector, DeltaSeconds, ScatterDamping);
			ScatterVelocity.X = HorizVel.X;
			ScatterVelocity.Y = HorizVel.Y;

			// NON-swept move so orbs fan out freely without colliding into pools.
			AddActorWorldOffset(ScatterVelocity * DeltaSeconds, false);

			// Land via a downward ground trace (only while falling and past the arm
			// time). This keeps orbs resting on the floor without a swept move.
			if (ScatterVelocity.Z <= 0.f && TimeAlive >= MinAirborneTime)
			{
				const FVector Start = GetActorLocation();
				const FVector End = Start - FVector(0.f, 0.f, GroundTraceDistance);

				FCollisionQueryParams Params;
				Params.AddIgnoredActor(this);

				FHitResult Ground;
				if (GetWorld()->LineTraceSingleByChannel(Ground, Start, End, ECC_WorldStatic, Params))
				{
					// If we've descended to (or below) the ground, snap and settle.
					if (Start.Z <= Ground.ImpactPoint.Z + GroundRestHeight)
					{
						SetActorLocation(Ground.ImpactPoint + FVector(0.f, 0.f, GroundRestHeight));
						bHasLanded = true;
						ScatterVelocity = FVector::ZeroVector;
					}
				}
			}
		}

		if (bPlayerValid && !bPlayerDead && TimeAlive >= ArmDelay)
		{
			const float DistSq = FVector::DistSquared(GetActorLocation(), CachedPlayer->GetActorLocation());
			if (DistSq <= FMath::Square(AttractRadius))
			{
				bIsAttracting = true;
				CurrentAttractSpeed = AttractInitialSpeed;
				AttractElapsed = 0.f;
				SetLifeSpan(0.f);
				OnAttractStarted.Broadcast();
			}
		}
		return;
	}

	// ---------- Attract phase ----------
	AttractElapsed += DeltaSeconds;
	if (AttractElapsed >= MaxAttractTime)
	{
		UE_LOG(LogGMTKCombat, Verbose, TEXT("Health orb self-destructed - attract timed out after %.1fs"), MaxAttractTime);
		Destroy();
		return;
	}

	if (!bPlayerValid)
	{
		return;
	}

	const FVector ToPlayer = CachedPlayer->GetActorLocation() - GetActorLocation();
	const float Distance = ToPlayer.Size();

	if (Distance <= CollectRadius)
	{
		Collect();
		return;
	}

	CurrentAttractSpeed += AttractAcceleration * DeltaSeconds;

	const FVector Step = ToPlayer.GetSafeNormal() * FMath::Min(CurrentAttractSpeed * DeltaSeconds, Distance);
	AddActorWorldOffset(Step, false);
}

void AHealthOrb::Collect()
{
	if (CachedPlayer)
	{
		if (UHealthComponent* Health = CachedPlayer->GetHealthComponent())
		{
			Health->Heal(HealAmount);
		}
	}

	OnCollected.Broadcast();

	UE_LOG(LogGMTKCombat, Verbose, TEXT("Health orb collected (+%.0f)"), HealAmount);

	Destroy();
}
