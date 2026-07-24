#include "AI/Tokens/CombatTokenSubsystem.h"

#include "Characters/Components/HealthComponent.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Utility/LogChannels.h"

void UCombatTokenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CurrentBudget = MaxBudget;
}

void UCombatTokenSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ReclaimStaleTokens();
	PruneQueue();

	// The budget only needs recalculating a few times a second; player health
	// doesn't change fast enough to justify doing this every frame.
	BudgetUpdateAccum += DeltaTime;
	if (BudgetUpdateAccum >= 0.25f)
	{
		BudgetUpdateAccum = 0.f;
		UpdateBudget();
	}

#if !UE_BUILD_SHIPPING
	if (bDrawDebugHUD && GEngine)
	{
		// Fixed key so the message replaces itself instead of stacking every frame.
		GEngine->AddOnScreenDebugMessage(
			/*Key*/ 90210, 0.f, FColor::Yellow, GetDebugString());
	}
#endif
}

int32 UCombatTokenSubsystem::GetCostForType(ETokenRequestType Type) const
{
	switch (Type)
	{
	case ETokenRequestType::RangedLaser: return RangedLaserCost;
	case ETokenRequestType::RangedBurst: return RangedBurstCost;
	case ETokenRequestType::Melee:
	default:                             return MeleeCost;
	}
}

int32 UCombatTokenSubsystem::GetSpentBudget() const
{
	int32 Sum = 0;
	for (const FTokenGrant& Grant : ActiveGrants)
	{
		if (Grant.Holder.IsValid())
		{
			Sum += Grant.Cost;
		}
	}
	return Sum;
}

FString UCombatTokenSubsystem::GetDebugString() const
{
	return FString::Printf(
		TEXT("[Tokens] %d/%d spent | %d holders | %d waiting"),
		GetSpentBudget(), CurrentBudget, ActiveGrants.Num(), WaitQueue.Num());
}

void UCombatTokenSubsystem::UpdateBudget()
{
	if (!bScaleBudgetWithPlayerHealth)
	{
		CurrentBudget = MaxBudget;
		return;
	}

	// Shrink the pool as the player gets hurt. This is the half of the Doom system
	// that stops low-health deaths feeling cheap - fewer enemies are allowed to be
	// attacking at once precisely when the player can least afford it.
	float HealthPct = 1.f;

	if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		if (const UHealthComponent* Health = PlayerPawn->FindComponentByClass<UHealthComponent>())
		{
			HealthPct = Health->GetHealthPercent();
		}
	}

	if (HealthPct >= PressureStartHealthPct || PressureStartHealthPct <= KINDA_SMALL_NUMBER)
	{
		CurrentBudget = MaxBudget;
	}
	else
	{
		const float Alpha = FMath::Clamp(HealthPct / PressureStartHealthPct, 0.f, 1.f);
		CurrentBudget = FMath::RoundToInt(
			FMath::Lerp(static_cast<float>(MinBudget), static_cast<float>(MaxBudget), Alpha));
	}
}

void UCombatTokenSubsystem::ReclaimStaleTokens()
{
	const float Now = GetWorld()->GetTimeSeconds();

	for (int32 Index = ActiveGrants.Num() - 1; Index >= 0; --Index)
	{
		const FTokenGrant& Grant = ActiveGrants[Index];

		const bool bHolderGone = !Grant.Holder.IsValid();
		const bool bHeldTooLong = (Now - Grant.GrantTime) > MaxTokenHoldTime;

		if (bHolderGone || bHeldTooLong)
		{
#if !UE_BUILD_SHIPPING
			if (bHeldTooLong && Grant.Holder.IsValid())
			{
				// If you ever see this, a Behavior Tree branch aborted without
				// running its ReleaseToken task. Find it - don't rely on this.
				UE_LOG(LogGMTKAI, Warning,
					TEXT("[Tokens] Force-reclaimed a token from %s after %.1fs. "
						 "A BT branch almost certainly aborted without releasing."),
					*Grant.Holder->GetName(), Now - Grant.GrantTime);
			}
#endif
			ActiveGrants.RemoveAtSwap(Index);
		}
	}
}

void UCombatTokenSubsystem::PruneQueue()
{
	const float Now = GetWorld()->GetTimeSeconds();

	// Drop anyone who stopped asking. If they still want a token they'll re-request
	// next time their BT branch ticks, which re-adds them.
	WaitQueue.RemoveAll([Now](const FTokenQueueEntry& Entry)
	{
		return !Entry.Requester.IsValid() || (Now - Entry.LastDenyTime) > 3.0f;
	});

	// Clean out expired lockouts so the map doesn't grow across a long fight.
	for (auto It = LockoutExpiry.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || Now > It.Value())
		{
			It.RemoveCurrent();
		}
	}
}

