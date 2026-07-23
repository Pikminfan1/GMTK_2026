#include "UI/BaseUserWidget.h"
#include "Core/BaseGameState.h"
#include "Characters/BaseCharacter.h"
#include "Kismet/GameplayStatics.h"

ABaseGameState* UBaseUserWidget::GetBaseGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<ABaseGameState>() : nullptr;
}

ABaseCharacter* UBaseUserWidget::GetOwningBaseCharacter() const
{
	return Cast<ABaseCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
}
