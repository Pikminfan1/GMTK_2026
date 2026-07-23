// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/HealthComponent.h"
#include "GameFramework/Character.h"
#include "Interfaces/Damageable.h"
#include "BaseCharacter.generated.h"


class UHealthComponent;

/**
 * Shared abstract base for the player and every enemy. Anything that applies to
 * "any character regardless of who controls it" goes here - health/damage handling
 * via IDamageable, and generic death handling. Camera, input, and AI-specific stuff
 * belong in PlayerCharacter/EnemyCharacter instead, not here.
 */
UCLASS(Abstract)
class GMTK_2026_API ABaseCharacter : public ACharacter, public IDamageable
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABaseCharacter();
	
	UFUNCTION(BlueprintPure, Category = "Health")
	UHealthComponent* GetHealthComponent() const { return HealthComponent; }
	
	//IDamageable interface
	virtual bool ApplyDamage_Implementation(float DamageAmount, AController* EventInstigator, AActor* DamageCauser) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	UFUNCTION(BlueprintNativeEvent, Category = "Health")
	void HandleDeath(AActor* DeadActor);
	virtual void HandleDeath_Implementation(AActor* DeadActor);
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHealthComponent> HealthComponent;

public:	
	// Called every frame
	//virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	//virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
