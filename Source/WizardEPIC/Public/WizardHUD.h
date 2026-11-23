// WizardHUD.h - creates the C++-built HUD widget.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "WizardHUD.generated.h"

class UWizardHUDWidget;

UCLASS()
class WIZARDEPIC_API AWizardHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(Transient, BlueprintReadOnly, Category = Wizard)
	TObjectPtr<UWizardHUDWidget> RootWidget = nullptr;
};
