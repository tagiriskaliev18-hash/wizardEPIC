// WizardDestructionSubsystem.h - registry of breakables and the single entry point for spell impacts.
//
// Spells never touch destructibles directly: they call ImpactRadial / ImpactCone /
// ImpactLaunchUpward here. The subsystem broadcasts the breaks of each frame as one batch
// (OnDestructibleBatch) and drives the feedback (FX, audio, camera, score) through it.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WizardTypes.h"
#include "WizardDestructionSubsystem.generated.h"

class AWizardBreakableBase;

UCLASS()
class WIZARDEPIC_API UWizardDestructionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWizardDestructionSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	void RegisterBreakable(AWizardBreakableBase* Breakable);
	void UnregisterBreakable(AWizardBreakableBase* Breakable);

	// ----- Impacts ----------------------------------------------------------------------------
	/** Radial blast. Returns the number of broken things. */
	int32 ImpactRadial(const FSpellImpact& Impact);

	/** Directional gust. Returns the number of broken things. */
	int32 ImpactCone(const FVector& Origin, const FVector& Direction, float HalfAngleDeg, float Range, const FSpellImpact& Impact);

	/** Upward launch in a vertical cylinder. Returns the number of broken things. */
	int32 ImpactLaunchUpward(const FVector& Origin, float Radius, float Height, const FSpellImpact& Impact);

	/** Pushes loose rubble of every breakable radially. */
	void PushDynamicBodies(const FVector& Origin, float Radius, float Speed, float UpBias);

	/** Restores every breakable and clears rubble. Broadcasts OnArenaReset. */
	void ResetAll();

	// ----- State ----------------------------------------------------------------------------
	int32 GetLoosePieceCount() const;
	int32 GetBreakableCount() const;

	/** Resting rubble is settled early when more loose pieces than this simulate at once. */
	int32 MaxLoosePieces = 900;

	/** Serial number increased by every reset: scheduled callbacks capture it and stop when it changed. */
	uint32 GetResetSerial() const { return ResetSerial; }

	UPROPERTY(BlueprintAssignable, Category = Wizard)
	FOnDestructibleBatch OnDestructibleBatch;

	UPROPERTY(BlueprintAssignable, Category = Wizard)
	FOnArenaReset OnArenaReset;

	/** Feedback for one batch: dust and chips, break sounds, camera shake, score. */
	void ReactToBatch(const TArray<FDestructibleEvent>& Events);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<AWizardBreakableBase>> Breakables;

	/** Breaks of the current frame, flushed at the end of the tick. */
	TArray<FDestructibleEvent> PendingEvents;

	uint32 ResetSerial = 0;
	float SettleCheckAccumulator = 0.f;

	void Flush();
	/** Settles the oldest loose pieces while above MaxLoosePieces. */
	void EnforceLooseLimit();
};
