#include "CameraFX.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(CameraFX)

// Skeleton build: presets are tuned in the next step.

UWizardCameraShake::UWizardCameraShake()
{
	bSingleInstance = true;
}

UWizardShakeExplosion::UWizardShakeExplosion()
{
}

UWizardShakeRumble::UWizardShakeRumble()
{
}

UWizardShakeWind::UWizardShakeWind()
{
}

UWizardShakeThunder::UWizardShakeThunder()
{
}

UWizardShakeDash::UWizardShakeDash()
{
}

UWizardShakeImpact::UWizardShakeImpact()
{
}

UWizardCameraModifier::UWizardCameraModifier()
{
}

void UWizardCameraModifier::FovKick(float DeltaFov, float InTime, float OutTime)
{
	FovDelta = DeltaFov;
	FovInTime = InTime;
	FovOutTime = OutTime;
	FovTime = 0.f;
}

void UWizardCameraModifier::Pulse(float Amount, float Duration, const FLinearColor& Tint)
{
	PulseAmount = Amount;
	PulseDuration = Duration;
	PulseTime = 0.f;
	PulseTint = Tint;
}

bool UWizardCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	return true;
}

void UWizardCameraModifier::ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings)
{
	PostProcessBlendWeight = 0.f;
}

void UWizardCameraLibrary::Shake(const UObject* WorldContext, TSubclassOf<UCameraShakeBase> ShakeClass, float Scale)
{
}

void UWizardCameraLibrary::ShakeAtLocation(const UObject* WorldContext, TSubclassOf<UCameraShakeBase> ShakeClass, FVector Location, float FullRadius, float ZeroRadius, float Scale)
{
}

void UWizardCameraLibrary::FovKick(const UObject* WorldContext, float DeltaFov, float InTime, float OutTime)
{
}

void UWizardCameraLibrary::Pulse(const UObject* WorldContext, float Amount, float Duration, const FLinearColor& Tint)
{
}

UWizardCameraModifier* UWizardCameraLibrary::GetModifier(const UObject* WorldContext)
{
	return nullptr;
}
