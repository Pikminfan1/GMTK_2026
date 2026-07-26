#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyHealBeamComponent.generated.h"

class UAnimMontage;
class UNiagaraComponent;
class UNiagaraSystem;

UENUM(BlueprintType)
enum class EHealBeamState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	WindUp		UMETA(DisplayName = "Wind Up"),
	Firing		UMETA(DisplayName = "Firing"),
	Cooldown	UMETA(DisplayName = "Cooldown")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHealFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHealWindUpStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHealBeamStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHealCooldownStarted);

/**
 * The sustained-beam ALLY HEAL: a telegraphed wind-up, then a continuous beam that
 * RESTORES a friendly target's health for as long as line of sight holds. Structurally
 * identical to the laser attack (same state machine, LOS, aiming, FX) but its target is
 * a wounded ally and its beam heals instead of damages.
 *
 * Deliberately owns no positioning logic - deciding *where* to stand is the
 * Behavior Tree's job (via EQS). This component only handles the attack itself
 * once the enemy is already in place, which keeps it reusable for any enemy that
 * wants a beam attack regardless of how it picks its firing position.
 *
 * FX are optional: assign BeamFX to get a Niagara beam, or leave it null and the
 * component debug-draws the beam instead so the attack is fully testable before
 * any art exists.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class GMTK_2026_API UEnemyHealBeamComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyHealBeamComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Start the wind-up. Returns false if already attacking or on cooldown. */
	UFUNCTION(BlueprintCallable, Category = "Heal")
	bool BeginAttack(AActor* InTarget);

	/** Stop immediately and go to cooldown. Safe to call in any state. */
	UFUNCTION(BlueprintCallable, Category = "Heal")
	void AbortAttack();

	UFUNCTION(BlueprintPure, Category = "Heal")
	EHealBeamState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Heal")
	bool IsBusy() const { return State != EHealBeamState::Idle; }

	/** True once LOS has been broken for longer than LOSGraceTime. */
	UFUNCTION(BlueprintPure, Category = "Heal")
	bool IsLOSLost() const { return TimeSinceLOS > LOSGraceTime; }

	/** Minimum range this attack is willing to fire from - BT decorators read this. */
	UFUNCTION(BlueprintPure, Category = "Heal")
	float GetMinRange() const { return MinRange; }

	UFUNCTION(BlueprintPure, Category = "Heal")
	float GetMaxRange() const { return MaxRange; }

	/** Fires when the attack ends for any reason - completion, LOS loss, or abort. */
	UPROPERTY(BlueprintAssignable, Category = "Heal")
	FOnHealFinished OnHealFinished;

	/** Fires when the wind-up (telegraph) phase begins. Hook charge-up VFX/SFX. */
	UPROPERTY(BlueprintAssignable, Category = "Heal")
	FOnHealWindUpStarted OnHealWindUpStarted;

	/** Fires when the beam actually starts firing. Hook the beam-start flash/boom. */
	UPROPERTY(BlueprintAssignable, Category = "Heal")
	FOnHealBeamStarted OnHealBeamStarted;

	/** Fires when the attack ends and cooldown begins. Same moment as OnHealFinished,
	 *  but named for the cooldown phase specifically (smoke, recovery anim). */
	UPROPERTY(BlueprintAssignable, Category = "Heal")
	FOnHealCooldownStarted OnHealCooldownStarted;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---------- Timing ----------

	/** Telegraph length. This is the player's window to break line of sight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|Timing")
	float WindUpDuration = 1.25f;

	/** Hard cap on beam duration so an enemy can't hold a token forever. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|Timing")
	float MaxFireDuration = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|Timing")
	float CooldownDuration = 3.0f;

	/** How long LOS may be broken before the beam gives up. Prevents flicker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|Timing")
	float LOSGraceTime = 0.4f;

	
	// ---------- Aiming ----------

	/** Degrees per second the enemy rotates to face the target while winding up and firing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|Aiming")
	float RotationSpeedDegrees = 360.f;

	/** If false, the enemy only rotates during wind-up, then holds still while firing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|Aiming")
	bool bTrackTargetWhileFiring = true;
	
	/** How many degrees off-aim is still allowed to start firing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|Aiming")
	float FireAngleTolerance = 12.f;
	
	// ---------- Heal ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|Heal")
	float HealPerSecond = 15.0f;

	/** Heal is applied in discrete ticks rather than per-frame, matching the laser's
	 *  damage cadence so hit/heal reactions have something to fire on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|Heal")
	float HealTickInterval = 0.25f;

	// ---------- Range ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|Range")
	float MinRange = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|Range")
	float MaxRange = 2000.f;

	// ---------- Trace ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|Trace")
	TEnumAsByte<ECollisionChannel> LOSChannel = ECC_Visibility;

	/** Socket the beam originates from. Falls back to actor location + height if missing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|Trace")
	FName MuzzleSocketName = TEXT("Muzzle");

	/** Used when MuzzleSocketName doesn't exist on the mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|Trace")
	float FallbackMuzzleHeight = 60.f;

	/** Aim point offset up from the target's actor location, toward the chest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|Trace")
	float TargetAimHeight = 50.f;

	// ---------- FX (all optional) ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|FX")
	TObjectPtr<UNiagaraSystem> WindUpFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|FX")
	TObjectPtr<UNiagaraSystem> BeamFX;

	/** Niagara user parameter (Vector) driving the beam's end point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|FX")
	FName BeamEndParamName = TEXT("BeamEnd");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|FX")
	TObjectPtr<UAnimMontage> WindUpMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|FX")
	TObjectPtr<UAnimMontage> FireLoopMontage;

	/** Draw the beam and wind-up with debug lines. Leave on until Niagara exists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|Debug")
	bool bDrawDebugBeam = false;

private:
	void EnterState(EHealBeamState NewState);
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
	void StopMontages();
	void ApplyHealTick();
	void DrawDebug(const FVector& Start, const FVector& End, bool bHasLOS) const;

	UPROPERTY()
	TObjectPtr<AActor> Target;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> ActiveBeam;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> ActiveWindUpFX;

	EHealBeamState State = EHealBeamState::Idle;
	float StateTime = 0.f;
	float TimeSinceLOS = 0.f;
	float HealAccumulator = 0.f;
};
