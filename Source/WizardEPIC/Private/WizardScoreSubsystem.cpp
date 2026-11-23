#include "WizardScoreSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WizardAudioSubsystem.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardScoreSubsystem)

TStatId UWizardScoreSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWizardScoreSubsystem, STATGROUP_Tickables);
}

UWizardScoreSubsystem* UWizardScoreSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	return World ? World->GetSubsystem<UWizardScoreSubsystem>() : nullptr;
}

void UWizardScoreSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadBest();
}

void UWizardScoreSubsystem::Tick(float DeltaTime)
{
	if (ComboTimer > 0.f)
	{
		ComboTimer = FMath::Max(0.f, ComboTimer - DeltaTime);
		if (ComboTimer <= 0.f)
		{
			ComboCount = 0;
			ComboMultiplier = 1.f;
		}
		OnComboChanged.Broadcast(ComboMultiplier, ComboTimer);
	}
}

void UWizardScoreSubsystem::HandleDestructibleBatch(const TArray<FDestructibleEvent>& Events)
{
	if (Events.IsEmpty())
	{
		return;
	}

	int64 BatchPoints = 0;
	FVector SumLoc = FVector::ZeroVector;
	int32 NormalCount = 0;
	bool bAnySpecial = false;

	for (const FDestructibleEvent& Ev : Events)
	{
		if (Ev.Points <= 0)
		{
			continue;
		}

		if (Ev.bIsCrystal)
		{
			bAnySpecial = true;
			AddBonus(Ev.Points, NSLOCTEXT("Wizard", "CrystalBonus", "КРИСТАЛЛ"), Ev.Location, FLinearColor(0.25f, 0.95f, 1.f));
			continue;
		}

		if (Ev.bIsCollapseBonus)
		{
			bAnySpecial = true;
			AddBonus(Ev.Points, NSLOCTEXT("Wizard", "CollapseBonus", "ОБРУШЕНИЕ"), Ev.Location, FLinearColor(1.f, 0.72f, 0.22f));
			continue;
		}

		ComboCount++;
		ComboMultiplier = FMath::Min(ComboMaxMultiplier, 1.f + (ComboCount - 1) * ComboStep);
		ComboTimer = ComboWindow;

		const int64 Awarded = FMath::RoundToInt64(static_cast<double>(Ev.Points) * static_cast<double>(ComboMultiplier));
		BatchPoints += Awarded;
		SumLoc += Ev.Location;
		NormalCount++;
	}

	if (BatchPoints > 0)
	{
		Score += BatchPoints;
		if (Score > BestScore)
		{
			BestScore = Score;
			SaveBest();
		}

		OnScoreChanged.Broadcast(Score, BestScore);
		OnComboChanged.Broadcast(ComboMultiplier, ComboTimer);

		FScorePopupInfo Popup;
		Popup.WorldLocation = NormalCount > 0 ? (SumLoc / static_cast<float>(NormalCount)) + FVector(0.f, 0.f, 45.f) : FVector::ZeroVector;
		Popup.Text = FText::FromString(FString::Printf(TEXT("+%lld"), BatchPoints));
		Popup.Color = ComboMultiplier >= 2.5f ? FLinearColor(1.f, 0.78f, 0.25f) : FLinearColor(0.96f, 0.97f, 1.f);
		Popup.Scale = FMath::Clamp(0.95f + 0.12f * ComboMultiplier, 1.f, 1.65f);
		Popup.Life = 1.15f;
		OnScorePopup.Broadcast(Popup);

		if (ComboCount > 0 && (ComboCount % 5) == 0 && !bAnySpecial)
		{
			if (UWizardAudioSubsystem* Audio = UWizardAudioSubsystem::Get(this))
			{
				Audio->Play2D(TEXT("Combo"), 0.55f, 0.95f + 0.08f * ComboMultiplier);
			}
		}
	}
}

void UWizardScoreSubsystem::AddBonus(int32 Points, const FText& Label, const FVector& AtLocation, const FLinearColor& Color)
{
	if (Points <= 0)
	{
		return;
	}

	ComboTimer = FMath::Max(ComboTimer, ComboWindow);
	const int64 Awarded = FMath::RoundToInt64(static_cast<double>(Points) * static_cast<double>(ComboMultiplier));
	Score += Awarded;
	if (Score > BestScore)
	{
		BestScore = Score;
		SaveBest();
	}

	OnScoreChanged.Broadcast(Score, BestScore);
	OnComboChanged.Broadcast(ComboMultiplier, ComboTimer);

	FScorePopupInfo Popup;
	Popup.WorldLocation = AtLocation + FVector(0.f, 0.f, 70.f);
	Popup.Text = FText::FromString(FString::Printf(TEXT("%s +%lld"), *Label.ToString(), Awarded));
	Popup.Color = Color;
	Popup.Scale = 1.65f;
	Popup.Life = 1.55f;
	OnScorePopup.Broadcast(Popup);
}

void UWizardScoreSubsystem::Reset()
{
	Score = 0;
	ComboCount = 0;
	ComboMultiplier = 1.f;
	ComboTimer = 0.f;
	OnScoreChanged.Broadcast(Score, BestScore);
	OnComboChanged.Broadcast(ComboMultiplier, ComboTimer);
}

void UWizardScoreSubsystem::LoadBest()
{
	const FString SavePath = FPaths::ProjectSavedDir() / TEXT("WizardBestScore.txt");
	FString Raw;
	if (FFileHelper::LoadFileToString(Raw, *SavePath))
	{
		BestScore = FMath::Max<int64>(0, FCString::Atoi64(*Raw.TrimStartAndEnd()));
	}
}

void UWizardScoreSubsystem::SaveBest()
{
	const FString SavePath = FPaths::ProjectSavedDir() / TEXT("WizardBestScore.txt");
	FFileHelper::SaveStringToFile(FString::Printf(TEXT("%lld"), BestScore), *SavePath);
}
