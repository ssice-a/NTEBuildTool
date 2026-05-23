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
			"UnrealEd",
			"AssetTools",
			"AssetRegistry",
			"ContentBrowser",
			"DesktopPlatform",
			"LevelEditor",
			"PhysicsUtilities",
			"Json",
			"JsonUtilities",
			"Slate",
			"SlateCore",
			"ToolMenus"
		});
	}
}
