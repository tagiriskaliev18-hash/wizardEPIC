// CrystalTarget.h - glowing crystal worth bonus points. Shatters when any spell touches it and
// grows back after RespawnTime.

#pragma once

#include "CoreMinimal.h"
#include "WizardBreakableSet.h"
#include "CrystalTarget.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

UCLASS()
class WIZARDEPIC_API ACrystalTarget : public AWizardBreakableBase
{
	GENERATED_BODY()

public:
	ACrystalTarget();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Crystal)
	int32 Score = 250;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Crystal)
	float RespawnTime = 22.f;

	/** Seconds the crystal takes to grow back in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Crystal)
	float GrowTime = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Crystal)
	FLinearColor Color = FLinearColor(0.25f, 0.9f, 1.f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Crystal)
	TObjectPtr<UStaticMeshComponent> CrystalMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Crystal)
	TObjectPtr<UPointLightComponent> GlowLight = nullptr;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	//~ Begin IBreakableTarget Interface
	virtual int32 ApplyImpactRadial(const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents) override;
	virtual int32 ApplyImpactCone(const FVector& Origin, const FVector& Direction, float HalfAngleDeg, float Range, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents) override;
	virtual int32 ApplyLaunchUpward(const FVector& Origin, float Radius, float Height, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents) override;
	virtual void Reset() override;
	virtual FBoxSphereBounds GetBreakableBounds() const override;
	//~ End IBreakableTarget Interface

	bool IsAlive() const { return bAlive; }

private:
	bool bAlive = true;
	float Age = 0.f;
	FTimerHandle RespawnHandle;

	/** Breaks the crystal if it is alive and the point lies inside its bounds inflated by Padding. */
	int32 HitTest(const FVector& Point, float Padding, const FVector& LaunchOrigin, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents);

	void Shatter(const FVector& LaunchOrigin, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents);
	void Respawn();
};
