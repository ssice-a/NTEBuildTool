// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "NteMaterialInstanceTool.h"

namespace NTEBuildTool::Material
{
bool ShowMaterialInstanceRecipeDialog(const FString& SourceMaterialJson, FNteMaterialInstanceOptions& OutOptions, TSharedPtr<FJsonObject>& OutSourceTextureOverrides);
}
