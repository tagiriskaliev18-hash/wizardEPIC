// Copyright (c) 2026 wizardEPIC. All rights reserved.

using UnrealBuildTool;

public class WizardEPIC : ModuleRules
{
	public WizardEPIC(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"EngineCameras",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Niagara",
			"PhysicsCore"
		});
	}
}
