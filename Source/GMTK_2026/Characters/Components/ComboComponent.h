// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ComboComponent.generated.h"

class ABaseGameState;

/** Fires whenever the combo count changes for any reason (increment or reset), so UI
 *  can update. NewCombo is the current streak count. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnComboChanged, int32, NewCombo);

/** Fires specifically when the combo goes UP - a kill continued the streak. Good for
 *  a "combo!" flourish / rising pitch SFX. Passes the new count. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnComboIncremented, int32, NewCombo);

/** Fires when the streak ends - the window elapsed with no kill. Passes the count the
 *  combo reached before ending, so you can reward/celebrate a big finished streak. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnComboEnded, int32, FinalCombo);

/** Fires when the number of reward STACKS changes (every KillsPerStack kills, and 0 on
 *  reset). The player's reward layer listens to this to apply the cycling rewards. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnComboStacksChanged, int32, NewStacks);

/**
 * Tracks a kill streak: consecutive enemy kills within a refreshing time window.
 *
 * Each kill increments the combo and refills the window to its full duration. The
 * window counts down every tick; if it reaches zero without another kill, the combo
 * ends and resets to zero. Kill fast enough and the streak keeps climbing.
 *
 * Self-wiring: on BeginPlay it finds the BaseGameState and subscribes to its central
 * OnEnemyKilled event, so the only setup is adding this component to the player.
 *
 * The combo count and events are exposed for a future powerup system to read (e.g.
 * scaling attack power with the current streak) - that system isn't built here, this
 * just provides the meter and signals it depends on.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class GMTK_2026_API UComboComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UComboComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// ---------- Queries ----------

	/** Current streak count. 0 when no combo is active. */
	UFUNCTION(BlueprintPure, Category = "Combo")
	int32 GetComboCount() const { return ComboCount; }

	/** Reward stacks earned = ComboCount / KillsPerStack. 0 until the first threshold. */
	UFUNCTION(BlueprintPure, Category = "Combo")
	int32 GetComboStacks() const { return ComboCount / FMath::Max(1, KillsPerStack); }

	UFUNCTION(BlueprintPure, Category = "Combo")
	bool IsComboActive() const { return ComboCount > 0; }

	/** Seconds left before the current combo ends. 0 if no combo is active. */
	UFUNCTION(BlueprintPure, Category = "Combo")
	float GetTimeRemaining() const { return TimeRemaining; }

	/** 0..1 fraction of the window remaining - handy for a radial/bar UI. */
	UFUNCTION(BlueprintPure, Category = "Combo")
	float GetTimeRemainingFraction() const
	{
		return ComboWindow > 0.f ? FMath::Clamp(TimeRemaining / ComboWindow, 0.f, 1.f) : 0.f;
	}

	/** Register a kill toward the combo. Normally called automatically via the
	 *  GameState's OnEnemyKilled, but exposed so other systems can feed it too. */
	UFUNCTION(BlueprintCallable, Category = "Combo")
	void RegisterKill();

	/** Force the combo to end immediately (e.g. on player death). */
	UFUNCTION(BlueprintCallable, Category = "Combo")
	void ResetCombo();

	// ---------- Events ----------

	UPROPERTY(BlueprintAssignable, Category = "Combo")
	FOnComboChanged OnComboChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combo")
	FOnComboIncremented OnComboIncremented;

	UPROPERTY(BlueprintAssignable, Category = "Combo")
	FOnComboEnded OnComboEnded;

	/** Fires when the reward stack tier changes (including back to 0 on reset). */
	UPROPERTY(BlueprintAssignable, Category = "Combo")
	FOnComboStacksChanged OnComboStacksChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Seconds allowed between kills to keep the streak alive. Each kill refills the
	 *  timer to this. Tune for feel - your spec mentioned ~10s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combo", meta = (ClampMin = "0.1"))
	float ComboWindow = 10.f;

	/** Kills required per reward stack. Every this-many kills advances the reward chain. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combo", meta = (ClampMin = "1"))
	int32 KillsPerStack = 5;

	/** Bound to the GameState's central kill event. */
	UFUNCTION()
	void HandleEnemyKilled(AActor* Killer, AActor* Victim);

private:
	UPROPERTY()
	TObjectPtr<ABaseGameState> CachedGameState;

	int32 ComboCount = 0;
	float TimeRemaining = 0.f;
	int32 LastBroadcastStacks = 0;
};
