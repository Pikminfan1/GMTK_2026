#include "UI/BaseHUD.h"
#include "UI/BaseUserWidget.h"
#include "Utility/LogChannels.h"

void ABaseHUD::BeginPlay()
{
	Super::BeginPlay();

	if (MainHUDWidgetClass)
	{
		MainHUDWidgetInstance = CreateWidget<UBaseUserWidget>(GetOwningPlayerController(), MainHUDWidgetClass);
		if (MainHUDWidgetInstance)
		{
			MainHUDWidgetInstance->AddToViewport();
		}
	}
	else
	{
		UE_LOG(LogGMTKCore, Warning, TEXT("%s has no MainHUDWidgetClass assigned"), *GetName());
	}
}