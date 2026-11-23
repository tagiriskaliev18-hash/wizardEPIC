// SpellCasting.h - the spell library: all four deliveries (projectile, ground line, cone, strike)
// and the shared impact feedback (FX, light, camera, audio, destruction).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WizardTypes.h"
#include "SpellCasting.generated.h"

class UWizardAbilityComponent;
class USpellDataAsset;
class UAudioComponent;

UCLASS()
class WIZARDEPIC_API UWizardSpellLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Launches a spell from the caster's hand toward the aim point. ChargeT01: 0 = tap, 1 = fully charged. */
	static void CastSpell(UWizardAbilityComponent* Caster, USpellDataAsset* Spell, const FVector& AimPoint, const FVector& AimDirection, float ChargeT01);

	/** Spell feedback at an impact point: FX, light, camera feel, audio and the destruction impact. Returns the number of broken things. */
	static int32 Impact(const UObject* WorldContext, const FSpellImpact& Impact);

	/** Same destruction impact and reduced FX, without camera feel (secondary strikes, chain hits). */
	static int32 ImpactSecondary(const UObject* WorldContext, const FSpellImpact& Impact);

	/** Plays the cast montage on the caster when its anim instance has a DefaultSlot. */
	static void PlayCastAnim(const UWizardAbilityComponent* Caster, bool bCharged);

	/** Plays the dash montage on the caster. */
	static void PlayDashAnim(const UWizardAbilityComponent* Caster);

	/** Dust / chip colour of a destructible family. */
	static FLinearColor MaterialColor(EDestructibleMaterial Material);

	/** Break cue of a destructible family. */
	static FName MaterialBreakSound(EDestructibleMaterial Material);
};
