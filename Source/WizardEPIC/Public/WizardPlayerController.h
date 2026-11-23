// WizardPlayerController.h - owns the input bindings (cast, spell selection, dash, reset) and
// forwards them to the pawn's UWizardAbilityComponent. Also owns aiming and camera feel.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "WizardPlayerController.generated.h"

class UWizardAbilityComponent;

UCLASS()
class WIZARDEPIC_API AWizardPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AWizardPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/** Screen-centre aim point and direction along the camera ray (first blocking hit, else a far point). */
	UFUNCTION(BlueprintCallable, Category = Wizard)
	bool GetAim(FVector& OutPoint, FVector& OutDirection, float MaxRange = 20000.f) const;

	/** Camera location and rotation of this frame. */
	void GetView(FVector& OutLocation, FRotator& OutRotation) const;

	UFUNCTION(BlueprintCallable, Category = Wizard)
	void ToggleMouseCapture();

	UFUNCTION(BlueprintPure, Category = Wizard)
	UWizardAbilityComponent* GetAbilities() const;

	// ----- Input handlers (bound in SetupInputComponent) -----
	void OnCastPressed();
	void OnCastReleased();
	void OnSelectSlot(int32 SlotIndex);
	void OnCycleSpell(int32 Delta);
	void OnDashPressed();
	void OnSprintPressed();
	void OnSprintReleased();
	void OnResetPressed();
	void OnHintsPressed();

private:
	/** True while the mouse is captured for gameplay. */
	bool bMouseCaptured = true;
};
