// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Components/HealthComponent.h"
#include "Utility/LogChannels.h"

// Sets default values for this component's properties
UHealthComponent::UHealthComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;

	// ...
	
}

bool UHealthComponent::TakeDamage(float DamageAmount, AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDead || DamageAmount <= 0.f)
	{
		return false;
	}

	const float OldHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.f, MaxHealth);
	const float Delta = CurrentHealth - OldHealth;

	UE_LOG(LogGMTKCombat, Verbose, TEXT("%s took %.1f damage (%.1f -> %.1f)"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"), DamageAmount, OldHealth, CurrentHealth);
	
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, Delta);

	if (CurrentHealth <= 0.f && !bIsDead)
	{
		bIsDead = true;
		OnDeath.Broadcast(GetOwner());
	}

	return true;
}

void UHealthComponent::Heal(float HealAmount)
{
	if (bIsDead || HealAmount <= 0.f)
	{
		return;
	}

	const float OldHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth + HealAmount, 0.f, MaxHealth);
	const float Delta = CurrentHealth - OldHealth;

	UE_LOG(LogGMTKCombat, Verbose, TEXT("%s healed %.1f health (%.1f -> %.1f)"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"), HealAmount, OldHealth, CurrentHealth);

	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, Delta);
}


// Called every frame
/*
void UHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}
*/

