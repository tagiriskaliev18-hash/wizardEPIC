#include "HUDWidgets.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "SpellDataAsset.h"
#include "UObject/SoftObjectPath.h"
#include "WizardAbilityComponent.h"
#include "WizardDestructionSubsystem.h"
#include "WizardScoreSubsystem.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(HUDWidgets)

namespace
{
	FSlateFontInfo MakeWizardFont(int32 Size, bool bDisplay = false)
	{
		const TCHAR* Path = bDisplay ? WizardPaths::DisplayFont : WizardPaths::BodyFont;
		if (UFont* Font = Cast<UFont>(FSoftObjectPath(Path).TryLoad()))
		{
			return FSlateFontInfo(Font, Size);
		}
		FSlateFontInfo Fallback = FCoreStyle::GetDefaultFontStyle(bDisplay ? "Bold" : "Regular", Size);
		return Fallback;
	}
}

// ---------------------------------------------------------------------------
// USpellSlotWidget
// ---------------------------------------------------------------------------

USpellSlotWidget::USpellSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USpellSlotWidget::Setup(int32 InSlotIndex)
{
	SlotIndex = InSlotIndex;
	if (!WidgetTree)
	{
		return;
	}

	Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Root->SetWidthOverride(78.f);
	Root->SetHeightOverride(78.f);
	WidgetTree->RootWidget = Root;

	UBorder* GlassBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	GlassBg->SetBrushColor(FLinearColor(0.035f, 0.035f, 0.06f, 0.78f));
	GlassBg->SetPadding(FMargin(4.f));
	Root->AddChild(GlassBg);

	UOverlay* Ov = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	GlassBg->AddChild(Ov);

	Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Icon->SetColorAndOpacity(FLinearColor(0.95f, 0.95f, 0.98f, 0.92f));
	if (UOverlaySlot* S = Ov->AddChildToOverlay(Icon))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(6.f));
	}

	CooldownImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	CooldownImage->SetColorAndOpacity(FLinearColor(0.02f, 0.02f, 0.04f, 0.72f));
	CooldownImage->SetVisibility(ESlateVisibility::Collapsed);
	if (UMaterialInterface* Mat = Cast<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Wizard/UI/Materials/M_UI_Cooldown")).TryLoad()))
	{
		CooldownMaterial = UMaterialInstanceDynamic::Create(Mat, this);
		CooldownImage->SetBrushFromMaterial(CooldownMaterial);
		CooldownImage->SetColorAndOpacity(FLinearColor::White);
	}
	if (UOverlaySlot* S = Ov->AddChildToOverlay(CooldownImage))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	KeyLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	KeyLabel->SetText(FText::AsNumber(SlotIndex + 1));
	KeyLabel->SetFont(MakeWizardFont(11, true));
	KeyLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.80f, 0.90f, 0.9f)));
	if (UOverlaySlot* S = Ov->AddChildToOverlay(KeyLabel))
	{
		S->SetHorizontalAlignment(HAlign_Left);
		S->SetVerticalAlignment(VAlign_Top);
		S->SetPadding(FMargin(3.f, 1.f, 0.f, 0.f));
	}

	CooldownLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CooldownLabel->SetText(FText::GetEmpty());
	CooldownLabel->SetFont(MakeWizardFont(18, true));
	CooldownLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	CooldownLabel->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* S = Ov->AddChildToOverlay(CooldownLabel))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}

	ChargeBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	ChargeBar->SetPercent(0.f);
	ChargeBar->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* S = Ov->AddChildToOverlay(ChargeBar))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Bottom);
		S->SetPadding(FMargin(2.f, 0.f, 2.f, 4.f));
	}

	UBorder* Mark = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Mark->SetBrushColor(FLinearColor(0.f, 0.96f, 1.f, 0.95f));
	Mark->SetDesiredSizeScale(FVector2D(1.f, 1.f));
	USizeBox* MarkBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	MarkBox->SetHeightOverride(3.f);
	MarkBox->AddChild(Mark);
	SelectionMark = MarkBox;
	SelectionMark->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* S = Ov->AddChildToOverlay(SelectionMark))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Bottom);
	}
}

