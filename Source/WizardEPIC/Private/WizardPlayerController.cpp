#include "WizardPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "CameraFX.h"
#include "Engine/World.h"
#include "WizardAbilityComponent.h"
#include "GameFramework/Pawn.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardPlayerController)

// Skeleton build: input bindings are implemented in the next step.

AWizardPlayerController::AWizardPlayerController()
{
	bShowMouseCursor = false;
}

void AWizardPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (PlayerCameraManager)
	{
		PlayerCameraManager->AddNewCameraModifier(UWizardCameraModifier::StaticClass());
	}
}

void AWizardPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
}

bool AWizardPlayerController::GetAim(FVector& OutPoint, FVector& OutDirection, float MaxRange) const
{
	return false;
}

void AWizardPlayerController::GetView(FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = PlayerCameraManager ? PlayerCameraManager->GetCameraLocation() : GetFocalLocation();
	OutRotation = PlayerCameraManager ? PlayerCameraManager->GetCameraRotation() : GetControlRotation();
}

void AWizardPlayerController::ToggleMouseCapture()
{
	bMouseCaptured = !bMouseCaptured;
	bShowMouseCursor = !bMouseCaptured;
}

UWizardAbilityComponent* AWizardPlayerController::GetAbilities() const
{
	APawn* ControlledPawn = GetPawn();
	return ControlledPawn ? ControlledPawn->FindComponentByClass<UWizardAbilityComponent>() : nullptr;
}

void AWizardPlayerController::OnCastPressed()
{
}

void AWizardPlayerController::OnCastReleased()
{
}

void AWizardPlayerController::OnSelectSlot(int32 SlotIndex)
{
}

void AWizardPlayerController::OnCycleSpell(int32 Delta)
{
}

void AWizardPlayerController::OnDashPressed()
{
}

void AWizardPlayerController::OnSprintPressed()
{
}

void AWizardPlayerController::OnSprintReleased()
{
}

void AWizardPlayerController::OnResetPressed()
{
}

void AWizardPlayerController::OnHintsPressed()
{
}
