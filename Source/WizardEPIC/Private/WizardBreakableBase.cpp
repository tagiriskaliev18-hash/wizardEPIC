#include "WizardBreakableBase.h"

#include "Components/StaticMeshComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardBreakableBase)
#include "WizardDestructionSubsystem.h"

AWizardBreakableBase::AWizardBreakableBase()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void AWizardBreakableBase::BeginPlay()
{
	Super::BeginPlay();
}

void AWizardBreakableBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

int32 AWizardBreakableBase::ApplyImpactRadial(const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	return 0;
}

int32 AWizardBreakableBase::ApplyImpactCone(const FVector& Origin, const FVector& Direction, float HalfAngleDeg, float Range, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	return 0;
}

int32 AWizardBreakableBase::ApplyLaunchUpward(const FVector& Origin, float Radius, float Height, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	return 0;
}

FBoxSphereBounds AWizardBreakableBase::GetBreakableBounds() const
{
	return Definition ? Definition->Bounds.TransformBy(GetActorTransform()) : FBoxSphereBounds(GetActorLocation(), FVector::ZeroVector, 0.f);
}

void AWizardBreakableBase::PushDynamic(const FVector& Origin, float Radius, float Speed, float UpBias)
{
}

void AWizardBreakableBase::Reset()
{
	DestroyLoosePieces();
}

int32 AWizardBreakableBase::GetLoosePieceCount() const
{
	return LoosePieces.Num();
}

void AWizardBreakableBase::TickLoose(float DeltaTime)
{
}

void AWizardBreakableBase::DestroyLoosePieces()
{
}

UDestructiblePieceComponent* AWizardBreakableBase::SpawnLoosePiece(const FPieceDefinition& Piece, const FTransform& PieceWorldTransform, const FVector& Velocity, float Spin)
{
	return nullptr;
}

void AWizardBreakableBase::FillEvent(FDestructibleEvent& Event, const FVector& Location, float Size, const FSpellImpact* Impact) const
{
	Event.Location = Location;
	if (Definition)
	{
		Event.Material = Definition->Material;
		Event.Points = Definition->ScorePerPiece;
		Event.Size = FMath::Max(Definition->Size, 1.f);
	}
	if (Impact)
	{
		Event.Element = Impact->Element;
		Event.ChargeScale = Impact->ChargeScale;
	}
}

int32 AWizardBreakableBase::GetPieceScore() const
{
	return Definition ? Definition->ScorePerPiece : 10;
}

UWizardDestructionSubsystem* AWizardBreakableBase::GetDestruction() const
{
	return nullptr;
}
