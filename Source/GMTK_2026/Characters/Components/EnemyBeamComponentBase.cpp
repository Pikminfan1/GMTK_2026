#include "Characters/Components/EnemyBeamComponentBase.h"

#include "Animation/AnimMontage.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Utility/LogChannels.h"

UEnemyBeamComponentBase::UEnemyBeamComponentBase()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Idle enemies shouldn't pay for a tick. Enabled on demand in BeginAttack().
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UEnemyBeamComponentBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyBeamFX();
	DestroyWindUpFX();

	Super::EndPlay(EndPlayReason);
}

AController* UEnemyBeamComponentBase::GetOwnerController() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return OwnerPawn ? OwnerPawn->GetController() : nullptr;
}

bool UEnemyBeamComponentBase::BeginAttack(AActor* InTarget)
{
	if (State != EBeamAttackState::Idle || !IsValid(InTarget))
	{
		return false;
	}

	Target = InTarget;
	SetComponentTickEnabled(true);
	EnterState(EBeamAttackState::WindUp);
	return true;
}

void UEnemyBeamComponentBase::AbortAttack()
{
	if (State == EBeamAttackState::Idle)
	{
		return;
	}

	DestroyBeamFX();
	DestroyWindUpFX();
	StopBeamMontages();
	EnterState(EBeamAttackState::Cooldown);
}

void UEnemyBeamComponentBase::EnterState(EBeamAttackState NewState)
{
	State = NewState;
	StateTime = 0.f;

	switch (State)
	{
	case EBeamAttackState::WindUp:
	{
		TimeSinceLOS = 0.f;

		PlayBeamMontage(WindUpMontage);

		if (WindUpFX)
		{
			if (const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
			{
				if (OwnerCharacter->GetMesh())
				{
					ActiveWindUpFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
						WindUpFX, OwnerCharacter->GetMesh(), MuzzleSocketName,
						FVector::ZeroVector, FRotator::ZeroRotator,
						EAttachLocation::SnapToTarget, /*bAutoDestroy*/ true);
				}
			}
		}

		NotifyWindUpStarted();
		break;
	}

	case EBeamAttackState::Firing:
	{
		DestroyWindUpFX();

		PlayBeamMontage(FireLoopMontage);

		EffectAccumulator = 0.f;
		SpawnBeamFX();
		NotifyFiringStarted();
		break;
	}

	case EBeamAttackState::Cooldown:
	{
		Target = nullptr;
		NotifyFinished();
		NotifyCooldownStarted();
		break;
	}

	case EBeamAttackState::Idle:
	{
		// Nothing left to do until the next BeginAttack, so stop ticking.
		SetComponentTickEnabled(false);
		break;
	}
	}
}

void UEnemyBeamComponentBase::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	StateTime += DeltaTime;

	switch (State)
	{
	case EBeamAttackState::WindUp:
		TickWindUp(DeltaTime);
		break;

	case EBeamAttackState::Firing:
		TickFiring(DeltaTime);
		break;

	case EBeamAttackState::Cooldown:
		if (StateTime >= CooldownDuration)
		{
			EnterState(EBeamAttackState::Idle);
		}
		break;

	case EBeamAttackState::Idle:
	default:
		break;
	}
}

void UEnemyBeamComponentBase::TickWindUp(float DeltaTime)
{
	if (!IsValid(Target))
	{
		AbortAttack();
		return;
	}

	// Turn to face the target during the telegraph so the beam starts aimed at it.
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
		EnterState(EBeamAttackState::Firing);
	}
}

bool UEnemyBeamComponentBase::IsFacingTarget() const
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

void UEnemyBeamComponentBase::TickFiring(float DeltaTime)
{
	if (!IsValid(Target))
	{
		DestroyBeamFX();
		StopBeamMontages();
		EnterState(EBeamAttackState::Cooldown);
		return;
	}

	// Keep tracking the target through the beam so it follows them as they move.
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
		DrawDebugBeamLines(Start, BeamEnd, bHasLOS);
	}

	// The effect lands in discrete ticks rather than per-frame: framerate-independent
	// in practice, and it gives hit/heal reactions a distinct moment to fire on.
	if (bHasLOS)
	{
		EffectAccumulator += DeltaTime;

		const float TickInterval = GetEffectTickInterval();
		while (EffectAccumulator >= TickInterval)
		{
			EffectAccumulator -= TickInterval;
			ApplyBeamTick();
		}
	}

	if (IsLOSLost() || StateTime >= MaxFireDuration)
	{
		DestroyBeamFX();
		StopBeamMontages();
		EnterState(EBeamAttackState::Cooldown);
	}
}

