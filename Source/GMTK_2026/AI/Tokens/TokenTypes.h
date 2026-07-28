#pragma once

#include "CoreMinimal.h"
#include "TokenTypes.generated.h"

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

/**
 * Why a holder lost its token. Passed to ITokenHolder::OnTokenRevoked so the holder
 * can react proportionally: a voluntary release needs nothing beyond bookkeeping,
 * but a steal or reclaim means the holder may be mid-attack and must abort
 * immediately rather than finishing a swing it no longer has permission for.
 */
UENUM(BlueprintType)
enum class ETokenRevokeReason : uint8
{
	/** The holder gave the token back itself - the normal end of an attack. */
	Released,

	/** Another enemy force-took the token (the damage-driven steal mechanic). */
	Stolen,

	/** The subsystem reclaimed a token held past MaxTokenHoldTime. */
	Reclaimed
};
