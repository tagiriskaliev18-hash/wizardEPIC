// Copyright (c) 2026 wizardEPIC. All rights reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class WizardEPICTarget : TargetRules
{
	public WizardEPICTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		bUseAdaptiveUnityBuild = false;
		ExtraModuleNames.Add("WizardEPIC");
	}
}
