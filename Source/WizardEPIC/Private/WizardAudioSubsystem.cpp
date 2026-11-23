#include "WizardAudioSubsystem.h"

#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "UObject/SoftObjectPath.h"
#include "WizardTypes.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardAudioSubsystem)

UWizardAudioSubsystem* UWizardAudioSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	return World ? World->GetSubsystem<UWizardAudioSubsystem>() : nullptr;
}

void UWizardAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Attenuation = NewObject<USoundAttenuation>(this);
	if (Attenuation)
	{
		Attenuation->Attenuation.bAttenuate = true;
		Attenuation->Attenuation.bSpatialize = true;
		Attenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		Attenuation->Attenuation.AttenuationShapeExtents = FVector(450.f, 0.f, 0.f);
		Attenuation->Attenuation.FalloffDistance = 4800.f;
	}

	Concurrency = NewObject<USoundConcurrency>(this);
	if (Concurrency)
	{
		Concurrency->Concurrency.MaxCount = 10;
		Concurrency->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopFarthestThenOldest;
	}

	// Default throttles to prevent audio spam during huge cascades
	SetThrottle(TEXT("Break_Stone"), 0.045f);
	SetThrottle(TEXT("Break_Rock"), 0.045f);
	SetThrottle(TEXT("Break_Wood"), 0.045f);
	SetThrottle(TEXT("Break_Pottery"), 0.04f);
	SetThrottle(TEXT("Break_Crystal"), 0.04f);
	SetThrottle(TEXT("Break_Foliage"), 0.06f);
	SetThrottle(TEXT("Collapse"), 0.25f);
}

void UWizardAudioSubsystem::Deinitialize()
{
	StopAmbient(0.f);
	Cache.Empty();
	LastPlayed.Empty();
	Throttle.Empty();
	ReportedMissing.Empty();
	Super::Deinitialize();
}

USoundBase* UWizardAudioSubsystem::GetCue(FName Cue)
{
	if (Cue.IsNone())
	{
		return nullptr;
	}

	if (TObjectPtr<USoundBase>* Found = Cache.Find(Cue))
	{
		return Found->Get();
	}

	if (ReportedMissing.Contains(Cue))
	{
		return nullptr;
	}

	const FString CueStr = Cue.ToString();
	FString AssetPath;
	if (CueStr.StartsWith(TEXT("/")))
	{
		AssetPath = CueStr;
	}
	else if (CueStr.StartsWith(TEXT("S_")))
	{
		AssetPath = FString::Printf(TEXT("%s/%s"), WizardPaths::SFX, *CueStr);
	}
	else
	{
		AssetPath = FString::Printf(TEXT("%s/S_%s"), WizardPaths::SFX, *CueStr);
	}

	USoundBase* Sound = Cast<USoundBase>(FSoftObjectPath(AssetPath).TryLoad());
	if (!Sound && !CueStr.StartsWith(TEXT("/")))
	{
		const FString AltPath = FString::Printf(TEXT("%s/%s"), WizardPaths::SFX, *CueStr);
		Sound = Cast<USoundBase>(FSoftObjectPath(AltPath).TryLoad());
	}

	if (Sound)
	{
		Cache.Add(Cue, Sound);
	}
	else
	{
		ReportedMissing.Add(Cue);
		UE_LOG(LogWizard, Verbose, TEXT("Audio cue '%s' not found at '%s' (skipping)"), *CueStr, *AssetPath);
	}

	return Sound;
}

