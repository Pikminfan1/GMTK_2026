#include "Characters/PlayerCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Characters/Components/ComboComponent.h"
#include "Characters/Components/HealthComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Weapons/WeaponBase.h"
#include "Utility/LogChannels.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

APlayerCharacter::APlayerCharacter()
{
	bUseControllerRotationYaw = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	
	ComboComponent = CreateDefaultSubobject<UComboComponent>(TEXT("ComboComponent"));

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
	}
}
//TODO: Account for single weapons, (shotguns)
void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UHealthComponent* Health = GetHealthComponent())
	{
		Health->OnDeath.AddDynamic(this, &APlayerCharacter::HandleDeathResetCombo);
	}
	if (DefaultWeaponClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		EquippedWeaponL = GetWorld()->SpawnActor<AWeaponBase>(DefaultWeaponClass, GetActorTransform(), SpawnParams);
		EquippedWeaponR = GetWorld()->SpawnActor<AWeaponBase>(DefaultWeaponClass, GetActorTransform(), SpawnParams);
		if (EquippedWeaponL && EquippedWeaponR)
		{
			EquippedWeaponL->Equip(this);
			EquippedWeaponL->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponAttachSocketNameL);

			EquippedWeaponR->Equip(this);
			EquippedWeaponR->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponAttachSocketNameR);

			EquippedWeaponL->OnRoundLoaded.AddDynamic(this, &APlayerCharacter::HandleWeaponRoundLoaded);
			EquippedWeaponR->OnRoundLoaded.AddDynamic(this, &APlayerCharacter::HandleWeaponRoundLoaded);
		}
	}
	else
	{
		UE_LOG(LogGMTKCombat, Warning, TEXT("%s has no DefaultWeaponClass assigned - nothing to auto-equip."), *GetName());
	}

	// Capture base values so combo buffs scale from them and reset restores them.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		BaseWalkSpeed = Movement->MaxWalkSpeed;
	}
	bBaseIsAltFire = bIsAltFire;

	// Drive the cycling reward chain off the combo component's stack changes.
	if (ComboComponent)
	{
		ComboComponent->OnComboStacksChanged.AddDynamic(this, &APlayerCharacter::HandleComboStacksChanged);
	}
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);
		}
		if (LookAction)
		{
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Look);
		}
		if (JumpAction)
		{
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}
		if (FireAction)
		{
			EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &APlayerCharacter::StartFire);
		}
		if (AimAction)
		{
			EnhancedInput->BindAction(AimAction, ETriggerEvent::Started, this, &APlayerCharacter::StartAim);
			EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &APlayerCharacter::StopAim);
		}
		if (ReloadAction)
		{
			EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &APlayerCharacter::StartReload);
			EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Completed, this, &APlayerCharacter::StopReload);
		}
	}
	else
	{
		UE_LOG(LogGMTKCombat, Warning, TEXT("APlayerCharacter expected an EnhancedInputComponent - check Project Settings > Input > Default Classes."));
	}
}

void APlayerCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller)
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), MovementVector.Y);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), MovementVector.X);
	}
}

void APlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void APlayerCharacter::StartFire(const FInputActionValue& Value)
{
	if (bIsDualWielding)
	{
		if (bIsAltFire)
		{
			// Fire the hand indicated by bFireRight; fall through to the other hand if it's empty
			AWeaponBase* PrimaryHand   = bFireRight ? EquippedWeaponR : EquippedWeaponL;
			AWeaponBase* SecondaryHand = bFireRight ? EquippedWeaponL : EquippedWeaponR;

			if (PrimaryHand)
			{
				PrimaryHand->Fire();
				bFireRight = !bFireRight;
			}
			else if (SecondaryHand)
			{
				SecondaryHand->Fire();
				// Leave bFireRight as-is so the next press returns to the intended hand
			}
			else
			{
				UE_LOG(LogGMTKCombat, Verbose, TEXT("Alt fire input received but no weapon is equipped"));
			}
		}
		else
		{
			// Simultaneous fire: fire whichever hands are equipped
			bool bFired = false;

			if (EquippedWeaponR)
			{
				EquippedWeaponR->Fire();
				bFired = true;
			}
			if (EquippedWeaponL)
			{
				EquippedWeaponL->Fire();
				bFired = true;
			}

			if (!bFired)
			{
				UE_LOG(LogGMTKCombat, Verbose, TEXT("Dual fire input received but no weapon is equipped"));
			}
		}
	}
	else
	{
		if (EquippedWeaponR)
		{
			EquippedWeaponR->Fire();
		}
		else
		{
			UE_LOG(LogGMTKCombat, Verbose, TEXT("Fire input received but no weapon is equipped"));
		}
	}
}

