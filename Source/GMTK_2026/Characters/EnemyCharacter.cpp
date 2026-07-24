// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/EnemyCharacter.h"
#include "AI/EnemyAIController.h"
#include "GameFramework/CharacterMovementComponent.h"

AEnemyCharacter::AEnemyCharacter()
{
	// Intentionally minimal - mesh, stats, and Behavior Tree assignment happen per
	// enemy-archetype Blueprint, not here.
}

AEnemyAIController* AEnemyCharacter::GetEnemyAIController() const
{
	return Cast<AEnemyAIController>(GetController());
}

float AEnemyCharacter::SetMovementSpeed(EEnemyMovementSpeed SpeedType)
{
	if (GetCharacterMovement())
	{
		float NewSpeed = 0.f;
		switch (SpeedType)
			{
			case EEnemyMovementSpeed::Idle:
				NewSpeed = 0.f;
				break;
			case EEnemyMovementSpeed::Walking:
				NewSpeed = 200.f;
				break;
			case EEnemyMovementSpeed::Jogging:
				NewSpeed = 400.f;
				break;
			case EEnemyMovementSpeed::Sprinting:
				NewSpeed = 600.f;
				break;
			default:
				UE_LOG(LogTemp, Warning, TEXT("Unknown EEnemyMovementSpeed value: %d"), static_cast<int32>(SpeedType));
				break;
			}
		GetCharacterMovement()->MaxWalkSpeed = NewSpeed;
		return NewSpeed;
	}
	return 0.f;
}
