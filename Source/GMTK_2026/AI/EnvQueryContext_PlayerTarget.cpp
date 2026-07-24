// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/EnvQueryContext_PlayerTarget.h"
#include "Kismet/GameplayStatics.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "GameFramework/Pawn.h"

void UEnvQueryContext_PlayerTarget::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	AActor* PlayerPawn = UGameplayStatics::GetPlayerPawn(QueryInstance.Owner.Get(), 0);

	// No player yet (testing pawn, level transition) - return an empty context
	// rather than asserting.
	if (!PlayerPawn)
	{
		return;
	}

	UEnvQueryItemType_Actor::SetContextHelper(ContextData, PlayerPawn);
}