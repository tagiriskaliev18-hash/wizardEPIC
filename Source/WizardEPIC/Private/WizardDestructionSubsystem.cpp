#include "WizardDestructionSubsystem.h"

#include "CameraFX.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "FXSubsystem.h"
#include "SpellCasting.h"
#include "WizardAudioSubsystem.h"
#include "WizardBreakableBase.h"
#include "WizardScoreSubsystem.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardDestructionSubsystem)

TStatId UWizardDestructionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWizardDestructionSubsystem, STATGROUP_Tickables);
}

UWizardDestructionSubsystem* UWizardDestructionSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	return World ? World->GetSubsystem<UWizardDestructionSubsystem>() : nullptr;
}

void UWizardDestructionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UWizardDestructionSubsystem::Deinitialize()
{
	Breakables.Empty();
	PendingEvents.Empty();
	Super::Deinitialize();
}

void UWizardDestructionSubsystem::Tick(float DeltaTime)
{
	for (int32 i = Breakables.Num() - 1; i >= 0; --i)
	{
		if (!Breakables[i] || !IsValid(Breakables[i]))
		{
			Breakables.RemoveAtSwap(i);
			continue;
		}
		Breakables[i]->TickLoose(DeltaTime);
	}

	SettleCheckAccumulator += DeltaTime;
	if (SettleCheckAccumulator >= 0.5f)
	{
		SettleCheckAccumulator = 0.f;
		EnforceLooseLimit();
	}

	if (!PendingEvents.IsEmpty())
	{
		Flush();
	}
}

void UWizardDestructionSubsystem::RegisterBreakable(AWizardBreakableBase* Breakable)
{
	if (Breakable && !Breakables.Contains(Breakable))
	{
		Breakables.Add(Breakable);
	}
}

void UWizardDestructionSubsystem::UnregisterBreakable(AWizardBreakableBase* Breakable)
{
	Breakables.Remove(Breakable);
}

int32 UWizardDestructionSubsystem::ImpactRadial(const FSpellImpact& Impact)
{
	int32 Total = 0;
	for (TObjectPtr<AWizardBreakableBase>& B : Breakables)
	{
		if (!B || !IsValid(B))
		{
			continue;
		}
		const FBoxSphereBounds Bounds = B->GetBreakableBounds();
		if (FVector::Dist(Impact.Origin, Bounds.Origin) > Bounds.SphereRadius + Impact.Radius)
		{
			continue;
		}
		Total += B->ApplyImpactRadial(Impact, PendingEvents);
	}
	return Total;
}

int32 UWizardDestructionSubsystem::ImpactCone(const FVector& Origin, const FVector& Direction, float HalfAngleDeg, float Range, const FSpellImpact& Impact)
{
	int32 Total = 0;
	for (TObjectPtr<AWizardBreakableBase>& B : Breakables)
	{
		if (!B || !IsValid(B))
		{
			continue;
		}
		const FBoxSphereBounds Bounds = B->GetBreakableBounds();
		if (FVector::Dist(Origin, Bounds.Origin) > Bounds.SphereRadius + Range)
		{
			continue;
		}
		Total += B->ApplyImpactCone(Origin, Direction, HalfAngleDeg, Range, Impact, PendingEvents);
	}
	return Total;
}

int32 UWizardDestructionSubsystem::ImpactLaunchUpward(const FVector& Origin, float Radius, float Height, const FSpellImpact& Impact)
{
	int32 Total = 0;
	for (TObjectPtr<AWizardBreakableBase>& B : Breakables)
	{
		if (!B || !IsValid(B))
		{
			continue;
		}
		const FBoxSphereBounds Bounds = B->GetBreakableBounds();
		if (FVector::Dist2D(Origin, Bounds.Origin) > Bounds.SphereRadius + Radius + 100.f)
		{
			continue;
		}
		Total += B->ApplyLaunchUpward(Origin, Radius, Height, Impact, PendingEvents);
	}
	return Total;
}

void UWizardDestructionSubsystem::PushDynamicBodies(const FVector& Origin, float Radius, float Speed, float UpBias)
{
	for (TObjectPtr<AWizardBreakableBase>& B : Breakables)
	{
		if (B && IsValid(B))
		{
			B->PushDynamic(Origin, Radius, Speed, UpBias);
		}
	}
}

