// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/BasePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Utility/LogChannels.h"
#include "Utility/CombatTeams.h"

FGenericTeamId ABasePlayerController::GetGenericTeamId() const
{
	return CombatTeams::Player;
}

void ABasePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, MappingPriority);
		}
	}
	else
	{
		UE_LOG(LogGMTKCore, Warning, TEXT("%s could not find EnhancedInputLocalPlayerSubsystem to add mapping context"), *GetName());
	}
}

