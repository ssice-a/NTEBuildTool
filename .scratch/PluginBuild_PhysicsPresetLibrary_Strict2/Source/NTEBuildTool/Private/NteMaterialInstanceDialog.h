// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "NteMaterialInstanceTool.h"

namespace NTEBuildTool::Material
{
bool ShowFModelMaterialLibraryRecipeDialog(const FNteMaterialInstanceOptions& InitialOptions, FNteMaterialInstanceOptions& OutOptions, TSharedPtr<FJsonObject>& OutSourceTextureOverrides);
bool ShowMaterialInstanceRecipeDialog(const FString& SourceMaterialJson, const FNteMaterialInstanceOptions& InitialOptions, FNteMaterialInstanceOptions& OutOptions, TSharedPtr<FJsonObject>& OutSourceTextureOverrides);
}
