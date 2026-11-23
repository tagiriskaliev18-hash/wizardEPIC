// SpellDataAsset.h - data-driven definition of one spell.
//
// The forge (Scripts/forge) creates four instances: DA_Spell_Fire, DA_Spell_Earth, DA_Spell_Wind,
// DA_Spell_Lightning. Designers tune numbers in the editor without recompiling, and Fab spell packs
// plug into the Niagara slots: when a slot is empty the built-in procedural FX play instead.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WizardTypes.h"
#include "SpellDataAsset.generated.h"

class UCameraShakeBase;
class UNiagaraSystem;
class UTexture2D;

/**
 * One elemental spell: identity, casting rules, impact numbers, look-and-feel and optional Niagara slots.
 * All numeric values describe a tap cast (charge scale 1).
 */
UCLASS(BlueprintType)
class USpellDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ----- Identity -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	ESpellElement Element = ESpellElement::Fire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	FText DisplayName;

	/** HUD slot icon. Falls back to SpellIcons[Element] when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	ESpellDelivery Delivery = ESpellDelivery::Projectile;

	// ----- Casting --------------------------------------------------------------------------

	/** Seconds before the spell can be cast again, counted from the launch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Casting", meta = (ClampMin = "0"))
	float Cooldown = 1.2f;

	/** Seconds between the release input and the launch (wind-up). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Casting", meta = (ClampMin = "0"))
	float CastDelay = 0.1f;

	/** Seconds of holding the cast button for a full charge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Casting", meta = (ClampMin = "0.05"))
	float ChargeTime = 1.f;

	/** Multiplier for damage, radius, launch speed and FX size at full charge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Casting", meta = (ClampMin = "1"))
	float ChargeScaleMax = 2.2f;

	/** A hold shorter than this is a tap: it casts instantly at charge scale 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Casting", meta = (ClampMin = "0"))
	float MinChargeTime = 0.22f;

	// ----- Impact numbers -------------------------------------------------------------------

	/** Raw damage at the centre of the impact. Compared against the health of destructible pieces. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	float Damage = 100.f;

	/** Pieces and props inside this radius take damage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "1"))
	float DamageRadius = 600.f;

	/** Launch speed (cm/s) of released pieces at the centre of the impact. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	float LaunchSpeed = 1400.f;

	/** Fraction of LaunchSpeed added straight up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0"))
	float UpBias = 0.25f;

	/** Random angular speed (deg/s) of released pieces. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0"))
	float Spin = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	FMaterialDamageTable DamageMultipliers;

	// ----- Delivery-specific numbers --------------------------------------------------------

	/** Projectile: flight speed in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "100"))
	float ProjectileSpeed = 4200.f;

	/** Projectile: collision sphere radius in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "1"))
	float ProjectileRadius = 28.f;

	/** Projectile: gravity scale while flying. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0"))
	float ProjectileGravityScale = 0.05f;

	/** Projectile: seconds before it detonates on its own if it hits nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.1"))
	float ProjectileLife = 3.f;

	/** GroundLine: number of pillars. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground Line", meta = (ClampMin = "1"))
	int32 PillarCount = 6;

	/** GroundLine: distance between pillar centres in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground Line")
	float PillarSpacing = 230.f;

	/** GroundLine: height of one pillar above ground in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground Line")
	float PillarHeight = 520.f;

	/** GroundLine: radius of one pillar in cm (also the launch radius around its centre). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground Line")
	float PillarRadius = 120.f;

	/** GroundLine: seconds one pillar takes to rise out of the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground Line")
	float PillarRiseTime = 0.16f;

	/** GroundLine: seconds between the eruptions of consecutive pillars. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground Line")
	float PillarStepDelay = 0.055f;

	/** GroundLine: seconds a pillar stays up before sinking back. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground Line")
	float PillarLifetime = 4.f;

	/** GroundLine: seconds a pillar takes to sink. Collision is off while it sinks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground Line")
	float PillarSinkTime = 0.5f;

	/** GroundLine: distance from the caster to the first pillar in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground Line")
	float PillarStartDistance = 250.f;

	/** Cone: reach in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cone")
	float ConeRange = 1600.f;

	/** Cone: half angle in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cone")
	float ConeHalfAngle = 22.f;

	/** Cone: seconds the gust keeps pushing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cone")
	float ConeDuration = 0.55f;

	/** Strike: number of bolts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strike", meta = (ClampMin = "1"))
	int32 StrikeCount = 3;

	/** Strike: seconds between bolts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strike")
	float StrikeInterval = 0.09f;

	/** Strike: extra damage multiplier for Stone and Rock - lightning shatters them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strike", meta = (ClampMin = "0"))
	float StrikeShatterBonus = 3.f;

	// ----- Look and feel --------------------------------------------------------------------

	/** Signature colour: light, FX tint, HUD accent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	FLinearColor Color = FLinearColor(1.f, 0.45f, 0.12f);

	/** Point light intensity in candela at the impact (0 = no light). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (ClampMin = "0"))
	float LightIntensity = 6.e6f;

	/** Impact light radius in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (ClampMin = "0"))
	float LightRadius = 2200.f;

	/** Seconds the impact light takes to fade out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (ClampMin = "0"))
	float LightDuration = 0.55f;

	/** Camera shake played at the impact. Null = no shake. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	TSubclassOf<UCameraShakeBase> ShakeClass = nullptr;

	/** Multiplier on the shake amplitude. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (ClampMin = "0"))
	float ShakeScale = 1.f;

	/** Degrees of temporary FOV widening at the impact. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (ClampMin = "0"))
	float FovKick = 3.5f;

	/** Seconds of hit-stop (time dilation 0.05) on a big impact. 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel", meta = (ClampMin = "0"))
	float HitStop = 0.045f;

	/** Audio cue names (files in /Game/Wizard/Audio/SFX). Empty = silent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	FName CastSound = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	FName ChargeSound = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	FName TravelSound = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feel")
	FName ImpactSound = NAME_None;

	// ----- Niagara slots (Fab spell packs) --------------------------------------------------
	// Leave empty to use the built-in procedural FX. When set, the system is spawned at the
	// matching moment: NiagaraCharge while charging (attached to the hand), NiagaraCast at launch
	// (attached to the hand), NiagaraTravel on a projectile (attached), NiagaraImpact at the
	// impact point (at location, scaled by charge).

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara")
	TSoftObjectPtr<UNiagaraSystem> NiagaraCharge;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara")
	TSoftObjectPtr<UNiagaraSystem> NiagaraCast;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara")
	TSoftObjectPtr<UNiagaraSystem> NiagaraTravel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara")
	TSoftObjectPtr<UNiagaraSystem> NiagaraImpact;

	// ----- Helpers --------------------------------------------------------------------------

	/** Charge scale for a normalised charge time: 1 at 0, ChargeScaleMax at 1. */
	float GetChargeScale(float ChargeT01) const;

	/** FSpellImpact for this spell. ChargeScale is already applied to damage, radius and launch speed. */
	FSpellImpact MakeImpact(const FVector& Origin, const FVector& Direction, float ChargeT01, AActor* Instigator) const;

	/** Full package path of the HUD icon: the explicit icon, else the default icon of the element. */
	FString GetIconPath() const;
};
