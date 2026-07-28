#include "AI/Tokens/CombatTokenSubsystem.h"

#include "AI/Tokens/TokenHolder.h"
#include "Characters/Components/EnemyBeamComponentBase.h"
#include "Characters/Components/EnemyMeleeAttackComponent.h"
#include "Characters/Components/HealthComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
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

	if (bDrawDebugMarkers)
	{
		DrawDebugMarkers();
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
		TEXT("[Tokens] %d/%d spent | %d holders | %d waiting%s"),
		GetSpentBudget(), CurrentBudget, ActiveGrants.Num(), WaitQueue.Num(),
		bGrantsFrozen ? TEXT(" | FROZEN") : TEXT(""));
}

FString UCombatTokenSubsystem::TokenTypeToString(ETokenRequestType Type)
{
	const UEnum* EnumPtr = StaticEnum<ETokenRequestType>();
	return EnumPtr
		? EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(Type)).ToString()
		: TEXT("Unknown");
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

		if (bHolderGone)
		{
			ActiveGrants.RemoveAtSwap(Index);
		}
		else if (bHeldTooLong)
		{
#if !UE_BUILD_SHIPPING
			// If you ever see this, a Behavior Tree branch aborted without
			// running its ReleaseToken task. Find it - don't rely on this.
			UE_LOG(LogGMTKAI, Warning,
				TEXT("[Tokens] Force-reclaimed a token from %s after %.1fs. "
					 "A BT branch almost certainly aborted without releasing."),
				*Grant.Holder->GetName(), Now - Grant.GrantTime);
#endif
			// RevokeToken removes the grant, applies the lockout, and tells the
			// holder it lost the token so it can abort whatever it was doing.
			RevokeToken(Grant.Holder.Get(), ETokenRevokeReason::Reclaimed);
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

void UCombatTokenSubsystem::AddOrRefreshQueueEntry(AController* Requester, ETokenRequestType Type)
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

bool UCombatTokenSubsystem::IsHighestPriorityWaiter(const AController* Requester) const
{
	const float Now = GetWorld()->GetTimeSeconds();

	float LongestWait = -1.f;
	const AController* LongestWaiter = nullptr;

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

bool UCombatTokenSubsystem::IsOnPlayerScreen(const AController* Holder) const
{
	const APawn* HolderPawn = Holder ? Holder->GetPawn() : nullptr;
	const APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);

	// Fail open: with no camera to test against, treat the enemy as visible so a
	// missing player can never deadlock the whole director.
	if (!HolderPawn || !PC || !PC->PlayerCameraManager)
	{
		return true;
	}

	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CamFwd = PC->PlayerCameraManager->GetCameraRotation().Vector();

	FVector ToPawn = HolderPawn->GetActorLocation() - CamLoc;
	ToPawn.Normalize();

	// Dot of unit vectors = cosine of the angle between camera forward and the
	// enemy. A cone test is enough here - this biases pacing, it doesn't need
	// exact frustum culling or per-request occlusion traces.
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(OnScreenHalfAngleDegrees));
	return FVector::DotProduct(CamFwd, ToPawn) >= CosHalfAngle;
}

bool UCombatTokenSubsystem::HasOnScreenWaiter(const AController* Excluding) const
{
	for (const FTokenQueueEntry& Entry : WaitQueue)
	{
		const AController* Waiter = Entry.Requester.Get();
		if (Waiter && Waiter != Excluding && IsOnPlayerScreen(Waiter))
		{
			return true;
		}
	}
	return false;
}

bool UCombatTokenSubsystem::RequestToken(AController* Requester, ETokenRequestType Type)
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

	// Console freeze: park everyone in the queue. Existing holders keep theirs, so
	// the visible effect is every enemy dropping into fallback behaviour as their
	// current attacks finish.
	if (bGrantsFrozen)
	{
		AddOrRefreshQueueEntry(Requester, Type);
		return false;
	}

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

	// A starving requester bypasses the directional check below - fairness beats
	// cinematography once somebody has waited long enough.
	const bool bStarving = GetWaitTime(Requester) >= StarvationPriorityTime;

	// Directional coordination: if the player can't see this enemy but CAN see
	// someone else who's waiting, this enemy yields. Attacks the player can watch
	// coming feel fair; damage arriving from off-screen feels cheap.
	if (!bStarving && bPreferOnScreenAttackers
		&& !IsOnPlayerScreen(Requester) && HasOnScreenWaiter(Requester))
	{
		AddOrRefreshQueueEntry(Requester, Type);
		return false;
	}

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

	NotifyGranted(Requester, Type);

	UE_LOG(LogGMTKAI, Verbose, TEXT("[Tokens] Granted %d to %s (%d/%d spent)"),
		Cost, *Requester->GetName(), GetSpentBudget(), CurrentBudget);

	return true;
}

