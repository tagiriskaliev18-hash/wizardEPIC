#include "SpellDataAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SpellDataAsset)

float USpellDataAsset::GetChargeScale(float ChargeT01) const
{
	return FMath::Lerp(1.f, ChargeScaleMax, FMath::Clamp(ChargeT01, 0.f, 1.f));
}

FSpellImpact USpellDataAsset::MakeImpact(const FVector& InOrigin, const FVector& InDirection, float ChargeT01, AActor* Instigator) const
{
	FSpellImpact Impact;
	Impact.Origin = InOrigin;
	Impact.Direction = InDirection.GetSafeNormal();
	Impact.Element = Element;
	Impact.ChargeScale = GetChargeScale(ChargeT01);
	Impact.Damage = Damage * Impact.ChargeScale;
	Impact.Radius = DamageRadius * Impact.ChargeScale;
	Impact.LaunchSpeed = LaunchSpeed * FMath::Sqrt(Impact.ChargeScale);
	Impact.UpBias = UpBias;
	Impact.Spin = Spin;
	Impact.Multipliers = DamageMultipliers;
	Impact.Instigator = Instigator;
	Impact.Spell = const_cast<USpellDataAsset*>(this);
	return Impact;
}

FString USpellDataAsset::GetIconPath() const
{
	if (!Icon.IsNull())
	{
		return Icon.ToSoftObjectPath().ToString();
	}
	const int32 Index = static_cast<int32>(Element);
	if (Index >= 0 && Index < 4)
	{
		return WizardPaths::SpellIcons[Index];
	}
	return FString();
}
