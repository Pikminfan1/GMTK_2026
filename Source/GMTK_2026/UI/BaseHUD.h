// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BaseHUD.generated.h"

class UBaseUserWidget;

/** Thin - just creates and holds the main HUD widget. The real UI logic lives in the widgets themselves. */
UCLASS()
class GMTK_2026_API ABaseHUD : public AHUD
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UBaseUserWidget> MainHUDWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	TObjectPtr<UBaseUserWidget> MainHUDWidgetInstance;

protected:
	virtual void BeginPlay() override;
};
