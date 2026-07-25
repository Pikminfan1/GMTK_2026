#pragma once

#include "CoreMinimal.h"
#include "Characters/BaseCharacter.h"
#include "PlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class AWeaponBase;
struct FInputActionValue;

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

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

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

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartFire(const FInputActionValue& Value);
	void StartAim(const FInputActionValue& Value);
	void StopAim(const FInputActionValue& Value);
	void StartReload(const FInputActionValue& Value);
	void StopReload(const FInputActionValue& Value);
};