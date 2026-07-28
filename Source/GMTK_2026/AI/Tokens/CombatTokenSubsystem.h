#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AI/Tokens/TokenTypes.h"
#include "CombatTokenSubsystem.generated.h"

class AController;

/** One outstanding permission-to-attack currently held by an AI controller. */
USTRUCT()
struct FTokenGrant
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AController> Holder;

	ETokenRequestType Type = ETokenRequestType::Melee;
	int32 Cost = 1;
	float GrantTime = 0.f;
};

/** A controller that asked and was denied. Tracked so nobody starves forever. */
USTRUCT()
struct FTokenQueueEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AController> Requester;

	ETokenRequestType Type = ETokenRequestType::Melee;
	float FirstRequestTime = 0.f;
	float LastDenyTime = 0.f;
};

/**
 * The Doom 2016-style combat director. Enemies must hold a token to attack; the
 * total value of outstanding tokens is capped, so only a few enemies can be
 * attacking at once no matter how many are alive. Everyone else is forced into
 * visible repositioning behaviour instead, which is what makes a big fight read
 * as chaotic without actually being unfair.
 *
 * Tokens are keyed on the AI CONTROLLER (the thing that decides to attack), and
 * state changes are pushed to holders through the ITokenHolder interface, so a
 * controller whose token is stolen or reclaimed can abort its attack the same
 * frame instead of discovering the loss on a later Behavior Tree tick.
 *
 * Fairness and pacing layers, in the order a request passes through them:
 *   1. Freeze       - console-driven kill switch for all new grants.
 *   2. Lockout      - a just-released enemy must sit out PostAttackLockout seconds.
 *   3. Direction    - off-screen enemies yield to on-screen waiters, so attacks
 *                     come from where the player is looking.
 *   4. Budget       - the request must be affordable within the current budget,
 *                     which shrinks as the player's health drops (mercy window).
 *   5. Starvation   - whoever has waited past StarvationPriorityTime is served
 *                     next regardless of the direction rule.
 *
 * Lives on the World, so there is exactly one per level and no actor to place.
 * Access it with GetWorld()->GetSubsystem<UCombatTokenSubsystem>().
 */
UCLASS()
class GMTK_2026_API UCombatTokenSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UCombatTokenSubsystem, STATGROUP_Tickables);
	}

	/** Only tick in real game worlds, not editor preview/inactive worlds. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	/**
	 * Ask for permission to attack. Safe to call every frame - if the requester
	 * already holds a token this returns true without spending anything else.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat Tokens")
	bool RequestToken(AController* Requester, ETokenRequestType Type);

	/** Give the token back. Idempotent - safe to call when not holding one. */
	UFUNCTION(BlueprintCallable, Category = "Combat Tokens")
	void ReleaseToken(AController* Requester);

	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	bool HoldsToken(const AController* Holder) const;

	/** Force-transfer a token to DamagedController by taking one from a current holder
	 *  that isn't mid-attack (an "idle" holder sitting on a token). Bypasses the normal
	 *  budget/queue/lockout since it's a forced steal, not a request. Prefers robbing
	 *  holders the player cannot see, so an enemy the player is watching never visibly
	 *  gives up its attack. No-op if the damaged controller already holds one, or if no
	 *  idle holder exists. Returns true if a token was transferred. Used to make a shot
	 *  enemy fight back. */
	UFUNCTION(BlueprintCallable, Category = "Combat Tokens")
	bool TryStealTokenFor(AController* DamagedController, ETokenRequestType Type);

	/** Seconds this controller has been waiting since it first got denied. Drives
	 *  fallback aggression (see BTService_EscalateSpeedByWaitTime). */
	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	float GetWaitTime(const AController* Requester) const;

	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	int32 GetAvailableBudget() const { return CurrentBudget - GetSpentBudget(); }

	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	int32 GetSpentBudget() const;

	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	int32 GetCurrentBudget() const { return CurrentBudget; }

	/** Exposed so BT decorators can check affordability before bothering to request. */
	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	int32 GetCostForType(ETokenRequestType Type) const;

	/** One-line budget/holder/waiter summary for on-screen display. */
	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	FString GetDebugString() const;

	// ---------- Console entry points (bound to the gmtk.Tokens.* commands) ----------

	/** Toggle the on-screen HUD line and the per-enemy world-space markers together. */
	void ToggleDebugDraw();

	/** Toggle refusing all NEW grants. Current holders keep their tokens, so the
	 *  visible effect is every enemy dropping into fallback behaviour as attacks end. */
	void ToggleFreeze();

	/** Override the healthy-player budget at runtime and re-derive the current budget. */
	void SetMaxBudgetOverride(int32 NewMaxBudget);

	/** Log every current grant, waiter, and lockout to the output log. */
	void DumpState() const;