void APlayerCharacter::StartReload(const FInputActionValue& Value)
{
	// Reload-zone gate: when enabled, the player can only reload inside an active zone.
	if (bReloadRequiresZone && !bIsInReloadZone)
	{
		UE_LOG(LogGMTKCombat, Verbose, TEXT("Reload blocked - not in a reload zone."));
		return;
	}

	// New reload action: clear the "loaded a round" flag so DidLastReloadLoadRounds()
	// reflects only this reload.
	bReloadLoadedRounds = false;

	// Both hands begin loading independently. Each weapon guards against reloading
	// when full or already loading, and runs its own per-round timer, so the two
	// mags fill on their own schedules.
	if (EquippedWeaponR)
	{
		EquippedWeaponR->StartReload();
	}
	if (EquippedWeaponL)
	{
		EquippedWeaponL->StartReload();
	}

	// Single player-level signal for shared reload art/SFX, fired once per action
	// rather than once per weapon (the per-weapon events still fire per hand).
	OnReloadStarted.Broadcast();
}

void APlayerCharacter::StopReload(const FInputActionValue& Value)
{
	if (EquippedWeaponR)
	{
		EquippedWeaponR->StopReload();
	}
	if (EquippedWeaponL)
	{
		EquippedWeaponL->StopReload();
	}

	OnReloadStopped.Broadcast();
}
	


void APlayerCharacter::StartAim(const FInputActionValue& Value)
{
	bIsAiming = true;
	bUseControllerRotationYaw = true;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = false;
	}

	OnAimStarted.Broadcast();
}

void APlayerCharacter::StopAim(const FInputActionValue& Value)
{
	bIsAiming = false;
	bUseControllerRotationYaw = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
	}

	OnAimStopped.Broadcast();
}

void APlayerCharacter::HandleDeathResetCombo(AActor* DeadActor)
{
	if (ComboComponent)
	{
		ComboComponent->ResetCombo();
	}
}

