#include "Characters/PlayerCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Weapons/WeaponBase.h"
#include "Utility/LogChannels.h"

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
		}
	}
	else
	{
		UE_LOG(LogGMTKCombat, Warning, TEXT("%s has no DefaultWeaponClass assigned - nothing to auto-equip."), *GetName());
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
	


void APlayerCharacter::StartAim(const FInputActionValue& Value)
{
	bIsAiming = true;
	bUseControllerRotationYaw = true;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = false;
	}
}

void APlayerCharacter::StopAim(const FInputActionValue& Value)
{
	bIsAiming = false;
	bUseControllerRotationYaw = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
	}
}