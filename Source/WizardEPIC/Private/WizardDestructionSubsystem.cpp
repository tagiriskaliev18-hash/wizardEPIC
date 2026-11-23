#include "WizardDestructionSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "WizardBreakableBase.h"
TStatId UWizardDestructionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWizardDestructionSubsystem, STATGROUP_Tickables);
}

#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardDestructionSubsystem)

// Skeleton build: impacts, batching and feedback are implemented in the next step.

UWizardDestructionSubsystem* UWizardDestructionSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	return World ? World->GetSubsystem<UWizardDestructionSubsystem>() : nullptr;
}

void UWizardDestructionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UWizardDestructionSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UWizardDestructionSubsystem::Tick(float DeltaTime)
{
}

void UWizardDestructionSubsystem::RegisterBreakable(AWizardBreakableBase* Breakable)
{
	if (Breakable && !Breakables.Contains(Breakable))
	{
		Breakables.Add(Breakable);
	}
}

void UWizardDestructionSubsystem::UnregisterBreakable(AWizardBreakableBase* Breakable)
{
	Breakables.Remove(Breakable);
}

int32 UWizardDestructionSubsystem::ImpactRadial(const FSpellImpact& Impact)
{
	return 0;
}

int32 UWizardDestructionSubsystem::ImpactCone(const FVector& Origin, const FVector& Direction, float HalfAngleDeg, float Range, const FSpellImpact& Impact)
{
	return 0;
}

int32 UWizardDestructionSubsystem::ImpactLaunchUpward(const FVector& Origin, float Radius, float Height, const FSpellImpact& Impact)
{
	return 0;
}

void UWizardDestructionSubsystem::PushDynamicBodies(const FVector& Origin, float Radius, float Speed, float UpBias)
{
}

void UWizardDestructionSubsystem::ResetAll()
{
}

int32 UWizardDestructionSubsystem::GetLoosePieceCount() const
{
	return 0;
}

int32 UWizardDestructionSubsystem::GetBreakableCount() const
{
	return Breakables.Num();
}

void UWizardDestructionSubsystem::Flush()
{
}

void UWizardDestructionSubsystem::ReactToBatch(const TArray<FDestructibleEvent>& Events)
{
}

void UWizardDestructionSubsystem::EnforceLooseLimit()
{
}