FVector UEnemyBeamComponentBase::GetMuzzleLocation() const
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

FVector UEnemyBeamComponentBase::GetAimPoint() const
{
	if (!IsValid(Target))
	{
		return FVector::ZeroVector;
	}

	// Aim at the chest rather than the actor origin, which sits at the feet for
	// characters and would make every trace clip the floor.
	return Target->GetActorLocation() + FVector(0.f, 0.f, TargetAimHeight);
}

bool UEnemyBeamComponentBase::TraceToTarget(FHitResult& OutHit, FVector& OutStart, FVector& OutEnd) const
{
	OutStart = GetMuzzleLocation();
	OutEnd = GetAimPoint();

	if (!IsValid(Target))
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeamLOS), /*bTraceComplex*/ false, GetOwner());
	Params.AddIgnoredActor(Target);

	// Ignoring the target means any blocking hit is by definition something in the
	// way, so "no hit" cleanly equals "clear line of sight".
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
		OutHit, OutStart, OutEnd, LOSChannel, Params);

	return !bBlocked;
}

void UEnemyBeamComponentBase::SpawnBeamFX()
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

void UEnemyBeamComponentBase::DestroyBeamFX()
{
	if (ActiveBeam)
	{
		// Deactivate rather than destroy so in-flight particles can finish.
		ActiveBeam->Deactivate();
		ActiveBeam->SetAutoDestroy(true);
		ActiveBeam = nullptr;
	}
}

void UEnemyBeamComponentBase::DestroyWindUpFX()
{
	if (ActiveWindUpFX)
	{
		ActiveWindUpFX->DestroyComponent();
		ActiveWindUpFX = nullptr;
	}
}

void UEnemyBeamComponentBase::PlayBeamMontage(UAnimMontage* Montage)
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
	// AnimInstance rejected it. The usual culprits are an Anim Blueprint with no
	// Slot node matching the montage's slot, or a montage built for a different
	// skeleton than the mesh. Both fail silently otherwise, so log it loudly.
	const float PlayLength = OwnerCharacter->PlayAnimMontage(Montage);
	if (PlayLength <= 0.f)
	{
		UE_LOG(LogGMTKCombat, Warning,
			TEXT("%s failed to play montage %s on %s - check that the AnimBP has a "
				 "matching Slot node and the montage's skeleton matches the mesh."),
			*GetName(), *Montage->GetName(), *OwnerCharacter->GetName());
	}
}

void UEnemyBeamComponentBase::StopBeamMontages()
{
	// Stop only the montages this component started. Montage_Stop on a montage
	// that isn't currently playing is a no-op, so both calls are always safe, and
	// any unrelated montage on the character (a hit react, another component's
	// attack) keeps playing.
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (WindUpMontage)
		{
			OwnerCharacter->StopAnimMontage(WindUpMontage);
		}
		if (FireLoopMontage)
		{
			OwnerCharacter->StopAnimMontage(FireLoopMontage);
		}
	}
}

void UEnemyBeamComponentBase::FaceTarget(float DeltaTime)
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

	// Yaw-only: the body turns toward the target without tipping up or down.
	const FVector ToTarget = Target->GetActorLocation() - Owner->GetActorLocation();
	FRotator CurrentRot = Owner->GetActorRotation();
	FRotator DesiredRot = ToTarget.Rotation();
	DesiredRot.Pitch = CurrentRot.Pitch;
	DesiredRot.Roll = CurrentRot.Roll;

	// Interpolate at a fixed angular rate so the turn reads as deliberate rather
	// than snapping instantly to face the target.
	const FRotator NewRot = FMath::RInterpConstantTo(
		CurrentRot, DesiredRot, DeltaTime, RotationSpeedDegrees);

	Owner->SetActorRotation(NewRot);
}

void UEnemyBeamComponentBase::DrawDebugBeamLines(const FVector& Start, const FVector& End, bool bHasLOS) const
{
#if ENABLE_DRAW_DEBUG
	const FColor BeamColor = bHasLOS ? FColor::Red : FColor::Silver;

	DrawDebugLine(GetWorld(), Start, End, BeamColor, false, -1.f, 0, 4.f);
	DrawDebugPoint(GetWorld(), End, 12.f, BeamColor, false, -1.f);
#endif
}
