#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyBeamComponentBase.generated.h"

class UAnimMontage;
class UNiagaraComponent;
class UNiagaraSystem;

/** Lifecycle shared by every sustained-beam ability. */
UENUM(BlueprintType)
enum class EBeamAttackState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	WindUp		UMETA(DisplayName = "Wind Up"),
	Firing		UMETA(DisplayName = "Firing"),
	Cooldown	UMETA(DisplayName = "Cooldown")
};

/**
 * Base class for sustained-beam abilities: a telegraphed wind-up, then a continuous
 * beam that applies its effect to a target on a fixed cadence for as long as line
 * of sight holds. WHAT the effect is - damage to the player (laser) or healing to
 * a wounded ally (heal beam) - is the only decision subclasses make, via
 * ApplyBeamTick() and GetEffectTickInterval().
 *
 * Everything else lives here exactly once:
 *  - The state machine (Idle -> WindUp -> Firing -> Cooldown -> Idle) and its
 *    timers. The component only ticks while an attack is in flight.
 *  - LOS handling: breaking line of sight during the wind-up cancels the attack
 *    outright (the player's reward for reacting to the telegraph); during firing,
 *    a short grace period stops the beam flickering off over doorframes before
 *    it gives up.
 *  - Yaw-only rotation toward the target, with an alignment gate so the beam
 *    never starts firing while pointed the wrong way.
 *  - Niagara FX and montage playback for both phases, plus debug-line drawing so
 *    the attack is fully testable before any art exists.
 *
 * Deliberately owns no positioning logic - deciding WHERE to stand is the
 * Behavior Tree's job (via EQS). This component only executes the attack once the
 * enemy is already in place, which keeps it reusable for any enemy that wants a
 * beam regardless of how it picks its firing position.
 */
