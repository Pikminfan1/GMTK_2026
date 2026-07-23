// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Damageable.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI, Blueprintable)
class UDamageable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implement this on any actor that can take damage (characters, destructible props, etc.).
 * Weapons and projectiles should only ever talk to actors through this interface - they
 * should never need to know or care whether they hit the player or an enemy.
 */
class GMTK_2026_API IDamageable
{
	GENERATED_BODY()

public:
	/** Apply damage to this actor. Return true if the damage was actually applied. */
	UFUNCTION(BlueprintNativeEvent, Category = "Damage")
	bool ApplyDamage(float DamageAmount, AController* EventInstigator, AActor* DamageCauser);
};