void UCombatTokenSubsystem::ReleaseToken(AController* Requester)
{
	RevokeToken(Requester, ETokenRevokeReason::Released);
}

void UCombatTokenSubsystem::RevokeToken(AController* Holder, ETokenRevokeReason Reason)
{
	if (!Holder)
	{
		return;
	}

	const int32 NumRemoved = ActiveGrants.RemoveAll([Holder](const FTokenGrant& Grant)
	{
		return Grant.Holder.Get() == Holder;
	});

	if (NumRemoved > 0)
	{
		LockoutExpiry.Add(Holder, GetWorld()->GetTimeSeconds() + PostAttackLockout);

		// Push the revoke through the interface. On a steal or reclaim the holder
		// uses this to abort its in-flight attack the same frame.
		if (Holder->Implements<UTokenHolder>())
		{
			ITokenHolder::Execute_OnTokenRevoked(Holder, Reason);
		}

		UE_LOG(LogGMTKAI, Verbose, TEXT("[Tokens] Revoked from %s (%d/%d spent)"),
			*Holder->GetName(), GetSpentBudget(), CurrentBudget);
	}
}

void UCombatTokenSubsystem::NotifyGranted(AController* Holder, ETokenRequestType Type) const
{
	if (Holder && Holder->Implements<UTokenHolder>())
	{
		ITokenHolder::Execute_OnTokenGranted(Holder, Type);
	}
}

bool UCombatTokenSubsystem::HoldsToken(const AController* Holder) const
{
	for (const FTokenGrant& Grant : ActiveGrants)
	{
		if (Grant.Holder.Get() == Holder)
		{
			return true;
		}
	}
	return false;
}

float UCombatTokenSubsystem::GetWaitTime(const AController* Requester) const
{
	const float Now = GetWorld()->GetTimeSeconds();

	for (const FTokenQueueEntry& Entry : WaitQueue)
	{
		if (Entry.Requester.Get() == Requester)
		{
			return Now - Entry.FirstRequestTime;
		}
	}
	return 0.f;
}

bool UCombatTokenSubsystem::IsHolderAttackBusy(const AController* Holder) const
{
	const APawn* HolderPawn = Holder ? Holder->GetPawn() : nullptr;
	if (!HolderPawn)
	{
		return false;
	}

	// An attack is "busy" while its component is anywhere in its wind-up / active /
	// cooldown cycle. Beam attacks (laser, heal) share one base class; melee has its
	// own component. A pawn with none of these has nothing threatening in progress,
	// so its token is fair game.
	TInlineComponentArray<UEnemyBeamComponentBase*> Beams(HolderPawn);
	for (const UEnemyBeamComponentBase* Beam : Beams)
	{
		if (Beam && Beam->IsBusy())
		{
			return true;
		}
	}

	if (const UEnemyMeleeAttackComponent* Melee = HolderPawn->FindComponentByClass<UEnemyMeleeAttackComponent>())
	{
		if (Melee->IsBusy())
		{
			return true;
		}
	}

	return false;
}

int32 UCombatTokenSubsystem::FindIdleHolderIndex(const AController* Excluding, bool bOffScreenOnly) const
{
	for (int32 i = 0; i < ActiveGrants.Num(); ++i)
	{
		const AController* Holder = ActiveGrants[i].Holder.Get();
		if (!Holder || Holder == Excluding || IsHolderAttackBusy(Holder))
		{
			continue;
		}
		if (bOffScreenOnly && IsOnPlayerScreen(Holder))
		{
			continue;
		}
		return i;
	}
	return INDEX_NONE;
}

