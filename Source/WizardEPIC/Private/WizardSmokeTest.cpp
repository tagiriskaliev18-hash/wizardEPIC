#include "WizardSmokeTest.h"

#include "Containers/Ticker.h"
#include "CrystalTarget.h"
#include "DestructibleStructure.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
#include "WizardBreakableBase.h"
#include "WizardBreakableSet.h"
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
	FScreenshotRequest::RequestScreenshot(ShotPath, true, false);

	UWorld* World = nullptr;
	if (GEngine)
	{
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if (Ctx.WorldType == EWorldType::Game || Ctx.WorldType == EWorldType::PIE)
			{
				if (UWorld* Candidate = Ctx.World())
				{
					World = Candidate;
					break;
				}
			}
		}
	}

	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	FVector CamLoc = FVector::ZeroVector;
	FRotator CamRot = FRotator::ZeroRotator;
	if (PC)
	{
		PC->GetPlayerViewPoint(CamLoc, CamRot);
	}
	const FString PawnName = Pawn ? Pawn->GetName() : TEXT("NONE");
	const FString PawnLocStr = Pawn ? Pawn->GetActorLocation().ToCompactString() : TEXT("NONE");
	Report(FString::Printf(TEXT("SHOT %s Pawn=%s Loc=%s CamLoc=%s CamRot=%s"),
		*Name, *PawnName, *PawnLocStr, *CamLoc.ToCompactString(), *CamRot.ToCompactString()));
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
	if (MoveTimeRemaining > 0.f)
	{
		UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext.Get(), EGetWorldErrorMode::ReturnNull) : nullptr;
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (APawn* Pawn = PC ? PC->GetPawn() : nullptr)
		{
			const FRotator YawRot(0.f, PC->GetControlRotation().Yaw, 0.f);
			const FVector ForwardDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
			const FVector RightDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
			if (!FMath::IsNearlyZero(MoveForward))
			{
				Pawn->AddMovementInput(ForwardDir, MoveForward);
			}
			if (!FMath::IsNearlyZero(MoveRight))
			{
				Pawn->AddMovementInput(RightDir, MoveRight);
			}
		}
		MoveTimeRemaining = FMath::Max(0.f, MoveTimeRemaining - DeltaTime);
	}

	if (WaitRemaining > 0.f)
	{
		WaitRemaining = FMath::Max(0.f, WaitRemaining - DeltaTime);
		return;
	}

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
		{ 0.10f, TEXT("Aim -8 90") },
		{ 0.50f, TEXT("Shot 01_arena_start") },
		{ 0.80f, TEXT("Move 1 0 0.7") },
		{ 1.60f, TEXT("AimNearest") },
		{ 1.70f, TEXT("Select 0") },
		{ 1.80f, TEXT("Cast 0 0.9") },
		{ 2.15f, TEXT("Shot 02_fire") },
		{ 2.80f, TEXT("AimNearest") },
		{ 2.90f, TEXT("Select 1") },
		{ 3.00f, TEXT("Cast 1 0.9") },
		{ 3.45f, TEXT("Shot 03_earth") },
		{ 4.20f, TEXT("AimNearest") },
		{ 4.30f, TEXT("Select 2") },
		{ 4.40f, TEXT("Cast 2 0.9") },
		{ 4.70f, TEXT("Shot 04_wind") },
		{ 5.40f, TEXT("AimNearest") },
		{ 5.50f, TEXT("Select 3") },
		{ 5.60f, TEXT("Cast 3 0.9") },
		{ 5.85f, TEXT("Shot 05_lightning") },
		{ 6.70f, TEXT("Stats") },
		{ 7.00f, TEXT("Reset") },
		{ 7.70f, TEXT("Shot 06_after_reset") },
		{ 8.30f, TEXT("Quit") }
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
	else if (Cmd.Equals(TEXT("Aim"), ESearchCase::IgnoreCase) && Tokens.Num() >= 3 && Ab)
	{
		const float Pitch = FCString::Atof(*Tokens[1]);
		const float Yaw = FCString::Atof(*Tokens[2]);
		Ab->SetViewRotation(FRotator(Pitch, Yaw, 0.f));
	}
	else if (Cmd.Equals(TEXT("AimNearest"), ESearchCase::IgnoreCase) && Ab)
	{
		AimAtNearestBreakable(World, Pawn, Ab);
	}
	else if (Cmd.Equals(TEXT("Move"), ESearchCase::IgnoreCase) && Tokens.Num() >= 4)
	{
		MoveForward = FMath::Clamp(FCString::Atof(*Tokens[1]), -1.f, 1.f);
		MoveRight = FMath::Clamp(FCString::Atof(*Tokens[2]), -1.f, 1.f);
		MoveTimeRemaining = FMath::Max(0.f, FCString::Atof(*Tokens[3]));
	}
	else if (Cmd.Equals(TEXT("Wait"), ESearchCase::IgnoreCase))
	{
		if (Tokens.Num() >= 2)
		{
			WaitRemaining = FMath::Max(0.f, FCString::Atof(*Tokens[1]));
		}
	}
	else if (Cmd.Equals(TEXT("Quit"), ESearchCase::IgnoreCase))
	{
		bFinished = true;
		RequestQuit(WorldContext.Get());
	}
}

