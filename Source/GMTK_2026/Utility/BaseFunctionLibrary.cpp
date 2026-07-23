#include "Utility/BaseFunctionLibrary.h"
#include "Characters/BaseCharacter.h"
#include "Kismet/GameplayStatics.h"

ABaseCharacter* UBaseFunctionLibrary::GetPlayerBaseCharacter(const UObject* WorldContextObject, int32 PlayerIndex)
{
	return Cast<ABaseCharacter>(UGameplayStatics::GetPlayerCharacter(WorldContextObject, PlayerIndex));
}
