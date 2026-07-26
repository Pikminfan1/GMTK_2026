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
 *
 * Spawn sequence: on spawn the enemy starts in a "spawning" state with its AI paused.
 * OnSpawnSequenceStarted fires for the Blueprint to play the spawn-in animation and
 * particle effect. When that animation finishes, the Blueprint calls FinishSpawn(),
 * which resumes the AI so the enemy begins acting.
 */
UCLASS()
class GMTK_2026_API AEnemyCharacter : public ABaseCharacter
{
	GENERATED_BODY()
	
public:
	AEnemyCharacter();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "AI")
	AEnemyAIController* GetEnemyAIController() const;
	
	UFUNCTION(BlueprintCallable, Category = "AI")
	float SetMovementSpeed(EEnemyMovementSpeed NewSpeed);

	UFUNCTION(BlueprintPure, Category = "Orbs")
	UHealthOrbSpawnerComponent* GetOrbSpawner() const { return OrbSpawner; }

	// ---------- Spawn sequence ----------

	/** True from spawn until FinishSpawn() is called. While true, the AI is paused. */
	UFUNCTION(BlueprintPure, Category = "Spawn")
	bool IsSpawning() const { return bIsSpawning; }

	/** Call from Blueprint when the spawn-in animation finishes. Resumes the AI so the
	 *  enemy starts acting. Safe to call once; further calls are ignored. */
	UFUNCTION(BlueprintCallable, Category = "Spawn")
	void FinishSpawn();

	/** Fires once on spawn (after possession) so the Blueprint can play the spawn-in
	 *  animation + particle effect. When the animation completes, the Blueprint should
	 *  call FinishSpawn(). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Spawn")
	void OnSpawnSequenceStarted();

	/** Fires once when FinishSpawn() runs, in case the Blueprint wants to react
	 *  (e.g. stop a looping spawn VFX). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Spawn")
	void OnSpawnSequenceFinished();

	/** Begins the spawn sequence: pauses AI, disables movement, fires the BP event.
	 *  Called by the AI controller's OnPossess after the Behavior Tree has started, so
	 *  the pause isn't immediately undone. Returns early if the sequence is disabled or
	 *  already ran. */
	void BeginSpawnSequence();

	/** Whether this archetype uses the spawn sequence. Read by the controller. */
	bool UsesSpawnSequence() const { return bUseSpawnSequence; }

protected:
	/** If true, the enemy plays the spawn sequence with AI paused. Set false on an
	 *  archetype that should just appear and act immediately. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawn")
	bool bUseSpawnSequence = true;

	/** Safety: if the Blueprint never calls FinishSpawn (e.g. no anim wired), auto-finish
	 *  after this many seconds so the enemy can't stay frozen forever. 0 = no failsafe. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawn")
	float SpawnFailsafeTime = 5.f;

	/** Drops health orbs when this enemy dies. Tuned per Blueprint archetype. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orbs", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHealthOrbSpawnerComponent> OrbSpawner;

	/** Override to drop orbs on death, then chain to the base death handling. */
	virtual void HandleDeath_Implementation(AActor* DeadActor) override;

private:
	bool bIsSpawning = false;
	bool bSpawnFinished = false;

	FTimerHandle SpawnFailsafeTimer;
};