bool UWizardSmokeTest::AimAtNearestBreakable(UWorld* World, APawn* Pawn, UWizardAbilityComponent* Abilities)
{
	if (!World || !Pawn || !Abilities)
	{
		return false;
	}

	const FVector Origin = Pawn->GetActorLocation();
	constexpr float MinPreferredDistSq = 550.f * 550.f;

	float BestPreferredDistSq = TNumericLimits<float>::Max();
	FVector BestPreferredPoint = FVector::ZeroVector;
	bool bFoundPreferred = false;

	float BestFallbackDistSq = TNumericLimits<float>::Max();
	FVector BestFallbackPoint = FVector::ZeroVector;
	bool bFoundFallback = false;

	auto ConsiderTarget = [&](const FVector& Target)
	{
		const float DSq = static_cast<float>(FVector::DistSquared(Origin, Target));
		if (DSq >= MinPreferredDistSq)
		{
			if (DSq < BestPreferredDistSq)
			{
				BestPreferredDistSq = DSq;
				BestPreferredPoint = Target;
				bFoundPreferred = true;
			}
		}
		else if (DSq < BestFallbackDistSq)
		{
			BestFallbackDistSq = DSq;
			BestFallbackPoint = Target;
			bFoundFallback = true;
		}
	};

	for (TActorIterator<AWizardBreakableBase> It(World); It; ++It)
	{
		AWizardBreakableBase* Breakable = *It;
		if (!Breakable || !IsValid(Breakable))
		{
			continue;
		}

		if (AWizardBreakableSet* Set = Cast<AWizardBreakableSet>(Breakable))
		{
			const int32 Count = Set->GetInstanceCount();
			for (int32 i = 0; i < Count; ++i)
			{
				if (!Set->IsInstanceAlive(i))
				{
					continue;
				}
				FTransform InstT;
				if (Set->GetInstanceTransform(i, InstT))
				{
					ConsiderTarget(InstT.GetLocation() + FVector(0.f, 0.f, 35.f));
				}
			}
		}
		else if (ADestructibleStructure* Struct = Cast<ADestructibleStructure>(Breakable))
		{
			const int32 PieceCount = Struct->GetPieceCount();
			for (int32 i = 0; i < PieceCount; ++i)
			{
				FVector PieceCentre;
				if (Struct->IsPieceIntact(i) && Struct->GetPieceCentre(i, PieceCentre))
				{
					ConsiderTarget(PieceCentre);
				}
			}
		}
		else if (ACrystalTarget* Crystal = Cast<ACrystalTarget>(Breakable))
		{
			if (!Crystal->IsAlive())
			{
				continue;
			}
			ConsiderTarget(Crystal->GetBreakableBounds().Origin);
		}
		else
		{
			ConsiderTarget(Breakable->GetBreakableBounds().Origin);
		}
	}

	if (bFoundPreferred)
	{
		Abilities->LookAt(BestPreferredPoint);
		return true;
	}
	if (bFoundFallback)
	{
		Abilities->LookAt(BestFallbackPoint);
		return true;
	}
	return false;
}
