#include "HUDWidgets.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Overlay.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(HUDWidgets)

// Skeleton build: the layout and animation are implemented in the next step.

USpellSlotWidget::USpellSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USpellSlotWidget::Setup(int32 InSlotIndex)
{
	SlotIndex = InSlotIndex;
}

void USpellSlotWidget::SetSpell(const USpellDataAsset* Spell)
{
}

void USpellSlotWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
}

void USpellSlotWidget::SetCooldown(float FractionRemaining, float SecondsRemaining)
{
}

void USpellSlotWidget::SetCharge(float Fraction)
{
}

void USpellSlotWidget::PlayCastFlash()
{
	Flash = 1.f;
}

void USpellSlotWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);
}

UWizardScorePopupWidget::UWizardScorePopupWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWizardScorePopupWidget::Activate(const FScorePopupInfo& Info)
{
}

void UWizardScorePopupWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);
}

UWizardHUDWidget::UWizardHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWizardHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

void UWizardHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UWizardHUDWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void UWizardHUDWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);
}

void UWizardHUDWidget::RefreshSpells()
{
}

void UWizardHUDWidget::SetHintsVisible(bool bVisible)
{
}

void UWizardHUDWidget::ShowTitleCard()
{
	TitleAge = 0.f;
}

void UWizardHUDWidget::PlayArenaResetFlash()
{
	ResetFlash = 1.f;
}

UWizardAbilityComponent* UWizardHUDWidget::GetAbilities()
{
	return nullptr;
}

void UWizardHUDWidget::OnScoreChanged(int64 Score, int64 BestScore)
{
}

void UWizardHUDWidget::OnComboChanged(float Multiplier, float TimeLeft)
{
}

void UWizardHUDWidget::OnPopup(const FScorePopupInfo& Info)
{
}

void UWizardHUDWidget::OnArenaReset()
{
}

void UWizardHUDWidget::OnSpellCast(int32 SlotIndex, float ChargeT01)
{
}
