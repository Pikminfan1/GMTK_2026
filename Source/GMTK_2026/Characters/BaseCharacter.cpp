// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/BaseCharacter.h"
#include "Characters/Components/HealthComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Utility/LogChannels.h"


// Sets default values
ABaseCharacter::ABaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
}

// Called when the game starts or when spawned
void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &ABaseCharacter::HandleDeath);
	}
	
}

bool ABaseCharacter::ApplyDamage_Implementation(float DamageAmount, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HealthComponent)
	{
		return false;
	}
	
	return HealthComponent->TakeDamage(DamageAmount, EventInstigator, DamageCauser);
}

void ABaseCharacter::HandleDeath_Implementation(AActor* DeadActor)
{
	UE_LOG(LogGMTKCombat, Log, TEXT("%s has died."), *GetName());
	
	// Disable movement and collision
	if (UCharacterMovementComponent* CharacterMovementComponent = GetCharacterMovement())
	{
		CharacterMovementComponent->DisableMovement();
	}
	SetActorEnableCollision(false);
	
	// TODO: ragdoll / death animation / VFX hook goes here once you've decided how
	// death should look per character type (this is a good candidate for an override
	// in PlayerCharacter vs EnemyCharacter rather than putting it all here).
}
/*
// Called every frame
void ABaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

*/