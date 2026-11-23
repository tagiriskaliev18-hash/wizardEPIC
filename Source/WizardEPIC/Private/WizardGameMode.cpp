#include "WizardGameMode.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/SoftObjectPath.h"
#include "WizardAbilityComponent.h"
#include "WizardHUD.h"
#include "WizardPlayerController.h"
#include "WizardSmokeTest.h"
#include "WizardDestructionSubsystem.h"
#include "WizardScoreSubsystem.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardGameMode)

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, WizardEPIC, "WizardEPIC");

AWizardGameMode::AWizardGameMode()
{
	HUDClass = AWizardHUD::StaticClass();
	PlayerControllerClass = AWizardPlayerController::StaticClass();
}

void AWizardGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	if (!WizardPawnClass)
	{
		if (UClass* Loaded = FSoftClassPath(WizardPaths::TemplatePawnClass).TryLoadClass<APawn>())
		{
			if (Loaded->IsChildOf(APawn::StaticClass()))
			{
				WizardPawnClass = Loaded;
			}
		}
	}

	if (WizardPawnClass)
	{
		DefaultPawnClass = WizardPawnClass;
	}
	else
	{
		UE_LOG(LogWizard, Error, TEXT("Template pawn class '%s' could not be loaded - using the default pawn"), WizardPaths::TemplatePawnClass);
	}

	Super::InitGame(MapName, Options, ErrorMessage);
}

void AWizardGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void AWizardGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	APawn* Pawn = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	if (Pawn && !Pawn->FindComponentByClass<UWizardAbilityComponent>())
	{
		UWizardAbilityComponent* Abilities = NewObject<UWizardAbilityComponent>(Pawn);
		Abilities->RegisterComponent();
	}

	APlayerController* PC = Cast<APlayerController>(NewPlayer);
	if (PC && !SmokeTest && UWizardSmokeTest::IsRequested())
	{
		SmokeTest = UWizardSmokeTest::TryStart(PC);
	}
}

void AWizardGameMode::ResetArena()
{
	if (UWizardDestructionSubsystem* Destruction = UWizardDestructionSubsystem::Get(this))
	{
		Destruction->ResetAll();
	}
	if (UWizardScoreSubsystem* Score = UWizardScoreSubsystem::Get(this))
	{
		Score->Reset();
	}
}

AWizardGameMode* AWizardGameMode::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	return World ? Cast<AWizardGameMode>(World->GetAuthGameMode()) : nullptr;
}
