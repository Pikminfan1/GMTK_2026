#pragma once

#include "CoreMinimal.h"
#include "Characters/BaseCharacter.h"
#include "PlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class AWeaponBase;
class UComboComponent;
class UHealthComponent;
struct FInputActionValue;

// Combo-reward UI signals. Each fires when the corresponding buff changes (including
// back to base on combo reset), passing the new value so UI can display it.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDamageBonusChanged, float, DamageMultiplier);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpeedBonusChanged, float, SpeedMultiplier);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAmmoBonusChanged, int32, BonusMaxAmmo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDoubleFireChanged, bool, bDoubleFireActive);

// Player-level reload signals, fired ONCE per reload action (not once per weapon).
// Bind shared reload art/SFX here; use the per-weapon AWeaponBase reload events only
// for per-gun art, since those fire once for each hand.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerReloadStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerReloadStopped);

// Player aim signals, fired when the player starts/stops aiming.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerAimStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerAimStopped);

/**
 * The player-controlled leaf class - camera boom, Enhanced Input bindings, and
 * equipped-weapon handling. Any custom movement component swap, if you ever need
 * one (dash/wall-run/etc.), belongs here, NOT in ABaseCharacter - that keeps
 * enemies unaffected since they derive from ABaseCharacter separately.
 */
