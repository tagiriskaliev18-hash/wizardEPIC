#include "DestructibleStructure.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DestructibleStructure)

// Skeleton build: piece release and the support cascade are implemented in the next step.

UDestructiblePieceComponent::UDestructiblePieceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetNotifyRigidBodyCollision(false);
	BodyInstance.bUseCCD = true;
}

void UDestructiblePieceComponent::Configure(const FPieceDefinition& Definition, float InHealthScale, const FTransform& LocalTransform)
{
}

void UDestructiblePieceComponent::Release(const FVector& Velocity, float SpinDegPerSec)
{
}

void UDestructiblePieceComponent::Settle()
{
}

void UDestructiblePieceComponent::RestoreIntact()
{
}

void UDestructiblePieceComponent::Remove()
{
}

FVector UDestructiblePieceComponent::GetCentre() const
{
	return GetComponentLocation();
}

FVector UDestructiblePieceComponent::ComputeLaunchVelocity(const FSpellImpact& Impact, const FVector& Centre, float Mass)
{
	return Impact.LaunchVelocityAt(Centre);
}

ADestructibleStructure::ADestructibleStructure()
{
}

void ADestructibleStructure::BeginPlay()
{
	Super::BeginPlay();
}

void ADestructibleStructure::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

int32 ADestructibleStructure::ApplyImpactRadial(const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	return 0;
}

int32 ADestructibleStructure::ApplyImpactCone(const FVector& Origin, const FVector& Direction, float HalfAngleDeg, float Range, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	return 0;
}

int32 ADestructibleStructure::ApplyLaunchUpward(const FVector& Origin, float Radius, float Height, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	return 0;
}

void ADestructibleStructure::Reset()
{
	Super::Reset();
}

FBoxSphereBounds ADestructibleStructure::GetBreakableBounds() const
{
	return Definition ? Definition->Bounds.TransformBy(GetActorTransform()) : FBoxSphereBounds(GetActorLocation(), FVector::ZeroVector, 0.f);
}

int32 ADestructibleStructure::GetLoosePieceCount() const
{
	return Super::GetLoosePieceCount();
}

int32 ADestructibleStructure::GetPieceCount() const
{
	return Pieces.Num();
}

int32 ADestructibleStructure::GetReleasedCount() const
{
	return 0;
}

bool ADestructibleStructure::IsFullyIntact() const
{
	return true;
}

bool ADestructibleStructure::GetPieceCentre(int32 PieceIndex, FVector& OutCentre) const
{
	if (!Pieces.IsValidIndex(PieceIndex))
	{
		return false;
	}
	OutCentre = Pieces[PieceIndex]->GetCentre();
	return true;
}

int32 ADestructibleStructure::DamagePiece(int32 PieceIndex, float Damage, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	return 0;
}

void ADestructibleStructure::BuildPieces()
{
}

void ADestructibleStructure::ScheduleCheck(int32 PieceIndex, float Delay)
{
}

void ADestructibleStructure::ProcessIntegrity()
{
}

void ADestructibleStructure::ReleasePiece(int32 PieceIndex, const FVector& Velocity, float Spin, const FSpellImpact* Impact, bool bFromCascade, TArray<FDestructibleEvent>& OutEvents)
{
}

void ADestructibleStructure::PropagateLoss(int32 ReleasedIndex, const FSpellImpact* Impact, TArray<FDestructibleEvent>& OutEvents)
{
}

void ADestructibleStructure::AwardCollapseIfDue(const FSpellImpact* Impact, TArray<FDestructibleEvent>& OutEvents)
{
}
