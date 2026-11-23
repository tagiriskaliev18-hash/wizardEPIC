// WizardScoreSubsystem.h - score, combo multiplier, floating score popups and the saved best score.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WizardTypes.h"
#include "WizardScoreSubsystem.generated.h"

UCLASS()
class WIZARDEPIC_API UWizardScoreSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWizardScoreSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Applies the combo multiplier to every break of a batch, adds the score and emits one popup at the batch centre. */
	void HandleDestructibleBatch(const TArray<FDestructibleEvent>& Events);

	/** Adds points outside the combo chain (crystal bonus, arena clear) and shows a labelled popup. */
	void AddBonus(int32 Points, const FText& Label, const FVector& AtLocation, const FLinearColor& Color);

	void Reset();

	UFUNCTION(BlueprintPure, Category = Wizard)
	int64 GetScore() const { return Score; }

	UFUNCTION(BlueprintPure, Category = Wizard)
	int64 GetBestScore() const { return BestScore; }

	UFUNCTION(BlueprintPure, Category = Wizard)
	float GetComboMultiplier() const { return ComboMultiplier; }

	UFUNCTION(BlueprintPure, Category = Wizard)
	float GetComboTimeLeft() const { return ComboTimer; }

	UFUNCTION(BlueprintPure, Category = Wizard)
	int32 GetComboCount() const { return ComboCount; }

	UPROPERTY(BlueprintAssignable, Category = Wizard)
	FOnScoreChanged OnScoreChanged;

	UPROPERTY(BlueprintAssignable, Category = Wizard)
	FOnComboChanged OnComboChanged;

	UPROPERTY(BlueprintAssignable, Category = Wizard)
	FOnScorePopup OnScorePopup;

	/** Seconds after the last break before the combo chain resets. */
	float ComboWindow = 2.f;

	/** Multiplier growth per break in the chain. */
	float ComboStep = 0.25f;

	float ComboMaxMultiplier = 5.f;

private:
	int64 Score = 0;
	int64 BestScore = 0;
	int32 ComboCount = 0;
	float ComboMultiplier = 1.f;
	float ComboTimer = 0.f;

	void LoadBest();
	void SaveBest();
};
