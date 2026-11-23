#include "SpellCasting.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "CameraFX.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "FXSubsystem.h"
#include "GameFramework/Character.h"
#include "Misc/PackageName.h"
#include "SpellDataAsset.h"
#include "SpellProjectile.h"
#include "UObject/SoftObjectPath.h"
#include "WizardAbilityComponent.h"
#include "WizardAudioSubsystem.h"
#include "WizardDestructionSubsystem.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(SpellCasting)

namespace
{
	FVector FindGroundLocation(UWorld* World, const FVector& SampleXY, const AActor* IgnoreActor, float FallbackZ)
	{
		FVector Out(SampleXY.X, SampleXY.Y, FallbackZ);
		if (!World)
		{
			return Out;
		}

		TArray<FHitResult> Hits;
		FCollisionQueryParams QP;
		if (IgnoreActor)
		{
			QP.AddIgnoredActor(IgnoreActor);
		}
		const FVector TraceStart(SampleXY.X, SampleXY.Y, FallbackZ + 1200.f);
		const FVector TraceEnd(SampleXY.X, SampleXY.Y, FallbackZ - 1600.f);
		if (World->LineTraceMultiByChannel(Hits, TraceStart, TraceEnd, ECC_WorldStatic, QP) && !Hits.IsEmpty())
		{
			// Pick the lowest static blocking hit (actual terrain / plaza surface, never top of a tall wall/crate!)
			float LowestZ = TNumericLimits<float>::Max();
			for (const FHitResult& H : Hits)
			{
				if (H.bBlockingHit && H.ImpactPoint.Z < LowestZ)
				{
					LowestZ = static_cast<float>(H.ImpactPoint.Z);
				}
			}
			if (LowestZ < TNumericLimits<float>::Max())
			{
				Out.Z = LowestZ;
			}
		}
		return Out;
	}

	FLinearColor GetElementArcaneColor(ESpellElement Element)
	{
		switch (Element)
		{
		case ESpellElement::Fire:      return FLinearColor(1.00f, 0.36f, 0.05f, 1.f);
		case ESpellElement::Earth:     return FLinearColor(0.22f, 0.92f, 0.38f, 1.f);
		case ESpellElement::Wind:      return FLinearColor(0.45f, 0.95f, 1.00f, 1.f);
		case ESpellElement::Lightning: return FLinearColor(0.44f, 0.56f, 1.00f, 1.f);
		default:                       return FLinearColor(1.00f, 0.45f, 0.12f, 1.f);
		}
	}

