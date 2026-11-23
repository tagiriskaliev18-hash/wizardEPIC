#include "DestructibleStructure.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "WizardDestructionSubsystem.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(DestructibleStructure)

UDestructiblePieceComponent::UDestructiblePieceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetMobility(EComponentMobility::Movable);
	SetGenerateOverlapEvents(false);
	SetCollisionProfileName(TEXT("BlockAll"));
	SetSimulatePhysics(false);
}

void UDestructiblePieceComponent::Configure(const FPieceDefinition& Definition, float HealthScale, const FTransform& LocalTransform)
{
	InitialLocalTransform = LocalTransform;
	Health = FMath::Max(1.f, Definition.Health * FMath::Max(0.1f, HealthScale));
	Damage = 0.f;
	bGrounded = Definition.bGrounded;
	Extent = Definition.Extent;
	PieceMass = FMath::Max(1.f, Definition.Mass);
	RestTime = 0.f;
	State = EPieceState::Intact;

	if (Definition.Mesh)
	{
		SetStaticMesh(Definition.Mesh);
	}

	SetSimulatePhysics(false);
	SetEnableGravity(false);
	SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetCollisionResponseToAllChannels(ECR_Block);
	SetVisibility(true);
	SetHiddenInGame(false);
	SetRelativeTransform(LocalTransform);
}

void UDestructiblePieceComponent::Release(const FVector& Velocity, float SpinDegPerSec)
{
	State = EPieceState::Dynamic;
	RestTime = 0.f;
	SetVisibility(true);
	SetHiddenInGame(false);
	SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetCollisionResponseToAllChannels(ECR_Block);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // Prevent player snagging on flying rubble
	SetEnableGravity(true);
	SetSimulatePhysics(true);
	SetMassOverrideInKg(NAME_None, PieceMass, true);
	SetLinearDamping(0.25f);
	SetAngularDamping(0.35f);
	SetPhysicsLinearVelocity(Velocity, false);

	if (SpinDegPerSec > KINDA_SMALL_NUMBER)
	{
		const FVector RandAng = FMath::VRand() * FMath::FRandRange(SpinDegPerSec * 0.35f, SpinDegPerSec);
		SetPhysicsAngularVelocityInDegrees(RandAng, false);
	}
}