void UCombatTokenSubsystem::AddOrRefreshQueueEntry(AActor* Requester, ETokenRequestType Type)
{
	const float Now = GetWorld()->GetTimeSeconds();

	for (FTokenQueueEntry& Entry : WaitQueue)
	{
		if (Entry.Requester.Get() == Requester)
		{
			Entry.LastDenyTime = Now;
			Entry.Type = Type;
			return;
		}
	}

	FTokenQueueEntry NewEntry;
	NewEntry.Requester = Requester;
	NewEntry.Type = Type;
	NewEntry.FirstRequestTime = Now;
	NewEntry.LastDenyTime = Now;
	WaitQueue.Add(NewEntry);
}

bool UCombatTokenSubsystem::IsHighestPriorityWaiter(const AActor* Requester) const
{
	const float Now = GetWorld()->GetTimeSeconds();

	float LongestWait = -1.f;
	const AActor* LongestWaiter = nullptr;

	for (const FTokenQueueEntry& Entry : WaitQueue)
	{
		if (!Entry.Requester.IsValid())
		{
			continue;
		}

		const float Wait = Now - Entry.FirstRequestTime;
		if (Wait > LongestWait)
		{
			LongestWait = Wait;
			LongestWaiter = Entry.Requester.Get();
		}
	}

	// Nobody is starving yet, so first-come-first-served is fine and we don't
	// need to block anyone.
	if (LongestWait < StarvationPriorityTime)
	{
		return true;
	}

	return LongestWaiter == Requester;
}

bool UCombatTokenSubsystem::RequestToken(AActor* Requester, ETokenRequestType Type)
{
	if (!IsValid(Requester))
	{
		return false;
	}

	// Already holding? Return success so a re-entrant BT tick doesn't double-spend.
	if (HoldsToken(Requester))
	{
		return true;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	// Post-attack lockout. Without this, a fast enemy that finishes an attack
	// immediately grabs the token again and no other enemy ever gets a turn.
	if (const float* Expiry = LockoutExpiry.Find(Requester))
	{
		if (Now < *Expiry)
		{
			AddOrRefreshQueueEntry(Requester, Type);
			return false;
		}
	}

	const int32 Cost = GetCostForType(Type);

	if (GetAvailableBudget() < Cost || !IsHighestPriorityWaiter(Requester))
	{
		AddOrRefreshQueueEntry(Requester, Type);
		return false;
	}

	FTokenGrant Grant;
	Grant.Holder = Requester;
	Grant.Type = Type;
	Grant.Cost = Cost;
	Grant.GrantTime = Now;
	ActiveGrants.Add(Grant);

	WaitQueue.RemoveAll([Requester](const FTokenQueueEntry& Entry)
	{
		return Entry.Requester.Get() == Requester;
	});

	UE_LOG(LogGMTKAI, Verbose, TEXT("[Tokens] Granted %d to %s (%d/%d spent)"),
		Cost, *Requester->GetName(), GetSpentBudget(), CurrentBudget);

	return true;
}

void UCombatTokenSubsystem::ReleaseToken(AActor* Requester)
{
	if (!Requester)
	{
		return;
	}

	const int32 NumRemoved = ActiveGrants.RemoveAll([Requester](const FTokenGrant& Grant)
	{
		return Grant.Holder.Get() == Requester;
	});

	if (NumRemoved > 0)
	{
		LockoutExpiry.Add(Requester, GetWorld()->GetTimeSeconds() + PostAttackLockout);

		UE_LOG(LogGMTKAI, Verbose, TEXT("[Tokens] Released by %s (%d/%d spent)"),
			*Requester->GetName(), GetSpentBudget(), CurrentBudget);
	}
}

bool UCombatTokenSubsystem::HoldsToken(const AActor* Actor) const
{
	for (const FTokenGrant& Grant : ActiveGrants)
	{
		if (Grant.Holder.Get() == Actor)
		{
			return true;
		}
	}
	return false;
}

float UCombatTokenSubsystem::GetWaitTime(const AActor* Actor) const
{
	const float Now = GetWorld()->GetTimeSeconds();

	for (const FTokenQueueEntry& Entry : WaitQueue)
	{
		if (Entry.Requester.Get() == Actor)
		{
			return Now - Entry.FirstRequestTime;
		}
	}
	return 0.f;
}
