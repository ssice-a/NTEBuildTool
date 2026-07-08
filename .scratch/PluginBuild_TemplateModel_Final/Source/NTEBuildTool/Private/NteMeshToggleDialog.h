// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "NteMeshToggleTypes.h"

class USkeletalMesh;

namespace NTEBuildTool::Toggle
{
bool ShowMeshToggleSetupDialog(USkeletalMesh& SkeletalMesh, FNteMeshToggleSetupOptions& OutOptions);
}