	void SpawnGroundRuneCircle(
		UWizardFXSubsystem* FX,
		const FVector& GroundPos,
		const FLinearColor& Color,
		float ScaleStart,
		float ScaleEnd,
		float LifeTime,
		float SpinDegPerSec)
	{
		if (!FX)
		{
			return;
		}
		FFXMeshParams Rune;
		Rune.MeshName = TEXT("SM_RuneCircle");
		Rune.MaterialName = TEXT("M_FX_RuneCircle");
		Rune.Transform = FTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), GroundPos + FVector(0.f, 0.f, 9.f), FVector(1.f));
		Rune.LifeTime = LifeTime;
		Rune.ScaleStart = FVector(ScaleStart, ScaleStart, 1.f);
		Rune.ScaleEnd = FVector(ScaleEnd, ScaleEnd, 1.f);
		Rune.ScaleTime = FMath::Min(0.28f, LifeTime * 0.45f);
		Rune.FadeOutTime = FMath::Min(0.45f, LifeTime * 0.45f);
		Rune.RotationRate = FRotator(0.f, SpinDegPerSec, 0.f);
		Rune.VectorParams.Add(TEXT("Color"), Color);
		Rune.ScalarParams.Add(TEXT("EmissiveScale"), 14.f);
		Rune.ScalarParams.Add(TEXT("Fade"), 1.f);
		FX->SpawnMesh(Rune);
	}

	void SpawnImpactFX(const UObject* WorldContext, const FSpellImpact& Impact, bool bPrimary)
	{
		UWizardFXSubsystem* FX = UWizardFXSubsystem::Get(WorldContext);
		if (!FX)
		{
			return;
		}

		UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
		const float Scale = Impact.ChargeScale;
		const float SqrtScale = FMath::Sqrt(FMath::Max(0.25f, Scale));
		const FLinearColor Color = Impact.Spell ? Impact.Spell->Color : GetElementArcaneColor(Impact.Element);
		const FVector GroundOrigin = FindGroundLocation(World, Impact.Origin, Impact.Instigator, static_cast<float>(Impact.Origin.Z));

		if (Impact.Spell && !Impact.Spell->NiagaraImpact.IsNull())
		{
			FX->SpawnNiagara(Impact.Spell->NiagaraImpact, FTransform(Impact.Origin), Scale);
		}

		// Light flash (calibrated to Lumens for golden-hour daylight)
		if (Impact.Spell && Impact.Spell->LightIntensity > 0.f)
		{
			FFXLightParams LP;
			LP.Color = Color;
			LP.Intensity = FMath::Clamp((bPrimary ? 145.f : 75.f) * SqrtScale, 45.f, 160.f);
			LP.AttenuationRadius = FMath::Clamp(Impact.Spell->LightRadius * 0.48f * SqrtScale, 380.f, 880.f);
			LP.Location = Impact.Origin + FVector(0.f, 0.f, 75.f);
			LP.LifeTime = Impact.Spell->LightDuration;
			LP.AttackTime = 0.02f;
			LP.Flicker = (Impact.Element == ESpellElement::Lightning) ? 0.35f : 0.12f;
			LP.bCastShadows = bPrimary;
			FX->SpawnLight(LP);
		}

		// Soft additive Fresnel starburst plasma core (skip spherical dome on Lightning for razor-sharp bolts!)
		if (Impact.Element != ESpellElement::Lightning)
		{
			FFXMeshParams Core;
			Core.MeshName = TEXT("SM_FX_Sphere");
			Core.MaterialName = TEXT("M_FX_Core");
			Core.Transform = FTransform(FRotator::ZeroRotator, Impact.Origin, FVector(1.f));
			Core.LifeTime = 0.30f;
			Core.ScaleStart = FVector(0.45f * SqrtScale);
			Core.ScaleEnd = FVector(1.25f * Scale);
			Core.ScaleTime = 0.12f;
			Core.FadeOutTime = 0.20f;
			Core.VectorParams.Add(TEXT("Tint"), Color);
			Core.ScalarParams.Add(TEXT("EmissiveScale"), 9.f);
			Core.ScalarParams.Add(TEXT("Fade"), 1.f);
			FX->SpawnMesh(Core);
		}

		// Expanding energy shockwave ring
		{
			FFXMeshParams Ring;
			Ring.MeshName = TEXT("SM_FX_Ring");
			Ring.MaterialName = TEXT("M_FX_Ring");
			Ring.Transform = FTransform(FRotator::ZeroRotator, GroundOrigin + FVector(0.f, 0.f, 18.f), FVector(1.f));
			Ring.LifeTime = 0.48f;
			Ring.ScaleStart = FVector(0.55f * SqrtScale, 0.55f * SqrtScale, 1.f);
			Ring.ScaleEnd = FVector((Impact.Radius / 100.f) * 1.2f, (Impact.Radius / 100.f) * 1.2f, 1.f);
			Ring.ScaleTime = 0.34f;
			Ring.FadeOutTime = 0.26f;
			Ring.RotationRate = FRotator(0.f, 160.f, 0.f);
			Ring.VectorParams.Add(TEXT("Tint"), (Impact.Element == ESpellElement::Lightning) ? FLinearColor(0.32f, 0.72f, 1.0f) : Color);
			Ring.ScalarParams.Add(TEXT("EmissiveScale"), (Impact.Element == ESpellElement::Lightning) ? 11.f : 7.5f);
			Ring.ScalarParams.Add(TEXT("Fade"), 1.f);
			FX->SpawnMesh(Ring);
		}

		// Element-specific signature spectacles
		if (Impact.Element == ESpellElement::Fire)
		{
			FFXBurstSpec Flash;
			Flash.Kind = EFXSpriteKind::Flash;
			Flash.Count = bPrimary ? 12 : 6;
			Flash.SpeedMin = 90.f;
			Flash.SpeedMax = 460.f;
			Flash.LifeMin = 0.16f;
			Flash.LifeMax = 0.34f;
			Flash.SizeMin = 110.f;
			Flash.SizeMax = 240.f;
			Flash.SizeEndMul = 1.8f;
			Flash.ColorA = FLinearColor(1.f, 0.88f, 0.45f) * 5.f;
			Flash.ColorB = FLinearColor(1.f, 0.30f, 0.04f) * 3.f;
			FX->Burst(Flash, FTransform(Impact.Origin), Scale);

			// Billowing upward spiral fire embers
			FFXBurstSpec Embers;
			Embers.Kind = EFXSpriteKind::Ember;
			Embers.Mode = EFXSpriteMode::VelocityAligned;
			Embers.Count = bPrimary ? 64 : 24;
			Embers.SpeedMin = 420.f;
			Embers.SpeedMax = 1650.f;
			Embers.UpBias = 0.85f;
			Embers.SpawnRadius = 90.f;
			Embers.LifeMin = 0.55f;
			Embers.LifeMax = 1.45f;
			Embers.SizeMin = 18.f;
			Embers.SizeMax = 42.f;
			Embers.SizeEndMul = 0.2f;
			Embers.ColorA = FLinearColor(1.f, 0.78f, 0.24f) * 5.f;
			Embers.ColorB = FLinearColor(1.f, 0.16f, 0.02f) * 2.5f;
			Embers.GravityScale = -0.35f;
			Embers.Stretch = 3.0f;
			FX->Burst(Embers, FTransform(GroundOrigin + FVector(0.f, 0.f, 60.f)), Scale);

			FFXBurstSpec Smoke;
			Smoke.Kind = EFXSpriteKind::Smoke;
			Smoke.Count = bPrimary ? 24 : 10;
			Smoke.SpeedMin = 140.f;
			Smoke.SpeedMax = 520.f;
			Smoke.UpBias = 0.65f;
			Smoke.LifeMin = 0.9f;
			Smoke.LifeMax = 1.9f;
			Smoke.SizeMin = 120.f;
			Smoke.SizeMax = 240.f;
			Smoke.SizeEndMul = 2.4f;
			Smoke.ColorA = FLinearColor(0.28f, 0.16f, 0.10f);
			Smoke.ColorB = FLinearColor(0.08f, 0.07f, 0.08f);
			Smoke.AlphaStart = 0.68f;
			Smoke.AlphaEnd = 0.f;
			FX->Burst(Smoke, FTransform(GroundOrigin + FVector(0.f, 0.f, 40.f)), Scale);

			// Softly feathered circular scorch crater with glowing molten ember core
			FFXDecalParams Scorch;
			Scorch.MaterialName = TEXT("M_Decal_Scorch");
			Scorch.Location = GroundOrigin;
			Scorch.Rotation = FRotator(-90.f, FMath::FRandRange(0.f, 360.f), 0.f);
			Scorch.Size = FVector(200.f, Impact.Radius * 0.72f, Impact.Radius * 0.72f);
			Scorch.LifeTime = 24.f;
			FX->SpawnDecal(Scorch);

			// Towering @BlendiByl ribbed multi-spiral Fire Tornado vortex erupting from the impact crater!
			if (bPrimary || Scale >= 1.15f)
			{
				const float VortexScale = FMath::Clamp(0.68f * SqrtScale, 0.62f, 1.15f);

				FFXMeshParams OuterTornado;
				OuterTornado.MeshName = TEXT("SM_FX_FireTornado");
				OuterTornado.MaterialName = TEXT("M_FX_FireTornado");
				OuterTornado.Transform = FTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), GroundOrigin - FVector(0.f, 0.f, 15.f), FVector(1.f));
				OuterTornado.LifeTime = 3.4f;
				OuterTornado.ScaleStart = FVector(0.36f * VortexScale, 0.36f * VortexScale, 0.12f * VortexScale);
				OuterTornado.ScaleEnd = FVector(0.92f * VortexScale, 0.92f * VortexScale, 0.96f * VortexScale);
				OuterTornado.ScaleTime = 0.28f;
				OuterTornado.FadeOutTime = 0.80f;
				OuterTornado.RotationRate = FRotator(0.f, 240.f, 0.f);
				OuterTornado.VectorParams.Add(TEXT("CoreColor"), FLinearColor(1.00f, 0.72f, 0.18f, 1.f));
				OuterTornado.VectorParams.Add(TEXT("EdgeColor"), FLinearColor(1.00f, 0.20f, 0.02f, 1.f));
				OuterTornado.VectorParams.Add(TEXT("Tint"), FLinearColor(1.00f, 0.42f, 0.06f, 1.f));
				OuterTornado.ScalarParams.Add(TEXT("EmissiveScale"), 22.f);
				OuterTornado.ScalarParams.Add(TEXT("Fade"), 1.f);
				FX->SpawnMesh(OuterTornado);

				// Counter-rotating blazing molten core inside the tornado funnel for layered volumetric depth
				FFXMeshParams InnerTornado;
				InnerTornado.MeshName = TEXT("SM_FX_FireTornado");
				InnerTornado.MaterialName = TEXT("M_FX_FireTornado");
				InnerTornado.Transform = FTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), GroundOrigin - FVector(0.f, 0.f, 10.f), FVector(1.f));
				InnerTornado.LifeTime = 3.2f;
				InnerTornado.ScaleStart = FVector(0.20f * VortexScale, 0.20f * VortexScale, 0.14f * VortexScale);
				InnerTornado.ScaleEnd = FVector(0.52f * VortexScale, 0.52f * VortexScale, 1.00f * VortexScale);
				InnerTornado.ScaleTime = 0.24f;
				InnerTornado.FadeOutTime = 0.75f;
				InnerTornado.RotationRate = FRotator(0.f, -360.f, 0.f);
				InnerTornado.VectorParams.Add(TEXT("CoreColor"), FLinearColor(1.00f, 0.80f, 0.26f, 1.f));
				InnerTornado.VectorParams.Add(TEXT("EdgeColor"), FLinearColor(1.00f, 0.26f, 0.03f, 1.f));
				InnerTornado.VectorParams.Add(TEXT("Tint"), FLinearColor(1.00f, 0.55f, 0.12f, 1.f));
				InnerTornado.ScalarParams.Add(TEXT("EmissiveScale"), 24.f);
				InnerTornado.ScalarParams.Add(TEXT("Fade"), 1.f);
				FX->SpawnMesh(InnerTornado);

				// Fiery crimson-gold arcane rune circle ringing the tornado base
				SpawnGroundRuneCircle(FX, GroundOrigin, FLinearColor(1.00f, 0.36f, 0.04f, 1.f), 1.1f * VortexScale, 3.1f * VortexScale, 1.85f, -95.f);

				// Thermal updraft lifting shattered rubble skyward inside the fire vortex
				if (World)
				{
					if (UWizardDestructionSubsystem* Dest = UWizardDestructionSubsystem::Get(World))
					{
						const uint32 ResetSerial = Dest->GetResetSerial();
						FX->Delay(0.14f, [World, GroundOrigin, VortexScale, ResetSerial]()
						{
							if (UWizardDestructionSubsystem* D = UWizardDestructionSubsystem::Get(World))
							{
								if (D->GetResetSerial() == ResetSerial)
								{
									D->PushDynamicBodies(GroundOrigin + FVector(0.f, 0.f, 120.f), 680.f * VortexScale, 1250.f, 0.85f);
								}
							}
						});
					}
				}
			}
		}
		else if (Impact.Element == ESpellElement::Lightning)
		{
			FFXBurstSpec Sparks;
			Sparks.Kind = EFXSpriteKind::Spark;
			Sparks.Mode = EFXSpriteMode::VelocityAligned;
			Sparks.Count = bPrimary ? 72 : 32;
			Sparks.SpeedMin = 650.f;
			Sparks.SpeedMax = 2100.f;
			Sparks.UpBias = 0.45f;
			Sparks.LifeMin = 0.25f;
			Sparks.LifeMax = 0.78f;
			Sparks.SizeMin = 15.f;
			Sparks.SizeMax = 34.f;
			Sparks.SizeEndMul = 0.15f;
			Sparks.ColorA = FLinearColor(0.88f, 0.96f, 1.f) * 7.f;
			Sparks.ColorB = FLinearColor(0.32f, 0.48f, 1.f) * 4.f;
			Sparks.GravityScale = -1.0f;
			Sparks.Stretch = 3.8f;
			FX->Burst(Sparks, FTransform(GroundOrigin + FVector(0.f, 0.f, 25.f)), Scale);

			// Sharp starburst flash sprites at strike epicenter
			FFXBurstSpec StarFlash;
			StarFlash.Kind = EFXSpriteKind::Flash;
			StarFlash.Count = bPrimary ? 8 : 4;
			StarFlash.SpeedMin = 40.f;
			StarFlash.SpeedMax = 220.f;
			StarFlash.LifeMin = 0.12f;
			StarFlash.LifeMax = 0.24f;
			StarFlash.SizeMin = 130.f;
			StarFlash.SizeMax = 260.f;
			StarFlash.SizeEndMul = 1.6f;
			StarFlash.ColorA = FLinearColor(0.95f, 0.98f, 1.f) * 6.f;
			StarFlash.ColorB = FLinearColor(0.36f, 0.56f, 1.f) * 4.f;
			FX->Burst(StarFlash, FTransform(GroundOrigin + FVector(0.f, 0.f, 40.f)), SqrtScale);

			FFXDecalParams Scorch;
			Scorch.MaterialName = TEXT("M_Decal_Scorch");
			Scorch.Location = GroundOrigin;
			Scorch.Rotation = FRotator(-90.f, FMath::FRandRange(0.f, 360.f), 0.f);
			Scorch.Size = FVector(200.f, Impact.Radius * 0.62f, Impact.Radius * 0.62f);
			Scorch.LifeTime = 20.f;
			FX->SpawnDecal(Scorch);
		}
		else
		{
			FFXBurstSpec Dust;
			Dust.Kind = EFXSpriteKind::Dust;
			Dust.Count = bPrimary ? 24 : 12;
			Dust.SpeedMin = 180.f;
			Dust.SpeedMax = 650.f;
			Dust.UpBias = 0.45f;
			Dust.LifeMin = 0.7f;
			Dust.LifeMax = 1.4f;
			Dust.SizeMin = 90.f;
			Dust.SizeMax = 190.f;
			Dust.SizeEndMul = 2.2f;
			Dust.ColorA = Color;
			Dust.ColorB = Color * 0.5f;
			Dust.AlphaStart = 0.65f;
			Dust.AlphaEnd = 0.f;
			FX->Burst(Dust, FTransform(GroundOrigin + FVector(0.f, 0.f, 25.f)), Scale);
		}
	}

	void CastGroundLine(UWizardAbilityComponent* Caster, USpellDataAsset* Spell, const FVector& AimPoint, const FVector& AimDirection, float ChargeT01)
	{
		UWorld* World = Caster ? Caster->GetWorld() : nullptr;
		UWizardFXSubsystem* FX = UWizardFXSubsystem::Get(World);
		UWizardDestructionSubsystem* Destruction = UWizardDestructionSubsystem::Get(World);
		if (!World || !FX || !Destruction)
		{
			return;
		}

		AActor* Owner = Caster->GetOwner();
		const FVector StartBase = Owner ? Owner->GetActorLocation() : AimPoint;
		const float OwnerFootZ = Owner ? static_cast<float>(Owner->GetActorLocation().Z - 90.f) : 0.f;
		FVector FlatDir = AimDirection;
		FlatDir.Z = 0.f;
		if (FlatDir.SizeSquared() < KINDA_SMALL_NUMBER)
		{
			FlatDir = Owner ? Owner->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
		}
		FlatDir.Normalize();

		const float ChargeScale = Spell->GetChargeScale(ChargeT01);
		const int32 Count = FMath::Max(1, FMath::RoundToInt(Spell->PillarCount * FMath::Lerp(1.f, 1.35f, ChargeT01)));
		const uint32 ResetSerial = Destruction->GetResetSerial();
		TWeakObjectPtr<AActor> WeakOwner(Owner);
		TWeakObjectPtr<USpellDataAsset> WeakSpell(Spell);

		// Clamp eruption origin strictly >= 420cm ahead of the wizard (+ clear over-shoulder camera frustum!)
		const float AimDist2D = static_cast<float>(FVector::Dist2D(StartBase, AimPoint));
		const float ClampedStartDist = FMath::Max3(560.f, Spell->PillarStartDistance, (AimDist2D < 420.f) ? 560.f : 420.f);
		const FVector LineOrigin = StartBase + FlatDir * ClampedStartDist;

		for (int32 i = 0; i < Count; ++i)
		{
			const float StepDelay = i * Spell->PillarStepDelay;
			const FVector SampleXY = LineOrigin + FlatDir * (i * Spell->PillarSpacing);

			FX->Delay(StepDelay, [World, WeakOwner, WeakSpell, SampleXY, OwnerFootZ, FlatDir, ChargeT01, ChargeScale, ResetSerial, i]()
			{
				UWizardDestructionSubsystem* Dest = UWizardDestructionSubsystem::Get(World);
				UWizardFXSubsystem* FXSys = UWizardFXSubsystem::Get(World);
				USpellDataAsset* Sp = WeakSpell.Get();
				if (!Dest || !FXSys || !Sp || Dest->GetResetSerial() != ResetSerial)
				{
					return;
				}

				// Always snap to true lowest terrain surface and embed base -40cm below ground (NEVER float!)
				const FVector GroundPos = FindGroundLocation(World, SampleXY, WeakOwner.Get(), OwnerFootZ);
				const FVector EmbeddedBase = GroundPos - FVector(0.f, 0.f, 40.f);

				const float SqrtScale = FMath::Sqrt(ChargeScale);
				const float PillarH = Sp->PillarHeight * FMath::Lerp(0.72f, 1.22f, static_cast<float>(i) / FMath::Max(1, Sp->PillarCount - 1)) * SqrtScale;
				const float PillarR = Sp->PillarRadius * SqrtScale;

				FFXMeshParams Pillar;
				Pillar.MeshName = TEXT("SM_FX_Pillar");
				Pillar.MaterialName = TEXT("M_FX_Pillar");
				Pillar.Transform = FTransform(
					FRotator(FMath::FRandRange(-7.f, 7.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(-7.f, 7.f)),
					EmbeddedBase,
					FVector(PillarR / 55.f, PillarR / 55.f, PillarH / 400.f));
				Pillar.LifeTime = Sp->PillarLifetime;
				Pillar.ScaleStart = FVector(1.f, 1.f, 0.05f);
				Pillar.ScaleEnd = FVector(1.f, 1.f, 1.f);
				Pillar.ScaleTime = Sp->PillarRiseTime;
				Pillar.FadeOutTime = Sp->PillarSinkTime;
				Pillar.bCastShadow = true;
				Pillar.bCollision = true;
				Pillar.VectorParams.Add(TEXT("Tint"), FLinearColor(0.34f, 0.36f, 0.32f, 1.f));
				Pillar.VectorParams.Add(TEXT("EmissiveTint"), FLinearColor(0.24f, 0.85f, 0.38f, 1.f));
				Pillar.ScalarParams.Add(TEXT("Fade"), 1.f);
				FXSys->SpawnMesh(Pillar);

				// Radial ground dust & emerald-amber shockwave ring at eruption base
				FFXMeshParams DustRing;
				DustRing.MeshName = TEXT("SM_FX_Ring");
				DustRing.MaterialName = TEXT("M_FX_Ring");
				DustRing.Transform = FTransform(FRotator::ZeroRotator, GroundPos + FVector(0.f, 0.f, 14.f), FVector(1.f));
				DustRing.LifeTime = 0.55f;
				DustRing.ScaleStart = FVector(0.7f * SqrtScale, 0.7f * SqrtScale, 1.f);
				DustRing.ScaleEnd = FVector(3.4f * SqrtScale, 3.4f * SqrtScale, 1.f);
				DustRing.ScaleTime = 0.38f;
				DustRing.FadeOutTime = 0.30f;
				DustRing.RotationRate = FRotator(0.f, 120.f, 0.f);
				DustRing.VectorParams.Add(TEXT("Tint"), FLinearColor(0.76f, 0.62f, 0.32f, 1.f));
				DustRing.ScalarParams.Add(TEXT("EmissiveScale"), 4.2f);
				DustRing.ScalarParams.Add(TEXT("Fade"), 1.f);
				FXSys->SpawnMesh(DustRing);

				if (i == 0 || i == 2)
				{
					SpawnGroundRuneCircle(FXSys, GroundPos, FLinearColor(0.22f, 0.92f, 0.38f, 1.f), 1.0f, 2.5f * SqrtScale, 1.25f, 85.f);
				}

				FFXDecalParams Crack;
				Crack.MaterialName = TEXT("M_Decal_Crack");
				Crack.Location = GroundPos;
				Crack.Rotation = FRotator(-90.f, FMath::FRandRange(0.f, 360.f), 0.f);
				Crack.Size = FVector(180.f, PillarR * 2.4f, PillarR * 2.4f);
				Crack.LifeTime = 22.f;
				FXSys->SpawnDecal(Crack);

				FFXBurstSpec Dust;
				Dust.Kind = EFXSpriteKind::Dust;
				Dust.Count = 24;
				Dust.SpeedMin = 240.f;
				Dust.SpeedMax = 780.f;
				Dust.UpBias = 0.65f;
				Dust.SpawnRadius = 60.f;
				Dust.LifeMin = 0.65f;
				Dust.LifeMax = 1.40f;
				Dust.SizeMin = 90.f;
				Dust.SizeMax = 195.f;
				Dust.SizeEndMul = 2.2f;
				Dust.ColorA = FLinearColor(0.68f, 0.56f, 0.38f);
				Dust.ColorB = FLinearColor(0.36f, 0.30f, 0.22f);
				Dust.AlphaStart = 0.78f;
				Dust.AlphaEnd = 0.f;
				FXSys->Burst(Dust, FTransform(GroundPos + FVector(0.f, 0.f, 35.f)), SqrtScale);

				FFXDebrisParams Rocks;
				Rocks.MeshName = TEXT("SM_FX_Chip");
				Rocks.MaterialName = TEXT("M_FX_Chip_Rock");
				Rocks.Count = 14;
				Rocks.Origin = FTransform(GroundPos + FVector(0.f, 0.f, 35.f));
				Rocks.SpeedMin = 450.f;
				Rocks.SpeedMax = 1220.f;
				Rocks.UpBias = 0.88f;
				FXSys->SpawnDebris(Rocks);

				FSpellImpact Impact = Sp->MakeImpact(GroundPos, FlatDir, ChargeT01, WeakOwner.Get());
				Dest->ImpactLaunchUpward(GroundPos, PillarR * 1.9f, PillarH + 140.f, Impact);

				if (UWizardAudioSubsystem* Audio = UWizardAudioSubsystem::Get(World))
				{
					Audio->PlayAt(TEXT("Erupt_Earth"), GroundPos, 0.95f, FMath::FRandRange(0.9f, 1.1f));
				}
				if (i == 0 || i == 2)
				{
					UWizardCameraLibrary::ShakeAtLocation(World, UWizardShakeRumble::StaticClass(), GroundPos, 1000.f, 3500.f, 0.9f * ChargeScale);
				}
			});
		}
	}

	void CastCone(UWizardAbilityComponent* Caster, USpellDataAsset* Spell, const FVector& AimDirection, float ChargeT01)
	{
		UWorld* World = Caster ? Caster->GetWorld() : nullptr;
		UWizardFXSubsystem* FX = UWizardFXSubsystem::Get(World);
		UWizardDestructionSubsystem* Destruction = UWizardDestructionSubsystem::Get(World);
		if (!World || !FX || !Destruction)
		{
			return;
		}

		AActor* Owner = Caster->GetOwner();
		const FVector Origin = Caster->GetHandLocation();
		const FVector DirN = AimDirection.GetSafeNormal();
		FVector FlatDir = DirN;
		FlatDir.Z = 0.f;
		if (!FlatDir.Normalize())
		{
			FlatDir = Owner ? Owner->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
		}

		const float ChargeScale = Spell->GetChargeScale(ChargeT01);
		const float SqrtScale = FMath::Sqrt(ChargeScale);
		const float Range = Spell->ConeRange * SqrtScale;
		const float HalfAngle = Spell->ConeHalfAngle;

		const float OwnerFootZ = Owner ? static_cast<float>(Owner->GetActorLocation().Z - 90.f) : 0.f;
		const FVector CycloneSample = (Owner ? Owner->GetActorLocation() : Origin) + FlatDir * FMath::Clamp(Range * 0.42f, 640.f, 920.f);
		const FVector CycloneGround = FindGroundLocation(World, CycloneSample, Owner, OwnerFootZ);

		// Cone blast + dramatic upward cyclone lift so destructible blocks/crates soar high into the tornado!
		const FSpellImpact ConeImpact = Spell->MakeImpact(Origin, DirN, ChargeT01, Owner);
		Destruction->ImpactCone(Origin, DirN, HalfAngle + 8.f, Range, ConeImpact);

		FSpellImpact LiftImpact = Spell->MakeImpact(CycloneGround, FVector::UpVector, ChargeT01, Owner);
		LiftImpact.UpBias = 0.95f;
		LiftImpact.LaunchSpeed = FMath::Max(LiftImpact.LaunchSpeed, 1850.f);
		LiftImpact.Spin = 860.f;
		LiftImpact.Damage *= 1.35f;
		Destruction->ImpactLaunchUpward(CycloneGround, 720.f * SqrtScale, 1050.f, LiftImpact);

		// Huge visible swirling @BlendiByl Wind Cyclone using sculpted SM_FX_FireTornado tinted icy-cyan & white!
		const float VortexScale = FMath::Clamp(0.82f * SqrtScale, 0.72f, 1.25f);
		{
			FFXMeshParams WindCyclone;
			WindCyclone.MeshName = TEXT("SM_FX_FireTornado");
			WindCyclone.MaterialName = TEXT("M_FX_FireTornado");
			WindCyclone.Transform = FTransform(FRotator::ZeroRotator, CycloneGround - FVector(0.f, 0.f, 15.f), FVector(1.f));
			WindCyclone.LifeTime = 2.2f;
			WindCyclone.ScaleStart = FVector(0.35f * VortexScale, 0.35f * VortexScale, 0.14f * VortexScale);
			WindCyclone.ScaleEnd = FVector(1.02f * VortexScale, 1.02f * VortexScale, 0.94f * VortexScale);
			WindCyclone.ScaleTime = 0.28f;
			WindCyclone.FadeOutTime = 0.65f;
			WindCyclone.RotationRate = FRotator(0.f, 420.f, 0.f);
			WindCyclone.VectorParams.Add(TEXT("CoreColor"), FLinearColor(0.95f, 0.99f, 1.00f, 1.f));
			WindCyclone.VectorParams.Add(TEXT("EdgeColor"), FLinearColor(0.28f, 0.88f, 1.00f, 1.f));
			WindCyclone.VectorParams.Add(TEXT("Tint"), FLinearColor(0.84f, 0.97f, 1.00f, 1.f));
			WindCyclone.ScalarParams.Add(TEXT("EmissiveScale"), 12.f);
			WindCyclone.ScalarParams.Add(TEXT("Fade"), 1.f);
			FX->SpawnMesh(WindCyclone);
		}

		// Stacked thin crisp concentric white wind-swirl rings orbiting up the cyclone funnel
		for (int32 r = 0; r < 5; ++r)
		{
			const float Frac = static_cast<float>(r) / 4.f;
			const float RingZ = 65.f + Frac * 660.f * VortexScale;
			const float RingRad = FMath::Lerp(1.1f, 3.3f, Frac) * VortexScale;

			FFXMeshParams Swirl;
			Swirl.MeshName = TEXT("SM_FX_WindVortex");
			Swirl.MaterialName = TEXT("M_FX_WindRibbon");
			Swirl.Transform = FTransform(FRotator(FMath::FRandRange(-8.f, 8.f), r * 72.f, FMath::FRandRange(-8.f, 8.f)), CycloneGround + FVector(0.f, 0.f, RingZ), FVector(1.f));
			Swirl.LifeTime = 1.85f;
			Swirl.ScaleStart = FVector(RingRad * 0.45f, RingRad * 0.45f, 0.65f);
			Swirl.ScaleEnd = FVector(RingRad * 1.15f, RingRad * 1.15f, 1.05f);
			Swirl.ScaleTime = 0.32f;
			Swirl.FadeOutTime = 0.55f;
			Swirl.RotationRate = FRotator(0.f, (r % 2 == 0) ? 520.f : -440.f, 0.f);
			Swirl.VectorParams.Add(TEXT("Tint"), FLinearColor(0.82f, 0.97f, 1.00f, 1.f));
			Swirl.ScalarParams.Add(TEXT("EmissiveScale"), 7.5f);
			Swirl.ScalarParams.Add(TEXT("Fade"), 1.f);
			FX->SpawnMesh(Swirl);
		}

		// Glowing icy-cyan arcane rune circle spinning rapidly beneath the wind cyclone
		SpawnGroundRuneCircle(FX, CycloneGround, FLinearColor(0.45f, 0.95f, 1.00f, 1.f), 1.2f * VortexScale, 3.6f * VortexScale, 1.9f, 240.f);

		// Swirling meadow leaves & bright white-cyan wind slipstream streaks
		FFXBurstSpec Leaves;
		Leaves.Kind = EFXSpriteKind::Leaf;
		Leaves.Mode = EFXSpriteMode::Tumble;
		Leaves.Count = 48;
		Leaves.SpeedMin = 850.f;
		Leaves.SpeedMax = 2150.f;
		Leaves.ConeHalfAngle = 140.f;
		Leaves.UpBias = 0.92f;
		Leaves.SpawnRadius = 180.f;
		Leaves.LifeMin = 0.65f;
		Leaves.LifeMax = 1.45f;
		Leaves.SizeMin = 24.f;
		Leaves.SizeMax = 52.f;
		Leaves.ColorA = FLinearColor(0.48f, 0.86f, 0.56f);
		Leaves.ColorB = FLinearColor(0.72f, 0.96f, 1.00f);
		Leaves.RotationSpeedMax = 520.f;
		FX->Burst(Leaves, FTransform(CycloneGround + FVector(0.f, 0.f, 90.f)), SqrtScale);

		FFXBurstSpec Streaks;
		Streaks.Kind = EFXSpriteKind::Glow;
		Streaks.Mode = EFXSpriteMode::VelocityAligned;
		Streaks.Count = 42;
		Streaks.SpeedMin = 1300.f;
		Streaks.SpeedMax = 2800.f;
		Streaks.ConeHalfAngle = HalfAngle;
		Streaks.UpBias = 0.35f;
		Streaks.LifeMin = 0.28f;
		Streaks.LifeMax = 0.62f;
		Streaks.SizeMin = 22.f;
		Streaks.SizeMax = 50.f;
		Streaks.ColorA = FLinearColor(0.75f, 0.97f, 1.00f) * 3.5f;
		Streaks.ColorB = FLinearColor::White * 2.2f;
		Streaks.Stretch = 5.0f;
		FX->Burst(Streaks, FTransform(DirN.Rotation(), Origin), SqrtScale);

		if (UWizardAudioSubsystem* Audio = UWizardAudioSubsystem::Get(World))
		{
			Audio->PlayAt(TEXT("Gust_Wind"), CycloneGround + FVector(0.f, 0.f, 120.f), 1.05f, 1.f);
		}
		UWizardCameraLibrary::Shake(World, UWizardShakeWind::StaticClass(), 1.05f * ChargeScale);
		UWizardCameraLibrary::FovKick(World, Spell->FovKick * ChargeScale, 0.08f, 0.35f);

		// Staggered cyclone updraft pulses keeping shattered blocks/crates swirling high in the tornado!
		const uint32 ResetSerial = Destruction->GetResetSerial();
		for (int32 p = 0; p < 3; ++p)
		{
			const float PulseDelay = 0.12f + p * 0.16f;
			FX->Delay(PulseDelay, [World, CycloneGround, VortexScale, LiftImpact, ResetSerial]()
			{
				if (UWizardDestructionSubsystem* Dest = UWizardDestructionSubsystem::Get(World))
				{
					if (Dest->GetResetSerial() == ResetSerial)
					{
						Dest->PushDynamicBodies(CycloneGround + FVector(0.f, 0.f, 200.f), 860.f * VortexScale, 1450.f, 0.88f);
					}
				}
			});
		}
	}

	void CastStrike(UWizardAbilityComponent* Caster, USpellDataAsset* Spell, const FVector& AimPoint, float ChargeT01)
	{
		UWorld* World = Caster ? Caster->GetWorld() : nullptr;
		UWizardFXSubsystem* FX = UWizardFXSubsystem::Get(World);
		UWizardDestructionSubsystem* Destruction = UWizardDestructionSubsystem::Get(World);
		if (!World || !FX || !Destruction)
		{
			return;
		}

		AActor* Owner = Caster->GetOwner();
		const float ChargeScale = Spell->GetChargeScale(ChargeT01);
		const float SqrtScale = FMath::Sqrt(ChargeScale);
		const int32 Strikes = FMath::Max(1, Spell->StrikeCount);
		const uint32 ResetSerial = Destruction->GetResetSerial();
		TWeakObjectPtr<AActor> WeakOwner(Owner);
		TWeakObjectPtr<USpellDataAsset> WeakSpell(Spell);

		for (int32 s = 0; s < Strikes; ++s)
		{
			const float DelaySec = s * Spell->StrikeInterval;
			const FVector Scatter = (s == 0) ? FVector::ZeroVector : FVector(FMath::FRandRange(-240.f, 240.f), FMath::FRandRange(-240.f, 240.f), 0.f);
			const FVector RawTarget = AimPoint + Scatter;

			FX->Delay(DelaySec, [World, WeakOwner, WeakSpell, RawTarget, ChargeT01, ChargeScale, SqrtScale, ResetSerial, s]()
			{
				UWizardDestructionSubsystem* Dest = UWizardDestructionSubsystem::Get(World);
				UWizardFXSubsystem* FXSys = UWizardFXSubsystem::Get(World);
				USpellDataAsset* Sp = WeakSpell.Get();
				if (!Dest || !FXSys || !Sp || Dest->GetResetSerial() != ResetSerial)
				{
					return;
				}

				const FVector Target = FindGroundLocation(World, RawTarget, WeakOwner.Get(), static_cast<float>(RawTarget.Z));

				// 1. Primary blinding multi-branch Crimson Desert sky-to-ground lightning bolt (1800cm tall, 2.0x luminous core)
				const FVector SkyStart = Target + FVector(FMath::FRandRange(-240.f, 240.f), FMath::FRandRange(-240.f, 240.f), 1800.f);
				FFXLightningParams MainBolt;
				MainBolt.Start = SkyStart;
				MainBolt.End = Target + FVector(0.f, 0.f, 15.f);
				MainBolt.Color = FLinearColor(0.30f, 0.76f, 1.00f, 1.f);
				MainBolt.LifeTime = 0.48f;
				MainBolt.Thickness = 26.f * SqrtScale;
				MainBolt.Jag = 185.f;
				MainBolt.Subdivisions = 4;
				MainBolt.BranchCount = 6;
				MainBolt.BranchLength = 560.f;
				MainBolt.BranchThicknessScale = 0.54f;
				MainBolt.RejitInterval = 0.04f;
				FXSys->SpawnLightning(MainBolt);

				// 2. Twin forked electric-violet leader bolt converging on the strike point
				FFXLightningParams TwinBolt;
				TwinBolt.Start = SkyStart + FVector(FMath::FRandRange(-360.f, 360.f), FMath::FRandRange(-360.f, 360.f), 120.f);
				TwinBolt.End = Target + FVector(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(-30.f, 30.f), 18.f);
				TwinBolt.Color = FLinearColor(0.56f, 0.34f, 1.00f, 1.f);
				TwinBolt.LifeTime = 0.42f;
				TwinBolt.Thickness = 17.f * SqrtScale;
				TwinBolt.Jag = 210.f;
				TwinBolt.Subdivisions = 4;
				TwinBolt.BranchCount = 4;
				TwinBolt.BranchLength = 460.f;
				TwinBolt.BranchThicknessScale = 0.50f;
				TwinBolt.RejitInterval = 0.045f;
				FXSys->SpawnLightning(TwinBolt);

				// 3. 8 radial branching ground electric spider-web arcs racing across the meadow!
				const int32 WebCount = (s == 0) ? 8 : 6;
				for (int32 w = 0; w < WebCount; ++w)
				{
					const float AngleRad = (2.f * PI * static_cast<float>(w) / static_cast<float>(WebCount)) + FMath::FRandRange(-0.20f, 0.20f);
					const float ArcLen = FMath::FRandRange(500.f, 860.f) * SqrtScale;
					const FVector RadialDir(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.f);
					const FVector EndSample = Target + RadialDir * ArcLen;
					const FVector EndGround = FindGroundLocation(World, EndSample, WeakOwner.Get(), static_cast<float>(Target.Z));

					FFXLightningParams GroundArc;
					GroundArc.Start = Target + FVector(0.f, 0.f, 22.f);
					GroundArc.End = EndGround + FVector(0.f, 0.f, 18.f);
					GroundArc.Color = (w % 2 == 0) ? FLinearColor(0.30f, 0.78f, 1.00f, 1.f) : FLinearColor(0.56f, 0.36f, 1.00f, 1.f);
					GroundArc.LifeTime = 0.44f;
					GroundArc.Thickness = FMath::FRandRange(9.5f, 13.5f) * SqrtScale;
					GroundArc.Jag = 74.f;
					GroundArc.Subdivisions = 3;
					GroundArc.BranchCount = 2;
					GroundArc.BranchLength = 260.f;
					GroundArc.BranchThicknessScale = 0.55f;
					GroundArc.RejitInterval = 0.045f;
					FXSys->SpawnLightning(GroundArc);
				}

				if (s == 0)
				{
					SpawnGroundRuneCircle(FXSys, Target, FLinearColor(0.45f, 0.55f, 1.00f, 1.f), 0.9f, 3.0f * SqrtScale, 0.75f, 280.f);
				}

				FSpellImpact Impact = Sp->MakeImpact(Target, FVector::DownVector, ChargeT01, WeakOwner.Get());
				Impact.Multipliers.Stone *= Sp->StrikeShatterBonus;
				Impact.Multipliers.Rock *= Sp->StrikeShatterBonus;

				if (s == 0)
				{
					UWizardSpellLibrary::Impact(World, Impact);
				}
				else
				{
					UWizardSpellLibrary::ImpactSecondary(World, Impact);
				}
			});
		}
	}
}

