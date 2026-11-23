#include "WizardBreakableBase.h"

#include "DestructibleStructure.h"
#include "Engine/World.h"
#include "WizardDestructionSubsystem.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardBreakableBase)

AWizardBreakableBase::AWizardBreakableBase()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void AWizardBreakableBase::BeginPlay()
{
	Super::BeginPlay();

	if (UWizardDestructionSubsystem* Destruction = GetDestruction())
	{
		Destruction->RegisterBreakable(this);
	}
}

void AWizardBreakableBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWizardDestructionSubsystem* Destruction = GetDestruction())
	{
		Destruction->UnregisterBreakable(this);
	}
	DestroyLoosePieces();
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

void AWizardBreakableBase::PushDynamic(const FVector& Origin, float Radius, float Speed, float UpBias)
{
	const float RadiusSq = Radius * Radius;
	for (TObjectPtr<UDestructiblePieceComponent>& Piece : LoosePieces)
	{
		if (!Piece || !IsValid(Piece))
		{
			continue;
		}
		if (Piece->State != EPieceState::Dynamic && Piece->State != EPieceState::Settled)
		{
			continue;
		}

		const FVector Centre = Piece->GetCentre();
		const float DistSq = static_cast<float>(FVector::DistSquared(Origin, Centre));
		if (DistSq > RadiusSq)
		{
			continue;
		}

		const float T = Radius > KINDA_SMALL_NUMBER ? FMath::Sqrt(DistSq) / Radius : 0.f;
		FVector Dir = Centre - Origin;
		Dir.Z *= 0.35f;
		if (Dir.SizeSquared() < KINDA_SMALL_NUMBER)
		{
			Dir = FVector::UpVector;
		}
		Dir.Normalize();
		const FVector Vel = (Dir + FVector(0.f, 0.f, UpBias)).GetSafeNormal() * (Speed * FMath::Lerp(1.f, 0.35f, T));

		if (Piece->State == EPieceState::Settled)
		{
			Piece->Release(Vel, 260.f);
		}
		else
		{
			Piece->AddImpulse(Vel, NAME_None, true);
		}
	}
}

void AWizardBreakableBase::Reset()
{
	DestroyLoosePieces();
}

FBoxSphereBounds AWizardBreakableBase::GetBreakableBounds() const
{
	if (Definition)
	{
		return Definition->Bounds.TransformBy(GetActorTransform());
	}
	return FBoxSphereBounds(GetActorLocation(), FVector(100.f), 175.f);
}

int32 AWizardBreakableBase::GetLoosePieceCount() const
{
	int32 Count = 0;
	for (const TObjectPtr<UDestructiblePieceComponent>& Piece : LoosePieces)
	{
		if (Piece && IsValid(Piece) && Piece->State == EPieceState::Dynamic)
		{
			Count++;
		}
	}
	return Count;
}

void AWizardBreakableBase::TickLoose(float DeltaTime)
{
	for (int32 i = LoosePieces.Num() - 1; i >= 0; --i)
	{
		UDestructiblePieceComponent* Piece = LoosePieces[i];
		if (!Piece || !IsValid(Piece))
		{
			LoosePieces.RemoveAtSwap(i);
			continue;
		}

		if (Piece->State != EPieceState::Dynamic)
		{
			continue;
		}

		const FVector Loc = Piece->GetComponentLocation();
		if (Loc.Z < -2000.f)
		{
			Piece->Remove();
			continue;
		}

		const float LinSpeedSq = static_cast<float>(Piece->GetPhysicsLinearVelocity().SizeSquared());
		const float AngSpeedSq = static_cast<float>(Piece->GetPhysicsAngularVelocityInDegrees().SizeSquared());
		if (LinSpeedSq < 225.f && AngSpeedSq < 225.f) // < 15 cm/s and < 15 deg/s
		{
			Piece->RestTime += DeltaTime;
			if (Piece->RestTime >= 0.75f)
			{
				Piece->Settle();
			}
		}
		else
		{
			Piece->RestTime = 0.f;
		}
	}
}

void AWizardBreakableBase::DestroyLoosePieces()
{
	for (TObjectPtr<UDestructiblePieceComponent>& Piece : LoosePieces)
	{
		if (Piece && IsValid(Piece))
		{
			Piece->DestroyComponent();
		}
	}
	LoosePieces.Reset();
}

UDestructiblePieceComponent* AWizardBreakableBase::SpawnLoosePiece(const FPieceDefinition& PieceDef, const FTransform& PieceWorldTransform, const FVector& Velocity, float Spin)
{
	UDestructiblePieceComponent* Comp = NewObject<UDestructiblePieceComponent>(this);
	if (!Comp)
	{
		return nullptr;
	}

	Comp->SetupAttachment(RootComponent);
	Comp->RegisterComponent();
	Comp->Configure(PieceDef, HealthScale, FTransform::Identity);
	Comp->SetWorldTransform(PieceWorldTransform);
	Comp->Release(Velocity, Spin);
	LoosePieces.Add(Comp);
	return Comp;
}

void AWizardBreakableBase::FillEvent(FDestructibleEvent& Event, const FVector& Location, float Size, const FSpellImpact* Impact) const
{
	Event.Location = Location;
	Event.Material = Definition ? Definition->Material : EDestructibleMaterial::Stone;
	Event.Points = GetPieceScore();
	Event.Size = Size;
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
	return UWizardDestructionSubsystem::Get(this);
}
