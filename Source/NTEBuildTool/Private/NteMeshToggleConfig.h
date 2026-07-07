// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "NteMeshToggleTypes.h"

namespace NTEBuildTool::Toggle
{
bool LoadMeshToggleSetupOptionsFromJsonFile(const FString& ConfigFilename, FNteMeshToggleSetupOptions& OutOptions, FString& OutError);
bool SaveMeshToggleSetupOptionsToJsonFile(const FNteMeshToggleSetupOptions& Options, const FString& ConfigFilename, FString& OutError);
bool RunMeshToggleUiSetup(FNteMeshToggleSetupOptions Options, FNteMeshToggleSetupResult& OutResult, FString& OutError);
}
