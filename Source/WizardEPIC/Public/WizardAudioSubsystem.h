// WizardAudioSubsystem.h - named sound cues with distance attenuation, throttling and the ambient loop.
//
// Every cue is a SoundWave under /Game/Wizard/Audio/SFX (synthesised by Scripts/forge). Missing
// cues are logged once and skipped, so the game stays silent-but-working without content.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WizardAudioSubsystem.generated.h"

class UAudioComponent;
class USceneComponent;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

UCLASS()
class WIZARDEPIC_API UWizardAudioSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWizardAudioSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Plays a cue (file name without extension) at a world location with distance attenuation. */
	UAudioComponent* PlayAt(FName Cue, const FVector& Location, float VolumeMul = 1.f, float PitchMul = 1.f, float FadeIn = 0.f);

	/** Plays a cue attached to a component socket (loops stop with Stop). */
	UAudioComponent* PlayAttached(FName Cue, USceneComponent* AttachTo, FName Socket = NAME_None, float VolumeMul = 1.f, float PitchMul = 1.f, float FadeIn = 0.f);

	/** Plays a cue without attenuation (UI, camera-relative feel). */
	UAudioComponent* Play2D(FName Cue, float VolumeMul = 1.f, float PitchMul = 1.f);

	void Stop(UAudioComponent* Component, float FadeOut = 0.1f);

	/** A cue started less than MinInterval seconds ago is skipped. */
	void SetThrottle(FName Cue, float MinInterval);

	void StartAmbient();
	void StopAmbient(float FadeOut = 1.f);

	UPROPERTY(EditAnywhere, Category = Wizard)
	float MasterVolume = 0.8f;

private:
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> Attenuation = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USoundConcurrency> Concurrency = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Ambient = nullptr;

	TMap<FName, TObjectPtr<USoundBase>> Cache;
	TMap<FName, float> LastPlayed;
	TMap<FName, float> Throttle;
	TSet<FName> ReportedMissing;

	USoundBase* GetCue(FName Cue);
};
