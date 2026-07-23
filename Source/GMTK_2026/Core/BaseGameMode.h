// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Spawning/Components/WaveManagerComponent.h"
#include "BaseGameMode.generated.h"

class UWaveManagerComponent;
/**
 * Owns overall match flow. Every wave/spawn decision routes through this class -
 * it creates and owns the WaveManagerComponent and kicks off the first wave.
 */
UCLASS()
class GMTK_2026_API ABaseGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	ABaseGameMode();
	
	UFUNCTION(BlueprintPure, Category = "Waves")
	UWaveManagerComponent* GetWaveManager() const { return WaveManagerComponent; }
	
protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waves", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWaveManagerComponent> WaveManagerComponent;
};
