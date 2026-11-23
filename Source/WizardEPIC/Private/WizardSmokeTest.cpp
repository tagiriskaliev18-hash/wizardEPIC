#include "WizardSmokeTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardSmokeTest)

// Skeleton build: the scripted sequence and console commands are implemented in the next step.

bool UWizardSmokeTest::IsRequested()
{
	return FParse::Param(FCommandLine::Get(), TEXT("wizardsmoke")) || FParse::Param(FCommandLine::Get(), TEXT("wizarddemo"));
}

UWizardSmokeTest* UWizardSmokeTest::TryStart(const UObject* WorldContext)
{
	return nullptr;
}

void UWizardSmokeTest::RequestShot(const FString& Name)
{
	FScreenshotRequest::RequestScreenshot(Name, false, false);
}

void UWizardSmokeTest::Report(const FString& Line)
{
}

void UWizardSmokeTest::RequestQuit(const UObject* WorldContext)
{
}

void UWizardSmokeTest::Start(float InitialDelay)
{
}

void UWizardSmokeTest::Step(float DeltaTime)
{
}

void UWizardSmokeTest::RunBuiltIn()
{
}

void UWizardSmokeTest::RunScriptFile(const FString& Path)
{
}

void UWizardSmokeTest::ExecuteLine(const FString& Line)
{
}
