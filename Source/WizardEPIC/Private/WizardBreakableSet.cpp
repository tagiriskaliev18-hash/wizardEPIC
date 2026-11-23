#include "WizardBreakableSet.h"

#include "Components/InstancedStaticMeshComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardBreakableSet)

AWizardBreakableSet::AWizardBreakableSet()
{
	Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Instances"));
	SetRootComponent(Instances);
	Instances->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void AWizardBreakableSet::BeginPlay()
{
	Super::BeginPlay();
	InitialInstanceCount = Instances ? Instances->GetInstanceCount() : 0;
	InstanceAlive.SetNum(InitialInstanceCount, EAllowShrinking::No);
	InstanceDamage.SetNum(InitialInstanceCount);
}

int32 AWizardBreakableSet::AddInstance(const FTransform& WorldTransform)
{
	if (!Instances)
	{
		return -1;
	}
	const int32 Index = Instances->AddInstance(WorldTransform, true);
	if (Index >= 0)
	{
		InstanceAlive.SetNum(Index + 1);
		InstanceAlive[Index] = true;
		InstanceDamage.SetNum(Index + 1);
		InstanceDamage[Index] = 0.f;
	}
	return Index;
}

int32 AWizardBreakableSet::GetInstanceCount() const
{
	return Instances ? Instances->GetInstanceCount() : 0;
}

bool AWizardBreakableSet::GetInstanceTransform(int32 Index, FTransform& OutTransform) const
{
	return Instances && Instances->GetInstanceTransform(Index, OutTransform, true);
}

int32 AWizardBreakableSet::ApplyImpactRadial(const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	return 0;
}

int32 AWizardBreakableSet::ApplyImpactCone(const FVector& Origin, const FVector& Direction, float HalfAngleDeg, float Range, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	return 0;
}

int32 AWizardBreakableSet::ApplyLaunchUpward(const FVector& Origin, float Radius, float Height, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	return 0;
}

void AWizardBreakableSet::Reset()
{
	Super::Reset();
}

FBoxSphereBounds AWizardBreakableSet::GetBreakableBounds() const
{
	return Instances ? Instances->Bounds : FBoxSphereBounds(GetActorLocation(), FVector::ZeroVector, 0.f);
}

int32 AWizardBreakableSet::BreakInstance(int32 Index, const FVector& LaunchOrigin, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	return 0;
}

bool AWizardBreakableSet::IsInstanceAlive(int32 Index) const
{
	return InstanceAlive.IsValidIndex(Index) && InstanceAlive[Index];
}

int32 AWizardBreakableSet::DamageInstances(const FSpellImpact& Impact, const TFunctionRef<bool(const FVector& Centre, FVector& OutLaunch)>& Selector, TArray<FDestructibleEvent>& OutEvents)
{
	return 0;
}