UCLASS()
class GMTK_2026_API APlayerCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	APlayerCharacter();
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsAiming() const { return bIsAiming; }
	
	UFUNCTION(BlueprintPure, Category = "Camera")
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	// ---------- Combo reward buffs (driven by the combo component's stacks) ----------

	UPROPERTY(BlueprintAssignable, Category = "Combo|Rewards")
	FOnDamageBonusChanged OnDamageBonusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combo|Rewards")
	FOnSpeedBonusChanged OnSpeedBonusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combo|Rewards")
	FOnAmmoBonusChanged OnAmmoBonusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combo|Rewards")
	FOnDoubleFireChanged OnDoubleFireChanged;

	UFUNCTION(BlueprintPure, Category = "Combo|Rewards")
	float GetDamageMultiplier() const { return CurrentDamageMultiplier; }

	UFUNCTION(BlueprintPure, Category = "Combo|Rewards")
	float GetSpeedMultiplier() const { return CurrentSpeedMultiplier; }

	UFUNCTION(BlueprintPure, Category = "Combo|Rewards")
	int32 GetBonusMaxAmmo() const { return CurrentBonusMaxAmmo; }

	/** Fires ONCE when the player begins a reload action (input pressed), regardless
	 *  of how many weapons actually reload. */
	UPROPERTY(BlueprintAssignable, Category = "Reload")
	FOnPlayerReloadStarted OnReloadStarted;

	/** Fires ONCE when the player ends the reload action (input released). */
	UPROPERTY(BlueprintAssignable, Category = "Reload")
	FOnPlayerReloadStopped OnReloadStopped;

	/** Fires when the player begins aiming (aim input pressed). */
	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnPlayerAimStarted OnAimStarted;

	/** Fires when the player stops aiming (aim input released). */
	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnPlayerAimStopped OnAimStopped;

	// ---------- Reload zone ----------

	/** Called by a reload zone when the player enters/leaves it. */
	UFUNCTION(BlueprintCallable, Category = "Reload")
	void SetInReloadZone(bool bInZone) { bIsInReloadZone = bInZone; }

	UFUNCTION(BlueprintPure, Category = "Reload")
	bool IsInReloadZone() const { return bIsInReloadZone; }

	/** If true, reloading is only allowed while inside a reload zone. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Reload")
	bool bReloadRequiresZone = true;

	/** True if the most recent reload action actually loaded a round (vs a no-op tap on
	 *  full mags). Reset when a new reload begins. The reload zone checks this. */
	UFUNCTION(BlueprintPure, Category = "Reload")
	bool DidLastReloadLoadRounds() const { return bReloadLoadedRounds; }

	// GetComboComponent must be PUBLIC: the GameMode calls it for combo-scaled scoring.
	UFUNCTION(BlueprintPure, Category = "Combat")
	UComboComponent* GetComboComponent() const { return ComboComponent; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UComboComponent> ComboComponent;
	
	// ---------- Combo reward tuning ----------
	// Rewards are granted ONE PER STACK in a cycling sequence:
	//   Damage -> Speed -> Ammo -> DoubleFire(once) -> Damage -> Speed -> Ammo -> ...
	// Damage/Speed/Ammo accumulate each time their slot comes up. DoubleFire triggers
	// only the first time its slot is reached; later DoubleFire slots are skipped and
	// grant Damage instead so a stack is never wasted. Everything resets on combo loss.

	/** Damage multiplier added each time the Damage slot is granted. 0.5 = +50%. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combo|Rewards")
	float DamageBonusPerTier = 0.5f;

	/** Move-speed multiplier added each time the Speed slot is granted. 0.15 = +15%. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combo|Rewards")
	float SpeedBonusPerTier = 0.15f;

	/** Max ammo (per weapon) added each time the Ammo slot is granted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combo|Rewards")
	int32 AmmoBonusPerTier = 2;

	/** Recomputes and applies all reward buffs for the given stack count by walking the
	 *  reward sequence from scratch (deterministic, reset-safe). Bound to the combo
	 *  component's OnComboStacksChanged. */
	UFUNCTION()
	void HandleComboStacksChanged(int32 NewStacks);

	/** Bound to each weapon's OnRoundLoaded so we know a real reload occurred. */
	UFUNCTION()
	void HandleWeaponRoundLoaded(int32 CurrentAmmo, int32 MaxAmmo);

	// Assign these Input Actions in the Class Defaults / a Blueprint subclass once you've
	// created them via Project Settings > Input (Enhanced Input).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AimAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ReloadAction;

	// Weapon spawned and attached automatically in BeginPlay.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	TSubclassOf<AWeaponBase> DefaultWeaponClass;
	
	// Used to determine if the player should be dual wielding or not
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	bool bIsDualWielding = true;
	
	//Determines if the player should be using the alternate fire mode or not
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	bool bIsAltFire = true;
	
	// Determines if the player should be firing from the right or left hand
	bool bFireRight = true;
	
	// Socket on THIS character's skeletal mesh (a hand socket, typically) that the weapon attaches to.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	FName WeaponAttachSocketNameR = "WeaponSocket_R";
	
	// Socket on THIS character's skeletal mesh (a hand socket, typically) that the weapon attaches to.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	FName WeaponAttachSocketNameL = "WeaponSocket_L";

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TObjectPtr<AWeaponBase> EquippedWeaponL;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TObjectPtr<AWeaponBase> EquippedWeaponR;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsAiming = false;

	// True while the player is standing in an active reload zone.
	bool bIsInReloadZone = false;

	// Set true when a round loads during the current reload; reset when reload starts.
	bool bReloadLoadedRounds = false;

	// Current applied reward buffs (base = 1.0 / 1.0 / 0), so UI and resets are exact.
	float CurrentDamageMultiplier = 1.f;
	float CurrentSpeedMultiplier = 1.f;
	int32 CurrentBonusMaxAmmo = 0;

	// Base walk speed + base alt-fire state captured at BeginPlay so buffs scale from
	// them and a reset restores the originals exactly.
	float BaseWalkSpeed = 600.f;
	bool bBaseIsAltFire = true;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartFire(const FInputActionValue& Value);
	void StartAim(const FInputActionValue& Value);
	void StopAim(const FInputActionValue& Value);
	void StartReload(const FInputActionValue& Value);
	void StopReload(const FInputActionValue& Value);
	
	UFUNCTION()
	void HandleDeathResetCombo(AActor* DeadActor);

	/** Override the base death handling to drop the player into a ragdoll so they
	 *  crumple, instead of freezing in place. Calls the base first (disables movement)
	 *  then enables physics on the mesh. */
	virtual void HandleDeath_Implementation(AActor* DeadActor) override;
	
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsAmmoMaxed();

	/** Collision profile the mesh switches to for ragdoll (must collide with world
	 *  static so the body rests on the floor). "Ragdoll" is the UE default profile. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Death")
	FName RagdollCollisionProfile = TEXT("Ragdoll");

	/** If true, hide the equipped weapons on death so they don't fling with the hands. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Death")
	bool bHideWeaponsOnDeath = true;
};
