#include "WizardBreakableSet.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "DestructibleStructure.h"
#include "Engine/StaticMesh.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardBreakableSet)

AWizardBreakableSet::AWizardBreakableSet()
{
	PrimaryActorTick.bCanEverTick = false;

	Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Instances"));
	Instances->SetupAttachment(RootComponent);
	Instances->SetMobility(EComponentMobility::Movable);
	Instances->SetCollisionProfileName(TEXT("BlockAll"));
}

void AWizardBreakableSet::BeginPlay()
{
	Super::BeginPlay();

	if (Definition && Definition->WholeMesh && Instances && !Instances->GetStaticMesh())
	{
		Instances->SetStaticMesh(Definition->WholeMesh);
	}

	const int32 Count = Instances ? Instances->GetInstanceCount() : 0;
	InitialInstanceCount = Count;
	InstanceDamage.Init(0.f, Count);
	InstanceAlive.Init(1, Count);
	InitialTransforms.SetNum(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		Instances->GetInstanceTransform(i, InitialTransforms[i], true);
	}
}

int32 AWizardBreakableSet::AddInstance(const FTransform& WorldTransform)
{
	if (!Instances)
	{
		return INDEX_NONE;
	}
	const int32 Idx = Instances->AddInstance(WorldTransform, true);
	InstanceDamage.Add(0.f);
	InstanceAlive.Add(1);
	InitialTransforms.Add(WorldTransform);
	InitialInstanceCount = Instances->GetInstanceCount();
	return Idx;
}

int32 AWizardBreakableSet::GetInstanceCount() const
{
	return Instances ? Instances->GetInstanceCount() : 0;
}

bool AWizardBreakableSet::GetInstanceTransform(int32 Index, FTransform& OutTransform) const
{
	if (!Instances || Index < 0 || Index >= Instances->GetInstanceCount())
	{
		return false;
	}
	return Instances->GetInstanceTransform(Index, OutTransform, true);
}

int32 AWizardBreakableSet::ApplyImpactRadial(const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	const float RadiusSq = Impact.Radius * Impact.Radius;
	return DamageInstances(Impact, [&](const FVector& Centre, FVector& OutLaunch, float& OutFalloff) -> bool
	{
		const float DistSq = static_cast<float>(FVector::DistSquared(Impact.Origin, Centre));
		if (DistSq <= RadiusSq)
		{
			OutLaunch = Impact.Origin;
			const float T = Impact.Radius > KINDA_SMALL_NUMBER ? FMath::Clamp(FMath::Sqrt(DistSq) / Impact.Radius, 0.f, 1.f) : 0.f;
			OutFalloff = FMath::Lerp(1.f, 0.35f, T);
			return true;
		}
		return false;
	}, OutEvents);
}

int32 AWizardBreakableSet::ApplyImpactCone(const FVector& Origin, const FVector& Direction, float HalfAngleDeg, float Range, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	const FVector DirN = Direction.GetSafeNormal();
	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(HalfAngleDeg, 1.f, 89.f)));
	const float RangeSq = Range * Range;

	return DamageInstances(Impact, [&](const FVector& Centre, FVector& OutLaunch, float& OutFalloff) -> bool
	{
		const FVector To = Centre - Origin;
		const float DistSq = static_cast<float>(To.SizeSquared());
		if (DistSq > RangeSq || DistSq < KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const float Dist = FMath::Sqrt(DistSq);
		if (FVector::DotProduct(DirN, To / Dist) < CosHalf)
		{
			return false;
		}
		OutLaunch = Origin;
		OutFalloff = 1.f - 0.55f * (Dist / FMath::Max(1.f, Range));
		return true;
	}, OutEvents);
}

int32 AWizardBreakableSet::ApplyLaunchUpward(const FVector& Origin, float Radius, float Height, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	const float RadiusSq = Radius * Radius;
	return DamageInstances(Impact, [&](const FVector& Centre, FVector& OutLaunch, float& OutFalloff) -> bool
	{
		const float DZ = static_cast<float>(Centre.Z - Origin.Z);
		if (DZ < -120.f || DZ > Height)
		{
			return false;
		}
		const float DistSq2D = static_cast<float>(FVector::DistSquared2D(Origin, Centre));
		if (DistSq2D <= RadiusSq)
		{
			OutLaunch = Origin;
			const float T = Radius > KINDA_SMALL_NUMBER ? FMath::Clamp(FMath::Sqrt(DistSq2D) / Radius, 0.f, 1.f) : 0.f;
			OutFalloff = FMath::Lerp(1.f, 0.45f, T);
			return true;
		}
		return false;
	}, OutEvents);
}

