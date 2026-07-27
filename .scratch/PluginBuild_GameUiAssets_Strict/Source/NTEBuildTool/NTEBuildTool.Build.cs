// Copyright (c) 2026 NTEBuildTool contributors.

using UnrealBuildTool;

public class NTEBuildTool : ModuleRules
{
	public NTEBuildTool(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"HTGame",
			"InputCore",
			"UnrealEd",
			"AssetTools",
			"AssetRegistry",
			"AnimGraph",
			"AnimGraphRuntime",
			"BlueprintGraph",
			"ContentBrowser",
			"DeveloperSettings",
			"DesktopPlatform",
			"LevelEditor",
			"PhysicsUtilities",
			"Projects",
			"Json",
			"JsonUtilities",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"UMG",
			"UMGEditor"
		});
	}
}