bool UCombatTokenSubsystem::TryStealTokenFor(AController* DamagedController, ETokenRequestType Type)
{
	if (!IsValid(DamagedController))
	{
		return false;
	}

	// Already armed - nothing to steal.
	if (HoldsToken(DamagedController))
	{
		return true;
	}

	// Find a current holder that isn't mid-attack. Prefer one the player cannot
	// see: robbing an on-screen enemy makes something the player is watching
	// visibly give up its attack, while an off-screen victim just quietly rejoins
	// the queue. Fall back to any idle holder.
	int32 VictimIndex = FindIdleHolderIndex(DamagedController, /*bOffScreenOnly*/ true);
	if (VictimIndex == INDEX_NONE)
	{
		VictimIndex = FindIdleHolderIndex(DamagedController, /*bOffScreenOnly*/ false);
	}

	if (VictimIndex == INDEX_NONE)
	{
		// Everyone holding a token is actively attacking - don't interrupt them. The
		// damaged enemy will still pursue; it just doesn't get to attack this instant.
		return false;
	}

	AController* Victim = ActiveGrants[VictimIndex].Holder.Get();

	// Take the victim's token (this notifies it so its BT stops trying to attack),
	// then hand a fresh grant to the controller that was damaged.
	RevokeToken(Victim, ETokenRevokeReason::Stolen);

	FTokenGrant Grant;
	Grant.Holder = DamagedController;
	Grant.Type = Type;
	Grant.Cost = GetCostForType(Type);
	Grant.GrantTime = GetWorld()->GetTimeSeconds();
	ActiveGrants.Add(Grant);

	// Clear any lockout on the damaged controller so it can act on the stolen token now.
	LockoutExpiry.Remove(DamagedController);

	// Drop it from the wait queue if it was waiting.
	WaitQueue.RemoveAll([DamagedController](const FTokenQueueEntry& Entry)
	{
		return Entry.Requester.Get() == DamagedController;
	});

	NotifyGranted(DamagedController, Type);

	UE_LOG(LogGMTKAI, Verbose, TEXT("[Tokens] %s stole a token from idle holder %s"),
		*DamagedController->GetName(), Victim ? *Victim->GetName() : TEXT("?"));

	return true;
}

// ---------------------------------------------------------------------------
// Debug drawing + console
// ---------------------------------------------------------------------------

void UCombatTokenSubsystem::DrawDebugMarkers() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	const FVector TextOffset(0.f, 0.f, 120.f);   // float the label above the head

	// Holders: green, showing type and hold time (a climbing number that never
	// resets is a token leak in progress).
	for (const FTokenGrant& Grant : ActiveGrants)
	{
		const AController* Holder = Grant.Holder.Get();
		const APawn* HolderPawn = Holder ? Holder->GetPawn() : nullptr;
		if (!HolderPawn)
		{
			continue;
		}

		DrawDebugString(World, HolderPawn->GetActorLocation() + TextOffset,
			FString::Printf(TEXT("TOKEN %s  %.1fs"),
				*TokenTypeToString(Grant.Type), Now - Grant.GrantTime),
			nullptr, FColor::Green, 0.f, /*bDrawShadow*/ true);
	}

	// Waiters: yellow, turning red once they cross the starvation threshold so a
	// stuck queue is visible at a glance.
	for (const FTokenQueueEntry& Entry : WaitQueue)
	{
		const AController* Waiter = Entry.Requester.Get();
		const APawn* WaiterPawn = Waiter ? Waiter->GetPawn() : nullptr;
		if (!WaiterPawn)
		{
			continue;
		}

		const float Wait = Now - Entry.FirstRequestTime;
		const bool bStarving = Wait >= StarvationPriorityTime;
		DrawDebugString(World, WaiterPawn->GetActorLocation() + TextOffset,
			FString::Printf(TEXT("WAIT %.1fs%s"), Wait,
				bStarving ? TEXT("  (PRIORITY)") : TEXT("")),
			nullptr, bStarving ? FColor::Red : FColor::Yellow, 0.f, true);
	}

	// Lockouts: silver countdown until the enemy may request again.
	for (const auto& Pair : LockoutExpiry)
	{
		const AController* Locked = Pair.Key.Get();
		const APawn* LockedPawn = Locked ? Locked->GetPawn() : nullptr;
		if (!LockedPawn || Now >= Pair.Value)
		{
			continue;
		}

		DrawDebugString(World, LockedPawn->GetActorLocation() + TextOffset,
			FString::Printf(TEXT("LOCKOUT %.1fs"), Pair.Value - Now),
			nullptr, FColor::Silver, 0.f, true);
	}
#endif
}

void UCombatTokenSubsystem::ToggleDebugDraw()
{
	const bool bEnable = !(bDrawDebugHUD || bDrawDebugMarkers);
	bDrawDebugHUD = bEnable;
	bDrawDebugMarkers = bEnable;

	UE_LOG(LogGMTKAI, Log, TEXT("[Tokens] Debug draw %s"), bEnable ? TEXT("ON") : TEXT("OFF"));
}