void AWizardBreakableSet::Reset()
{
	Super::Reset();

	if (!Instances)
	{
		return;
	}

	const int32 Count = FMath::Min(Instances->GetInstanceCount(), InitialTransforms.Num());
	InstanceDamage.Init(0.f, Count);
	InstanceAlive.Init(1, Count);
	for (int32 i = 0; i < Count; ++i)
	{
		Instances->UpdateInstanceTransform(i, InitialTransforms[i], true, i == Count - 1, true);
	}
}

FBoxSphereBounds AWizardBreakableSet::GetBreakableBounds() const
{
	if (Instances && Instances->GetInstanceCount() > 0)
	{
		return Instances->Bounds;
	}
	return Super::GetBreakableBounds();
}

int32 AWizardBreakableSet::DamageInstances(const FSpellImpact& Impact, const TFunctionRef<bool(const FVector& Centre, FVector& OutLaunch, float& OutFalloff)>& Selector, TArray<FDestructibleEvent>& OutEvents)
{
	if (!Instances || !Definition)
	{
		return 0;
	}

	const float MaxHP = FMath::Max(1.f, Definition->Health * FMath::Max(0.1f, HealthScale));
	const float MatMul = Impact.Multipliers.For(Definition->Material);
	int32 BrokenCount = 0;
	bool bDirty = false;

	const int32 Count = FMath::Min(Instances->GetInstanceCount(), InstanceAlive.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		if (!InstanceAlive[i])
		{
			continue;
		}

		FTransform T;
		if (!Instances->GetInstanceTransform(i, T, true))
		{
			continue;
		}

		FVector LaunchOrigin;
		float Falloff = 1.f;
		if (!Selector(T.GetLocation(), LaunchOrigin, Falloff))
		{
			continue;
		}

		const float Dmg = Impact.Damage * FMath::Clamp(Falloff, 0.f, 1.f) * MatMul;
		InstanceDamage[i] += Dmg;
		if (InstanceDamage[i] >= MaxHP)
		{
			BrokenCount += BreakInstance(i, LaunchOrigin, Impact, OutEvents);
			bDirty = true;
		}
	}

	if (bDirty)
	{
		Instances->MarkRenderStateDirty();
	}

	return BrokenCount;
}

int32 AWizardBreakableSet::BreakInstance(int32 Index, const FVector& LaunchOrigin, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	if (!Instances || !InstanceAlive.IsValidIndex(Index) || !InstanceAlive[Index])
	{
		return 0;
	}

	FTransform WorldT;
	Instances->GetInstanceTransform(Index, WorldT, true);

	InstanceAlive[Index] = 0;
	FTransform HiddenT = WorldT;
	HiddenT.SetScale3D(FVector::ZeroVector);
	Instances->UpdateInstanceTransform(Index, HiddenT, true, true, true);

	if (Definition && !Definition->Pieces.IsEmpty())
	{
		for (const FPieceDefinition& PieceDef : Definition->Pieces)
		{
			const FTransform PieceWorld = PieceDef.LocalTransform * WorldT;
			const FVector Vel = UDestructiblePieceComponent::ComputeLaunchVelocity(Impact, PieceWorld.GetLocation(), PieceDef.Mass);
			SpawnLoosePiece(PieceDef, PieceWorld, Vel, Impact.Spin);
		}
	}

	FDestructibleEvent Ev;
	FillEvent(Ev, WorldT.GetLocation(), Definition ? Definition->Size : 80.f, &Impact);
	Ev.bWasWholeProp = true;
	OutEvents.Add(Ev);

	OnInstanceBroken(Index, WorldT, Impact);
	return 1;
}

bool AWizardBreakableSet::IsInstanceAlive(int32 Index) const
{
	return InstanceAlive.IsValidIndex(Index) && InstanceAlive[Index] != 0;
}