void APlayerCharacter::HandleComboStacksChanged(int32 NewStacks)
{
	// Walk the reward sequence from scratch for NewStacks stacks. Recomputing from
	// zero each time (rather than applying a delta) is deterministic and reset-safe:
	// NewStacks == 0 naturally restores every buff to base.
	//
	// Sequence per stack position (1-based):
	//   1 -> Damage, 2 -> Speed, 3 -> Ammo, 4 -> DoubleFire (once),
	//   then cycle Damage/Speed/Ammo, skipping DoubleFire (already on) and granting
	//   Damage in its place so a stack is never wasted.

	int32 DamageTiers = 0;
	int32 SpeedTiers = 0;
	int32 AmmoTiers = 0;
	bool bDoubleFire = false;

	for (int32 Position = 1; Position <= NewStacks; ++Position)
	{
		// Map position to a slot in the 4-length cycle: 1=Dmg,2=Spd,3=Ammo,0=DoubleFire.
		const int32 Slot = Position % 4;
		switch (Slot)
		{
		case 1: DamageTiers++; break;
		case 2: SpeedTiers++;  break;
		case 3: AmmoTiers++;   break;
		case 0: // DoubleFire slot
			if (!bDoubleFire)
			{
				bDoubleFire = true;   // first time: turn it on
			}
			else
			{
				DamageTiers++;        // already on: don't waste the stack, grant Damage
			}
			break;
		}
	}

	// Compute the resulting buff values.
	CurrentDamageMultiplier = 1.f + (DamageBonusPerTier * DamageTiers);
	CurrentSpeedMultiplier  = 1.f + (SpeedBonusPerTier  * SpeedTiers);
	CurrentBonusMaxAmmo     = AmmoBonusPerTier * AmmoTiers;

	// Apply to both weapons.
	if (EquippedWeaponR)
	{
		EquippedWeaponR->SetDamageMultiplier(CurrentDamageMultiplier);
		EquippedWeaponR->SetBonusMaxAmmo(CurrentBonusMaxAmmo);
	}
	if (EquippedWeaponL)
	{
		EquippedWeaponL->SetDamageMultiplier(CurrentDamageMultiplier);
		EquippedWeaponL->SetBonusMaxAmmo(CurrentBonusMaxAmmo);
	}

	// Apply speed to the player.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = BaseWalkSpeed * CurrentSpeedMultiplier;
	}

	// Double-fire: bIsAltFire == false makes both hands fire per pull (double output).
	// When the reward is active we force it off; otherwise restore the base setting.
	bIsAltFire = bDoubleFire ? false : bBaseIsAltFire;

	// Notify UI.
	OnDamageBonusChanged.Broadcast(CurrentDamageMultiplier);
	OnSpeedBonusChanged.Broadcast(CurrentSpeedMultiplier);
	OnAmmoBonusChanged.Broadcast(CurrentBonusMaxAmmo);
	OnDoubleFireChanged.Broadcast(bDoubleFire);
}

void APlayerCharacter::HandleWeaponRoundLoaded(int32 CurrentAmmo, int32 MaxAmmo)
{
	// Only count as a real reload if a weapon is actually mid-reload (filters out the
	// combo ammo reward's OnRoundLoaded broadcasts, which aren't reloads).
	const bool bAnyReloading =
		(EquippedWeaponR && EquippedWeaponR->IsReloading()) ||
		(EquippedWeaponL && EquippedWeaponL->IsReloading());

	if (bAnyReloading)
	{
		bReloadLoadedRounds = true;
	}
}

void APlayerCharacter::HandleDeath_Implementation(AActor* DeadActor)
{
	// Base handling first: stops movement. Note the base also disables ACTOR collision,
	// which is fine - we re-enable physics collision on the mesh specifically below so
	// the ragdoll still rests on the floor.
	Super::HandleDeath_Implementation(DeadActor);

	// Optionally hide the weapons so they don't fling around when the hands go limp.
	if (bHideWeaponsOnDeath)
	{
		if (EquippedWeaponR) { EquippedWeaponR->SetActorHiddenInGame(true); }
		if (EquippedWeaponL) { EquippedWeaponL->SetActorHiddenInGame(true); }
	}

	// The capsule must stop blocking so it can't hold the ragdoll up in a T-pose.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Drop the skeletal mesh into physics simulation (ragdoll). This overrides the Anim
	// BP pose - physics drives the bones directly - so the player crumples under gravity.
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionProfileName(RagdollCollisionProfile);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComp->SetAllBodiesSimulatePhysics(true);
		MeshComp->SetSimulatePhysics(true);
		MeshComp->WakeAllRigidBodies();
	}
}


bool APlayerCharacter::IsAmmoMaxed()
{
	if (EquippedWeaponL && EquippedWeaponR)
	{
		return EquippedWeaponL->IsMagazineFull() && EquippedWeaponR->IsMagazineFull();
	}
	else if (EquippedWeaponL)
	{
		return EquippedWeaponL->IsMagazineFull();
	}
	else if (EquippedWeaponR)
	{
		return EquippedWeaponR->IsMagazineFull();
	}

	return true; // no weapons equipped, so ammo is "maxed"
}
