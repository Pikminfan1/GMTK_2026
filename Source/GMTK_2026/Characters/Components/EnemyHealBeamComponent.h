#pragma once

#include "CoreMinimal.h"
#include "Characters/Components/EnemyBeamComponentBase.h"
#include "EnemyHealBeamComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHealFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHealWindUpStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHealBeamStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHealCooldownStarted);

/**
 * The friendly beam: restores a wounded ally's health for as long as the beam
 * holds. Target selection (which ally deserves the heal) belongs to the Behavior
 * Tree (see BTTask_HealAlly); the full beam lifecycle lives in
 * UEnemyBeamComponentBase. This class only defines the healing payload and exposes
 * heal-named events for Blueprints to hook.
 *
 * Healing goes to the ally's HealthComponent directly rather than through
 * IDamageable - that interface is the project's damage path, and keeping heals off
 * it means nothing ever has to disambiguate "negative damage".
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class GMTK_2026_API UEnemyHealBeamComponent : public UEnemyBeamComponentBase
{
	GENERATED_BODY()

public:
	/** The ally currently being healed (valid during WindUp/Firing, null when idle).
	 *  Use this actor's location as the heal beam's end point. */
	UFUNCTION(BlueprintPure, Category = "Heal")
	AActor* GetHealTarget() const { return GetBeamTarget(); }

	/** Fires when the heal ends for any reason - completion, LOS loss, or abort. */
	UPROPERTY(BlueprintAssignable, Category = "Heal")
	FOnHealFinished OnHealFinished;

	/** Fires when the wind-up (telegraph) phase begins. Hook charge-up VFX/SFX. */
	UPROPERTY(BlueprintAssignable, Category = "Heal")
	FOnHealWindUpStarted OnHealWindUpStarted;

	/** Fires when the beam actually starts healing. Hook the beam-start flash. */
	UPROPERTY(BlueprintAssignable, Category = "Heal")
	FOnHealBeamStarted OnHealBeamStarted;

	/** Fires when the heal ends and cooldown begins. Same moment as OnHealFinished,
	 *  but named for the cooldown phase specifically (smoke, recovery anim). */
	UPROPERTY(BlueprintAssignable, Category = "Heal")
	FOnHealCooldownStarted OnHealCooldownStarted;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|Heal")
	float HealPerSecond = 15.0f;

	/** Seconds between heal applications. Each tick restores
	 *  HealPerSecond * HealTickInterval, so total output is interval-independent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heal|Heal")
	float HealTickInterval = 0.25f;

	/** Restores one tick of health to the targeted ally's HealthComponent. */
	virtual void ApplyBeamTick() override;

	virtual float GetEffectTickInterval() const override { return HealTickInterval; }

	virtual void NotifyWindUpStarted() override   { OnHealWindUpStarted.Broadcast(); }
	virtual void NotifyFiringStarted() override   { OnHealBeamStarted.Broadcast(); }
	virtual void NotifyFinished() override        { OnHealFinished.Broadcast(); }
	virtual void NotifyCooldownStarted() override { OnHealCooldownStarted.Broadcast(); }
};
