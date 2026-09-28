// Copyright Broken Rock Studios LLC. All Rights Reserved.

using UnrealBuildTool;

public class VarnPythonTools : ModuleRules
{
	public VarnPythonTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange([
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"ToolMenus",
			"Projects",
			"Settings",
			"PythonScriptPlugin"
		]);
	}
}