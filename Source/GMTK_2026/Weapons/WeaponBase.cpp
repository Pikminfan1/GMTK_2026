// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapons/WeaponBase.h"
#include "Weapons/ProjectileBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Interfaces/Damageable.h"
#include "Utility/LogChannels.h"
#include "DrawDebugHelpers.h"
#include "Camera/CameraComponent.h"
#include "Characters/PlayerCharacter.h"
#include "TimerManager.h"

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	CurrentAmmo = MaxAmmo;
}

void AWeaponBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Don't leave a reload timer pointing at a destroyed weapon.
	GetWorldTimerManager().ClearTimer(ReloadTimerHandle);
	Super::EndPlay(EndPlayReason);
}

//TODO: May need to add attach logic here
void AWeaponBase::Equip(AActor* NewOwnerActor)
{
	SetOwner(NewOwnerActor);

	if (APlayerCharacter* PlayerOwner = Cast<APlayerCharacter>(NewOwnerActor))
	{
		CachedAimCamera = PlayerOwner->GetFollowCamera();
	}
	else
	{
		CachedAimCamera = nullptr;
	}
}

void AWeaponBase::Fire()
{
	// Firing cancels a reload - and it's a *fire* cancel, distinct from the player
	// releasing the reload button, so it fires OnReloadCancelledByFire (no interrupt SFX).
	if (bIsReloading)
	{
		EndReload(EReloadEndReason::CancelledByFire);
	}

	if (CurrentAmmo <= 0)
	{
		UE_LOG(LogGMTKCombat, Verbose, TEXT("%s dry-fired (no ammo)"), *GetName());
		OnDryFire.Broadcast();
		return;
	}

	CurrentAmmo--;

	switch (FireMode)
	{
	case EWeaponFireMode::Hitscan:
		FireHitscan();
		break;
	case EWeaponFireMode::Projectile:
		FireProjectile();
		break;
	}

	OnFired.Broadcast(CurrentAmmo, MaxAmmo);
}

void AWeaponBase::StartReload()
{
	if (bIsReloading || IsMagazineFull())
	{
		return;
	}

	bIsReloading = true;
	OnReloadStarted.Broadcast();

	UE_LOG(LogGMTKCombat, Verbose, TEXT("%s started incremental reload (%.2fs/round)"),
		*GetName(), RoundReloadInterval);

	if (ReloadStartDelay <= 0.f)
	{
		LoadOneRound();
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			ReloadTimerHandle, this, &AWeaponBase::LoadOneRound, ReloadStartDelay, false);
	}
}

void AWeaponBase::StopReload()
{
	// Called on button release. If we're not reloading there's nothing to interrupt.
	if (!bIsReloading)
	{
		return;
	}

	EndReload(EReloadEndReason::Interrupted);
}

void AWeaponBase::LoadOneRound()
{
	CurrentAmmo = FMath::Min(CurrentAmmo + 1, MaxAmmo);

	OnRoundLoaded.Broadcast(CurrentAmmo, MaxAmmo);

	UE_LOG(LogGMTKCombat, Verbose, TEXT("%s loaded a round (%d/%d)"),
		*GetName(), CurrentAmmo, MaxAmmo);

	// Full? End as Completed.
	if (IsMagazineFull())
	{
		EndReload(EReloadEndReason::Completed);
		return;
	}

	// Otherwise schedule the next round. If the player releases the button,
	// StopReload clears this timer before it fires.
	GetWorldTimerManager().SetTimer(
		ReloadTimerHandle, this, &AWeaponBase::LoadOneRound, RoundReloadInterval, false);
}

void AWeaponBase::EndReload(EReloadEndReason Reason)
{
	GetWorldTimerManager().ClearTimer(ReloadTimerHandle);
	bIsReloading = false;

	switch (Reason)
	{
	case EReloadEndReason::Completed:
		UE_LOG(LogGMTKCombat, Verbose, TEXT("%s reload completed (%d/%d)"), *GetName(), CurrentAmmo, MaxAmmo);
		OnReloadCompleted.Broadcast();
		break;

	case EReloadEndReason::Interrupted:
		UE_LOG(LogGMTKCombat, Verbose, TEXT("%s reload interrupted by release (%d/%d)"), *GetName(), CurrentAmmo, MaxAmmo);
		OnReloadInterrupted.Broadcast();
		break;

	case EReloadEndReason::CancelledByFire:
		UE_LOG(LogGMTKCombat, Verbose, TEXT("%s reload cancelled by fire (%d/%d)"), *GetName(), CurrentAmmo, MaxAmmo);
		OnReloadCancelledByFire.Broadcast();
		break;
	}
}

void AWeaponBase::FireHitscan()
{
	FVector TraceStart;
	FVector TraceDirection;

	if (CachedAimCamera)
	{
		TraceStart = CachedAimCamera->GetComponentLocation();
		TraceDirection = CachedAimCamera->GetForwardVector();
	}
	else if (WeaponMesh && WeaponMesh->DoesSocketExist(MuzzleSocketName))
	{
		const FTransform MuzzleTransform = WeaponMesh->GetSocketTransform(MuzzleSocketName);
		TraceStart = MuzzleTransform.GetLocation();
		TraceDirection = MuzzleTransform.GetRotation().GetForwardVector();
	}
	else
	{
		UE_LOG(LogGMTKCombat, Warning, TEXT("%s has no aim camera and no muzzle socket named %s - cannot fire"), *GetName(), *MuzzleSocketName.ToString());
		return;
	}

	const FVector TraceEnd = TraceStart + TraceDirection * HitscanRange;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (AActor* CurrentOwner = GetOwner())
	{
		Params.AddIgnoredActor(CurrentOwner);
	}

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Pawn, Params);

#if ENABLE_DRAW_DEBUG
	if (bShowDebugTrace)
	{
		const FVector DebugEnd = bHit ? Hit.ImpactPoint : TraceEnd;
		DrawDebugLine(GetWorld(), TraceStart, DebugEnd, bHit ? FColor::Green : FColor::Red, false, 2.f, 0, 1.5f);
		if (bHit)
		{
			DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 8.f, 12, FColor::Yellow, false, 2.f);
		}
	}
#endif

	if (bHit && Hit.GetActor() && Hit.GetActor()->Implements<UDamageable>())
	{
		AController* InstigatorController = nullptr;
		if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			InstigatorController = OwnerPawn->GetController();
		}
		IDamageable::Execute_ApplyDamage(Hit.GetActor(), Damage, InstigatorController, this);
	}
}

void AWeaponBase::FireProjectile()
{
	if (!ProjectileClass || !WeaponMesh || !WeaponMesh->DoesSocketExist(MuzzleSocketName))
	{
		return;
	}

	const FTransform MuzzleTransform = WeaponMesh->GetSocketTransform(MuzzleSocketName);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetOwner() ? GetOwner()->GetInstigator() : nullptr;

	GetWorld()->SpawnActor<AProjectileBase>(ProjectileClass, MuzzleTransform, SpawnParams);
}
