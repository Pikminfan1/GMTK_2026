#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyMeleeAttackComponent.generated.h"

class UAnimMontage;

UENUM(BlueprintType)
enum class EMeleeAttackState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	WindUp		UMETA(DisplayName = "Wind Up"),
	Swing		UMETA(DisplayName = "Swing"),
	Cooldown	UMETA(DisplayName = "Cooldown")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMeleeWindUpStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMeleeSwingStarted);
/** bConnected is false on a whiff - hook a "whoosh" vs "impact" SFX split here. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeleeSwingResolved, bool, bConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMeleeFinished);

/**
 * The grunt's close-range attack: a short telegraphed wind-up, one swing that
 * applies damage at a fixed moment, then a cooldown.
 *
 * Damage is a single range + facing-angle check at the impact moment rather than
 * a continuous trace, which is what makes the attack dodgeable: a player who
 * moves out of range or around the grunt after the wind-up starts turns the
 * swing into a whiff. Whiffing is the counterplay.
 *
 * Same division of labour as the beam components: the Behavior Tree decides WHEN
 * to attack (and holds the combat token for it); this component only executes
 * the swing once told to.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class GMTK_2026_API UEnemyMeleeAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyMeleeAttackComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Start the wind-up. Returns false if already attacking or on cooldown. */
	UFUNCTION(BlueprintCallable, Category = "Melee")
	bool BeginAttack(AActor* InTarget);

	/** Stop immediately and go to cooldown. Safe to call in any state. */
	UFUNCTION(BlueprintCallable, Category = "Melee")
	void AbortAttack();

	UFUNCTION(BlueprintPure, Category = "Melee")
	EMeleeAttackState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Melee")
	bool IsBusy() const { return State != EMeleeAttackState::Idle; }

	/** Max distance at which a swing can connect - BT decorators read this to know
	 *  when the grunt is close enough to attack. */
	UFUNCTION(BlueprintPure, Category = "Melee")
	float GetAttackRange() const { return HitRange; }

	/** Fires when the wind-up (telegraph) phase begins. Hook the raised-arm anim cue. */
	UPROPERTY(BlueprintAssignable, Category = "Melee")
	FOnMeleeWindUpStarted OnWindUpStarted;

	/** Fires the moment the swing phase begins. */
	UPROPERTY(BlueprintAssignable, Category = "Melee")
	FOnMeleeSwingStarted OnSwingStarted;

	/** Fires at the swing's impact moment with whether it connected. */
	UPROPERTY(BlueprintAssignable, Category = "Melee")
	FOnMeleeSwingResolved OnSwingResolved;

	/** Fires when the attack ends for any reason and cooldown begins. */
	UPROPERTY(BlueprintAssignable, Category = "Melee")
	FOnMeleeFinished OnMeleeFinished;

protected:
	// ---------- Timing ----------

	/** Telegraph before the swing - the player's dodge window. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Timing")
	float WindUpDuration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Timing")
	float SwingDuration = 0.4f;

	/** How far into the swing the damage check happens. Sync with the swing
	 *  montage's impact frame; clamped to SwingDuration at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Timing")
	float SwingHitDelay = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Timing")
	float CooldownDuration = 1.5f;

	// ---------- Damage ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Damage")
	float Damage = 15.f;

	/** Max distance at the impact moment for the swing to connect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Damage")
	float HitRange = 220.f;

	/** Half-angle of the frontal cone the target must be inside at the impact
	 *  moment. Stops a swing connecting behind the grunt's back when the player
	 *  strafes through. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Damage")
	float HitHalfAngleDegrees = 60.f;

	// ---------- Aiming ----------

	/** Turn rate while winding up. Faster than the beam attacks' - at melee range
	 *  a slow turn reads as unresponsive rather than deliberate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Aiming")
	float RotationSpeedDegrees = 540.f;

	// ---------- FX (optional) ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|FX")
	TObjectPtr<UAnimMontage> WindUpMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|FX")
	TObjectPtr<UAnimMontage> SwingMontage;

private:
	void EnterState(EMeleeAttackState NewState);

	/** Rotates the owner toward the target on the yaw axis only. */
	void FaceTarget(float DeltaTime);

	/** The single damage check: range + facing angle at the impact moment. */
	void ResolveSwing();

	/** Plays a montage on the owning character, logging a warning if the
	 *  AnimInstance rejects it (missing Slot node or skeleton mismatch). */
	void PlayMeleeMontage(UAnimMontage* Montage);

	/** Stops only this component's montages, leaving others untouched. */
	void StopMeleeMontages();

	UPROPERTY()
	TObjectPtr<AActor> Target;

	EMeleeAttackState State = EMeleeAttackState::Idle;
	float StateTime = 0.f;

	/** Guards ResolveSwing so the impact check runs exactly once per swing. */
	bool bSwingResolved = false;
};