void USpellSlotWidget::SetSpell(const USpellDataAsset* Spell)
{
	if (!Spell)
	{
		return;
	}
	Accent = Spell->Color;
	if (ChargeBar)
	{
		ChargeBar->SetFillColorAndOpacity(Accent);
	}
	if (Icon)
	{
		const FString IconPath = Spell->GetIconPath();
		if (UTexture2D* Tex = Cast<UTexture2D>(FSoftObjectPath(IconPath).TryLoad()))
		{
			Icon->SetBrushFromTexture(Tex, false);
			Icon->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			Icon->SetColorAndOpacity(FLinearColor(Accent.R, Accent.G, Accent.B, 0.85f));
		}
	}
}

void USpellSlotWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	TargetScale = bSelected ? 1.12f : 0.96f;
	if (SelectionMark)
	{
		SelectionMark->SetVisibility(bSelected ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void USpellSlotWidget::SetCooldown(float FractionRemaining, float SecondsRemaining)
{
	const bool bOnCD = FractionRemaining > 0.005f;
	if (CooldownImage)
	{
		CooldownImage->SetVisibility(bOnCD ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (bOnCD && CooldownMaterial)
		{
			CooldownMaterial->SetScalarParameterValue(TEXT("Fraction"), FractionRemaining);
		}
	}
	if (CooldownLabel)
	{
		CooldownLabel->SetVisibility(bOnCD ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (bOnCD)
		{
			CooldownLabel->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), SecondsRemaining)));
		}
	}
}

void USpellSlotWidget::SetCharge(float Fraction)
{
	if (!ChargeBar)
	{
		return;
	}
	if (Fraction > 0.01f)
	{
		ChargeBar->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ChargeBar->SetPercent(FMath::Clamp(Fraction, 0.f, 1.f));
	}
	else
	{
		ChargeBar->SetVisibility(ESlateVisibility::Collapsed);
		ChargeBar->SetPercent(0.f);
	}
}

void USpellSlotWidget::PlayCastFlash()
{
	Flash = 1.f;
}

void USpellSlotWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);

	CurrentScale = FMath::FInterpTo(CurrentScale, TargetScale, DeltaTime, 14.f);
	if (Flash > 0.f)
	{
		Flash = FMath::Max(0.f, Flash - DeltaTime * 5.f);
	}
	const float Scale = CurrentScale + Flash * 0.14f;
	SetRenderScale(FVector2D(Scale, Scale));
}

// ---------------------------------------------------------------------------
// UWizardScorePopupWidget
// ---------------------------------------------------------------------------

UWizardScorePopupWidget::UWizardScorePopupWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWizardScorePopupWidget::Activate(const FScorePopupInfo& Info)
{
	WorldLocation = Info.WorldLocation;
	Life = FMath::Max(0.25f, Info.Life);
	Age = 0.f;
	Scale = Info.Scale;
	Color = Info.Color;

	if (!Label && WidgetTree)
	{
		Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Label->SetFont(MakeWizardFont(FMath::RoundToInt(22.f * Scale), true));
		Label->SetShadowOffset(FVector2D(1.5f, 1.5f));
		Label->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		WidgetTree->RootWidget = Label;
	}

	if (Label)
	{
		Label->SetText(Info.Text);
		Label->SetColorAndOpacity(FSlateColor(Color));
	}
	SetRenderOpacity(1.f);
	UpdateScreenPosition();
}

void UWizardScorePopupWidget::UpdateScreenPosition()
{
	APlayerController* PC = GetOwningPlayer();
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
	if (!PC || !CanvasSlot)
	{
		return;
	}

	FVector2D ScreenPos = FVector2D::ZeroVector;
	if (!WorldLocation.IsNearlyZero() && PC->ProjectWorldLocationToScreen(WorldLocation, ScreenPos, true))
	{
		const float ViewportScale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));
		CanvasSlot->SetPosition(ScreenPos / ViewportScale);
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else if (WorldLocation.IsNearlyZero())
	{
		const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
		const float ViewportScale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));
		CanvasSlot->SetPosition((ViewportSize / ViewportScale) * FVector2D(0.5f, 0.42f));
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UWizardScorePopupWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);

	Age += DeltaTime;
	if (Age >= Life)
	{
		RemoveFromParent();
		return;
	}

	UpdateScreenPosition();
	const float T = FMath::Clamp(Age / Life, 0.f, 1.f);
	SetRenderTranslation(FVector2D(0.f, -45.f * T));
	SetRenderOpacity(T < 0.65f ? 1.f : FMath::Clamp(1.f - (T - 0.65f) / 0.35f, 0.f, 1.f));
}