void UWizardSpellLibrary::CastSpell(UWizardAbilityComponent* Caster, USpellDataAsset* Spell, const FVector& AimPoint, const FVector& AimDirection, float ChargeT01)
{
	if (!Caster || !Spell)
	{
		return;
	}

	PlayCastAnim(Caster, ChargeT01 >= 0.45f);

	UWorld* World = Caster->GetWorld();
	const FVector HandLoc = Caster->GetHandLocation();
	const float ChargeScale = Spell->GetChargeScale(ChargeT01);
	const FLinearColor ArcaneCol = GetElementArcaneColor(Spell->Element);

	if (UWizardFXSubsystem* FX = UWizardFXSubsystem::Get(World))
	{
		if (AActor* Owner = Caster->GetOwner())
		{
			if (USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>())
			{
				FX->SpawnNiagaraAttached(Spell->NiagaraCast, Mesh, TEXT("hand_r"), ChargeScale);
			}

			// Project glowing arcane rune circle onto the meadow beneath the casting wizard!
			const FVector CasterGround = FindGroundLocation(World, Owner->GetActorLocation(), Owner, static_cast<float>(Owner->GetActorLocation().Z - 90.f));
			SpawnGroundRuneCircle(FX, CasterGround, ArcaneCol, 1.25f, 2.05f * FMath::Sqrt(ChargeScale), 1.15f, 130.f);
		}

		FFXLightParams CastFlash;
		CastFlash.Color = ArcaneCol;
		CastFlash.Intensity = FMath::Clamp(75.f * FMath::Sqrt(ChargeScale), 45.f, 160.f);
		CastFlash.AttenuationRadius = 460.f;
		CastFlash.Location = HandLoc;
		CastFlash.LifeTime = 0.24f;
		CastFlash.bCastShadows = false;
		FX->SpawnLight(CastFlash);

		const bool bIsLightningCast = (Spell->Element == ESpellElement::Lightning);
		const float MuzzleMul = bIsLightningCast ? 0.35f : 1.0f;

		FFXMeshParams FlashCore;
		FlashCore.MeshName = TEXT("SM_FX_Sphere");
		FlashCore.MaterialName = TEXT("M_FX_Core");
		FlashCore.Transform = FTransform(FRotator::ZeroRotator, HandLoc, FVector(1.f));
		FlashCore.LifeTime = bIsLightningCast ? 0.08f : 0.18f;
		FlashCore.ScaleStart = FVector(0.18f * ChargeScale * MuzzleMul);
		FlashCore.ScaleEnd = FVector(0.55f * ChargeScale * MuzzleMul);
		FlashCore.ScaleTime = bIsLightningCast ? 0.04f : 0.08f;
		FlashCore.FadeOutTime = bIsLightningCast ? 0.05f : 0.12f;
		FlashCore.VectorParams.Add(TEXT("Tint"), ArcaneCol);
		FlashCore.ScalarParams.Add(TEXT("EmissiveScale"), bIsLightningCast ? 5.5f : 8.5f);
		FlashCore.ScalarParams.Add(TEXT("Fade"), 1.f);
		FX->SpawnMesh(FlashCore);

		FFXBurstSpec CastBurst;
		CastBurst.Kind = EFXSpriteKind::Flash;
		CastBurst.Count = bIsLightningCast ? 3 : 5;
		CastBurst.SpeedMin = 40.f;
		CastBurst.SpeedMax = 180.f;
		CastBurst.ConeHalfAngle = 40.f;
		CastBurst.LifeMin = bIsLightningCast ? 0.05f : 0.08f;
		CastBurst.LifeMax = bIsLightningCast ? 0.08f : 0.18f;
		CastBurst.SizeMin = 40.f * MuzzleMul;
		CastBurst.SizeMax = 85.f * MuzzleMul;
		CastBurst.SizeEndMul = bIsLightningCast ? 1.15f : 1.5f;
		CastBurst.ColorA = FLinearColor::White * (bIsLightningCast ? 2.2f : 3.5f);
		CastBurst.ColorB = ArcaneCol * (bIsLightningCast ? 1.8f : 2.5f);
		FX->Burst(CastBurst, FTransform(AimDirection.Rotation(), HandLoc), FMath::Sqrt(ChargeScale));
	}

	if (UWizardAudioSubsystem* Audio = UWizardAudioSubsystem::Get(World))
	{
		if (!Spell->CastSound.IsNone())
		{
			Audio->PlayAt(Spell->CastSound, HandLoc, 0.95f, 1.f);
		}
	}

	switch (Spell->Delivery)
	{
	case ESpellDelivery::Projectile:
	{
		const FVector DirN = AimDirection.GetSafeNormal();
		const FVector SpawnLoc = HandLoc + DirN * 70.f;
		FVector LaunchDir = (AimPoint - SpawnLoc).GetSafeNormal();
		if (LaunchDir.IsNearlyZero() || FVector::DotProduct(LaunchDir, DirN) <= 0.1f)
		{
			LaunchDir = DirN;
		}
		AWizardProjectile::Launch(Spell, ChargeT01, SpawnLoc, LaunchDir, Caster->GetOwner());
		break;
	}

	case ESpellDelivery::GroundLine:
		CastGroundLine(Caster, Spell, AimPoint, AimDirection, ChargeT01);
		break;

	case ESpellDelivery::Cone:
		CastCone(Caster, Spell, AimDirection, ChargeT01);
		break;

	case ESpellDelivery::Strike:
		CastStrike(Caster, Spell, AimPoint, ChargeT01);
		break;

	default:
		break;
	}
}

