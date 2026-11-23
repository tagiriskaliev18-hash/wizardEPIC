#include "SpellCasting.h"

#include "WizardAbilityComponent.h"
#include "SpellDataAsset.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(SpellCasting)

// Skeleton build: the four deliveries and the impact feedback are implemented in the next step.

void UWizardSpellLibrary::CastSpell(UWizardAbilityComponent* Caster, USpellDataAsset* Spell, const FVector& AimPoint, const FVector& AimDirection, float ChargeT01)
{
}

int32 UWizardSpellLibrary::Impact(const UObject* WorldContext, const FSpellImpact& Impact)
{
	return 0;
}

int32 UWizardSpellLibrary::ImpactSecondary(const UObject* WorldContext, const FSpellImpact& Impact)
{
	return 0;
}

void UWizardSpellLibrary::PlayCastAnim(const UWizardAbilityComponent* Caster, bool bCharged)
{
}

void UWizardSpellLibrary::PlayDashAnim(const UWizardAbilityComponent* Caster)
{
}

FLinearColor UWizardSpellLibrary::MaterialColor(EDestructibleMaterial Material)
{
	return FLinearColor::White;
}

FName UWizardSpellLibrary::MaterialBreakSound(EDestructibleMaterial Material)
{
	switch (Material)
	{
	case EDestructibleMaterial::Stone:
		return TEXT("Break_Stone");
	case EDestructibleMaterial::Wood:
		return TEXT("Break_Wood");
	case EDestructibleMaterial::Pottery:
		return TEXT("Break_Pottery");
	case EDestructibleMaterial::Rock:
		return TEXT("Break_Rock");
	case EDestructibleMaterial::Crystal:
		return TEXT("Break_Crystal");
	case EDestructibleMaterial::Foliage:
		return TEXT("Break_Foliage");
	default:
		return NAME_None;
	}
}
