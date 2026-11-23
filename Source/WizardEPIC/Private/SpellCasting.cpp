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
#include "SpellDataAsset.h"
#include "SpellProjectile.h"
#include "UObject/SoftObjectPath.h"
#include "WizardAbilityComponent.h"
#include "WizardAudioSubsystem.h"
#include "WizardDestructionSubsystem.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(SpellCasting)

namespace
{
	void SpawnImpactFX(const UObject* WorldContext, const FSpellImpact& Impact, bool bPrimary)
	{
		UWizardFXSubsystem* FX = UWizardFXSubsystem::Get(WorldContext);
		if (!FX)
		{
			return;
		}

		const float Scale = Impact.ChargeScale;
		const FLinearColor Color = Impact.Spell ? Impact.Spell->Color : FLinearColor(1.f, 0.45f, 0.12f);

		if (Impact.Spell && !Impact.Spell->NiagaraImpact.IsNull())
		{
			FX->SpawnNiagara(Impact.Spell->NiagaraImpact, FTransform(Impact.Origin), Scale);
		}

		// Light flash
		if (Impact.Spell && Impact.Spell->LightIntensity > 0.f)
		{
			FFXLightParams LP;
			LP.Color = Color;
			LP.Intensity = Impact.Spell->LightIntensity * Scale * (bPrimary ? 1.f : 0.6f);
			LP.AttenuationRadius = Impact.Spell->LightRadius * FMath::Sqrt(Scale);
			LP.Location = Impact.Origin + FVector(0.f, 0.f, 60.f);
			LP.LifeTime = Impact.Spell->LightDuration;
			LP.AttackTime = 0.02f;
			LP.Flicker = (Impact.Element == ESpellElement::Lightning) ? 0.35f : 0.1f;
			LP.bCastShadows = bPrimary;
			FX->SpawnLight(LP);
		}

		// Core flash sphere
		{
			FFXMeshParams Core;
			Core.MeshName = TEXT("SM_FX_Sphere");
			Core.MaterialName = TEXT("M_FX_Core");
			Core.Transform = FTransform(FRotator::ZeroRotator, Impact.Origin, FVector(1.f));
			Core.LifeTime = 0.36f;
			Core.ScaleStart = FVector(0.5f * Scale);
			Core.ScaleEnd = FVector(3.8f * Scale);
			Core.ScaleTime = 0.18f;
			Core.FadeOutTime = 0.24f;
			Core.VectorParams.Add(TEXT("Tint"), Color);
			Core.ScalarParams.Add(TEXT("EmissiveScale"), 28.f);
			Core.ScalarParams.Add(TEXT("Fade"), 1.f);
			FX->SpawnMesh(Core);
		}

		// Shockwave ring
		{
			FFXMeshParams Ring;
			Ring.MeshName = TEXT("SM_FX_Ring");
			Ring.MaterialName = TEXT("M_FX_Ring");
			Ring.Transform = FTransform(FRotator::ZeroRotator, Impact.Origin + FVector(0.f, 0.f, 18.f), FVector(1.f));
			Ring.LifeTime = 0.45f;
			Ring.ScaleStart = FVector(0.6f * Scale, 0.6f * Scale, 1.f);
			Ring.ScaleEnd = FVector((Impact.Radius / 100.f) * 1.15f, (Impact.Radius / 100.f) * 1.15f, 1.f);
			Ring.ScaleTime = 0.34f;
			Ring.FadeOutTime = 0.26f;
			Ring.VectorParams.Add(TEXT("Tint"), Color);
			Ring.ScalarParams.Add(TEXT("EmissiveScale"), 14.f);
			Ring.ScalarParams.Add(TEXT("Fade"), 1.f);
			FX->SpawnMesh(Ring);
		}

		// Element-specific bursts & decals
		if (Impact.Element == ESpellElement::Fire)
		{
			FFXBurstSpec Flash;
			Flash.Kind = EFXSpriteKind::Flash;
			Flash.Count = bPrimary ? 10 : 5;
			Flash.SpeedMin = 80.f;
			Flash.SpeedMax = 420.f;
			Flash.LifeMin = 0.15f;
			Flash.LifeMax = 0.32f;
			Flash.SizeMin = 120.f;
			Flash.SizeMax = 240.f;
			Flash.SizeEndMul = 1.8f;
			Flash.ColorA = FLinearColor(1.f, 0.85f, 0.45f) * 4.f;
			Flash.ColorB = Color * 2.f;
			FX->Burst(Flash, FTransform(Impact.Origin), Scale);

			FFXBurstSpec Embers;
			Embers.Kind = EFXSpriteKind::Ember;
			Embers.Mode = EFXSpriteMode::VelocityAligned;
			Embers.Count = bPrimary ? 42 : 18;
			Embers.SpeedMin = 450.f;
			Embers.SpeedMax = 1550.f;
			Embers.UpBias = 0.45f;
			Embers.LifeMin = 0.45f;
			Embers.LifeMax = 1.15f;
			Embers.SizeMin = 18.f;
			Embers.SizeMax = 38.f;
			Embers.SizeEndMul = 0.2f;
			Embers.ColorA = FLinearColor(1.f, 0.72f, 0.22f) * 4.f;
			Embers.ColorB = FLinearColor(0.9f, 0.18f, 0.04f) * 2.f;
			Embers.GravityScale = -0.8f;
			Embers.Stretch = 2.8f;
			FX->Burst(Embers, FTransform(Impact.Origin), Scale);

			FFXBurstSpec Smoke;
			Smoke.Kind = EFXSpriteKind::Smoke;
			Smoke.Count = bPrimary ? 24 : 10;
			Smoke.SpeedMin = 140.f;
			Smoke.SpeedMax = 520.f;
			Smoke.UpBias = 0.5f;
			Smoke.LifeMin = 0.9f;
			Smoke.LifeMax = 1.9f;
			Smoke.SizeMin = 120.f;
			Smoke.SizeMax = 240.f;
			Smoke.SizeEndMul = 2.4f;
			Smoke.ColorA = FLinearColor(0.28f, 0.18f, 0.14f);
			Smoke.ColorB = FLinearColor(0.08f, 0.08f, 0.09f);
			Smoke.AlphaStart = 0.72f;
			Smoke.AlphaEnd = 0.f;
			FX->Burst(Smoke, FTransform(Impact.Origin), Scale);

			FFXDecalParams Scorch;
			Scorch.MaterialName = TEXT("M_Decal_Scorch");
			Scorch.Location = Impact.Origin;
			Scorch.Rotation = FRotator(-90.f, FMath::FRandRange(0.f, 360.f), 0.f);
			Scorch.Size = FVector(220.f, Impact.Radius * 0.75f, Impact.Radius * 0.75f);
			Scorch.LifeTime = 24.f;
			FX->SpawnDecal(Scorch);
		}
		else if (Impact.Element == ESpellElement::Lightning)
		{
			FFXBurstSpec Sparks;
			Sparks.Kind = EFXSpriteKind::Spark;
			Sparks.Mode = EFXSpriteMode::VelocityAligned;
			Sparks.Count = bPrimary ? 54 : 24;
			Sparks.SpeedMin = 600.f;
			Sparks.SpeedMax = 1900.f;
			Sparks.UpBias = 0.4f;
			Sparks.LifeMin = 0.25f;
			Sparks.LifeMax = 0.75f;
			Sparks.SizeMin = 14.f;
			Sparks.SizeMax = 32.f;
			Sparks.SizeEndMul = 0.15f;
			Sparks.ColorA = FLinearColor(0.85f, 0.96f, 1.f) * 6.f;
			Sparks.ColorB = FLinearColor(0.25f, 0.65f, 1.f) * 3.f;
			Sparks.GravityScale = -1.1f;
			Sparks.Stretch = 3.5f;
			FX->Burst(Sparks, FTransform(Impact.Origin), Scale);

			FFXDecalParams Scorch;
			Scorch.MaterialName = TEXT("M_Decal_Scorch");
			Scorch.Location = Impact.Origin;
			Scorch.Rotation = FRotator(-90.f, FMath::FRandRange(0.f, 360.f), 0.f);
			Scorch.Size = FVector(220.f, Impact.Radius * 0.65f, Impact.Radius * 0.65f);
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
			FX->Burst(Dust, FTransform(Impact.Origin), Scale);
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

		for (int32 i = 0; i < Count; ++i)
		{
			const float StepDelay = i * Spell->PillarStepDelay;
			const float Dist = Spell->PillarStartDistance + i * Spell->PillarSpacing;
			const FVector SampleXY = StartBase + FlatDir * Dist;

			FX->Delay(StepDelay, [World, WeakOwner, WeakSpell, SampleXY, FlatDir, ChargeT01, ChargeScale, ResetSerial, i]()
			{
				UWizardDestructionSubsystem* Dest = UWizardDestructionSubsystem::Get(World);
				UWizardFXSubsystem* FXSys = UWizardFXSubsystem::Get(World);
				USpellDataAsset* Sp = WeakSpell.Get();
				if (!Dest || !FXSys || !Sp || Dest->GetResetSerial() != ResetSerial)
				{
					return;
				}

				FVector GroundPos = SampleXY;
				FHitResult Hit;
				FCollisionQueryParams QP;
				if (WeakOwner.IsValid())
				{
					QP.AddIgnoredActor(WeakOwner.Get());
				}
				if (World->LineTraceSingleByChannel(Hit, SampleXY + FVector(0.f, 0.f, 1200.f), SampleXY - FVector(0.f, 0.f, 1600.f), ECC_WorldStatic, QP))
				{
					GroundPos = Hit.ImpactPoint;
				}

				const float PillarH = Sp->PillarHeight * FMath::Lerp(0.85f, 1.25f, static_cast<float>(i) / FMath::Max(1, Sp->PillarCount - 1)) * FMath::Sqrt(ChargeScale);
				const float PillarR = Sp->PillarRadius * FMath::Sqrt(ChargeScale);

				FFXMeshParams Pillar;
				Pillar.MeshName = TEXT("SM_FX_Pillar");
				Pillar.MaterialName = TEXT("M_FX_Pillar");
				Pillar.Transform = FTransform(FRotator(FMath::FRandRange(-8.f, 8.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(-8.f, 8.f)), GroundPos, FVector(PillarR / 55.f, PillarR / 55.f, PillarH / 400.f));
				Pillar.LifeTime = Sp->PillarLifetime;
				Pillar.ScaleStart = FVector(1.f, 1.f, 0.05f);
				Pillar.ScaleEnd = FVector(1.f, 1.f, 1.f);
				Pillar.ScaleTime = Sp->PillarRiseTime;
				Pillar.FadeOutTime = Sp->PillarSinkTime;
				Pillar.bCastShadow = true;
				Pillar.bCollision = true;
				Pillar.VectorParams.Add(TEXT("Tint"), Sp->Color);
				Pillar.ScalarParams.Add(TEXT("Fade"), 1.f);
				FXSys->SpawnMesh(Pillar);

				FFXDecalParams Crack;
				Crack.MaterialName = TEXT("M_Decal_Crack");
				Crack.Location = GroundPos;
				Crack.Rotation = FRotator(-90.f, FMath::FRandRange(0.f, 360.f), 0.f);
				Crack.Size = FVector(180.f, PillarR * 2.2f, PillarR * 2.2f);
				Crack.LifeTime = 22.f;
				FXSys->SpawnDecal(Crack);

				FFXBurstSpec Dust;
				Dust.Kind = EFXSpriteKind::Dust;
				Dust.Count = 14;
				Dust.SpeedMin = 220.f;
				Dust.SpeedMax = 680.f;
				Dust.UpBias = 0.7f;
				Dust.LifeMin = 0.6f;
				Dust.LifeMax = 1.25f;
				Dust.SizeMin = 80.f;
				Dust.SizeMax = 170.f;
				Dust.SizeEndMul = 2.f;
				Dust.ColorA = Sp->Color;
				Dust.ColorB = Sp->Color * 0.55f;
				Dust.AlphaStart = 0.75f;
				Dust.AlphaEnd = 0.f;
				FXSys->Burst(Dust, FTransform(GroundPos + FVector(0.f, 0.f, 40.f)), FMath::Sqrt(ChargeScale));

				FFXDebrisParams Rocks;
				Rocks.MeshName = TEXT("SM_FX_Chip");
				Rocks.MaterialName = TEXT("M_FX_Chip_Rock");
				Rocks.Count = 10;
				Rocks.Origin = FTransform(GroundPos + FVector(0.f, 0.f, 35.f));
				Rocks.SpeedMin = 420.f;
				Rocks.SpeedMax = 1100.f;
				Rocks.UpBias = 0.85f;
				FXSys->SpawnDebris(Rocks);

				FSpellImpact Impact = Sp->MakeImpact(GroundPos, FlatDir, ChargeT01, WeakOwner.Get());
				Dest->ImpactLaunchUpward(GroundPos, PillarR * 1.8f, PillarH + 120.f, Impact);

				if (UWizardAudioSubsystem* Audio = UWizardAudioSubsystem::Get(World))
				{
					Audio->PlayAt(TEXT("Erupt_Earth"), GroundPos, 0.9f, FMath::FRandRange(0.9f, 1.1f));
				}
				if (i == 0 || i == 2)
				{
					UWizardCameraLibrary::ShakeAtLocation(World, UWizardShakeRumble::StaticClass(), GroundPos, 1000.f, 3500.f, 0.85f * ChargeScale);
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
		const float ChargeScale = Spell->GetChargeScale(ChargeT01);
		const float Range = Spell->ConeRange * FMath::Sqrt(ChargeScale);
		const float HalfAngle = Spell->ConeHalfAngle;

		const FSpellImpact Impact = Spell->MakeImpact(Origin, DirN, ChargeT01, Owner);
		Destruction->ImpactCone(Origin, DirN, HalfAngle, Range, Impact);

		// Wind ribbons along cone
		for (int32 r = 0; r < 4; ++r)
		{
			const float Dist = 240.f + r * (Range * 0.22f);
			FFXMeshParams Ribbon;
			Ribbon.MeshName = TEXT("SM_FX_Ring");
			Ribbon.MaterialName = TEXT("M_FX_WindRibbon");
			Ribbon.Transform = FTransform(DirN.Rotation() + FRotator(90.f, 0.f, 0.f), Origin + DirN * Dist, FVector(1.f));
			Ribbon.LifeTime = Spell->ConeDuration;
			Ribbon.ScaleStart = FVector(0.5f + r * 0.45f);
			Ribbon.ScaleEnd = FVector((1.8f + r * 1.1f) * FMath::Sqrt(ChargeScale));
			Ribbon.ScaleTime = Spell->ConeDuration * 0.85f;
			Ribbon.FadeOutTime = 0.25f;
			Ribbon.VectorParams.Add(TEXT("Tint"), Spell->Color);
			Ribbon.ScalarParams.Add(TEXT("Fade"), 1.f);
			FX->SpawnMesh(Ribbon);
		}

		// Swirling leaves & wind glow sprites
		FFXBurstSpec Leaves;
		Leaves.Kind = EFXSpriteKind::Leaf;
		Leaves.Mode = EFXSpriteMode::Tumble;
		Leaves.Count = 34;
		Leaves.SpeedMin = 800.f;
		Leaves.SpeedMax = 2100.f;
		Leaves.ConeHalfAngle = HalfAngle;
		Leaves.UpBias = 0.12f;
		Leaves.LifeMin = 0.55f;
		Leaves.LifeMax = 1.25f;
		Leaves.SizeMin = 24.f;
		Leaves.SizeMax = 52.f;
		Leaves.ColorA = FLinearColor(0.55f, 0.85f, 0.62f);
		Leaves.ColorB = Spell->Color;
		Leaves.RotationSpeedMax = 420.f;
		FX->Burst(Leaves, FTransform(DirN.Rotation(), Origin), FMath::Sqrt(ChargeScale));

		FFXBurstSpec Streaks;
		Streaks.Kind = EFXSpriteKind::Glow;
		Streaks.Mode = EFXSpriteMode::VelocityAligned;
		Streaks.Count = 28;
		Streaks.SpeedMin = 1200.f;
		Streaks.SpeedMax = 2600.f;
		Streaks.ConeHalfAngle = HalfAngle;
		Streaks.LifeMin = 0.25f;
		Streaks.LifeMax = 0.55f;
		Streaks.SizeMin = 22.f;
		Streaks.SizeMax = 48.f;
		Streaks.ColorA = Spell->Color * 2.5f;
		Streaks.ColorB = FLinearColor::White * 1.5f;
		Streaks.Stretch = 4.5f;
		FX->Burst(Streaks, FTransform(DirN.Rotation(), Origin), FMath::Sqrt(ChargeScale));

		if (UWizardAudioSubsystem* Audio = UWizardAudioSubsystem::Get(World))
		{
			Audio->PlayAt(TEXT("Gust_Wind"), Origin + DirN * 350.f, 1.f, 1.f);
		}
		UWizardCameraLibrary::Shake(World, UWizardShakeWind::StaticClass(), 0.9f * ChargeScale);
		UWizardCameraLibrary::FovKick(World, Spell->FovKick * ChargeScale, 0.08f, 0.35f);

		// Sustained gust push halfway through ConeDuration
		const uint32 ResetSerial = Destruction->GetResetSerial();
		FX->Delay(Spell->ConeDuration * 0.45f, [World, Origin, DirN, Range, Impact, ResetSerial]()
		{
			if (UWizardDestructionSubsystem* Dest = UWizardDestructionSubsystem::Get(World))
			{
				if (Dest->GetResetSerial() == ResetSerial)
				{
					Dest->PushDynamicBodies(Origin + DirN * (Range * 0.45f), Range * 0.65f, Impact.LaunchSpeed * 0.85f, 0.28f);
				}
			}
		});
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
		const int32 Strikes = FMath::Max(1, Spell->StrikeCount);
		const uint32 ResetSerial = Destruction->GetResetSerial();
		TWeakObjectPtr<AActor> WeakOwner(Owner);
		TWeakObjectPtr<USpellDataAsset> WeakSpell(Spell);

		for (int32 s = 0; s < Strikes; ++s)
		{
			const float DelaySec = s * Spell->StrikeInterval;
			const FVector Scatter = (s == 0) ? FVector::ZeroVector : FVector(FMath::FRandRange(-240.f, 240.f), FMath::FRandRange(-240.f, 240.f), 0.f);
			const FVector Target = AimPoint + Scatter;

			FX->Delay(DelaySec, [World, WeakOwner, WeakSpell, Target, ChargeT01, ChargeScale, ResetSerial, s]()
			{
				UWizardDestructionSubsystem* Dest = UWizardDestructionSubsystem::Get(World);
				UWizardFXSubsystem* FXSys = UWizardFXSubsystem::Get(World);
				USpellDataAsset* Sp = WeakSpell.Get();
				if (!Dest || !FXSys || !Sp || Dest->GetResetSerial() != ResetSerial)
				{
					return;
				}

				const FVector SkyStart = Target + FVector(FMath::FRandRange(-350.f, 350.f), FMath::FRandRange(-350.f, 350.f), 2800.f);
				FFXLightningParams Bolt;
				Bolt.Start = SkyStart;
				Bolt.End = Target;
				Bolt.Color = Sp->Color;
				Bolt.LifeTime = 0.34f;
				Bolt.Thickness = 11.f * FMath::Sqrt(ChargeScale);
				Bolt.Jag = 165.f;
				Bolt.Subdivisions = 4;
				Bolt.BranchCount = 3;
				Bolt.BranchLength = 480.f;
				Bolt.RejitInterval = 0.045f;
				FXSys->SpawnLightning(Bolt);

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

	if (UWizardFXSubsystem* FX = UWizardFXSubsystem::Get(World))
	{
		if (AActor* Owner = Caster->GetOwner())
		{
			if (USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>())
			{
				FX->SpawnNiagaraAttached(Spell->NiagaraCast, Mesh, TEXT("hand_r"), ChargeScale);
			}
		}

		FFXLightParams CastFlash;
		CastFlash.Color = Spell->Color;
		CastFlash.Intensity = 1.2e6f * ChargeScale;
		CastFlash.AttenuationRadius = 700.f;
		CastFlash.Location = HandLoc;
		CastFlash.LifeTime = 0.22f;
		CastFlash.bCastShadows = false;
		FX->SpawnLight(CastFlash);
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
		AWizardProjectile::Launch(Spell, ChargeT01, HandLoc + AimDirection.GetSafeNormal() * 45.f, AimDirection, Caster->GetOwner());
		break;

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
		UWizardCameraLibrary::ShakeAtLocation(WorldContext, ShakeCls, Impact.Origin, 1200.f, 4600.f, Impact.Spell->ShakeScale * FMath::Sqrt(Impact.ChargeScale));

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
		? TEXT("/Game/Variant_Combat/Anims/AM_ChargedAttack.AM_ChargedAttack")
		: TEXT("/Game/Variant_Combat/Anims/AM_ComboAttack.AM_ComboAttack");
	if (UAnimMontage* Montage = Cast<UAnimMontage>(FSoftObjectPath(Path).TryLoad()))
	{
		AnimInst->Montage_Play(Montage, bCharged ? 1.15f : 1.35f);
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