// ---------------------------------------------------------------------------
// UWizardHUDWidget
// ---------------------------------------------------------------------------

UWizardHUDWidget::UWizardHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWizardHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		return;
	}

	Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;

	// 1. Crosshair
	{
		USizeBox* DotBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		DotBox->SetWidthOverride(7.f);
		DotBox->SetHeightOverride(7.f);
		CrosshairDot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		CrosshairDot->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.88f));
		DotBox->AddChild(CrosshairDot);

		if (UOverlaySlot* S = Root->AddChildToOverlay(DotBox))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetVerticalAlignment(VAlign_Center);
		}
	}

	// 2. Top-Right Score & Combo Panel
	{
		UBorder* ScorePanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		ScorePanel->SetBrushColor(FLinearColor(0.035f, 0.035f, 0.06f, 0.72f));
		ScorePanel->SetPadding(FMargin(18.f, 12.f));

		UVerticalBox* ScoreBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		ScorePanel->AddChild(ScoreBox);

		ScoreLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		ScoreLabel->SetText(FText::FromString(TEXT("0")));
		ScoreLabel->SetFont(MakeWizardFont(34, true));
		ScoreLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		ScoreBox->AddChildToVerticalBox(ScoreLabel);

		BestLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		BestLabel->SetText(FText::FromString(TEXT("РЕКОРД: 0")));
		BestLabel->SetFont(MakeWizardFont(11, false));
		BestLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.76f, 0.86f, 0.82f)));
		ScoreBox->AddChildToVerticalBox(BestLabel);

		ComboLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		ComboLabel->SetText(FText::GetEmpty());
		ComboLabel->SetFont(MakeWizardFont(18, true));
		ComboLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.f, 0.96f, 1.f, 1.f)));
		ComboLabel->SetVisibility(ESlateVisibility::Collapsed);
		ScoreBox->AddChildToVerticalBox(ComboLabel);

		if (UOverlaySlot* S = Root->AddChildToOverlay(ScorePanel))
		{
			S->SetHorizontalAlignment(HAlign_Right);
			S->SetVerticalAlignment(VAlign_Top);
			S->SetPadding(FMargin(0.f, 24.f, 28.f, 0.f));
		}
	}

	// 3. Bottom-Center Spell Bar
	{
		UVerticalBox* BottomBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		SpellNameLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		SpellNameLabel->SetText(FText::FromString(TEXT("ПЕПЕЛЬНАЯ СФЕРА")));
		SpellNameLabel->SetFont(MakeWizardFont(13, true));
		SpellNameLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.94f, 0.98f, 0.92f)));
		if (UVerticalBoxSlot* S = BottomBox->AddChildToVerticalBox(SpellNameLabel))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		}

		UHorizontalBox* Bar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Slots.Reset(4);
		for (int32 i = 0; i < 4; ++i)
		{
			USpellSlotWidget* SlotW = CreateWidget<USpellSlotWidget>(GetOwningPlayer(), USpellSlotWidget::StaticClass());
			if (SlotW)
			{
				SlotW->Setup(i);
				Slots.Add(SlotW);
				if (UHorizontalBoxSlot* HS = Bar->AddChildToHorizontalBox(SlotW))
				{
					HS->SetPadding(FMargin(6.f, 0.f));
				}
			}
		}
		BottomBox->AddChildToVerticalBox(Bar);

		if (UOverlaySlot* S = Root->AddChildToOverlay(BottomBox))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetVerticalAlignment(VAlign_Bottom);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 26.f));
		}
	}

	// 4. Bottom-Left Controls Hint
	{
		UBorder* HintPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		HintPanel->SetBrushColor(FLinearColor(0.035f, 0.035f, 0.06f, 0.56f));
		HintPanel->SetPadding(FMargin(12.f, 8.f));

		HintLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		HintLabel->SetText(FText::FromString(TEXT("ЛКМ — Каст / Заряд\n1–4 / Колесо — Стихия\nПКМ / Q — Рывок · Shift — Спринт\nR — Сброс арены · H — Подсказки")));
		HintLabel->SetFont(MakeWizardFont(11, false));
		HintLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.84f, 0.88f, 0.95f, 0.86f)));
		HintPanel->AddChild(HintLabel);

		if (UOverlaySlot* S = Root->AddChildToOverlay(HintPanel))
		{
			S->SetHorizontalAlignment(HAlign_Left);
			S->SetVerticalAlignment(VAlign_Bottom);
			S->SetPadding(FMargin(24.f, 0.f, 0.f, 26.f));
		}
	}

	// 5. Title Card (Intro)
	{
		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Title->SetText(FText::FromString(TEXT("АРЕНА СТИХИЙ")));
		Title->SetFont(MakeWizardFont(42, true));
		Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.96f, 0.97f, 1.f, 1.f)));
		Title->SetShadowOffset(FVector2D(2.f, 2.f));
		Title->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.65f));
		TitleCard = Title;

		if (UOverlaySlot* S = Root->AddChildToOverlay(TitleCard))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetVerticalAlignment(VAlign_Top);
			S->SetPadding(FMargin(0.f, 72.f, 0.f, 0.f));
		}
	}

	// 6. Floating Popups Layer (screen-space canvas projected from WorldLocation)
	{
		PopupLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		if (UOverlaySlot* S = Root->AddChildToOverlay(PopupLayer))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}
	}

	// 7. Arena Reset Flash Overlay
	{
		ResetFlashOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		ResetFlashOverlay->SetColorAndOpacity(FLinearColor(0.85f, 0.96f, 1.f, 0.f));
		ResetFlashOverlay->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* S = Root->AddChildToOverlay(ResetFlashOverlay))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}
	}
}

void UWizardHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UWizardScoreSubsystem* Score = UWizardScoreSubsystem::Get(this))
	{
		Score->OnScoreChanged.AddDynamic(this, &UWizardHUDWidget::OnScoreChanged);
		Score->OnComboChanged.AddDynamic(this, &UWizardHUDWidget::OnComboChanged);
		Score->OnScorePopup.AddDynamic(this, &UWizardHUDWidget::OnPopup);
		OnScoreChanged(Score->GetScore(), Score->GetBestScore());
	}

	if (UWizardDestructionSubsystem* Dest = UWizardDestructionSubsystem::Get(this))
	{
		Dest->OnArenaReset.AddDynamic(this, &UWizardHUDWidget::OnArenaReset);
	}

	RefreshSpells();
	ShowTitleCard();
}

void UWizardHUDWidget::NativeDestruct()
{
	if (UWizardScoreSubsystem* Score = UWizardScoreSubsystem::Get(this))
	{
		Score->OnScoreChanged.RemoveDynamic(this, &UWizardHUDWidget::OnScoreChanged);
		Score->OnComboChanged.RemoveDynamic(this, &UWizardHUDWidget::OnComboChanged);
		Score->OnScorePopup.RemoveDynamic(this, &UWizardHUDWidget::OnPopup);
	}

	if (UWizardDestructionSubsystem* Dest = UWizardDestructionSubsystem::Get(this))
	{
		Dest->OnArenaReset.RemoveDynamic(this, &UWizardHUDWidget::OnArenaReset);
	}

	if (UWizardAbilityComponent* Ab = Abilities.Get())
	{
		Ab->OnSpellCast.RemoveDynamic(this, &UWizardHUDWidget::OnSpellCast);
	}

	Super::NativeDestruct();
}

void UWizardHUDWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);

	if (UWizardAbilityComponent* Ab = GetAbilities())
	{
		const int32 Sel = Ab->GetSelectedIndex();
		const float Charge = Ab->GetChargeFraction();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			if (!Slots[i])
			{
				continue;
			}
			Slots[i]->SetSelected(i == Sel);
			float Frac = 0.f, Sec = 0.f;
			Ab->GetCooldown(i, Frac, Sec);
			Slots[i]->SetCooldown(Frac, Sec);
			Slots[i]->SetCharge(i == Sel ? Charge : 0.f);
		}
		if (SpellNameLabel)
		{
			if (const USpellDataAsset* Sp = Ab->GetSelectedSpell())
			{
				SpellNameLabel->SetText(Sp->DisplayName);
			}
		}
	}

	if (CrosshairDot && CrosshairKick > 0.f)
	{
		CrosshairKick = FMath::Max(0.f, CrosshairKick - DeltaTime * 5.f);
		const float S = 1.f + CrosshairKick * 0.9f;
		CrosshairDot->SetRenderScale(FVector2D(S, S));
	}

	if (TitleCard && TitleAge >= 0.f)
	{
		TitleAge += DeltaTime;
		if (TitleAge < 0.5f)
		{
			TitleCard->SetRenderOpacity(TitleAge / 0.5f);
		}
		else if (TitleAge < 2.6f)
		{
			TitleCard->SetRenderOpacity(1.f);
		}
		else if (TitleAge < 3.6f)
		{
			TitleCard->SetRenderOpacity(FMath::Clamp(1.f - (TitleAge - 2.6f), 0.f, 1.f));
		}
		else
		{
			TitleCard->SetVisibility(ESlateVisibility::Collapsed);
			TitleAge = -1.f;
		}
	}

	if (ResetFlash > 0.f)
	{
		ResetFlash = FMath::Max(0.f, ResetFlash - DeltaTime * 2.5f);
		if (ResetFlashOverlay)
		{
			if (ResetFlash > 0.f)
			{
				const float Alpha = FMath::Pow(ResetFlash, 1.6f) * 0.55f;
				ResetFlashOverlay->SetColorAndOpacity(FLinearColor(0.85f, 0.96f, 1.f, Alpha));
			}
			else
			{
				ResetFlashOverlay->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

void UWizardHUDWidget::RefreshSpells()
{
	UWizardAbilityComponent* Ab = GetAbilities();
	if (!Ab)
	{
		return;
	}
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (Slots[i])
		{
			Slots[i]->SetSpell(Ab->GetSpell(i));
			Slots[i]->SetSelected(i == Ab->GetSelectedIndex());
		}
	}
}

void UWizardHUDWidget::SetHintsVisible(bool bVisible)
{
	if (HintLabel && HintLabel->GetParent())
	{
		HintLabel->GetParent()->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UWizardHUDWidget::ShowTitleCard()
{
	if (TitleCard)
	{
		TitleCard->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		TitleCard->SetRenderOpacity(0.f);
		TitleAge = 0.f;
	}
}

void UWizardHUDWidget::PlayArenaResetFlash()
{
	ResetFlash = 1.f;
	if (ResetFlashOverlay)
	{
		ResetFlashOverlay->SetColorAndOpacity(FLinearColor(0.85f, 0.96f, 1.f, 0.55f));
		ResetFlashOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

UWizardAbilityComponent* UWizardHUDWidget::GetAbilities()
{
	if (Abilities.IsValid())
	{
		return Abilities.Get();
	}
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (UWizardAbilityComponent* Found = Pawn->FindComponentByClass<UWizardAbilityComponent>())
			{
				Abilities = Found;
				Found->OnSpellCast.AddDynamic(this, &UWizardHUDWidget::OnSpellCast);
				RefreshSpells();
				return Found;
			}
		}
	}
	return nullptr;
}

void UWizardHUDWidget::OnScoreChanged(int64 Score, int64 BestScore)
{
	if (ScoreLabel)
	{
		ScoreLabel->SetText(FText::AsNumber(Score));
	}
	if (BestLabel)
	{
		BestLabel->SetText(FText::FromString(FString::Printf(TEXT("РЕКОРД: %lld"), BestScore)));
	}
}

void UWizardHUDWidget::OnComboChanged(float Multiplier, float TimeLeft)
{
	if (!ComboLabel)
	{
		return;
	}
	if (Multiplier > 1.05f && TimeLeft > 0.f)
	{
		ComboLabel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ComboLabel->SetText(FText::FromString(FString::Printf(TEXT("x%.2f"), Multiplier)));
	}
	else
	{
		ComboLabel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UWizardHUDWidget::OnPopup(const FScorePopupInfo& Info)
{
	if (!PopupLayer)
	{
		return;
	}
	UWizardScorePopupWidget* Popup = CreateWidget<UWizardScorePopupWidget>(GetOwningPlayer(), UWizardScorePopupWidget::StaticClass());
	if (Popup)
	{
		if (UCanvasPanelSlot* CanvasSlot = PopupLayer->AddChildToCanvas(Popup))
		{
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		}
		Popup->Activate(Info);
		Popups.Add(Popup);
		if (Popups.Num() > 10)
		{
			if (Popups[0])
			{
				Popups[0]->RemoveFromParent();
			}
			Popups.RemoveAt(0);
		}
	}
}

void UWizardHUDWidget::OnArenaReset()
{
	PlayArenaResetFlash();
}

void UWizardHUDWidget::OnSpellCast(int32 SlotIndex, float ChargeT01)
{
	CrosshairKick = 1.f;
	if (Slots.IsValidIndex(SlotIndex) && Slots[SlotIndex])
	{
		Slots[SlotIndex]->PlayCastFlash();
	}
}
