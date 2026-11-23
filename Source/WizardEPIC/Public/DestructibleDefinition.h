// DestructibleDefinition.h - data of a destructible thing: its intact mesh and its fracture pieces.
//
// Scripts/forge computes convex Voronoi pieces for every shape and writes them here together
// with the support graph that drives progressive collapse in ADestructibleStructure.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Math/BoxSphereBounds.h"
#include "WizardTypes.h"
#include "DestructibleDefinition.generated.h"

class UStaticMesh;

/** One fracture piece of a destructible. */
USTRUCT(BlueprintType)
struct FPieceDefinition
{
	GENERATED_BODY()

	/** Convex chunk with simple collision, two material slots: 0 = original surface, 1 = broken interior. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Piece)
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	/** Transform relative to the destructible actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Piece)
	FTransform LocalTransform = FTransform::Identity;

	/** Mass in kg. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Piece)
	float Mass = 25.f;

	/** Health in damage points. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Piece)
	float Health = 100.f;

	/** Half extents of the piece's bounding box in its own space, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Piece)
	FVector Extent = FVector(25.f);

	/** True when the piece rests on the ground: it never falls because other pieces broke. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Piece)
	bool bGrounded = false;

	/** Indices of pieces directly below that carry this piece. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Piece)
	TArray<int32> Supports;

	/** Indices of side neighbours. They only delay a fall and crumble when too many of them break. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Piece)
	TArray<int32> Laterals;
};

UCLASS(BlueprintType)
class WIZARDEPIC_API UDestructibleDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Definition)
	EDestructibleMaterial Material = EDestructibleMaterial::Stone;

	/** The unbroken mesh (used for intact breakable-set instances and crystals). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Definition)
	TObjectPtr<UStaticMesh> WholeMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Definition)
	TArray<FPieceDefinition> Pieces;

	/** Health of an intact whole prop (pots, crates, crystals). Structure pieces use FPieceDefinition::Health. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Definition)
	float Health = 100.f;

	/** Points for one broken piece or prop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Definition)
	int32 ScorePerPiece = 10;

	/** Bonus points when at least 80% of a structure's pieces have broken. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Definition)
	int32 CollapseBonus = 100;

	/** Audio cue names (files in /Game/Wizard/Audio/SFX). Empty = the material default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Definition)
	FName BreakSound = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Definition)
	FName ImpactSound = NAME_None;

	/** Bounds of the intact shape in actor space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Definition)
	FBoxSphereBounds Bounds = FBoxSphereBounds(FVector::ZeroVector, FVector(50.f), 87.f);

	/** Longest extent of the intact shape in cm (FX and sound scale). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Definition)
	float Size = 100.f;
};
