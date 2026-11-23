// Copyright (c) 2026 wizardEPIC. All rights reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class WizardEPICEditorTarget : TargetRules
{
	public WizardEPICEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		bUseAdaptiveUnityBuild = false;
		ExtraModuleNames.Add("WizardEPIC");
	}
}
