#include "WizardSmokeTest.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "WizardAbilityComponent.h"
#include "WizardDestructionSubsystem.h"
#include "WizardGameMode.h"
#include "WizardScoreSubsystem.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardSmokeTest)

bool UWizardSmokeTest::IsRequested()
{
	const TCHAR* Cmd = FCommandLine::Get();
	if (FParse::Param(Cmd, TEXT("wizardsmoke")))
	{
		return true;
	}
	FString DemoPath;
	return FParse::Value(Cmd, TEXT("wizarddemo="), DemoPath) && !DemoPath.IsEmpty();
}

UWizardSmokeTest* UWizardSmokeTest::TryStart(const UObject* InWorldContext)
{
	if (!IsRequested() || !InWorldContext)
	{
		return nullptr;
	}

	UWizardSmokeTest* Test = NewObject<UWizardSmokeTest>(GetTransientPackage());
	if (!Test)
	{
		return nullptr;
	}
	Test->WorldContext = const_cast<UObject*>(InWorldContext);
	Test->Start(1.2f);
	return Test;
}

void UWizardSmokeTest::RequestShot(const FString& Name)
{
	const FString ShotPath = FPaths::ProjectSavedDir() / TEXT("Screenshots/Windows") / (Name + TEXT(".png"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ShotPath), true);
	FScreenshotRequest::RequestScreenshot(ShotPath, false, false);
	Report(FString::Printf(TEXT("SHOT %s"), *Name));
}

void UWizardSmokeTest::Report(const FString& Line)
{
	const FString ReportPath = FPaths::ProjectSavedDir() / TEXT("SmokeTest/report.txt");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	FFileHelper::SaveStringToFile(Line + LINE_TERMINATOR, *ReportPath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
	UE_LOG(LogWizard, Log, TEXT("[SmokeTest] %s"), *Line);
}

void UWizardSmokeTest::RequestQuit(const UObject* InWorldContext)
{
	Report(TEXT("QUIT requested"));
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(InWorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (World)
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			UKismetSystemLibrary::QuitGame(World, PC, EQuitPreference::Quit, false);
			return;
		}
	}
	FPlatformMisc::RequestExit(false);
}

void UWizardSmokeTest::Start(float InitialDelay)
{
	Time = -InitialDelay;
	NextStep = 0;
	bFinished = false;
	Report(TEXT("=== wizardEPIC SmokeTest START ==="));

	TWeakObjectPtr<UWizardSmokeTest> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float DeltaTime) -> bool
	{
		UWizardSmokeTest* Self = WeakThis.Get();
		if (!Self || Self->bFinished || !Self->WorldContext.IsValid())
		{
			return false;
		}
		Self->Step(DeltaTime);
		return !Self->bFinished;
	}));
}

void UWizardSmokeTest::Step(float DeltaTime)
{
	Time += DeltaTime;
	if (Time < 0.f)
	{
		return;
	}

	FString DemoPath;
	if (FParse::Value(FCommandLine::Get(), TEXT("wizarddemo="), DemoPath) && !DemoPath.IsEmpty())
	{
		RunScriptFile(DemoPath);
	}
	else
	{
		RunBuiltIn();
	}
}

void UWizardSmokeTest::RunBuiltIn()
{
	struct FStepDef { float At; const TCHAR* Cmd; };
	static const FStepDef Steps[] =
	{
		{ 0.2f,  TEXT("Shot 01_arena_start") },
		{ 1.2f,  TEXT("Select 0") },
		{ 1.4f,  TEXT("Cast 0 0.8") },
		{ 2.6f,  TEXT("Shot 02_fire") },
		{ 3.2f,  TEXT("Select 1") },
		{ 3.4f,  TEXT("Cast 1 0.8") },
		{ 4.8f,  TEXT("Shot 03_earth") },
		{ 5.4f,  TEXT("Select 2") },
		{ 5.6f,  TEXT("Cast 2 0.8") },
		{ 6.6f,  TEXT("Shot 04_wind") },
		{ 7.2f,  TEXT("Select 3") },
		{ 7.4f,  TEXT("Cast 3 0.8") },
		{ 8.8f,  TEXT("Shot 05_lightning") },
		{ 9.4f,  TEXT("Stats") },
		{ 10.0f, TEXT("Reset") },
		{ 11.0f, TEXT("Shot 06_after_reset") },
		{ 11.8f, TEXT("Quit") }
	};

	const int32 NumSteps = UE_ARRAY_COUNT(Steps);
	while (NextStep < NumSteps && Time >= Steps[NextStep].At)
	{
		ExecuteLine(Steps[NextStep].Cmd);
		NextStep++;
	}

	if (NextStep >= NumSteps)
	{
		bFinished = true;
	}
}

