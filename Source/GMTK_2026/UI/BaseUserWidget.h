// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BaseUserWidget.generated.h"

/**
 * Common C++ base for all widget Blueprints (HUD, menus, wave-complete screens, etc.)
 * so teammates building widgets inherit shared helpers instead of reinventing them per-widget.
 */
UCLASS()
class GMTK_2026_API UBaseUserWidget : public UUserWidget
{
	GENERATED_BODY()
	
	
public:
	UFUNCTION(BlueprintPure, Category = "UI")
	ABaseGameState* GetBaseGameState() const;

	UFUNCTION(BlueprintPure, Category = "UI")
	ABaseCharacter* GetOwningBaseCharacter() const;
};

