// DestructibleStructure.h - piecewise destructible structure with progressive collapse.
//
// Every piece starts frozen (static, blocking). A spell releases damaged pieces as rigid
// bodies; pieces that lose most of their support fall a moment later, which propagates
// upward and outward as a staggered collapse. Resting rubble settles back to static.

#pragma once

#include "CoreMinimal.h"
#include "BreakableTarget.h"
#include "DestructibleDefinition.h"
#include "WizardBreakableBase.h"
#include "DestructibleStructure.generated.h"

UENUM(BlueprintType)
enum class EPieceState : uint8
{
	/** Frozen in place, blocks everything. */
	Intact,
	/** Lost its support and waits for its cascade delay. Still frozen. */
	Pending,
	/** Simulating physics. */
	Dynamic,
	/** Came to rest as static rubble (no physics cost). */
	Settled,
	/** Hidden and disabled. */
	Gone
};

/** One fracture piece of a structure. */
UCLASS(ClassGroup = (Wizard), meta = (BlueprintSpawnableComponent))
class WIZARDEPIC_API UDestructiblePieceComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UDestructiblePieceComponent();

	int32 PieceIndex = INDEX_NONE;
	EPieceState State = EPieceState::Intact;

	/** Damage accumulated so far. */
	float Damage = 0.f;
	float Health = 100.f;
	bool bGrounded = false;
	FVector Extent = FVector(25.f);

	/** Freezes the piece at LocalTransform (relative to the owner). */
	void Configure(const FPieceDefinition& Definition, float HealthScale, const FTransform& LocalTransform);

	/** Turns the piece into a rigid body and launches it. */
	void Release(const FVector& Velocity, float SpinDegPerSec);

	/** Freezes a resting piece as static rubble. */
	void Settle();

	/** Back to the frozen state at its original transform. */
	void RestoreIntact();

	/** Hides and disables the piece. */
	void Remove();

	/** World-space centre of the piece. */
	FVector GetCentre() const;

	/** World-space launch direction and speed derived from a spell impact. */
	static FVector ComputeLaunchVelocity(const FSpellImpact& Impact, const FVector& Centre, float Mass);
};

/**
 * A structure placed in the level. The forge places one actor per wall, tower, pillar or arch and
 * assigns a UDestructibleDefinition. Pieces are created in BeginPlay.
 */
UCLASS()
class WIZARDEPIC_API ADestructibleStructure : public AWizardBreakableBase
{
	GENERATED_BODY()

public:
	ADestructibleStructure();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//~ Begin IBreakableTarget Interface
	virtual int32 ApplyImpactRadial(const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents) override;
	virtual int32 ApplyImpactCone(const FVector& Origin, const FVector& Direction, float HalfAngleDeg, float Range, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents) override;
	virtual int32 ApplyLaunchUpward(const FVector& Origin, float Radius, float Height, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents) override;
	virtual void Reset() override;
	virtual FBoxSphereBounds GetBreakableBounds() const override;
	virtual int32 GetLoosePieceCount() const override;
	//~ End IBreakableTarget Interface

	int32 GetPieceCount() const;

	/** Number of pieces that are not Intact/Pending any more. */
	int32 GetReleasedCount() const;

	bool IsFullyIntact() const;

	/** World-space centre of an intact piece (INDEX_NONE guard). */
	bool GetPieceCentre(int32 PieceIndex, FVector& OutCentre) const;

	/** Applies damage to one intact piece from an external source and releases it when the health is spent. */
	int32 DamagePiece(int32 PieceIndex, float Damage, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDestructiblePieceComponent>> Pieces;

	/** Per piece support bookkeeping. */
	TArray<float> SupportTotal;
	TArray<float> SupportLost;
	TArray<float> PendingDue;
	TArray<int32> PendingPieces;

	/** Bookkeeping of intact laterals per piece (for edge crumbling). */
	TArray<int32> IntactLaterals;

	bool bCollapseAwarded = false;
	FTimerHandle CascadeHandle;

	void BuildPieces();
	/** Queues an integrity check for an intact piece after Delay seconds. */
	void ScheduleCheck(int32 PieceIndex, float Delay);
	/** Processes due integrity checks: releases unsupported pieces and propagates. */
	void ProcessIntegrity();
	/** Releases a piece. Impact may be null for cascade releases. */
	void ReleasePiece(int32 PieceIndex, const FVector& Velocity, float Spin, const FSpellImpact* Impact, bool bFromCascade, TArray<FDestructibleEvent>& OutEvents);
	/** Removes the support this piece gave to others and queues their checks. */
	void PropagateLoss(int32 ReleasedIndex, const FSpellImpact* Impact, TArray<FDestructibleEvent>& OutEvents);
	void AwardCollapseIfDue(const FSpellImpact* Impact, TArray<FDestructibleEvent>& OutEvents);
};