protected:
	/** Pool size while the player is healthy. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Budget")
	int32 MaxBudget = 6;

	/** Pool size when the player is nearly dead - the mercy window. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Budget")
	int32 MinBudget = 1;

	/** Below this fraction of player health, the budget starts shrinking toward MinBudget. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Budget", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PressureStartHealthPct = 0.5f;

	/** Set false to disable health-based scaling entirely (budget stays at MaxBudget). */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Budget")
	bool bScaleBudgetWithPlayerHealth = true;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Cost")
	int32 MeleeCost = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Cost")
	int32 RangedBurstCost = 1;

	/** The laser is a heavy commitment - it should eat a large slice of the pool. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Cost")
	int32 RangedLaserCost = 2;

	/** After releasing, an enemy cannot re-request for this long. Forces rotation. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Fairness")
	float PostAttackLockout = 2.0f;

	/** Waiting longer than this makes an enemy the guaranteed next grantee. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Fairness")
	float StarvationPriorityTime = 6.0f;

	/** Safety net: reclaim a token held longer than this, and log loudly about it. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Fairness")
	float MaxTokenHoldTime = 12.0f;

	// ---------- Directional coordination ----------

	/** Prefer granting attack permission to enemies the player can currently see.
	 *  An attacker on-screen reads as fair; one opening up from off-screen reads as
	 *  cheap. When there's contention, off-screen requesters yield to any on-screen
	 *  waiter. Starvation priority overrides this, so off-screen enemies are
	 *  delayed - never locked out forever. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Coordination")
	bool bPreferOnScreenAttackers = true;

	/** Half-angle (degrees) from the camera forward vector inside which an enemy
	 *  counts as "on screen". Deliberately wider than the real FOV so enemies near
	 *  the screen edge don't flicker between the two states. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Coordination",
		meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float OnScreenHalfAngleDegrees = 70.f;

	// ---------- Debug ----------

	/** Draw budget state on screen during PIE. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Debug")
	bool bDrawDebugHUD = false;

	/** Per-enemy world-space markers: green = holding a token, yellow = waiting
	 *  (red once starving), silver = in post-attack lockout. Toggle at runtime with
	 *  the console command gmtk.Tokens.Debug. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Debug")
	bool bDrawDebugMarkers = false;

private:
	void UpdateBudget();
	void ReclaimStaleTokens();
	void PruneQueue();
	void AddOrRefreshQueueEntry(AController* Requester, ETokenRequestType Type);

	/** Removes any grant held by Holder, starts its lockout, and pushes the revoke
	 *  through ITokenHolder. Every path that takes a token away funnels through here
	 *  so holders always hear about it. */
	void RevokeToken(AController* Holder, ETokenRevokeReason Reason);

	/** Pushes a grant notification through ITokenHolder if the holder implements it. */
	void NotifyGranted(AController* Holder, ETokenRequestType Type) const;

	/** True if the holder's pawn is mid-attack or winding up (its token is committed
	 *  to something visible, so it must not be stolen). */
	bool IsHolderAttackBusy(const AController* Holder) const;

	bool IsHighestPriorityWaiter(const AController* Requester) const;

	/** Cheap cone test against the player camera: is this holder's pawn within
	 *  OnScreenHalfAngleDegrees of where the player is looking? Fails open (returns
	 *  true) when no camera can be resolved, so a missing player never deadlocks
	 *  the director. */
	bool IsOnPlayerScreen(const AController* Holder) const;

	/** True if anyone else in the wait queue is currently on the player's screen. */
	bool HasOnScreenWaiter(const AController* Excluding) const;

	/** Index into ActiveGrants of a holder that isn't mid-attack, optionally
	 *  restricted to off-screen holders. INDEX_NONE if there isn't one. */
	int32 FindIdleHolderIndex(const AController* Excluding, bool bOffScreenOnly) const;

	/** World-space token-state markers above each tracked enemy's head. */
	void DrawDebugMarkers() const;

	static FString TokenTypeToString(ETokenRequestType Type);

	UPROPERTY()
	TArray<FTokenGrant> ActiveGrants;

	UPROPERTY()
	TArray<FTokenQueueEntry> WaitQueue;

	/** Controller -> world time at which its post-attack lockout expires. */
	TMap<TWeakObjectPtr<AController>, float> LockoutExpiry;

	int32 CurrentBudget = 6;
	float BudgetUpdateAccum = 0.f;

	/** Console-driven: while true, all new grant requests are denied and queued. */
	bool bGrantsFrozen = false;
};
