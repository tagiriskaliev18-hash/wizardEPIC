#include "WizardHUD.h"

#include "Blueprint/UserWidget.h"
#include "HUDWidgets.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardHUD)

void AWizardHUD::BeginPlay()
{
	Super::BeginPlay();

	if (RootWidget)
	{
		return;
	}

	APlayerController* OwningPC = GetOwningPlayerController();
	if (!OwningPC)
	{
		return;
	}

	RootWidget = CreateWidget<UWizardHUDWidget>(OwningPC, UWizardHUDWidget::StaticClass());
	if (RootWidget)
	{
		RootWidget->AddToViewport(0);
	}
	else
	{
		UE_LOG(LogWizard, Error, TEXT("Wizard HUD widget could not be created"));
	}
}

void AWizardHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (RootWidget)
	{
		RootWidget->RemoveFromParent();
		RootWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}
