#include "Characters/Components/EnemyHealBeamComponent.h"
#include "Characters/Components/HealthComponent.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Utility/LogChannels.h"

UEnemyHealBeamComponent::UEnemyHealBeamComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Idle enemies shouldn't pay for a tick. Enabled on demand in BeginAttack().
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UEnemyHealBeamComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UEnemyHealBeamComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyBeamFX();

	if (ActiveWindUpFX)
	{
		ActiveWindUpFX->DestroyComponent();
		ActiveWindUpFX = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

bool UEnemyHealBeamComponent::BeginAttack(AActor* InTarget)
{
	if (State != EHealBeamState::Idle || !IsValid(InTarget))
	{
		return false;
	}

	Target = InTarget;
	SetComponentTickEnabled(true);
	EnterState(EHealBeamState::WindUp);
	return true;
}

void UEnemyHealBeamComponent::AbortAttack()
{
	if (State == EHealBeamState::Idle)
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
	EnterState(EHealBeamState::Cooldown);
}

void UEnemyHealBeamComponent::EnterState(EHealBeamState NewState)
{
	State = NewState;
	StateTime = 0.f;

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());

	switch (State)
	{
	case EHealBeamState::WindUp:
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

		OnHealWindUpStarted.Broadcast();
		break;
	}

	case EHealBeamState::Firing:
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

		HealAccumulator = 0.f;
		SpawnBeamFX();
		OnHealBeamStarted.Broadcast();
		break;
	}

	case EHealBeamState::Cooldown:
	{
		Target = nullptr;
		OnHealFinished.Broadcast();
		OnHealCooldownStarted.Broadcast();
		break;
	}

	case EHealBeamState::Idle:
	{
		// Nothing left to do until the next BeginAttack, so stop ticking.
		SetComponentTickEnabled(false);
		break;
	}
	}
}

void UEnemyHealBeamComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	StateTime += DeltaTime;

	switch (State)
	{
	case EHealBeamState::WindUp:
		TickWindUp(DeltaTime);
		break;

	case EHealBeamState::Firing:
		TickFiring(DeltaTime);
		break;

	case EHealBeamState::Cooldown:
		if (StateTime >= CooldownDuration)
		{
			EnterState(EHealBeamState::Idle);
		}
		break;

	case EHealBeamState::Idle:
	default:
		break;
	}
}

void UEnemyHealBeamComponent::TickWindUp(float DeltaTime)
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
		EnterState(EHealBeamState::Firing);
	}
}

bool UEnemyHealBeamComponent::IsFacingTarget() const
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

void UEnemyHealBeamComponent::TickFiring(float DeltaTime)
{
	if (!IsValid(Target))
	{
		DestroyBeamFX();
		StopMontages();
		EnterState(EHealBeamState::Cooldown);
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
		HealAccumulator += DeltaTime;
		while (HealAccumulator >= HealTickInterval)
		{
			HealAccumulator -= HealTickInterval;
			ApplyHealTick();
		}
	}

	if (IsLOSLost() || StateTime >= MaxFireDuration)
	{
		DestroyBeamFX();
		StopMontages();
		EnterState(EHealBeamState::Cooldown);
	}
}

FVector UEnemyHealBeamComponent::GetMuzzleLocation() const
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

FVector UEnemyHealBeamComponent::GetAimPoint() const
{
	if (!IsValid(Target))
	{
		return FVector::ZeroVector;
	}

	// Aim at the chest rather than the actor origin, which sits at the feet for
	// characters and would make every trace clip the floor.
	return Target->GetActorLocation() + FVector(0.f, 0.f, TargetAimHeight);
}

bool UEnemyHealBeamComponent::TraceToTarget(FHitResult& OutHit, FVector& OutStart, FVector& OutEnd) const
{
	OutStart = GetMuzzleLocation();
	OutEnd = GetAimPoint();

	if (!IsValid(Target))
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HealBeamLOS), /*bTraceComplex*/ false, GetOwner());
	Params.AddIgnoredActor(Target);

	// Ignoring the target means any blocking hit is by definition something in the
	// way, so "no hit" cleanly equals "clear line of sight".
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
		OutHit, OutStart, OutEnd, LOSChannel, Params);

	return !bBlocked;
}

void UEnemyHealBeamComponent::SpawnBeamFX()
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

void UEnemyHealBeamComponent::DestroyBeamFX()
{
	if (ActiveBeam)
	{
		// Deactivate rather than destroy so in-flight particles can finish.
		ActiveBeam->Deactivate();
		ActiveBeam->SetAutoDestroy(true);
		ActiveBeam = nullptr;
	}
}

void UEnemyHealBeamComponent::StopMontages()
{
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		OwnerCharacter->StopAnimMontage();
	}
}

void UEnemyHealBeamComponent::ApplyHealTick()
{
	if (!IsValid(Target))
	{
		return;
	}

	const float HealThisTick = HealPerSecond * HealTickInterval;

	// Heal the ally directly via its HealthComponent (healing is a friendly-target op,
	// not routed through IDamageable which is for damage).
	if (UHealthComponent* TargetHealth = Target->FindComponentByClass<UHealthComponent>())
	{
		TargetHealth->Heal(HealThisTick);
	}
	else
	{
		UE_LOG(LogGMTKCombat, Warning,
			TEXT("Heal target %s has no HealthComponent - no heal applied."),
			*Target->GetName());
	}
}
void UEnemyHealBeamComponent::FaceTarget(float DeltaTime)
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

void UEnemyHealBeamComponent::DrawDebug(const FVector& Start, const FVector& End, bool bHasLOS) const
{
#if ENABLE_DRAW_DEBUG
	const FColor BeamColor = bHasLOS ? FColor::Red : FColor::Silver;

	DrawDebugLine(GetWorld(), Start, End, BeamColor, false, -1.f, 0, 4.f);
	DrawDebugPoint(GetWorld(), End, 12.f, BeamColor, false, -1.f);
#endif
}