void UDestructiblePieceComponent::Settle()
{
	State = EPieceState::Settled;
	SetSimulatePhysics(false);
	SetEnableGravity(false);
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void UDestructiblePieceComponent::RestoreIntact()
{
	State = EPieceState::Intact;
	Damage = 0.f;
	RestTime = 0.f;
	SetSimulatePhysics(false);
	SetEnableGravity(false);
	SetVisibility(true);
	SetHiddenInGame(false);
	SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetCollisionResponseToAllChannels(ECR_Block);
	SetRelativeTransform(InitialLocalTransform);
}

void UDestructiblePieceComponent::Remove()
{
	State = EPieceState::Gone;
	SetSimulatePhysics(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetVisibility(false);
	SetHiddenInGame(true);
}

FVector UDestructiblePieceComponent::GetCentre() const
{
	return Bounds.Origin.IsNearlyZero() ? GetComponentLocation() : Bounds.Origin;
}

FVector UDestructiblePieceComponent::ComputeLaunchVelocity(const FSpellImpact& Impact, const FVector& Centre, float Mass)
{
	const float MassScale = FMath::Clamp(FMath::Sqrt(25.f / FMath::Max(5.f, Mass)), 0.45f, 1.45f);
	return Impact.LaunchVelocityAt(Centre) * MassScale;
}

ADestructibleStructure::ADestructibleStructure()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ADestructibleStructure::BeginPlay()
{
	Super::BeginPlay();
	BuildPieces();
}

void ADestructibleStructure::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CascadeHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void ADestructibleStructure::BuildPieces()
{
	if (!Definition || Definition->Pieces.IsEmpty())
	{
		return;
	}

	const int32 Num = Definition->Pieces.Num();
	Pieces.Reset(Num);
	SupportTotal.SetNumZeroed(Num);
	SupportLost.SetNumZeroed(Num);
	PendingDue.SetNumZeroed(Num);
	PendingPieces.Reset();
	IntactLaterals.SetNumZeroed(Num);
	bCollapseAwarded = false;

	for (int32 i = 0; i < Num; ++i)
	{
		const FPieceDefinition& Def = Definition->Pieces[i];
		UDestructiblePieceComponent* Comp = NewObject<UDestructiblePieceComponent>(this);
		Comp->PieceIndex = i;
		Comp->SetupAttachment(RootComponent);
		Comp->RegisterComponent();
		Comp->Configure(Def, HealthScale, Def.LocalTransform);
		Pieces.Add(Comp);

		SupportTotal[i] = static_cast<float>(Def.Supports.Num());
		IntactLaterals[i] = Def.Laterals.Num();
	}
}

int32 ADestructibleStructure::ApplyImpactRadial(const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	if (!Definition || Pieces.IsEmpty())
	{
		return 0;
	}

	const float MatMul = Impact.Multipliers.For(Definition->Material);
	int32 Broken = 0;

	for (int32 i = 0; i < Pieces.Num(); ++i)
	{
		UDestructiblePieceComponent* Piece = Pieces[i];
		if (!Piece)
		{
			continue;
		}

		const FVector Centre = Piece->GetCentre();
		if (Piece->State == EPieceState::Intact || Piece->State == EPieceState::Pending)
		{
			const float RawDmg = Impact.DamageAt(Centre) * MatMul;
			if (RawDmg > KINDA_SMALL_NUMBER)
			{
				Broken += DamagePiece(i, RawDmg, Impact, OutEvents);
			}
		}
		else if (Piece->State == EPieceState::Dynamic || Piece->State == EPieceState::Settled)
		{
			if (FVector::DistSquared(Impact.Origin, Centre) <= Impact.Radius * Impact.Radius)
			{
				const FVector Vel = UDestructiblePieceComponent::ComputeLaunchVelocity(Impact, Centre, Piece->PieceMass);
				if (Piece->State == EPieceState::Settled)
				{
					Piece->Release(Vel, Impact.Spin);
				}
				else
				{
					Piece->AddImpulse(Vel, NAME_None, true);
				}
			}
		}
	}

	return Broken;
}

int32 ADestructibleStructure::ApplyImpactCone(const FVector& Origin, const FVector& Direction, float HalfAngleDeg, float Range, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	if (!Definition || Pieces.IsEmpty())
	{
		return 0;
	}

	const FVector DirN = Direction.GetSafeNormal();
	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(HalfAngleDeg, 1.f, 89.f)));
	const float RangeSq = Range * Range;
	const float MatMul = Impact.Multipliers.For(Definition->Material);
	int32 Broken = 0;

	for (int32 i = 0; i < Pieces.Num(); ++i)
	{
		UDestructiblePieceComponent* Piece = Pieces[i];
		if (!Piece)
		{
			continue;
		}

		const FVector Centre = Piece->GetCentre();
		const FVector ToPiece = Centre - Origin;
		const float DistSq = static_cast<float>(ToPiece.SizeSquared());
		if (DistSq > RangeSq || DistSq < KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const FVector ToDir = ToPiece / FMath::Sqrt(DistSq);
		if (FVector::DotProduct(DirN, ToDir) < CosHalf)
		{
			continue;
		}

		const float Falloff = 1.f - 0.55f * (FMath::Sqrt(DistSq) / FMath::Max(1.f, Range));
		if (Piece->State == EPieceState::Intact || Piece->State == EPieceState::Pending)
		{
			const float RawDmg = Impact.Damage * Falloff * MatMul;
			if (RawDmg > KINDA_SMALL_NUMBER)
			{
				Broken += DamagePiece(i, RawDmg, Impact, OutEvents);
			}
		}
		else if (Piece->State == EPieceState::Dynamic || Piece->State == EPieceState::Settled)
		{
			const FVector Vel = (DirN + FVector(0.f, 0.f, Impact.UpBias)).GetSafeNormal() * (Impact.LaunchSpeed * Falloff);
			if (Piece->State == EPieceState::Settled)
			{
				Piece->Release(Vel, Impact.Spin);
			}
			else
			{
				Piece->AddImpulse(Vel, NAME_None, true);
			}
		}
	}

	return Broken;
}

int32 ADestructibleStructure::ApplyLaunchUpward(const FVector& Origin, float Radius, float Height, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	if (!Definition || Pieces.IsEmpty())
	{
		return 0;
	}

	const float RadiusSq = Radius * Radius;
	const float MatMul = Impact.Multipliers.For(Definition->Material);
	int32 Broken = 0;

	for (int32 i = 0; i < Pieces.Num(); ++i)
	{
		UDestructiblePieceComponent* Piece = Pieces[i];
		if (!Piece)
		{
			continue;
		}

		const FVector Centre = Piece->GetCentre();
		const float DZ = static_cast<float>(Centre.Z - Origin.Z);
		if (DZ < -120.f || DZ > Height)
		{
			continue;
		}
		const float DistSq2D = static_cast<float>(FVector::DistSquared2D(Origin, Centre));
		if (DistSq2D > RadiusSq)
		{
			continue;
		}

		if (Piece->State == EPieceState::Intact || Piece->State == EPieceState::Pending)
		{
			const float RawDmg = Impact.Damage * MatMul;
			if (RawDmg > KINDA_SMALL_NUMBER)
			{
				Broken += DamagePiece(i, RawDmg, Impact, OutEvents);
			}
		}
		else if (Piece->State == EPieceState::Dynamic || Piece->State == EPieceState::Settled)
		{
			const FVector Out = (Centre - Origin).GetSafeNormal2D();
			const FVector Vel = (Out * 0.35f + FVector::UpVector).GetSafeNormal() * Impact.LaunchSpeed;
			if (Piece->State == EPieceState::Settled)
			{
				Piece->Release(Vel, Impact.Spin);
			}
			else
			{
				Piece->AddImpulse(Vel, NAME_None, true);
			}
		}
	}

	return Broken;
}

void ADestructibleStructure::Reset()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CascadeHandle);
	}
	PendingPieces.Reset();
	bCollapseAwarded = false;

	const int32 Num = Pieces.Num();
	SupportLost.Init(0.f, Num);
	PendingDue.Init(0.f, Num);
	if (Definition && Definition->Pieces.Num() == Num)
	{
		for (int32 i = 0; i < Num; ++i)
		{
			IntactLaterals[i] = Definition->Pieces[i].Laterals.Num();
			if (Pieces[i])
			{
				Pieces[i]->RestoreIntact();
			}
		}
	}

	LoosePieces.Reset();
}

