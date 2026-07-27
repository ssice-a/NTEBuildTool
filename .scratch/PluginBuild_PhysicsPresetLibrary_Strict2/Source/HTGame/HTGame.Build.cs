// Copyright (c) 2026 NTEBuildTool contributors.

using UnrealBuildTool;

public class HTGame : ModuleRules
{
	public HTGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});
	}
}
