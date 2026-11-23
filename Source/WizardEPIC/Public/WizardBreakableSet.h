// WizardBreakableSet.h - whole props (pots, crates, barrels, small rocks, trees) as instanced
// meshes that shatter into the fracture pieces of a UDestructibleDefinition.

#pragma once

#include "CoreMinimal.h"
#include "WizardBreakableBase.h"
#include "WizardBreakableSet.generated.h"

class UInstancedStaticMeshComponent;

/**
 * One instanced static mesh component = many identical breakable props. The forge places one
 * actor per prop type, sets the static mesh on Instances and adds instances. A broken instance
 * is hidden (zero scale, its slot is reused on reset) and replaced by loose pieces.
 */
UCLASS()
class WIZARDEPIC_API AWizardBreakableSet : public AWizardBreakableBase
{
	GENERATED_BODY()

public:
	AWizardBreakableSet();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Destructible)
	TObjectPtr<UInstancedStaticMeshComponent> Instances = nullptr;

	virtual void BeginPlay() override;

	/** Adds an intact prop. Returns its index. */
	UFUNCTION(BlueprintCallable, Category = Wizard)
	int32 AddInstance(const FTransform& WorldTransform);

	int32 GetInstanceCount() const;

	/** World transform of an instance. False when the index is out of range. */
	bool GetInstanceTransform(int32 Index, FTransform& OutTransform) const;

	//~ Begin IBreakableTarget Interface
	virtual int32 ApplyImpactRadial(const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents) override;
	virtual int32 ApplyImpactCone(const FVector& Origin, const FVector& Direction, float HalfAngleDeg, float Range, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents) override;
	virtual int32 ApplyLaunchUpward(const FVector& Origin, float Radius, float Height, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents) override;
	virtual void Reset() override;
	virtual FBoxSphereBounds GetBreakableBounds() const override;
	//~ End IBreakableTarget Interface

	/** Breaks one instance regardless of damage (crystal targets, scripts). Returns the broken piece count. */
	int32 BreakInstance(int32 Index, const FVector& LaunchOrigin, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents);

	bool IsInstanceAlive(int32 Index) const;

protected:
	/** Damage accumulated per instance. */
	TArray<float> InstanceDamage;

	/** Alive flags per instance (indices are stable: broken instances keep their slot). */
	TArray<uint8> InstanceAlive;

	/** True for instances that were alive when the level started. */
	int32 InitialInstanceCount = 0;

	/** Applies damage to alive instances inside a test volume and breaks the spent ones. */
	int32 DamageInstances(const FSpellImpact& Impact, const TFunctionRef<bool(const FVector& Centre, FVector& OutLaunch)>& Selector, TArray<FDestructibleEvent>& OutEvents);

	/** Called after an instance broke. Children may spawn extra feedback. */
	virtual void OnInstanceBroken(int32 Index, const FTransform& WorldTransform, const FSpellImpact& Impact) {}
};
