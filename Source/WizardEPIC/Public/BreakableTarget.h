// BreakableTarget.h - interface of everything a spell can break.

#pragma once

#include "CoreMinimal.h"
#include "Math/BoxSphereBounds.h"
#include "UObject/Interface.h"
#include "WizardTypes.h"
#include "BreakableTarget.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UBreakableTarget : public UInterface
{
	GENERATED_BODY()
};

/** Implemented by AWizardBreakableBase and its children (structures, breakable sets, crystal targets). */
class IBreakableTarget : public IInterface
{
	GENERATED_BODY()

public:
	/** Radial blast: releases and launches pieces with their centre inside Impact.Radius. Returns the number of broken things. */
	virtual int32 ApplyImpactRadial(const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents) = 0;

	/** Directional gust from Origin along Direction (HalfAngleDeg, Range in cm). Returns the number of broken things. */
	virtual int32 ApplyImpactCone(const FVector& Origin, const FVector& Direction, float HalfAngleDeg, float Range, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents) = 0;

	/** Upward launch inside a vertical cylinder (Origin = base centre, Radius, Height in cm). Returns the number of broken things. */
	virtual int32 ApplyLaunchUpward(const FVector& Origin, float Radius, float Height, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents) = 0;

	/** Pushes loose rubble without breaking anything new. */
	virtual void PushDynamic(const FVector& Origin, float Radius, float Speed, float UpBias) = 0;

	/** Restores the intact state and removes the rubble. */
	virtual void Reset() = 0;

	/** World-space bounds used by the broad phase. */
	virtual FBoxSphereBounds GetBreakableBounds() const = 0;

	/** Number of pieces currently simulating physics. */
	virtual int32 GetLoosePieceCount() const = 0;
};