FBoxSphereBounds ADestructibleStructure::GetBreakableBounds() const
{
	return Super::GetBreakableBounds();
}

int32 ADestructibleStructure::GetLoosePieceCount() const
{
	int32 Count = 0;
	for (const TObjectPtr<UDestructiblePieceComponent>& Piece : Pieces)
	{
		if (Piece && Piece->State == EPieceState::Dynamic)
		{
			Count++;
		}
	}
	return Count;
}

int32 ADestructibleStructure::GetPieceCount() const
{
	return Pieces.Num();
}

int32 ADestructibleStructure::GetReleasedCount() const
{
	int32 Count = 0;
	for (const TObjectPtr<UDestructiblePieceComponent>& Piece : Pieces)
	{
		if (Piece && Piece->State != EPieceState::Intact && Piece->State != EPieceState::Pending)
		{
			Count++;
		}
	}
	return Count;
}

bool ADestructibleStructure::IsFullyIntact() const
{
	return GetReleasedCount() == 0;
}

bool ADestructibleStructure::GetPieceCentre(int32 PieceIndex, FVector& OutCentre) const
{
	if (!Pieces.IsValidIndex(PieceIndex) || !Pieces[PieceIndex])
	{
		return false;
	}
	OutCentre = Pieces[PieceIndex]->GetCentre();
	return true;
}

int32 ADestructibleStructure::DamagePiece(int32 PieceIndex, float InDamage, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	if (!Pieces.IsValidIndex(PieceIndex))
	{
		return 0;
	}
	UDestructiblePieceComponent* Piece = Pieces[PieceIndex];
	if (!Piece || (Piece->State != EPieceState::Intact && Piece->State != EPieceState::Pending))
	{
		return 0;
	}

	Piece->Damage += InDamage;
	if (Piece->Damage < Piece->Health)
	{
		return 0;
	}

	const FVector Vel = UDestructiblePieceComponent::ComputeLaunchVelocity(Impact, Piece->GetCentre(), Piece->PieceMass);
	ReleasePiece(PieceIndex, Vel, Impact.Spin, &Impact, false, OutEvents);
	return 1;
}

void ADestructibleStructure::ReleasePiece(int32 PieceIndex, const FVector& Velocity, float Spin, const FSpellImpact* Impact, bool bFromCascade, TArray<FDestructibleEvent>& OutEvents)
{
	if (!Pieces.IsValidIndex(PieceIndex))
	{
		return;
	}
	UDestructiblePieceComponent* Piece = Pieces[PieceIndex];
	if (!Piece || (Piece->State != EPieceState::Intact && Piece->State != EPieceState::Pending))
	{
		return;
	}

	Piece->Release(Velocity, Spin);
	LoosePieces.AddUnique(Piece);

	FDestructibleEvent Ev;
	FillEvent(Ev, Piece->GetCentre(), static_cast<float>(Piece->Extent.GetMax() * 2.f), Impact);
	Ev.bFromCascade = bFromCascade;
	OutEvents.Add(Ev);

	PropagateLoss(PieceIndex, Impact, OutEvents);
	AwardCollapseIfDue(Impact, OutEvents);
}