void UWizardSmokeTest::RunScriptFile(const FString& Path)
{
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *Path))
	{
		Report(FString::Printf(TEXT("ERROR: could not load script '%s'"), *Path));
		bFinished = true;
		RequestQuit(WorldContext.Get());
		return;
	}

	while (NextStep < Lines.Num())
	{
		const FString Trimmed = Lines[NextStep].TrimStartAndEnd();
		if (Trimmed.IsEmpty() || Trimmed.StartsWith(TEXT("#")))
		{
			NextStep++;
			continue;
		}
		FString TimeStr, Rest;
		if (Trimmed.Split(TEXT(" "), &TimeStr, &Rest))
		{
			const float At = FCString::Atof(*TimeStr);
			if (Time < At)
			{
				break;
			}
			ExecuteLine(Rest.TrimStartAndEnd());
		}
		NextStep++;
	}

	if (NextStep >= Lines.Num())
	{
		bFinished = true;
	}
}

void UWizardSmokeTest::ExecuteLine(const FString& Line)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext.Get(), EGetWorldErrorMode::ReturnNull) : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UWizardAbilityComponent* Ab = Pawn ? Pawn->FindComponentByClass<UWizardAbilityComponent>() : nullptr;

	TArray<FString> Tokens;
	Line.ParseIntoArrayWS(Tokens);
	if (Tokens.IsEmpty())
	{
		return;
	}

	const FString& Cmd = Tokens[0];
	if (Cmd.Equals(TEXT("Shot"), ESearchCase::IgnoreCase) && Tokens.Num() >= 2)
	{
		RequestShot(Tokens[1]);
	}
	else if (Cmd.Equals(TEXT("Select"), ESearchCase::IgnoreCase) && Tokens.Num() >= 2 && Ab)
	{
		Ab->SelectSpell(FCString::Atoi(*Tokens[1]));
	}
	else if (Cmd.Equals(TEXT("Cast"), ESearchCase::IgnoreCase) && Ab)
	{
		const int32 Slot = Tokens.Num() >= 2 ? FCString::Atoi(*Tokens[1]) : Ab->GetSelectedIndex();
		const float Charge = Tokens.Num() >= 3 ? FCString::Atof(*Tokens[2]) : 0.8f;
		Ab->SelectSpell(Slot);
		FVector AimPt, AimDir;
		Ab->GetAim(AimPt, AimDir);
		Ab->CastAt(AimPt, Charge);
		Report(FString::Printf(TEXT("CAST slot=%d charge=%.2f"), Slot, Charge));
	}
	else if (Cmd.Equals(TEXT("Dash"), ESearchCase::IgnoreCase) && Ab)
	{
		Ab->Dash();
	}
	else if (Cmd.Equals(TEXT("Reset"), ESearchCase::IgnoreCase))
	{
		if (AWizardGameMode* GM = AWizardGameMode::Get(World))
		{
			GM->ResetArena();
			Report(TEXT("RESET arena"));
		}
	}
	else if (Cmd.Equals(TEXT("Stats"), ESearchCase::IgnoreCase))
	{
		int64 ScoreVal = 0;
		if (UWizardScoreSubsystem* Sc = UWizardScoreSubsystem::Get(World))
		{
			ScoreVal = Sc->GetScore();
		}
		int32 Breakables = 0, Loose = 0;
		if (UWizardDestructionSubsystem* Dest = UWizardDestructionSubsystem::Get(World))
		{
			Breakables = Dest->GetBreakableCount();
			Loose = Dest->GetLoosePieceCount();
		}
		Report(FString::Printf(TEXT("STATS score=%lld breakables=%d loose=%d"), ScoreVal, Breakables, Loose));
	}
	else if (Cmd.Equals(TEXT("Teleport"), ESearchCase::IgnoreCase) && Tokens.Num() >= 4 && Pawn)
	{
		Pawn->SetActorLocation(FVector(FCString::Atof(*Tokens[1]), FCString::Atof(*Tokens[2]), FCString::Atof(*Tokens[3])));
	}
	else if (Cmd.Equals(TEXT("LookAt"), ESearchCase::IgnoreCase) && Tokens.Num() >= 4 && Ab)
	{
		Ab->LookAt(FVector(FCString::Atof(*Tokens[1]), FCString::Atof(*Tokens[2]), FCString::Atof(*Tokens[3])));
	}
	else if (Cmd.Equals(TEXT("Quit"), ESearchCase::IgnoreCase))
	{
		bFinished = true;
		RequestQuit(WorldContext.Get());
	}
}
