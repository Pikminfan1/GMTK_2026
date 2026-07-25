// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthChanged, float, NewHealth, float, MaxHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeath, AActor*, DeadActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealed, float, AmountHealed, float, NewHealth);

/**
 * Tracks current/max health and death state for anything that can take damage.
 * Attached by ABaseCharacter's constructor, so both the player and enemies get one
 * automatically - a single place for damage/death logic to live rather than duplicating
 * it across two unrelated character hierarchies.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class GMTK_2026_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UHealthComponent();
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	float MaxHealth = 100.f;
	
	UPROPERTY(EditAnywhere, Category = "Health")
	float CurrentHealth = 0.f;
	
	/* Broadcast health changes, for UI SFX VFX etc */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChanged OnHealthChanged;
	
	/* Broadcast death, for ragdoll / death animation / VFX etc */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDeath OnDeath;

	/* Broadcast specifically when healed (not on damage), for pickup VFX/SFX. */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealed OnHealed;
	
	/** Apply damage to this component's owner. Return true if the damage was actually applied. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	bool TakeDamage(float DamageAmount, AController* EventInstigator, AActor* DamageCauserser);
	
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Heal(float HealAmount);
	
	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bIsDead; }
	
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const { return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f; }

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
private:
	UPROPERTY()
	bool bIsDead = false;

	/*
public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		*/
};
