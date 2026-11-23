// CameraFX.h - camera feel: legacy shake presets and a camera modifier for FOV kicks and
// post-process pulses (chromatic aberration, bloom, vignette).

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "Shakes/LegacyCameraShake.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CameraFX.generated.h"

class APlayerController;
class UCameraShakeBase;

/** Base of all shake presets. The oscillation patterns are configured in each subclass constructor. */
UCLASS(Abstract, Blueprintable)
class WIZARDEPIC_API UWizardCameraShake : public ULegacyCameraShake
{
	GENERATED_BODY()

public:
	UWizardCameraShake();
};

/** Heavy low-frequency rumble with a sharp vertical kick: explosions, tower collapse. */
UCLASS(Blueprintable)
class WIZARDEPIC_API UWizardShakeExplosion : public UWizardCameraShake
{
	GENERATED_BODY()

public:
	UWizardShakeExplosion();
};

/** Slow rolling rumble: earth pillars. */
UCLASS(Blueprintable)
class WIZARDEPIC_API UWizardShakeRumble : public UWizardCameraShake
{
	GENERATED_BODY()

public:
	UWizardShakeRumble();
};

/** Soft high-frequency flutter: wind gust. */
UCLASS(Blueprintable)
class WIZARDEPIC_API UWizardShakeWind : public UWizardCameraShake
{
	GENERATED_BODY()

public:
	UWizardShakeWind();
};

/** One violent crack followed by a short decaying rattle: lightning. */
UCLASS(Blueprintable)
class WIZARDEPIC_API UWizardShakeThunder : public UWizardCameraShake
{
	GENERATED_BODY()

public:
	UWizardShakeThunder();
};

/** Quick forward push: dash. */
UCLASS(Blueprintable)
class WIZARDEPIC_API UWizardShakeDash : public UWizardCameraShake
{
	GENERATED_BODY()

public:
	UWizardShakeDash();
};

/** Small local kick: a single prop breaking near the player. */
UCLASS(Blueprintable)
class WIZARDEPIC_API UWizardShakeImpact : public UWizardCameraShake
{
	GENERATED_BODY()

public:
	UWizardShakeImpact();
};

/**
 * Added to the player camera manager by the player controller. Applies the FOV kick and a
 * post-process pulse through its blend weight.
 */
UCLASS()
class WIZARDEPIC_API UWizardCameraModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	UWizardCameraModifier();

	/** Widens (positive) or narrows (negative) the FOV by DeltaFov over InTime, then eases back over OutTime. */
	void FovKick(float DeltaFov, float InTime, float OutTime);

	/** Pulses chromatic aberration, bloom and vignette (Amount 0..1) and lerps a colour tint. */
	void Pulse(float Amount, float Duration, const FLinearColor& Tint);

	//~ Begin UCameraModifier Interface
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;

protected:
	virtual void ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings) override;
	//~ End UCameraModifier Interface

private:
	/** FOV kick envelope. */
	float FovDelta = 0.f;
	float FovInTime = 0.05f;
	float FovOutTime = 0.25f;
	float FovTime = 0.f;

	/** Post-process pulse envelope. */
	float PulseAmount = 0.f;
	float PulseDuration = 0.3f;
	float PulseTime = 0.f;
	FLinearColor PulseTint = FLinearColor::Black;

	/** Current weight used for the post-process blend (max of both envelopes). */
	float BlendWeight = 0.f;

	/** Settings blended on top of the scene post process. */
	UPROPERTY(Transient)
	FPostProcessSettings PPSettings;
};

UCLASS()
class WIZARDEPIC_API UWizardCameraLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Plays a shake on the local player. */
	UFUNCTION(BlueprintCallable, Category = "Wizard|Camera")
	static void Shake(const UObject* WorldContext, TSubclassOf<UCameraShakeBase> ShakeClass, float Scale = 1.f);

	/** Plays a shake with a falloff: full strength inside FullRadius, linear to zero at ZeroRadius. */
	UFUNCTION(BlueprintCallable, Category = "Wizard|Camera")
	static void ShakeAtLocation(const UObject* WorldContext, TSubclassOf<UCameraShakeBase> ShakeClass, FVector Location, float FullRadius, float ZeroRadius, float Scale = 1.f);

	/** See UWizardCameraModifier::FovKick. */
	UFUNCTION(BlueprintCallable, Category = "Wizard|Camera")
	static void FovKick(const UObject* WorldContext, float DeltaFov, float InTime, float OutTime);

	/** See UWizardCameraModifier::Pulse. */
	UFUNCTION(BlueprintCallable, Category = "Wizard|Camera")
	static void Pulse(const UObject* WorldContext, float Amount, float Duration, const FLinearColor& Tint);

	/** The modifier of the local player (null when there is no local player yet). */
	UFUNCTION(BlueprintPure, Category = "Wizard|Camera")
	static UWizardCameraModifier* GetModifier(const UObject* WorldContext);
};
