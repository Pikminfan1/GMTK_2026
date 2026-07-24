// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/BaseCharacter.h"
#include "EnemyCharacter.generated.h"

/**
 * The one and only C++ enemy class. Different enemy archetypes (grunt, heavy, etc.)
 * should be Blueprint subclasses of THIS class with different meshes/stats/Behavior
 * Trees - not new C++ classes - so teammates without C++ access can add enemy types.
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
};
