#include "WizardGameMode.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/SoftObjectPath.h"
#include "WizardAbilityComponent.h"
#include "WizardCharacter.h"
#include "WizardHUD.h"
#include "WizardPlayerController.h"
#include "WizardSmokeTest.h"
#include "WizardDestructionSubsystem.h"
#include "WizardScoreSubsystem.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardGameMode)

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, WizardEPIC, "WizardEPIC");

AWizardGameMode::AWizardGameMode()
{
	DefaultPawnClass = AWizardCharacter::StaticClass();
	HUDClass = AWizardHUD::StaticClass();
	PlayerControllerClass = AWizardPlayerController::StaticClass();
}

void AWizardGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	DefaultPawnClass = WizardPawnClass ? WizardPawnClass : TSubclassOf<APawn>(AWizardCharacter::StaticClass());

	Super::InitGame(MapName, Options, ErrorMessage);
}

void AWizardGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void AWizardGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	if (NewPlayer && !NewPlayer->GetPawn())
	{
		if (UWorld* World = GetWorld())
		{
			UClass* PawnClassToSpawn = DefaultPawnClass ? DefaultPawnClass.Get() : AWizardCharacter::StaticClass();
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FTransform FallbackTransform(FRotator(0.f, 90.f, 0.f), FVector(0.f, -900.f, 160.f));
			if (APawn* SpawnedPawn = World->SpawnActor<APawn>(PawnClassToSpawn, FallbackTransform, SpawnParams))
			{
				NewPlayer->Possess(SpawnedPawn);
			}
		}
	}

	APawn* Pawn = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	if (Pawn)
	{
		const FRotator SpawnedRot(0.f, Pawn->GetActorRotation().Yaw, 0.f);
		Pawn->SetActorRotation(SpawnedRot);
		NewPlayer->SetControlRotation(FRotator(-8.f, SpawnedRot.Yaw, 0.f));

		if (!Pawn->FindComponentByClass<UWizardAbilityComponent>())
		{
			UWizardAbilityComponent* Abilities = NewObject<UWizardAbilityComponent>(Pawn);
			Abilities->RegisterComponent();
		}
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
