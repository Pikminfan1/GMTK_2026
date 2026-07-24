#include "Weapons/WeaponBase.h"
#include "Weapons/ProjectileBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Interfaces/Damageable.h"
#include "Utility/LogChannels.h"
#include "DrawDebugHelpers.h"
#include "Camera/CameraComponent.h"
#include "Characters/PlayerCharacter.h"

// Sets default values
AWeaponBase::AWeaponBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);

}

// Called when the game starts or when spawned
void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	CurrentAmmo = MaxAmmo;
	
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
	if (CurrentAmmo <= 0)
	{
		UE_LOG(LogGMTKCombat, Verbose, TEXT("%s tried to fire with no ammo"), *GetName());
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

	// TODO: muzzle flash VFX, fire sound, and recoil/animation montage hooks go here.
	// TODO: May also add the casting health drain attack here as well
}

void AWeaponBase::Reload()
{
	CurrentAmmo = MaxAmmo;
	// TODO: reload animation/sound hook.
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