UAudioComponent* UWizardAudioSubsystem::PlayAt(FName Cue, const FVector& Location, float VolumeMul, float PitchMul, float FadeIn)
{
	UWorld* World = GetWorld();
	if (!World || Cue.IsNone())
	{
		return nullptr;
	}

	const float Now = World->GetTimeSeconds();
	if (const float* MinInt = Throttle.Find(Cue))
	{
		if (const float* Last = LastPlayed.Find(Cue))
		{
			if ((Now - *Last) < *MinInt)
			{
				return nullptr;
			}
		}
	}

	USoundBase* Sound = GetCue(Cue);
	if (!Sound)
	{
		return nullptr;
	}

	LastPlayed.Add(Cue, Now);

	const float PitchJitter = FMath::FRandRange(0.94f, 1.06f);
	UAudioComponent* Comp = UGameplayStatics::SpawnSoundAtLocation(
		World,
		Sound,
		Location,
		FRotator::ZeroRotator,
		FMath::Clamp(VolumeMul * MasterVolume, 0.f, 2.f),
		FMath::Clamp(PitchMul * PitchJitter, 0.4f, 2.2f),
		0.f,
		Attenuation,
		Concurrency,
		true);

	if (Comp && FadeIn > 0.f)
	{
		Comp->FadeIn(FadeIn, FMath::Clamp(VolumeMul * MasterVolume, 0.f, 2.f));
	}

	return Comp;
}

UAudioComponent* UWizardAudioSubsystem::PlayAttached(FName Cue, USceneComponent* AttachTo, FName Socket, float VolumeMul, float PitchMul, float FadeIn)
{
	if (!AttachTo || Cue.IsNone())
	{
		return nullptr;
	}

	USoundBase* Sound = GetCue(Cue);
	if (!Sound)
	{
		return nullptr;
	}

	UAudioComponent* Comp = UGameplayStatics::SpawnSoundAttached(
		Sound,
		AttachTo,
		Socket,
		FVector::ZeroVector,
		EAttachLocation::KeepRelativeOffset,
		true,
		FMath::Clamp(VolumeMul * MasterVolume, 0.f, 2.f),
		FMath::Clamp(PitchMul, 0.4f, 2.2f),
		0.f,
		Attenuation,
		Concurrency,
		true);

	if (Comp && FadeIn > 0.f)
	{
		Comp->FadeIn(FadeIn, FMath::Clamp(VolumeMul * MasterVolume, 0.f, 2.f));
	}

	return Comp;
}

UAudioComponent* UWizardAudioSubsystem::Play2D(FName Cue, float VolumeMul, float PitchMul)
{
	UWorld* World = GetWorld();
	if (!World || Cue.IsNone())
	{
		return nullptr;
	}

	USoundBase* Sound = GetCue(Cue);
	if (!Sound)
	{
		return nullptr;
	}

	return UGameplayStatics::SpawnSound2D(
		World,
		Sound,
		FMath::Clamp(VolumeMul * MasterVolume, 0.f, 2.f),
		FMath::Clamp(PitchMul, 0.4f, 2.2f),
		0.f,
		Concurrency,
		false,
		true);
}

void UWizardAudioSubsystem::Stop(UAudioComponent* Component, float FadeOut)
{
	if (!Component || !IsValid(Component))
	{
		return;
	}

	if (FadeOut > KINDA_SMALL_NUMBER)
	{
		Component->FadeOut(FadeOut, 0.f);
	}
	else
	{
		Component->Stop();
	}
}

void UWizardAudioSubsystem::SetThrottle(FName Cue, float MinInterval)
{
	if (!Cue.IsNone())
	{
		Throttle.Add(Cue, FMath::Max(0.f, MinInterval));
	}
}

void UWizardAudioSubsystem::StartAmbient()
{
	UWorld* World = GetWorld();
	if (!World || (Ambient && Ambient->IsPlaying()))
	{
		return;
	}

	const FString AmbientPath = FString::Printf(TEXT("%s/S_AmbientLoop"), WizardPaths::Music);
	USoundBase* AmbientSound = Cast<USoundBase>(FSoftObjectPath(AmbientPath).TryLoad());
	if (!AmbientSound)
	{
		return;
	}

	Ambient = UGameplayStatics::SpawnSound2D(World, AmbientSound, 0.35f * MasterVolume, 1.f, 0.f, nullptr, false, false);
	if (Ambient)
	{
		Ambient->FadeIn(1.5f, 0.35f * MasterVolume);
	}
}

void UWizardAudioSubsystem::StopAmbient(float FadeOut)
{
	if (Ambient)
	{
		Stop(Ambient, FadeOut);
		Ambient = nullptr;
	}
}
