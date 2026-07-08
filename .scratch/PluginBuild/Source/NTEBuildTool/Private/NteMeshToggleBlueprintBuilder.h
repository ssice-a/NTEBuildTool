// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "NteMeshToggleTypes.h"

namespace NTEBuildTool::Toggle
{
struct FNteMeshToggleBlueprintBuildResult
{
	TArray<FString> Actions;
	TArray<FString> Warnings;
};

bool BuildMeshToggleRuntimeBlueprints(
	const FNteMeshToggleSetupOptions& Options,
	FNteMeshToggleSetupResult& InOutResult,
	FNteMeshToggleBlueprintBuildResult& OutBuildResult,
	FString& OutError);
}