int32 UWizardSpellLibrary::Impact(const UObject* WorldContext, const FSpellImpact& Impact)
{
	SpawnImpactFX(WorldContext, Impact, true);

	int32 Broken = 0;
	if (UWizardDestructionSubsystem* Destruction = UWizardDestructionSubsystem::Get(WorldContext))
	{
		Broken = Destruction->ImpactRadial(Impact);
	}

	if (Impact.Spell)
	{
		if (Impact.Spell->HitStop > KINDA_SMALL_NUMBER && (Broken >= 2 || Impact.ChargeScale >= 1.4f))
		{
			if (UWizardFXSubsystem* FX = UWizardFXSubsystem::Get(WorldContext))
			{
				FX->HitStop(Impact.Spell->HitStop, 0.08f);
			}
		}

		TSubclassOf<UCameraShakeBase> ShakeCls = Impact.Spell->ShakeClass;
		if (!ShakeCls)
		{
			switch (Impact.Element)
			{
			case ESpellElement::Fire:      ShakeCls = UWizardShakeExplosion::StaticClass(); break;
			case ESpellElement::Earth:     ShakeCls = UWizardShakeRumble::StaticClass(); break;
			case ESpellElement::Wind:      ShakeCls = UWizardShakeWind::StaticClass(); break;
			case ESpellElement::Lightning: ShakeCls = UWizardShakeThunder::StaticClass(); break;
			default:                       ShakeCls = UWizardShakeExplosion::StaticClass(); break;
			}
		}
		UWizardCameraLibrary::ShakeAtLocation(WorldContext, ShakeCls, Impact.Origin, 1200.f, 4600.f, Impact.Spell->ShakeScale * FMath::Sqrt(Impact.ChargeScale) * 1.15f);

		if (Impact.Spell->FovKick > KINDA_SMALL_NUMBER)
		{
			UWizardCameraLibrary::FovKick(WorldContext, Impact.Spell->FovKick * FMath::Sqrt(Impact.ChargeScale), 0.045f, 0.32f);
		}
		UWizardCameraLibrary::Pulse(WorldContext, FMath::Clamp(0.32f * Impact.ChargeScale, 0.f, 0.85f), 0.28f, Impact.Spell->Color);

		if (UWizardAudioSubsystem* Audio = UWizardAudioSubsystem::Get(WorldContext))
		{
			if (!Impact.Spell->ImpactSound.IsNone())
			{
				Audio->PlayAt(Impact.Spell->ImpactSound, Impact.Origin, 1.1f * FMath::Sqrt(Impact.ChargeScale), 1.f);
			}
		}
	}

	return Broken;
}

