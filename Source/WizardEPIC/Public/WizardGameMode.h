// WizardGameMode.h - game mode: spawns the animated template pawn, attaches the ability
// component to it, starts the smoke test when requested and handles the arena reset.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WizardGameMode.generated.h"

class USpellDataAsset;
class UWizardSmokeTest;

UCLASS()
class WIZARDEPIC_API AWizardGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWizardGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	/** Restores every destructible and resets the score. */
	UFUNCTION(BlueprintCallable, Category = Wizard)
	void ResetArena();

	UFUNCTION(BlueprintPure, Category = Wizard)
	static AWizardGameMode* Get(const UObject* WorldContext);

	/** Spell data assets in HUD-slot order. Empty entries fall back to WizardPaths::Spells. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Wizard)
	TArray<TObjectPtr<USpellDataAsset>> SpellLoadout;

	/** Pawn class. Defaults to the engine's Third Person template character. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Wizard)
	TSubclassOf<APawn> WizardPawnClass = nullptr;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWizardSmokeTest> SmokeTest = nullptr;
};
