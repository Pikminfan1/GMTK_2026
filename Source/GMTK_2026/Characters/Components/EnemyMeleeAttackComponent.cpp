#include "Characters/Components/EnemyMeleeAttackComponent.h"

#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "Interfaces/Damageable.h"
#include "Utility/LogChannels.h"

UEnemyMeleeAttackComponent::UEnemyMeleeAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Idle enemies shouldn't pay for a tick. Enabled on demand in BeginAttack().
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UEnemyMeleeAttackComponent::BeginAttack(AActor* InTarget)
{
	if (State != EMeleeAttackState::Idle || !IsValid(InTarget))
	{
		return false;
	}

	Target = InTarget;
	SetComponentTickEnabled(true);
	EnterState(EMeleeAttackState::WindUp);
	return true;
}

void UEnemyMeleeAttackComponent::AbortAttack()
{
	if (State == EMeleeAttackState::Idle)
	{
		return;
	}

	StopMeleeMontages();
	EnterState(EMeleeAttackState::Cooldown);
}

void UEnemyMeleeAttackComponent::EnterState(EMeleeAttackState NewState)
{
	State = NewState;
	StateTime = 0.f;

	switch (State)
	{
	case EMeleeAttackState::WindUp:
	{
		PlayMeleeMontage(WindUpMontage);
		OnWindUpStarted.Broadcast();
		break;
	}

	case EMeleeAttackState::Swing:
	{
		bSwingResolved = false;
		PlayMeleeMontage(SwingMontage);
		OnSwingStarted.Broadcast();
		break;
	}

	case EMeleeAttackState::Cooldown:
	{
		Target = nullptr;
		OnMeleeFinished.Broadcast();
		break;
	}

	case EMeleeAttackState::Idle:
	{
		// Nothing left to do until the next BeginAttack, so stop ticking.
		SetComponentTickEnabled(false);
		break;
	}
	}
}

void UEnemyMeleeAttackComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	StateTime += DeltaTime;

	switch (State)
	{
	case EMeleeAttackState::WindUp:
	{
		if (!IsValid(Target))
		{
			AbortAttack();
			return;
		}

		// Track the target through the telegraph so the swing starts aimed at
		// wherever the player actually is, not where they were when it began.
		FaceTarget(DeltaTime);

		if (StateTime >= WindUpDuration)
		{
			EnterState(EMeleeAttackState::Swing);
		}
		break;
	}

	case EMeleeAttackState::Swing:
	{
		// The swing itself is committed: no more turning, so the player can dodge
		// around it. The impact check runs once at SwingHitDelay into the swing.
		if (!bSwingResolved && StateTime >= FMath::Min(SwingHitDelay, SwingDuration))
		{
			ResolveSwing();
		}

		if (StateTime >= SwingDuration)
		{
			EnterState(EMeleeAttackState::Cooldown);
		}
		break;
	}

	case EMeleeAttackState::Cooldown:
	{
		if (StateTime >= CooldownDuration)
		{
			EnterState(EMeleeAttackState::Idle);
		}
		break;
	}

	case EMeleeAttackState::Idle:
	default:
		break;
	}
}

void UEnemyMeleeAttackComponent::ResolveSwing()
{
	bSwingResolved = true;

	AActor* Owner = GetOwner();
	if (!Owner || !IsValid(Target))
	{
		OnSwingResolved.Broadcast(false);
		return;
	}

	// Range check first - the cheapest way for a dodge to succeed.
	const FVector ToTarget = Target->GetActorLocation() - Owner->GetActorLocation();
	if (ToTarget.Size() > HitRange)
	{
		OnSwingResolved.Broadcast(false);
		return;
	}

	// Facing check: the target must be inside the frontal cone at the impact
	// moment, so strafing through or behind the grunt during the swing whiffs it.
	FVector ToTargetFlat = ToTarget;
	ToTargetFlat.Z = 0.f;
	ToTargetFlat.Normalize();

	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(HitHalfAngleDegrees));
	if (FVector::DotProduct(Owner->GetActorForwardVector(), ToTargetFlat) < CosHalfAngle)
	{
		OnSwingResolved.Broadcast(false);
		return;
	}

	// Route through IDamageable, the project's single damage path, so the swing
	// works on anything damageable rather than only characters.
	if (Target->Implements<UDamageable>())
	{
		AController* InstigatorController = nullptr;
		if (const APawn* OwnerPawn = Cast<APawn>(Owner))
		{
			InstigatorController = OwnerPawn->GetController();
		}

		IDamageable::Execute_ApplyDamage(Target, Damage, InstigatorController, Owner);
		OnSwingResolved.Broadcast(true);
	}
	else
	{
		OnSwingResolved.Broadcast(false);
	}
}

void UEnemyMeleeAttackComponent::FaceTarget(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner || !IsValid(Target))
	{
		return;
	}

	// Yaw-only: the body turns toward the target without tipping up or down.
	const FVector ToTarget = Target->GetActorLocation() - Owner->GetActorLocation();
	FRotator CurrentRot = Owner->GetActorRotation();
	FRotator DesiredRot = ToTarget.Rotation();
	DesiredRot.Pitch = CurrentRot.Pitch;
	DesiredRot.Roll = CurrentRot.Roll;

	const FRotator NewRot = FMath::RInterpConstantTo(
		CurrentRot, DesiredRot, DeltaTime, RotationSpeedDegrees);

	Owner->SetActorRotation(NewRot);
}

void UEnemyMeleeAttackComponent::PlayMeleeMontage(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		return;
	}

	// PlayAnimMontage returns the montage's play length; zero or less means the
	// AnimInstance rejected it - usually a missing Slot node in the AnimBP or a
	// skeleton mismatch. Both fail silently otherwise, so log it loudly.
	const float PlayLength = OwnerCharacter->PlayAnimMontage(Montage);
	if (PlayLength <= 0.f)
	{
		UE_LOG(LogGMTKCombat, Warning,
			TEXT("%s failed to play montage %s on %s - check that the AnimBP has a "
				 "matching Slot node and the montage's skeleton matches the mesh."),
			*GetName(), *Montage->GetName(), *OwnerCharacter->GetName());
	}
}

void UEnemyMeleeAttackComponent::StopMeleeMontages()
{
	// Stop only the montages this component started; any unrelated montage on the
	// character keeps playing. Montage_Stop on a non-playing montage is a no-op.
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (WindUpMontage)
		{
			OwnerCharacter->StopAnimMontage(WindUpMontage);
		}
		if (SwingMontage)
		{
			OwnerCharacter->StopAnimMontage(SwingMontage);
		}
	}
}
