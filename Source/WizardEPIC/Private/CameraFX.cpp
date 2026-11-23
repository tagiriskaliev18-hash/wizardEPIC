#include "CameraFX.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(CameraFX)

UWizardCameraShake::UWizardCameraShake()
{
	bSingleInstance = false;
	OscillationBlendInTime = 0.04f;
	OscillationBlendOutTime = 0.25f;
}

UWizardShakeExplosion::UWizardShakeExplosion()
{
	OscillationDuration = 0.55f;
	OscillationBlendInTime = 0.03f;
	OscillationBlendOutTime = 0.35f;

	RotOscillation.Pitch.Amplitude = 2.8f;
	RotOscillation.Pitch.Frequency = 18.f;
	RotOscillation.Yaw.Amplitude = 1.6f;
	RotOscillation.Yaw.Frequency = 14.f;
	RotOscillation.Roll.Amplitude = 2.2f;
	RotOscillation.Roll.Frequency = 16.f;

	LocOscillation.Z.Amplitude = 18.f;
	LocOscillation.Z.Frequency = 22.f;
	LocOscillation.X.Amplitude = 8.f;
	LocOscillation.X.Frequency = 18.f;
}

UWizardShakeRumble::UWizardShakeRumble()
{
	OscillationDuration = 0.65f;
	OscillationBlendInTime = 0.08f;
	OscillationBlendOutTime = 0.35f;

	RotOscillation.Pitch.Amplitude = 1.4f;
	RotOscillation.Pitch.Frequency = 11.f;
	RotOscillation.Roll.Amplitude = 1.2f;
	RotOscillation.Roll.Frequency = 9.f;

	LocOscillation.Z.Amplitude = 12.f;
	LocOscillation.Z.Frequency = 13.f;
	LocOscillation.Y.Amplitude = 6.f;
	LocOscillation.Y.Frequency = 10.f;
}

UWizardShakeWind::UWizardShakeWind()
{
	OscillationDuration = 0.45f;
	OscillationBlendInTime = 0.06f;
	OscillationBlendOutTime = 0.22f;

	RotOscillation.Yaw.Amplitude = 0.7f;
	RotOscillation.Yaw.Frequency = 26.f;
	RotOscillation.Roll.Amplitude = 0.9f;
	RotOscillation.Roll.Frequency = 28.f;

	LocOscillation.Y.Amplitude = 4.f;
	LocOscillation.Y.Frequency = 24.f;
}

UWizardShakeThunder::UWizardShakeThunder()
{
	OscillationDuration = 0.42f;
	OscillationBlendInTime = 0.015f;
	OscillationBlendOutTime = 0.28f;

	RotOscillation.Pitch.Amplitude = 3.2f;
	RotOscillation.Pitch.Frequency = 32.f;
	RotOscillation.Yaw.Amplitude = 2.4f;
	RotOscillation.Yaw.Frequency = 29.f;
	RotOscillation.Roll.Amplitude = 2.8f;
	RotOscillation.Roll.Frequency = 34.f;

	LocOscillation.Z.Amplitude = 14.f;
	LocOscillation.Z.Frequency = 35.f;
}

UWizardShakeDash::UWizardShakeDash()
{
	OscillationDuration = 0.24f;
	OscillationBlendInTime = 0.02f;
	OscillationBlendOutTime = 0.16f;

	RotOscillation.Pitch.Amplitude = 0.6f;
	RotOscillation.Pitch.Frequency = 16.f;
	LocOscillation.X.Amplitude = 10.f;
	LocOscillation.X.Frequency = 14.f;
}

UWizardShakeImpact::UWizardShakeImpact()
{
	OscillationDuration = 0.22f;
	OscillationBlendInTime = 0.02f;
	OscillationBlendOutTime = 0.14f;

	RotOscillation.Pitch.Amplitude = 0.9f;
	RotOscillation.Pitch.Frequency = 22.f;
	RotOscillation.Roll.Amplitude = 0.7f;
	RotOscillation.Roll.Frequency = 20.f;
	LocOscillation.Z.Amplitude = 5.f;
	LocOscillation.Z.Frequency = 20.f;
}

UWizardCameraModifier::UWizardCameraModifier()
{
	AlphaInTime = 0.f;
	AlphaOutTime = 0.f;
	Priority = 128;
}

void UWizardCameraModifier::FovKick(float DeltaFov, float InTime, float OutTime)
{
	FovDelta = DeltaFov;
	FovInTime = FMath::Max(0.01f, InTime);
	FovOutTime = FMath::Max(0.01f, OutTime);
	FovTime = 0.f;
}

void UWizardCameraModifier::Pulse(float Amount, float Duration, const FLinearColor& Tint)
{
	PulseAmount = FMath::Clamp(Amount, 0.f, 1.f);
	PulseDuration = FMath::Max(0.05f, Duration);
	PulseTime = 0.f;
	PulseTint = Tint;
}

