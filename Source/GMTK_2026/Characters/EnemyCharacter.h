// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/BaseCharacter.h"
#include "EnemyCharacter.generated.h"

class AEnemyAIController;
enum class EEnemyMovementSpeed : uint8;
class UHealthOrbSpawnerComponent;

/**
 * The one and only C++ enemy class. Different enemy archetypes (grunt, heavy, etc.)
 * should be Blueprint subclasses of THIS class with different meshes/stats/Behavior
 * Trees - not new C++ classes - so teammates without C++ access can add enemy types.
 *
 * Every enemy carries a HealthOrbSpawnerComponent so it drops health on death by
 * default. A Blueprint archetype tunes the drop (or sets the count to 0 to drop
 * nothing) without needing to add or remove the component.
 */
UCLASS()
class GMTK_2026_API AEnemyCharacter : public ABaseCharacter
{
	GENERATED_BODY()
	
public:
	AEnemyCharacter();

	UFUNCTION(BlueprintPure, Category = "AI")
	AEnemyAIController* GetEnemyAIController() const;
	
	UFUNCTION(BlueprintCallable, Category = "AI")
	float SetMovementSpeed(EEnemyMovementSpeed NewSpeed);

	UFUNCTION(BlueprintPure, Category = "Orbs")
	UHealthOrbSpawnerComponent* GetOrbSpawner() const { return OrbSpawner; }

protected:
	/** Drops health orbs when this enemy dies. Tuned per Blueprint archetype. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orbs", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHealthOrbSpawnerComponent> OrbSpawner;

	/** Override to drop orbs on death, then chain to the base death handling. */
	virtual void HandleDeath_Implementation(AActor* DeadActor) override;
};
