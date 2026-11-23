// HUDWidgets.h - the whole HUD built in C++ (no UMG blueprint): four spell slots with radial
// cooldowns, charge arc, score and combo, crosshair, hints, title card and floating score popups.
//
// Design intent (see .sdd/api/ART_DIRECTION.md when present): minimal and stylish - dark glass
// panels with a thin accent border in the element colour, Roboto with wide tracking for labels,
// heavy numerals for the score, short eased animations (120-250 ms), nothing else on screen.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WizardTypes.h"
#include "HUDWidgets.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UOverlay;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UWizardAbilityComponent;
class USpellDataAsset;
class UWidget;

/** One spell slot: key label, icon, radial cooldown, charge glow, selection state. */
UCLASS()
class WIZARDEPIC_API USpellSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USpellSlotWidget(const FObjectInitializer& ObjectInitializer);

	void Setup(int32 InSlotIndex);
	void SetSpell(const USpellDataAsset* Spell);
	void SetSelected(bool bInSelected);
	/** FractionRemaining 0..1, SecondsRemaining for the label. */
	void SetCooldown(float FractionRemaining, float SecondsRemaining);
	/** 0 when not charging. */
	void SetCharge(float Fraction);
	/** Short flash when the spell launches. */
	void PlayCastFlash();
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

private:
	int32 SlotIndex = 0;
	bool bSelected = false;
	float TargetScale = 1.f;
	float CurrentScale = 1.f;
	float Flash = 0.f;
	FLinearColor Accent = FLinearColor::White;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> Root = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Icon = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UImage> CooldownImage = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> SelectionMark = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> KeyLabel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CooldownLabel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ChargeBar = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CooldownMaterial = nullptr;
};

/** Floating "+120" / "КРИСТАЛЛ +250" text that rises and fades. */
UCLASS()
class WIZARDEPIC_API UWizardScorePopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWizardScorePopupWidget(const FObjectInitializer& ObjectInitializer);

	void Activate(const FScorePopupInfo& Info);
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

private:
	FVector WorldLocation = FVector::ZeroVector;
	float Life = 1.f;
	float Age = 0.f;
	float Scale = 1.f;
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label = nullptr;
};

/** The HUD: crosshair, spell bar, score, combo, hints, title card, popups. */
UCLASS()
class WIZARDEPIC_API UWizardHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWizardHUDWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

	void RefreshSpells();
	void SetHintsVisible(bool bVisible);
	/** Plays the intro wordmark once. */
	void ShowTitleCard();
	void PlayArenaResetFlash();

private:
	UPROPERTY(Transient)
	TObjectPtr<UOverlay> Root = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UImage> CrosshairDot = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ScoreLabel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BestLabel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ComboLabel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpellNameLabel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintLabel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> TitleCard = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> PopupLayer = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USpellSlotWidget>> Slots;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWizardScorePopupWidget>> Popups;

	TWeakObjectPtr<UWizardAbilityComponent> Abilities;
	float CrosshairKick = 0.f;
	int64 LastShownScore = -1;
	float TitleAge = -1.f;
	float ResetFlash = 0.f;

	UWizardAbilityComponent* GetAbilities();
	void OnScoreChanged(int64 Score, int64 BestScore);
	void OnComboChanged(float Multiplier, float TimeLeft);
	void OnPopup(const FScorePopupInfo& Info);
	void OnArenaReset();
	void OnSpellCast(int32 SlotIndex, float ChargeT01);
};
