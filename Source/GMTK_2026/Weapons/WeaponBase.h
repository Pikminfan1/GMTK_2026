// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class USkeletalMeshComponent;
class AProjectileBase;
class UCameraComponent;

UENUM(BlueprintType)
enum class EWeaponFireMode : uint8
{
	Hitscan,
	Projectile
};

/** Why an incremental reload stopped. Drives which reload-end event fires. */
UENUM(BlueprintType)
enum class EReloadEndReason : uint8
{
	/** The magazine reached full. */
	Completed,
	/** The player released the reload button before the mag filled. */
	Interrupted,
	/** Firing cancelled the reload. */
	CancelledByFire
};

// Event signatures. All BlueprintAssignable so a teammate can hook SFX/VFX/anim in
// a weapon Blueprint without touching this class.

/** A shot fired. Carries the ammo count after firing. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeaponFired, int32, CurrentAmmo, int32, MaxAmmo);
/** Trigger pulled on an empty magazine. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponDryFire);
/** An incremental reload began. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponReloadStarted);
/** A single round was loaded. Carries the ammo count after loading. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeaponRoundLoaded, int32, CurrentAmmo, int32, MaxAmmo);
/** Reload ended because the magazine filled. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponReloadCompleted);
/** Reload ended because the player released the button before the mag was full.
 *  This is the one to hook an "interrupted reload" SFX to - it never fires on a
 *  fire-cancel. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponReloadInterrupted);
/** Reload ended because the player fired. Separate so you can react to it without
 *  triggering the interrupted-reload SFX. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponReloadCancelledByFire);

/**
 * Abstract base for anything the player (or later, an enemy) can wield and fire.
 * Covers both hitscan and projectile weapons through the same class rather than
 * splitting into two hierarchies - which behavior a given weapon uses is just a
 * property (FireMode), not a difference in class structure.
 *
 * Ammo is per-weapon-instance (CurrentAmmo lives here), so a dual-wielding player
 * with two weapon actors gets an independent magazine per hand for free.
 *
 * Reload is incremental and hold-driven: while a reload is active it loads one round
 * every RoundReloadInterval seconds (revolver / shotgun style). It ends for one of
 * three reasons (see EReloadEndReason), each firing its own event.
 */
UCLASS()
class GMTK_2026_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void Fire();

	/** Begin loading rounds one at a time on an interval. Call while the reload
	 *  button is held. No-op if already reloading or the mag is full. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void StartReload();

	/** Stop an in-progress incremental reload because the button was released. Fires
	 *  OnReloadInterrupted (unless the mag happened to just fill). */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void StopReload();

	/** Attaches this weapon's ownership to whoever is wielding it (player or enemy). */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void Equip(AActor* NewOwnerActor);

	// ---------- Queries ----------

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetMaxAmmo() const { return MaxAmmo; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReloading() const { return bIsReloading; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsMagazineFull() const { return CurrentAmmo >= MaxAmmo; }

	// ---------- Events ----------

	/** A shot fired. Carries ammo after the shot. */
	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnWeaponFired OnFired;

	/** Trigger pulled with an empty magazine. */
	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnWeaponDryFire OnDryFire;

	/** An incremental reload began. */
	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnWeaponReloadStarted OnReloadStarted;

	/** A single round was loaded. Carries ammo after loading - good for a per-shell
	 *  sound AND for driving the ammo HUD during reload. */
	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnWeaponRoundLoaded OnRoundLoaded;

	/** Reload finished because the mag filled. */
	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnWeaponReloadCompleted OnReloadCompleted;

	/** Reload ended early because the button was released. Hook interrupted-reload
	 *  SFX here - never fires on a fire-cancel. */
	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnWeaponReloadInterrupted OnReloadInterrupted;

	/** Reload was cancelled by firing. Reacting here is optional. */
	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FOnWeaponReloadCancelledByFire OnReloadCancelledByFire;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Cached once in Equip() rather than re-cast on every shot - null if the current owner has no camera to aim from.
	UPROPERTY()
	TObjectPtr<UCameraComponent> CachedAimCamera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	EWeaponFireMode FireMode = EWeaponFireMode::Hitscan;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	FName MuzzleSocketName = "Muzzle";

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	float Damage = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	float HitscanRange = 5000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	TSubclassOf<AProjectileBase> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	int32 MaxAmmo = 30;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	int32 CurrentAmmo = 0;

	/** Seconds between each round loaded during an incremental reload. Tune for feel -
	 *  e.g. ~0.5 for a revolver, faster for a mag-fed gun. Exposed for later balancing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Reload", meta = (ClampMin = "0.01"))
	float RoundReloadInterval = 0.5f;

	/** Optional one-time delay before the FIRST round loads, for a wind-up animation.
	 *  0 loads the first round immediately on button press. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Reload", meta = (ClampMin = "0.0"))
	float ReloadStartDelay = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Debug")
	bool bShowDebugTrace = true;

	void FireHitscan();
	void FireProjectile();

	/** Timer callback: loads exactly one round, then reschedules itself or ends. */
	void LoadOneRound();

	/** Central reload-teardown. Clears the timer, flips the flag, and fires the
	 *  event matching why the reload ended. */
	void EndReload(EReloadEndReason Reason);

private:
	UPROPERTY()
	bool bIsReloading = false;

	FTimerHandle ReloadTimerHandle;
};
