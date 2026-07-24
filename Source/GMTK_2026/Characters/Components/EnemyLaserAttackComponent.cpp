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

	if (StateTime >= WindUpDuration)
	{
		EnterState(ELaserState::Firing);
	}
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

void UEnemyLaserAttackComponent::DrawDebug(const FVector& Start, const FVector& End, bool bHasLOS) const
{
#if ENABLE_DRAW_DEBUG
	const FColor BeamColor = bHasLOS ? FColor::Red : FColor::Silver;

	DrawDebugLine(GetWorld(), Start, End, BeamColor, false, -1.f, 0, 4.f);
	DrawDebugPoint(GetWorld(), End, 12.f, BeamColor, false, -1.f);
#endif
}
