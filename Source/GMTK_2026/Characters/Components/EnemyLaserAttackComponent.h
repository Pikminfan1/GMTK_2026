#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyLaserAttackComponent.generated.h"

class UAnimMontage;
class UNiagaraComponent;
class UNiagaraSystem;

UENUM(BlueprintType)
enum class ELaserState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	WindUp		UMETA(DisplayName = "Wind Up"),
	Firing		UMETA(DisplayName = "Firing"),
	Cooldown	UMETA(DisplayName = "Cooldown")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLaserFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLaserWindUpStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLaserFiringStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLaserCooldownStarted);

/**
 * The sustained-beam ranged attack: a telegraphed wind-up, then a continuous beam
 * that drains health for as long as line of sight holds.
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
class GMTK_2026_API UEnemyLaserAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyLaserAttackComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Start the wind-up. Returns false if already attacking or on cooldown. */
	UFUNCTION(BlueprintCallable, Category = "Laser")
	bool BeginAttack(AActor* InTarget);

	/** Stop immediately and go to cooldown. Safe to call in any state. */
	UFUNCTION(BlueprintCallable, Category = "Laser")
	void AbortAttack();

	UFUNCTION(BlueprintPure, Category = "Laser")
	ELaserState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Laser")
	bool IsBusy() const { return State != ELaserState::Idle; }

	/** True once LOS has been broken for longer than LOSGraceTime. */
	UFUNCTION(BlueprintPure, Category = "Laser")
	bool IsLOSLost() const { return TimeSinceLOS > LOSGraceTime; }

	/** Minimum range this attack is willing to fire from - BT decorators read this. */
	UFUNCTION(BlueprintPure, Category = "Laser")
	float GetMinRange() const { return MinRange; }

	UFUNCTION(BlueprintPure, Category = "Laser")
	float GetMaxRange() const { return MaxRange; }

	/** Fires when the attack ends for any reason - completion, LOS loss, or abort. */
	UPROPERTY(BlueprintAssignable, Category = "Laser")
	FOnLaserFinished OnLaserFinished;

	/** Fires when the wind-up (telegraph) phase begins. Hook charge-up VFX/SFX. */
	UPROPERTY(BlueprintAssignable, Category = "Laser")
	FOnLaserWindUpStarted OnWindUpStarted;

	/** Fires when the beam actually starts firing. Hook the beam-start flash/boom. */
	UPROPERTY(BlueprintAssignable, Category = "Laser")
	FOnLaserFiringStarted OnFiringStarted;

	/** Fires when the attack ends and cooldown begins. Same moment as OnLaserFinished,
	 *  but named for the cooldown phase specifically (smoke, recovery anim). */
	UPROPERTY(BlueprintAssignable, Category = "Laser")
	FOnLaserCooldownStarted OnCooldownStarted;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---------- Timing ----------

	/** Telegraph length. This is the player's window to break line of sight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|Timing")
	float WindUpDuration = 1.25f;

	/** Hard cap on beam duration so an enemy can't hold a token forever. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|Timing")
	float MaxFireDuration = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|Timing")
	float CooldownDuration = 3.0f;

	/** How long LOS may be broken before the beam gives up. Prevents flicker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|Timing")
	float LOSGraceTime = 0.4f;

	
	// ---------- Aiming ----------

	/** Degrees per second the enemy rotates to face the target while winding up and firing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|Aiming")
	float RotationSpeedDegrees = 360.f;

	/** If false, the enemy only rotates during wind-up, then holds still while firing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|Aiming")
	bool bTrackTargetWhileFiring = true;
	
	/** How many degrees off-aim is still allowed to start firing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|Aiming")
	float FireAngleTolerance = 12.f;
	
	// ---------- Damage ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|Damage")
	float DamagePerSecond = 12.0f;

	/**
	 * Damage is applied in discrete ticks rather than per-frame - it keeps damage
	 * framerate-independent in practice and gives hit reactions something to fire on.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|Damage")
	float DamageTickInterval = 0.25f;

	// ---------- Range ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|Range")
	float MinRange = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|Range")
	float MaxRange = 2000.f;

	// ---------- Trace ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|Trace")
	TEnumAsByte<ECollisionChannel> LOSChannel = ECC_Visibility;

	/** Socket the beam originates from. Falls back to actor location + height if missing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|Trace")
	FName MuzzleSocketName = TEXT("Muzzle");

	/** Used when MuzzleSocketName doesn't exist on the mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|Trace")
	float FallbackMuzzleHeight = 60.f;

	/** Aim point offset up from the target's actor location, toward the chest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|Trace")
	float TargetAimHeight = 50.f;

	// ---------- FX (all optional) ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|FX")
	TObjectPtr<UNiagaraSystem> WindUpFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|FX")
	TObjectPtr<UNiagaraSystem> BeamFX;

	/** Niagara user parameter (Vector) driving the beam's end point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|FX")
	FName BeamEndParamName = TEXT("BeamEnd");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|FX")
	TObjectPtr<UAnimMontage> WindUpMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|FX")
	TObjectPtr<UAnimMontage> FireLoopMontage;

	/** Draw the beam and wind-up with debug lines. Leave on until Niagara exists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|Debug")
	bool bDrawDebugBeam = false;

private:
	void EnterState(ELaserState NewState);
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
	void ApplyDamageTick();
	void DrawDebug(const FVector& Start, const FVector& End, bool bHasLOS) const;

	UPROPERTY()
	TObjectPtr<AActor> Target;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> ActiveBeam;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> ActiveWindUpFX;

	ELaserState State = ELaserState::Idle;
	float StateTime = 0.f;
	float TimeSinceLOS = 0.f;
	float DamageAccumulator = 0.f;
};