bool UWizardCameraModifier::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	const float FovTotal = FovInTime + FovOutTime;
	float FovWeight = 0.f;
	if (FMath::Abs(FovDelta) > KINDA_SMALL_NUMBER && FovTime < FovTotal)
	{
		FovTime += DeltaTime;
		if (FovTime <= FovInTime)
		{
			const float T = FMath::Clamp(FovTime / FovInTime, 0.f, 1.f);
			FovWeight = 1.f - FMath::Pow(1.f - T, 3.f);
		}
		else if (FovTime < FovTotal)
		{
			const float T = FMath::Clamp((FovTime - FovInTime) / FovOutTime, 0.f, 1.f);
			FovWeight = FMath::Pow(1.f - T, 2.5f);
		}
		else
		{
			FovDelta = 0.f;
			FovWeight = 0.f;
		}
		InOutPOV.FOV = FMath::Clamp(InOutPOV.FOV + FovDelta * FovWeight, 55.f, 120.f);
	}

	float PulseWeight = 0.f;
	if (PulseAmount > KINDA_SMALL_NUMBER && PulseTime < PulseDuration)
	{
		PulseTime += DeltaTime;
		const float T = FMath::Clamp(PulseTime / PulseDuration, 0.f, 1.f);
		// Fast attack in first 12%, smooth exponential decay after
		if (T < 0.12f)
		{
			PulseWeight = (T / 0.12f) * PulseAmount;
		}
		else
		{
			const float DecayT = (T - 0.12f) / 0.88f;
			PulseWeight = FMath::Pow(1.f - DecayT, 2.2f) * PulseAmount;
		}

		if (PulseTime >= PulseDuration)
		{
			PulseAmount = 0.f;
			PulseWeight = 0.f;
		}
	}

	BlendWeight = FMath::Clamp(PulseWeight, 0.f, 1.f);
	if (BlendWeight > KINDA_SMALL_NUMBER)
	{
		PPSettings = FPostProcessSettings();
		PPSettings.bOverride_SceneFringeIntensity = true;
		PPSettings.SceneFringeIntensity = 3.2f * BlendWeight;

		PPSettings.bOverride_BloomIntensity = true;
		PPSettings.BloomIntensity = 0.675f + 1.8f * BlendWeight;

		PPSettings.bOverride_VignetteIntensity = true;
		PPSettings.VignetteIntensity = 0.4f + 0.45f * BlendWeight;

		PPSettings.bOverride_SceneColorTint = true;
		PPSettings.SceneColorTint = FMath::Lerp(FLinearColor::White, PulseTint, 0.25f * BlendWeight);
	}

	// Return false so other modifiers continue processing.
	return false;
}

void UWizardCameraModifier::ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings)
{
	PostProcessBlendWeight = BlendWeight;
	if (BlendWeight > KINDA_SMALL_NUMBER)
	{
		PostProcessSettings = PPSettings;
	}
}

void UWizardCameraLibrary::Shake(const UObject* WorldContext, TSubclassOf<UCameraShakeBase> ShakeClass, float Scale)
{
	if (!ShakeClass || Scale <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraShake(ShakeClass, Scale);
	}
}

void UWizardCameraLibrary::ShakeAtLocation(const UObject* WorldContext, TSubclassOf<UCameraShakeBase> ShakeClass, FVector Location, float FullRadius, float ZeroRadius, float Scale)
{
	if (!ShakeClass || Scale <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	const float Dist = static_cast<float>(FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), Location));
	if (Dist >= ZeroRadius)
	{
		return;
	}

	float Falloff = 1.f;
	if (Dist > FullRadius && ZeroRadius > FullRadius)
	{
		Falloff = 1.f - (Dist - FullRadius) / (ZeroRadius - FullRadius);
	}
	PC->PlayerCameraManager->StartCameraShake(ShakeClass, Scale * FMath::Clamp(Falloff, 0.f, 1.f));
}

void UWizardCameraLibrary::FovKick(const UObject* WorldContext, float DeltaFov, float InTime, float OutTime)
{
	if (UWizardCameraModifier* Mod = GetModifier(WorldContext))
	{
		Mod->FovKick(DeltaFov, InTime, OutTime);
	}
}

void UWizardCameraLibrary::Pulse(const UObject* WorldContext, float Amount, float Duration, const FLinearColor& Tint)
{
	if (UWizardCameraModifier* Mod = GetModifier(WorldContext))
	{
		Mod->Pulse(Amount, Duration, Tint);
	}
}

UWizardCameraModifier* UWizardCameraLibrary::GetModifier(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager)
	{
		return nullptr;
	}
	return Cast<UWizardCameraModifier>(PC->PlayerCameraManager->FindCameraModifierByClass(UWizardCameraModifier::StaticClass()));
}