void UWizardDestructionSubsystem::ResetAll()
{
	ResetSerial++;
	PendingEvents.Reset();

	if (UWizardFXSubsystem* FX = UWizardFXSubsystem::Get(this))
	{
		FX->ClearTransient();
	}

	for (TObjectPtr<AWizardBreakableBase>& B : Breakables)
	{
		if (B && IsValid(B))
		{
			B->Reset();
		}
	}

	if (UWizardAudioSubsystem* Audio = UWizardAudioSubsystem::Get(this))
	{
		Audio->Play2D(TEXT("Reset"), 0.8f, 1.f);
	}

	OnArenaReset.Broadcast();
}

int32 UWizardDestructionSubsystem::GetLoosePieceCount() const
{
	int32 Total = 0;
	for (const TObjectPtr<AWizardBreakableBase>& B : Breakables)
	{
		if (B && IsValid(B))
		{
			Total += B->GetLoosePieceCount();
		}
	}
	return Total;
}

int32 UWizardDestructionSubsystem::GetBreakableCount() const
{
	return Breakables.Num();
}

void UWizardDestructionSubsystem::Flush()
{
	TArray<FDestructibleEvent> Batch = MoveTemp(PendingEvents);
	PendingEvents.Reset();

	if (Batch.IsEmpty())
	{
		return;
	}

	ReactToBatch(Batch);
	OnDestructibleBatch.Broadcast(Batch);
}

void UWizardDestructionSubsystem::ReactToBatch(const TArray<FDestructibleEvent>& Events)
{
	if (UWizardScoreSubsystem* Score = UWizardScoreSubsystem::Get(this))
	{
		Score->HandleDestructibleBatch(Events);
	}

	UWizardFXSubsystem* FX = UWizardFXSubsystem::Get(this);
	UWizardAudioSubsystem* Audio = UWizardAudioSubsystem::Get(this);

	int32 DustSpawned = 0;
	bool bAnyCollapse = false;
	FVector CollapseLoc = FVector::ZeroVector;

	for (const FDestructibleEvent& Ev : Events)
	{
		if (Ev.bIsCollapseBonus)
		{
			bAnyCollapse = true;
			CollapseLoc = Ev.Location;
			continue;
		}

		if (Audio && DustSpawned < 3)
		{
			const FName Cue = UWizardSpellLibrary::MaterialBreakSound(Ev.Material);
			Audio->PlayAt(Cue, Ev.Location, FMath::Clamp(Ev.Size / 120.f, 0.5f, 1.2f), 1.f);
		}

		if (FX && DustSpawned < 6)
		{
			FFXBurstSpec Dust;
			Dust.Kind = (Ev.Material == EDestructibleMaterial::Foliage) ? EFXSpriteKind::Leaf : EFXSpriteKind::Dust;
			Dust.Mode = (Ev.Material == EDestructibleMaterial::Foliage) ? EFXSpriteMode::Tumble : EFXSpriteMode::Billboard;
			Dust.Count = FMath::Clamp(FMath::RoundToInt(Ev.Size / 25.f), 3, 8);
			Dust.SpeedMin = 90.f;
			Dust.SpeedMax = 320.f;
			Dust.UpBias = 0.25f;
			Dust.LifeMin = 0.55f;
			Dust.LifeMax = 1.2f;
			Dust.SizeMin = 45.f;
			Dust.SizeMax = 110.f;
			Dust.SizeEndMul = 2.f;
			Dust.ColorA = UWizardSpellLibrary::MaterialColor(Ev.Material);
			Dust.ColorB = Dust.ColorA * 0.7f;
			Dust.AlphaStart = 0.65f;
			Dust.AlphaEnd = 0.f;
			FX->Burst(Dust, FTransform(Ev.Location), 1.f, FLinearColor::White);
			DustSpawned++;
		}
	}

	if (bAnyCollapse)
	{
		if (Audio)
		{
			Audio->PlayAt(TEXT("Collapse"), CollapseLoc, 1.2f, 0.95f);
		}
		UWizardCameraLibrary::ShakeAtLocation(this, UWizardShakeExplosion::StaticClass(), CollapseLoc, 1000.f, 4500.f, 0.85f);
	}
	else if (Events.Num() >= 3)
	{
		UWizardCameraLibrary::ShakeAtLocation(this, UWizardShakeImpact::StaticClass(), Events[0].Location, 600.f, 2400.f, 0.45f);
	}
}

void UWizardDestructionSubsystem::EnforceLooseLimit()
{
	if (GetLoosePieceCount() <= MaxLoosePieces)
	{
		return;
	}
	// If above limit, nudge breakables to tick extra settling time
	for (TObjectPtr<AWizardBreakableBase>& B : Breakables)
	{
		if (B && IsValid(B))
		{
			B->TickLoose(0.5f);
		}
	}
}
