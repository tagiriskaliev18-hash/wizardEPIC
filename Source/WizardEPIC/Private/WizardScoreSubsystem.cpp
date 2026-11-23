#include "WizardScoreSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
TStatId UWizardScoreSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWizardScoreSubsystem, STATGROUP_Tickables);
}

#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardScoreSubsystem)

// Skeleton build: combo chain, popups and best-score persistence are implemented in the next step.

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
}

void UWizardScoreSubsystem::HandleDestructibleBatch(const TArray<FDestructibleEvent>& Events)
{
}

void UWizardScoreSubsystem::AddBonus(int32 Points, const FText& Label, const FVector& AtLocation, const FLinearColor& Color)
{
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
}

void UWizardScoreSubsystem::SaveBest()
{
}
