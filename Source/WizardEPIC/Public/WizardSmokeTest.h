// WizardSmokeTest.h - scripted verification without a human.
//
//   UnrealEditor.exe wizardEPIC.uproject -game -wizardsmoke
//       built-in sequence: screenshots of the arena, every spell, a structure collapse, the reset.
//
//   UnrealEditor.exe wizardEPIC.uproject -game -wizarddemo="D:\demo.txt"
//       a script file, one command per line:  <seconds>  <command> [args]
//       commands: Cast <slot> <charge0-1>   Select <slot>   Aim <pitch> <yaw>   LookAt <x> <y> <z>
//                 Teleport <x> <y> <z>   Move <forward-1..1> <right-1..1> <seconds>   Dash
//                 Shot <name>   Reset   Wait   Quit   Stats
//
// Screenshots go to <Project>/Saved/Screenshots/Windows/ and a summary is written to
// <Project>/Saved/SmokeTest/report.txt. The console commands below work in any session.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WizardSmokeTest.generated.h"

UCLASS()
class WIZARDEPIC_API UWizardSmokeTest : public UObject
{
	GENERATED_BODY()

public:
	/** True for -wizardsmoke or -wizarddemo=<file> on the command line. */
	static bool IsRequested();

	/** Creates and starts the test when requested. Call once the local player is ready. */
	static UWizardSmokeTest* TryStart(const UObject* WorldContext);

	/** Requests a screenshot saved as <Name>.png under Saved/Screenshots/Windows. */
	static void RequestShot(const FString& Name);

	/** Appends a line to Saved/SmokeTest/report.txt. */
	static void Report(const FString& Line);

	/** Requests a graceful exit. */
	static void RequestQuit(const UObject* WorldContext);

	void Start(float InitialDelay);
	void Step(float DeltaTime);

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> WorldContext;

	float Time = 0.f;
	int32 NextStep = 0;
	bool bFinished = false;

	void RunBuiltIn();
	void RunScriptFile(const FString& Path);
	void ExecuteLine(const FString& Line);
};
