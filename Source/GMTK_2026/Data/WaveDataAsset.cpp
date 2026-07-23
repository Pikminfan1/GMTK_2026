#include "Data/WaveDataAsset.h"

FPrimaryAssetId UWaveDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType("Wave"), GetFName());
}