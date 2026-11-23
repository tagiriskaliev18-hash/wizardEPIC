#include "WizardTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardTypes)

DEFINE_LOG_CATEGORY(LogWizard);

float FMaterialDamageTable::For(EDestructibleMaterial Material) const
{
	switch (Material)
	{
	case EDestructibleMaterial::Stone:
		return Stone;
	case EDestructibleMaterial::Wood:
		return Wood;
	case EDestructibleMaterial::Pottery:
		return Pottery;
	case EDestructibleMaterial::Rock:
		return Rock;
	case EDestructibleMaterial::Crystal:
		return Crystal;
	case EDestructibleMaterial::Foliage:
		return Foliage;
	default:
		return 1.f;
	}
}

float FSpellImpact::DamageAt(const FVector& WorldLocation) const
{
	const float Distance = static_cast<float>(FVector::Dist(Origin, WorldLocation));
	if (Distance > Radius)
	{
		return 0.f;
	}
	const float T = Radius > KINDA_SMALL_NUMBER ? Distance / Radius : 0.f;
	return Damage * FMath::Lerp(1.f, 0.35f, T);
}

FVector FSpellImpact::LaunchVelocityAt(const FVector& WorldLocation) const
{
	FVector Radial = WorldLocation - Origin;
	Radial.Z *= 0.35f;
	if (Radial.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		Radial = FVector::UpVector;
	}
	Radial.Normalize();

	const float Distance = static_cast<float>(FVector::Dist(Origin, WorldLocation));
	const float T = Radius > KINDA_SMALL_NUMBER ? FMath::Clamp(Distance / Radius, 0.f, 1.f) : 0.f;
	const float Speed = LaunchSpeed * FMath::Lerp(1.f, 0.5f, T);
	return (Radial + FVector(0.f, 0.f, UpBias)).GetSafeNormal() * Speed;
}
