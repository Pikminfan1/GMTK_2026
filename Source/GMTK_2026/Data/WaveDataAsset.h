// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WaveDataAsset.generated.h"

class AEnemyCharacter;

USTRUCT(BlueprintType)
struct FWaveEnemyEntry
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	TSubclassOf<AEnemyCharacter> EnemyClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	int32 Count;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	float SpawnInterval = 0.5f;
};

/**
 * Designer-facing wave definition. UPrimaryDataAsset (rather than plain UDataAsset) so
 * the Asset Manager can auto-discover every wave asset that exists once you register
 * the "Wave" Primary Asset Type under Project Settings > Asset Manager.
 */
UCLASS()
class GMTK_2026_API UWaveDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	TArray<FWaveEnemyEntry> EnemyEntries;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	float DelayBeforeWave = 3.f;
	
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	
};
