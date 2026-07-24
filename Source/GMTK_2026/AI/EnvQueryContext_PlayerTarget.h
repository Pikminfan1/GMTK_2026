// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryContext_PlayerTarget.generated.h"

/**
 * Returns the local player pawn as an EQS context. Functionally the same as the
 * engine template's version, but fails soft when there is no player (e.g. an
 * EQSTestingPawn running a query before player 0 is possessed) instead of
 * asserting and taking down the editor.
 */
UCLASS()
class GMTK_2026_API UEnvQueryContext_PlayerTarget : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};