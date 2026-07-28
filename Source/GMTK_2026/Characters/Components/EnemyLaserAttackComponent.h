#pragma once

#include "CoreMinimal.h"
#include "Characters/Components/EnemyBeamComponentBase.h"
#include "EnemyLaserAttackComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLaserFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLaserWindUpStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLaserFiringStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLaserCooldownStarted);

/**
 * The hostile beam: drains the target's health through the IDamageable interface
 * for as long as the beam holds. The full attack lifecycle (telegraph, LOS rules,
 * aiming, FX, montages) lives in UEnemyBeamComponentBase - this class only defines
 * the damage payload and exposes laser-named events for Blueprints to hook.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class GMTK_2026_API UEnemyLaserAttackComponent : public UEnemyBeamComponentBase
{
	GENERATED_BODY()

public:
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|Damage")
	float DamagePerSecond = 12.0f;

	/** Seconds between damage applications. Each tick deals
	 *  DamagePerSecond * DamageTickInterval, so total output is interval-independent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser|Damage")
	float DamageTickInterval = 0.25f;

	/** Deals one tick of damage to the beam target via IDamageable, attributing the
	 *  owning pawn's controller as the instigator. */
	virtual void ApplyBeamTick() override;

	virtual float GetEffectTickInterval() const override { return DamageTickInterval; }

	virtual void NotifyWindUpStarted() override   { OnWindUpStarted.Broadcast(); }
	virtual void NotifyFiringStarted() override   { OnFiringStarted.Broadcast(); }
	virtual void NotifyFinished() override        { OnLaserFinished.Broadcast(); }
	virtual void NotifyCooldownStarted() override { OnCooldownStarted.Broadcast(); }
};
