#include "WizardAudioSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardAudioSubsystem)

// Skeleton build: cue playback, attenuation and the ambient loop are implemented in the next step.

UWizardAudioSubsystem* UWizardAudioSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	return World ? World->GetSubsystem<UWizardAudioSubsystem>() : nullptr;
}

void UWizardAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UWizardAudioSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

UAudioComponent* UWizardAudioSubsystem::PlayAt(FName Cue, const FVector& Location, float VolumeMul, float PitchMul, float FadeIn)
{
	return nullptr;
}

UAudioComponent* UWizardAudioSubsystem::PlayAttached(FName Cue, USceneComponent* AttachTo, FName Socket, float VolumeMul, float PitchMul, float FadeIn)
{
	return nullptr;
}

UAudioComponent* UWizardAudioSubsystem::Play2D(FName Cue, float VolumeMul, float PitchMul)
{
	return nullptr;
}

void UWizardAudioSubsystem::Stop(UAudioComponent* Component, float FadeOut)
{
}

void UWizardAudioSubsystem::SetThrottle(FName Cue, float MinInterval)
{
}

void UWizardAudioSubsystem::StartAmbient()
{
}

void UWizardAudioSubsystem::StopAmbient(float FadeOut)
{
}

USoundBase* UWizardAudioSubsystem::GetCue(FName Cue)
{
	return nullptr;
}