UCLASS(Abstract)
class GMTK_2026_API UEnemyBeamComponentBase : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyBeamComponentBase();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Start the wind-up. Returns false if already attacking or on cooldown. */
	UFUNCTION(BlueprintCallable, Category = "Beam")
	bool BeginAttack(AActor* InTarget);

	/** Stop immediately and go to cooldown. Safe to call in any state. */
	UFUNCTION(BlueprintCallable, Category = "Beam")
	void AbortAttack();

	UFUNCTION(BlueprintPure, Category = "Beam")
	EBeamAttackState GetState() const { return State; }

	/** The actor this beam is aimed at. Valid during WindUp/Firing, null when idle. */
	UFUNCTION(BlueprintPure, Category = "Beam")
	AActor* GetBeamTarget() const { return Target; }

	UFUNCTION(BlueprintPure, Category = "Beam")
	bool IsBusy() const { return State != EBeamAttackState::Idle; }

	/** True once LOS has been broken for longer than LOSGraceTime. */
	UFUNCTION(BlueprintPure, Category = "Beam")
	bool IsLOSLost() const { return TimeSinceLOS > LOSGraceTime; }

	/** Minimum range this attack is willing to fire from - BT decorators read this. */
	UFUNCTION(BlueprintPure, Category = "Beam")
	float GetMinRange() const { return MinRange; }

	UFUNCTION(BlueprintPure, Category = "Beam")
	float GetMaxRange() const { return MaxRange; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---------- Subclass contract ----------

	/** Apply one tick's worth of the beam's effect to Target. Called every
	 *  GetEffectTickInterval() seconds while firing with clear line of sight. */
	virtual void ApplyBeamTick() PURE_VIRTUAL(UEnemyBeamComponentBase::ApplyBeamTick, );

	/** Seconds between ApplyBeamTick calls. Subclasses return their own tuned
	 *  property so the cadence is configured next to the per-tick amount. */
	virtual float GetEffectTickInterval() const { return 0.25f; }

	/** Phase notifications. Subclasses broadcast their own named delegates from
	 *  these so Blueprints bind to attack-specific events (laser vs heal) while the
	 *  base decides when each phase actually happens. */
	virtual void NotifyWindUpStarted() {}
	virtual void NotifyFiringStarted() {}
	virtual void NotifyFinished() {}
	virtual void NotifyCooldownStarted() {}

	/** Controller of the owning pawn - subclasses use it for damage instigation. */
	AController* GetOwnerController() const;

	// ---------- Timing ----------

	/** Telegraph length. This is the player's window to break line of sight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|Timing")
	float WindUpDuration = 1.25f;

	/** Hard cap on beam duration so an enemy can't hold a combat token forever. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|Timing")
	float MaxFireDuration = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|Timing")
	float CooldownDuration = 3.0f;

	/** How long LOS may be broken before the beam gives up. Prevents flicker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|Timing")
	float LOSGraceTime = 0.4f;

	// ---------- Aiming ----------

	/** Degrees per second the enemy rotates to face the target while winding up and firing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|Aiming")
	float RotationSpeedDegrees = 360.f;

	/** If false, the enemy only rotates during wind-up, then holds still while firing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|Aiming")
	bool bTrackTargetWhileFiring = true;

	/** How many degrees off-aim is still allowed to start firing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|Aiming")
	float FireAngleTolerance = 12.f;

	// ---------- Range ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|Range")
	float MinRange = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|Range")
	float MaxRange = 2000.f;

	// ---------- Trace ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|Trace")
	TEnumAsByte<ECollisionChannel> LOSChannel = ECC_Visibility;

	/** Socket the beam originates from. Falls back to actor location + height if missing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|Trace")
	FName MuzzleSocketName = TEXT("Muzzle");

	/** Used when MuzzleSocketName doesn't exist on the mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|Trace")
	float FallbackMuzzleHeight = 60.f;

	/** Aim point offset up from the target's actor location, toward the chest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|Trace")
	float TargetAimHeight = 50.f;

	// ---------- FX (all optional) ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|FX")
	TObjectPtr<UNiagaraSystem> WindUpFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|FX")
	TObjectPtr<UNiagaraSystem> BeamFX;

	/** Niagara user parameter (Vector) driving the beam's end point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|FX")
	FName BeamEndParamName = TEXT("BeamEnd");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|FX")
	TObjectPtr<UAnimMontage> WindUpMontage;

	/** Plays for the duration of the Firing phase. For a continuous visual this
	 *  montage's section must loop back to itself in the Montage editor; a
	 *  non-looping montage plays once and blends out while the beam continues. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|FX")
	TObjectPtr<UAnimMontage> FireLoopMontage;

	/** Draw the beam and wind-up with debug lines. Leave on until Niagara exists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beam|Debug")
	bool bDrawDebugBeam = false;

private:
	void EnterState(EBeamAttackState NewState);
	void TickWindUp(float DeltaTime);
	void TickFiring(float DeltaTime);

	/** Rotates the owner toward the target on the yaw axis only. */
	void FaceTarget(float DeltaTime);

	/** Traces muzzle -> target. Returns true if nothing blocked the path. */
	bool TraceToTarget(FHitResult& OutHit, FVector& OutStart, FVector& OutEnd) const;

	/** True if the owner's forward vector is within FireAngleTolerance of the target. */
	bool IsFacingTarget() const;

	FVector GetMuzzleLocation() const;
	FVector GetAimPoint() const;

	void SpawnBeamFX();
	void DestroyBeamFX();
	void DestroyWindUpFX();

	/** Plays a montage on the owning character's mesh, logging a warning if the
	 *  AnimInstance rejects it (missing Slot node in the AnimBP or a skeleton
	 *  mismatch - failures that are otherwise invisible). */
	void PlayBeamMontage(UAnimMontage* Montage);

	/** Stops only this component's montages, leaving any other montage on the
	 *  character (hit reacts, another component's attack) untouched. */
	void StopBeamMontages();

	void DrawDebugBeamLines(const FVector& Start, const FVector& End, bool bHasLOS) const;

	UPROPERTY()
	TObjectPtr<AActor> Target;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> ActiveBeam;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> ActiveWindUpFX;

	EBeamAttackState State = EBeamAttackState::Idle;
	float StateTime = 0.f;
	float TimeSinceLOS = 0.f;

	/** Accumulates firing time and pays it out in GetEffectTickInterval() chunks,
	 *  keeping the effect framerate-independent. */
	float EffectAccumulator = 0.f;
};