int32 UWizardSpellLibrary::ImpactSecondary(const UObject* WorldContext, const FSpellImpact& Impact)
{
	SpawnImpactFX(WorldContext, Impact, false);

	int32 Broken = 0;
	if (UWizardDestructionSubsystem* Destruction = UWizardDestructionSubsystem::Get(WorldContext))
	{
		Broken = Destruction->ImpactRadial(Impact);
	}

	if (Impact.Spell && !Impact.Spell->ImpactSound.IsNone())
	{
		if (UWizardAudioSubsystem* Audio = UWizardAudioSubsystem::Get(WorldContext))
		{
			Audio->PlayAt(Impact.Spell->ImpactSound, Impact.Origin, 0.75f, FMath::FRandRange(0.92f, 1.12f));
		}
	}
	return Broken;
}

void UWizardSpellLibrary::PlayCastAnim(const UWizardAbilityComponent* Caster, bool bCharged)
{
	if (!Caster)
	{
		return;
	}
	AActor* Owner = Caster->GetOwner();
	USkeletalMeshComponent* Mesh = Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	UAnimInstance* AnimInst = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInst)
	{
		return;
	}

	const TCHAR* Path = bCharged
		? TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack.MM_ChargedAttack")
		: TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01");
	const FSoftObjectPath SoftPath(Path);
	const FString PackageName = SoftPath.GetLongPackageName();
	if (!PackageName.IsEmpty() && FPackageName::DoesPackageExist(PackageName))
	{
		if (UAnimMontage* Montage = Cast<UAnimMontage>(SoftPath.TryLoad()))
		{
			AnimInst->Montage_Play(Montage, bCharged ? 1.15f : 1.35f);
		}
	}
}

