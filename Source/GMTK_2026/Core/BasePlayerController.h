// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "BasePlayerController.generated.h"

class UInputMappingContext;

/**
 * Applies the Enhanced Input Mapping Context on possession and is the home for
 * pause/menu input handling later.
 */
UCLASS()
class GMTK_2026_API ABasePlayerController : public APlayerController , public IGenericTeamAgentInterface
{
	GENERATED_BODY()
public:
	//~ Begin IGenericTeamAgentInterface
	virtual FGenericTeamId GetGenericTeamId() const override;
	//~ End IGenericTeamAgentInterface
protected:
	virtual void BeginPlay() override;

	// Assign in the Class Defaults / a Blueprint subclass once you've created your
	// Input Mapping Context asset via Project Settings > Input (Enhanced Input).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input")
	int32 MappingPriority = 0;
};
