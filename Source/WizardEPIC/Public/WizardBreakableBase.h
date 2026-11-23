// WizardBreakableBase.h - common base of everything breakable: piece pool, rubble settling,
// subsystem registration and event helpers.

#pragma once

#include "CoreMinimal.h"
#include "BreakableTarget.h"
#include "DestructibleDefinition.h"
#include "WizardBreakableBase.generated.h"

class UDestructiblePieceComponent;
class UWizardDestructionSubsystem;

/**
 * Base class. Children decide how their intact state is shown (frozen piece components for
 * structures, instanced meshes for prop sets) and how spells reach their pieces. The base owns
 * every loose piece that was ever released, so rubble settles, gets capped and resets uniformly.
 */
UCLASS(Abstract)
class WIZARDEPIC_API AWizardBreakableBase : public AActor, public IBreakableTarget
{
	GENERATED_BODY()

public:
	AWizardBreakableBase();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Destructible)
	TObjectPtr<UDestructibleDefinition> Definition = nullptr;

	/** Multiplier on all health values. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Destructible)
	float HealthScale = 1.f;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//~ Begin IBreakableTarget Interface
	/** Default: nothing breaks. Children override. */
	virtual int32 ApplyImpactRadial(const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents) override;
	virtual int32 ApplyImpactCone(const FVector& Origin, const FVector& Direction, float HalfAngleDeg, float Range, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents) override;
	virtual int32 ApplyLaunchUpward(const FVector& Origin, float Radius, float Height, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents) override;
	virtual void PushDynamic(const FVector& Origin, float Radius, float Speed, float UpBias) override;
	virtual void Reset() override;
	virtual FBoxSphereBounds GetBreakableBounds() const override;
	virtual int32 GetLoosePieceCount() const override;
	//~ End IBreakableTarget Interface

	/** Called by the destruction subsystem every frame: settles resting rubble, removes fallen pieces. */
	void TickLoose(float DeltaTime);

	/** Removes every loose piece (used by Reset implementations). */
	void DestroyLoosePieces();

protected:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDestructiblePieceComponent>> LoosePieces;

	/** Creates a loose piece in world space and launches it. */
	UDestructiblePieceComponent* SpawnLoosePiece(const FPieceDefinition& Piece, const FTransform& PieceWorldTransform, const FVector& Velocity, float Spin);

	/** Fills the shared fields of an event. */
	void FillEvent(FDestructibleEvent& Event, const FVector& Location, float Size, const FSpellImpact* Impact) const;

	/** Points for one broken piece of this breakable. */
	int32 GetPieceScore() const;

	UWizardDestructionSubsystem* GetDestruction() const;
};
