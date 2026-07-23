// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/EnemyCharacter.h"
#include "AI/EnemyAIController.h"

AEnemyCharacter::AEnemyCharacter()
{
	// Intentionally minimal - mesh, stats, and Behavior Tree assignment happen per
	// enemy-archetype Blueprint, not here.
}

AEnemyAIController* AEnemyCharacter::GetEnemyAIController() const
{
	return Cast<AEnemyAIController>(GetController());
}