void UWizardSpellLibrary::PlayDashAnim(const UWizardAbilityComponent* Caster)
{
	PlayCastAnim(Caster, false);
}

FLinearColor UWizardSpellLibrary::MaterialColor(EDestructibleMaterial Material)
{
	switch (Material)
	{
	case EDestructibleMaterial::Stone:   return FLinearColor(0.58f, 0.56f, 0.52f);
	case EDestructibleMaterial::Wood:    return FLinearColor(0.45f, 0.30f, 0.16f);
	case EDestructibleMaterial::Pottery: return FLinearColor(0.72f, 0.42f, 0.26f);
	case EDestructibleMaterial::Rock:    return FLinearColor(0.42f, 0.40f, 0.38f);
	case EDestructibleMaterial::Crystal: return FLinearColor(0.28f, 0.90f, 1.00f);
	case EDestructibleMaterial::Foliage: return FLinearColor(0.24f, 0.52f, 0.22f);
	default:                             return FLinearColor(0.55f, 0.55f, 0.55f);
	}
}

FName UWizardSpellLibrary::MaterialBreakSound(EDestructibleMaterial Material)
{
	switch (Material)
	{
	case EDestructibleMaterial::Stone:   return TEXT("Break_Stone");
	case EDestructibleMaterial::Wood:    return TEXT("Break_Wood");
	case EDestructibleMaterial::Pottery: return TEXT("Break_Pottery");
	case EDestructibleMaterial::Rock:    return TEXT("Break_Rock");
	case EDestructibleMaterial::Crystal: return TEXT("Break_Crystal");
	case EDestructibleMaterial::Foliage: return TEXT("Break_Foliage");
	default:                             return TEXT("Break_Stone");
	}
}