void UCombatTokenSubsystem::ToggleFreeze()
{
	bGrantsFrozen = !bGrantsFrozen;
	UE_LOG(LogGMTKAI, Log, TEXT("[Tokens] Grants %s"), bGrantsFrozen ? TEXT("FROZEN") : TEXT("unfrozen"));
}

void UCombatTokenSubsystem::SetMaxBudgetOverride(int32 NewMaxBudget)
{
	MaxBudget = FMath::Max(0, NewMaxBudget);
	UpdateBudget();
	UE_LOG(LogGMTKAI, Log, TEXT("[Tokens] MaxBudget set to %d (current budget %d)"), MaxBudget, CurrentBudget);
}

void UCombatTokenSubsystem::DumpState() const
{
	const float Now = GetWorld()->GetTimeSeconds();

	UE_LOG(LogGMTKAI, Log, TEXT("%s"), *GetDebugString());

	for (const FTokenGrant& Grant : ActiveGrants)
	{
		UE_LOG(LogGMTKAI, Log, TEXT("  HOLDER  %-40s %s cost=%d held=%.1fs"),
			Grant.Holder.IsValid() ? *Grant.Holder->GetName() : TEXT("<stale>"),
			*TokenTypeToString(Grant.Type), Grant.Cost, Now - Grant.GrantTime);
	}

	for (const FTokenQueueEntry& Entry : WaitQueue)
	{
		UE_LOG(LogGMTKAI, Log, TEXT("  WAITER  %-40s %s waiting=%.1fs"),
			Entry.Requester.IsValid() ? *Entry.Requester->GetName() : TEXT("<stale>"),
			*TokenTypeToString(Entry.Type), Now - Entry.FirstRequestTime);
	}

	for (const auto& Pair : LockoutExpiry)
	{
		if (Pair.Key.IsValid() && Now < Pair.Value)
		{
			UE_LOG(LogGMTKAI, Log, TEXT("  LOCKOUT %-40s %.1fs remaining"),
				*Pair.Key->GetName(), Pair.Value - Now);
		}
	}
}

#if !UE_BUILD_SHIPPING

// Console commands for tuning and demonstrating the director live in PIE.
// All of them resolve the subsystem from the world the command runs in, so they
// work in every PIE session without any setup.

static UCombatTokenSubsystem* GetTokenSubsystem(UWorld* World)
{
	return World ? World->GetSubsystem<UCombatTokenSubsystem>() : nullptr;
}

static FAutoConsoleCommandWithWorldAndArgs GTokensDebugCmd(
	TEXT("gmtk.Tokens.Debug"),
	TEXT("Toggle the combat-token debug HUD and per-enemy world markers."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>&, UWorld* World)
		{
			if (UCombatTokenSubsystem* Tokens = GetTokenSubsystem(World))
			{
				Tokens->ToggleDebugDraw();
			}
		}));

static FAutoConsoleCommandWithWorldAndArgs GTokensSetMaxBudgetCmd(
	TEXT("gmtk.Tokens.SetMaxBudget"),
	TEXT("gmtk.Tokens.SetMaxBudget <N> - override the healthy-player token budget."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			UCombatTokenSubsystem* Tokens = GetTokenSubsystem(World);
			if (Tokens && Args.Num() > 0)
			{
				Tokens->SetMaxBudgetOverride(FCString::Atoi(*Args[0]));
			}
		}));

static FAutoConsoleCommandWithWorldAndArgs GTokensFreezeCmd(
	TEXT("gmtk.Tokens.Freeze"),
	TEXT("Toggle freezing all new token grants (current holders keep theirs)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>&, UWorld* World)
		{
			if (UCombatTokenSubsystem* Tokens = GetTokenSubsystem(World))
			{
				Tokens->ToggleFreeze();
			}
		}));

static FAutoConsoleCommandWithWorldAndArgs GTokensDumpCmd(
	TEXT("gmtk.Tokens.Dump"),
	TEXT("Log every current token grant, waiter, and lockout."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>&, UWorld* World)
		{
			if (UCombatTokenSubsystem* Tokens = GetTokenSubsystem(World))
			{
				Tokens->DumpState();
			}
		}));

#endif // !UE_BUILD_SHIPPING
