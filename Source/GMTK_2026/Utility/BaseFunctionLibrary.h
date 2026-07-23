#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BaseFunctionLibrary.generated.h"

class ABaseCharacter;

/** Static BlueprintCallable helpers shared across widgets/Blueprints. */
UCLASS()
class GMTK_2026_API UBaseFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GMTK", meta = (WorldContext = "WorldContextObject"))
	static ABaseCharacter* GetPlayerBaseCharacter(const UObject* WorldContextObject, int32 PlayerIndex = 0);
};
