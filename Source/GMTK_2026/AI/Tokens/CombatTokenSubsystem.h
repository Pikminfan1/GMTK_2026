#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatTokenSubsystem.generated.h"

/**
 * What kind of attack an enemy is asking permission for. Different types cost
 * different amounts out of the shared budget, which is what makes a Laser enemy
 * "expensive" and a Melee enemy "cheap" without any special-casing elsewhere.
 */
UENUM(BlueprintType)
enum class ETokenRequestType : uint8
{
	Melee		UMETA(DisplayName = "Melee"),
	RangedLaser	UMETA(DisplayName = "Ranged Laser"),
	RangedBurst	UMETA(DisplayName = "Ranged Burst")
};

/** One outstanding permission-to-attack currently held by an enemy. */
USTRUCT()
struct FTokenGrant
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Holder;

	ETokenRequestType Type = ETokenRequestType::Melee;
	int32 Cost = 1;
	float GrantTime = 0.f;
};

/** An enemy that asked and was denied. Tracked so nobody starves forever. */
USTRUCT()
struct FTokenQueueEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Requester;

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
	bool RequestToken(AActor* Requester, ETokenRequestType Type);

	/** Give the token back. Idempotent - safe to call when not holding one. */
	UFUNCTION(BlueprintCallable, Category = "Combat Tokens")
	void ReleaseToken(AActor* Requester);

	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	bool HoldsToken(const AActor* Actor) const;

	/** Force-transfer a token to DamagedEnemy by taking one from any current holder that
	 *  isn't mid-attack or winding up (an "idle" holder sitting on a token). Bypasses the
	 *  normal budget/queue/lockout since it's a forced steal, not a request. No-op if the
	 *  damaged enemy already holds one, or if no idle holder can be found. Returns true if
	 *  a token was transferred. Used to make a shot enemy fight back. */
	UFUNCTION(BlueprintCallable, Category = "Combat Tokens")
	bool TryStealTokenFor(AActor* DamagedEnemy, ETokenRequestType Type);

	/** Seconds this actor has been waiting since it first got denied. Drives fallback aggression. */
	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	float GetWaitTime(const AActor* Actor) const;

	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	int32 GetAvailableBudget() const { return CurrentBudget - GetSpentBudget(); }

	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	int32 GetSpentBudget() const;

	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	int32 GetCurrentBudget() const { return CurrentBudget; }

	/** Exposed so BT decorators can check affordability before bothering to request. */
	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	int32 GetCostForType(ETokenRequestType Type) const;

	/** Debug string for on-screen display. */
	UFUNCTION(BlueprintPure, Category = "Combat Tokens")
	FString GetDebugString() const;

protected:
	/** Pool size while the player is healthy. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Budget")
	int32 MaxBudget = 6;

	/** Pool size when the player is nearly dead - the mercy window. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Budget")
	int32 MinBudget = 2;

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
	int32 RangedLaserCost = 3;

	/** After releasing, an enemy cannot re-request for this long. Forces rotation. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Fairness")
	float PostAttackLockout = 2.0f;

	/** Waiting longer than this makes an enemy the guaranteed next grantee. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Fairness")
	float StarvationPriorityTime = 6.0f;

	/** Safety net: reclaim a token held longer than this, and log loudly about it. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Fairness")
	float MaxTokenHoldTime = 12.0f;

	/** Draw budget state on screen during PIE. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Tokens|Debug")
	bool bDrawDebugHUD = false;

private:
	void UpdateBudget();
	void ReclaimStaleTokens();
	void PruneQueue();
	void AddOrRefreshQueueEntry(AActor* Requester, ETokenRequestType Type);

	/** True if the actor is mid-attack or winding up (so its token shouldn't be stolen). */
	bool IsActorAttackBusy(const AActor* Actor) const;
	bool IsHighestPriorityWaiter(const AActor* Requester) const;

	UPROPERTY()
	TArray<FTokenGrant> ActiveGrants;

	UPROPERTY()
	TArray<FTokenQueueEntry> WaitQueue;

	/** Actor -> world time at which its post-attack lockout expires. */
	TMap<TWeakObjectPtr<AActor>, float> LockoutExpiry;

	int32 CurrentBudget = 6;
	float BudgetUpdateAccum = 0.f;
};