void ADestructibleStructure::PropagateLoss(int32 ReleasedIndex, const FSpellImpact* Impact, TArray<FDestructibleEvent>& OutEvents)
{
	if (!Definition || Definition->Pieces.Num() != Pieces.Num())
	{
		return;
	}

	const int32 Num = Pieces.Num();
	for (int32 i = 0; i < Num; ++i)
	{
		UDestructiblePieceComponent* Other = Pieces[i];
		if (!Other || Other->State != EPieceState::Intact || Other->bGrounded)
		{
			continue;
		}

		const FPieceDefinition& OtherDef = Definition->Pieces[i];
		bool bAffected = false;
		if (OtherDef.Supports.Contains(ReleasedIndex))
		{
			SupportLost[i] += 1.f;
			bAffected = true;
		}
		if (OtherDef.Laterals.Contains(ReleasedIndex))
		{
			IntactLaterals[i] = FMath::Max(0, IntactLaterals[i] - 1);
			bAffected = true;
		}

		if (!bAffected)
		{
			continue;
		}

		const float TotalSup = SupportTotal[i];
		const bool bLostSupport = (TotalSup > 0.f && (SupportLost[i] / TotalSup) >= 0.5f);
		const bool bEdgeCrumble = (TotalSup <= 0.f && OtherDef.Laterals.Num() > 0 && IntactLaterals[i] == 0);

		if (bLostSupport || bEdgeCrumble)
		{
			ScheduleCheck(i, FMath::FRandRange(0.055f, 0.16f));
		}
	}
}

void ADestructibleStructure::ScheduleCheck(int32 PieceIndex, float Delay)
{
	UWorld* World = GetWorld();
	if (!World || !Pieces.IsValidIndex(PieceIndex))
	{
		return;
	}
	UDestructiblePieceComponent* Piece = Pieces[PieceIndex];
	if (!Piece || Piece->State != EPieceState::Intact)
	{
		return;
	}

	Piece->State = EPieceState::Pending;
	PendingDue[PieceIndex] = World->GetTimeSeconds() + Delay;
	PendingPieces.AddUnique(PieceIndex);

	if (!World->GetTimerManager().IsTimerActive(CascadeHandle))
	{
		World->GetTimerManager().SetTimer(CascadeHandle, this, &ADestructibleStructure::ProcessIntegrity, 0.05f, true);
	}
}

void ADestructibleStructure::ProcessIntegrity()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	TArray<FDestructibleEvent> CascadeEvents;

	for (int32 i = PendingPieces.Num() - 1; i >= 0; --i)
	{
		const int32 Idx = PendingPieces[i];
		if (!Pieces.IsValidIndex(Idx) || Pieces[Idx]->State != EPieceState::Pending)
		{
			PendingPieces.RemoveAtSwap(i);
			continue;
		}

		if (Now >= PendingDue[Idx])
		{
			PendingPieces.RemoveAtSwap(i);
			const FVector FallVel(FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(-90.f, 90.f), -60.f);
			ReleasePiece(Idx, FallVel, 140.f, nullptr, true, CascadeEvents);
		}
	}

	if (!CascadeEvents.IsEmpty())
	{
		if (UWizardDestructionSubsystem* Destruction = GetDestruction())
		{
			Destruction->OnDestructibleBatch.Broadcast(CascadeEvents);
		}
	}

	if (PendingPieces.IsEmpty())
	{
		World->GetTimerManager().ClearTimer(CascadeHandle);
	}
}

void ADestructibleStructure::AwardCollapseIfDue(const FSpellImpact* Impact, TArray<FDestructibleEvent>& OutEvents)
{
	if (bCollapseAwarded || !Definition || Pieces.IsEmpty())
	{
		return;
	}

	const float Ratio = static_cast<float>(GetReleasedCount()) / static_cast<float>(Pieces.Num());
	if (Ratio >= 0.8f)
	{
		bCollapseAwarded = true;
		FDestructibleEvent BonusEv;
		FillEvent(BonusEv, GetBreakableBounds().Origin, Definition->Size, Impact);
		BonusEv.Points = Definition->CollapseBonus;
		BonusEv.bIsCollapseBonus = true;
		OutEvents.Add(BonusEv);
	}
}
