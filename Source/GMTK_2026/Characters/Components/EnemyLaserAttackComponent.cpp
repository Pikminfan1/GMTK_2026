#include "Characters/Components/EnemyLaserAttackComponent.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "Interfaces/Damageable.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Utility/LogChannels.h"

UEnemyLaserAttackComponent::UEnemyLaserAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Idle enemies shouldn't pay for a tick. Enabled on demand in BeginAttack().
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UEnemyLaserAttackComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UEnemyLaserAttackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyBeamFX();

	if (ActiveWindUpFX)
	{
		ActiveWindUpFX->DestroyComponent();
		ActiveWindUpFX = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

bool UEnemyLaserAttackComponent::BeginAttack(AActor* InTarget)
{
	if (State != ELaserState::Idle || !IsValid(InTarget))
	{
		return false;
	}

	Target = InTarget;
	SetComponentTickEnabled(true);
	EnterState(ELaserState::WindUp);
	return true;
}

void UEnemyLaserAttackComponent::AbortAttack()
{
	if (State == ELaserState::Idle)
	{
		return;
	}

	DestroyBeamFX();

	if (ActiveWindUpFX)
	{
		ActiveWindUpFX->DestroyComponent();
		ActiveWindUpFX = nullptr;
	}

	StopMontages();
	EnterState(ELaserState::Cooldown);
}

void UEnemyLaserAttackComponent::EnterState(ELaserState NewState)
{
	State = NewState;
	StateTime = 0.f;

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());

	switch (State)
	{
	case ELaserState::WindUp:
	{
		TimeSinceLOS = 0.f;

		if (WindUpMontage && OwnerCharacter)
		{
			OwnerCharacter->PlayAnimMontage(WindUpMontage);
		}

		if (WindUpFX && OwnerCharacter && OwnerCharacter->GetMesh())
		{
			ActiveWindUpFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
				WindUpFX, OwnerCharacter->GetMesh(), MuzzleSocketName,
				FVector::ZeroVector, FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget, /*bAutoDestroy*/ true);
		}
		break;
	}

	case ELaserState::Firing:
	{
		if (ActiveWindUpFX)
		{
			ActiveWindUpFX->DestroyComponent();
			ActiveWindUpFX = nullptr;
		}

		if (FireLoopMontage && OwnerCharacter)
		{
			OwnerCharacter->PlayAnimMontage(FireLoopMontage);
		}

		DamageAccumulator = 0.f;
		SpawnBeamFX();
		break;
	}

	case ELaserState::Cooldown:
	{
		Target = nullptr;
		OnLaserFinished.Broadcast();
		break;
	}

	case ELaserState::Idle:
	{
		// Nothing left to do until the next BeginAttack, so stop ticking.
		SetComponentTickEnabled(false);
		break;
	}
	}
}

void UEnemyLaserAttackComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	StateTime += DeltaTime;

	switch (State)
	{
	case ELaserState::WindUp:
		TickWindUp(DeltaTime);
		break;

	case ELaserState::Firing:
		TickFiring(DeltaTime);
		break;

	case ELaserState::Cooldown:
		if (StateTime >= CooldownDuration)
		{
			EnterState(ELaserState::Idle);
		}
		break;

	case ELaserState::Idle:
	default:
		break;
	}
}

void UEnemyLaserAttackComponent::TickWindUp(float DeltaTime)
{
	if (!IsValid(Target))
	{
		AbortAttack();
		return;
	}
	
	// Turn to face the player during the telegraph so the beam starts aimed at them.
	FaceTarget(DeltaTime);

	FHitResult Hit;
	FVector Start, End;
	const bool bHasLOS = TraceToTarget(Hit, Start, End);

	TimeSinceLOS = bHasLOS ? 0.f : TimeSinceLOS + DeltaTime;

	if (bDrawDebugBeam)
	{
		// Thin warning line during the telegraph so the tell is readable pre-VFX.
		DrawDebugLine(GetWorld(), Start, bHasLOS ? End : Hit.ImpactPoint,
			FColor::Orange, false, -1.f, 0, 1.5f);
	}

	// Breaking LOS during the wind-up cancels the attack outright. This is the
	// player's reward for reacting to the telegraph.
	if (IsLOSLost())
	{
		AbortAttack();
		return;
	}

	// Only advance to Firing once the wind-up time has elapsed AND we're actually
	// pointed at the target. Without the alignment gate, a wind-up that starts
	// facing away fires its first frame before the turn finishes, so the first
	// shot misses.
	const bool bTimeElapsed = StateTime >= WindUpDuration;
	const bool bOverdue = StateTime >= WindUpDuration * 2.f;

	if ((bTimeElapsed && IsFacingTarget()) || bOverdue)
	{
		EnterState(ELaserState::Firing);
	}
}

bool UEnemyLaserAttackComponent::IsFacingTarget() const
{
	if (!IsValid(Target) || !GetOwner())
	{
		return false;
	}

	const FVector Forward = GetOwner()->GetActorForwardVector();
	FVector ToTarget = Target->GetActorLocation() - GetOwner()->GetActorLocation();
	ToTarget.Z = 0.f;
	ToTarget.Normalize();

	// Dot of two unit vectors = cosine of the angle between them.
	const float Dot = FVector::DotProduct(Forward, ToTarget);
	const float CosTolerance = FMath::Cos(FMath::DegreesToRadians(FireAngleTolerance));

	return Dot >= CosTolerance;
}

