#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AI/Tokens/TokenTypes.h"
#include "TokenHolder.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UTokenHolder : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by anything that can hold a combat token - in practice, AI controllers.
 *
 * The token subsystem PUSHES grant/revoke events through this interface instead of
 * holders polling HoldsToken() every Behavior Tree tick. The payoff is on steals:
 * the victim finds out the same frame its token is taken and can abort its attack
 * (and stop its montage) immediately, rather than playing out a wind-up it no
 * longer has permission to finish.
 */
class GMTK_2026_API ITokenHolder
{
	GENERATED_BODY()

public:
	/** The subsystem granted this holder a token, via a request or a steal. */
	UFUNCTION(BlueprintNativeEvent, Category = "Combat Tokens")
	void OnTokenGranted(ETokenRequestType Type);

	/** This holder no longer has a token. See ETokenRevokeReason for why. */
	UFUNCTION(BlueprintNativeEvent, Category = "Combat Tokens")
	void OnTokenRevoked(ETokenRevokeReason Reason);
};
