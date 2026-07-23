// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BaseGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScoreChanged, int32, NewScore);

/**
 * Holds the match data the UI actually reads - wave number, enemies remaining, score.
 * Kept separate from GameMode so UI widgets bind here instead of reaching into GameMode directly.
 */
UCLASS()
class GMTK_2026_API ABaseGameState : public AGameStateBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly, Category = "Match")
	int32 CurrentWaveNumber = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "Match")
	int32 EnemiesRemaining = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "Match")
	int32 Score = 0;
	
	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnScoreChanged OnScoreChanged;
	
	UFUNCTION(BlueprintCallable, Category = "Match")
	void AddScore(int32 Amount);
	
};