void UEnemyLaserAttackComponent::TickFiring(float DeltaTime)
{
	if (!IsValid(Target))
	{
		DestroyBeamFX();
		StopMontages();
		EnterState(ELaserState::Cooldown);
		return;
	}
	
	// Keep tracking the player through the beam so it follows them as they move.
	if (bTrackTargetWhileFiring)
	{
		FaceTarget(DeltaTime);
	}

	FHitResult Hit;
	FVector Start, End;
	const bool bHasLOS = TraceToTarget(Hit, Start, End);

	TimeSinceLOS = bHasLOS ? 0.f : TimeSinceLOS + DeltaTime;

	// When LOS is blocked the beam should visibly stop at the obstruction rather
	// than passing through it.
	const FVector BeamEnd = bHasLOS
		? End
		: (Hit.bBlockingHit ? Hit.ImpactPoint : End);

	if (ActiveBeam)
	{
		ActiveBeam->SetVectorParameter(BeamEndParamName, BeamEnd);
	}

	if (bDrawDebugBeam)
	{
		DrawDebug(Start, BeamEnd, bHasLOS);
	}

	if (bHasLOS)
	{
		DamageAccumulator += DeltaTime;
		while (DamageAccumulator >= DamageTickInterval)
		{
			DamageAccumulator -= DamageTickInterval;
			ApplyDamageTick();
		}
	}

	if (IsLOSLost() || StateTime >= MaxFireDuration)
	{
		DestroyBeamFX();
		StopMontages();
		EnterState(ELaserState::Cooldown);
	}
}

FVector UEnemyLaserAttackComponent::GetMuzzleLocation() const
{
	if (const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (const USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh())
		{
			if (Mesh->DoesSocketExist(MuzzleSocketName))
			{
				return Mesh->GetSocketLocation(MuzzleSocketName);
			}
		}
	}

	return GetOwner()->GetActorLocation() + FVector(0.f, 0.f, FallbackMuzzleHeight);
}

FVector UEnemyLaserAttackComponent::GetAimPoint() const
{
	if (!IsValid(Target))
	{
		return FVector::ZeroVector;
	}

	// Aim at the chest rather than the actor origin, which sits at the feet for
	// characters and would make every trace clip the floor.
	return Target->GetActorLocation() + FVector(0.f, 0.f, TargetAimHeight);
}

bool UEnemyLaserAttackComponent::TraceToTarget(FHitResult& OutHit, FVector& OutStart, FVector& OutEnd) const
{
	OutStart = GetMuzzleLocation();
	OutEnd = GetAimPoint();

	if (!IsValid(Target))
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LaserLOS), /*bTraceComplex*/ false, GetOwner());
	Params.AddIgnoredActor(Target);

	// Ignoring the target means any blocking hit is by definition something in the
	// way, so "no hit" cleanly equals "clear line of sight".
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
		OutHit, OutStart, OutEnd, LOSChannel, Params);

	return !bBlocked;
}

void UEnemyLaserAttackComponent::SpawnBeamFX()
{
	if (!BeamFX)
	{
		return;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	USceneComponent* AttachTo = OwnerCharacter && OwnerCharacter->GetMesh()
		? Cast<USceneComponent>(OwnerCharacter->GetMesh())
		: GetOwner()->GetRootComponent();

	ActiveBeam = UNiagaraFunctionLibrary::SpawnSystemAttached(
		BeamFX, AttachTo, MuzzleSocketName,
		FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget, /*bAutoDestroy*/ false);
}

void UEnemyLaserAttackComponent::DestroyBeamFX()
{
	if (ActiveBeam)
	{
		// Deactivate rather than destroy so in-flight particles can finish.
		ActiveBeam->Deactivate();
		ActiveBeam->SetAutoDestroy(true);
		ActiveBeam = nullptr;
	}
}

void UEnemyLaserAttackComponent::StopMontages()
{
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		OwnerCharacter->StopAnimMontage();
	}
}

void UEnemyLaserAttackComponent::ApplyDamageTick()
{
	if (!IsValid(Target))
	{
		return;
	}

	const float DamageThisTick = DamagePerSecond * DamageTickInterval;

	AController* InstigatorController = nullptr;
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		InstigatorController = OwnerPawn->GetController();
	}

	// Route through IDamageable rather than the health component directly - that's
	// the project convention, and it means this works on anything damageable, not
	// just characters.
	if (Target->Implements<UDamageable>())
	{
		IDamageable::Execute_ApplyDamage(Target, DamageThisTick, InstigatorController, GetOwner());
	}
	else
	{
		UE_LOG(LogGMTKCombat, Warning,
			TEXT("Laser target %s does not implement IDamageable - no damage applied."),
			*Target->GetName());
	}
}

void UEnemyLaserAttackComponent::FaceTarget(float DeltaTime)
{
	if (!IsValid(Target))
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Yaw-only: we want the body to turn toward the player, not tip up or down.
	const FVector ToTarget = Target->GetActorLocation() - Owner->GetActorLocation();
	FRotator CurrentRot = Owner->GetActorRotation();
	FRotator DesiredRot = ToTarget.Rotation();
	DesiredRot.Pitch = CurrentRot.Pitch;
	DesiredRot.Roll = CurrentRot.Roll;

	// Interpolate at a fixed angular rate so the turn reads as deliberate rather
	// than snapping instantly to face the player.
	const FRotator NewRot = FMath::RInterpConstantTo(
		CurrentRot, DesiredRot, DeltaTime, RotationSpeedDegrees);

	Owner->SetActorRotation(NewRot);
}

void UEnemyLaserAttackComponent::DrawDebug(const FVector& Start, const FVector& End, bool bHasLOS) const
{
#if ENABLE_DRAW_DEBUG
	const FColor BeamColor = bHasLOS ? FColor::Red : FColor::Silver;

	DrawDebugLine(GetWorld(), Start, End, BeamColor, false, -1.f, 0, 4.f);
	DrawDebugPoint(GetWorld(), End, 12.f, BeamColor, false, -1.f);
#endif
}
