// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;

namespace NTEBuildTool::Workspace
{
enum class ENteMeshModWorkspaceAction
{
	None,
	CreateMaterialInstance,
	ConfigureToggleRuntime,
	BuildPackage
};

bool ShowMeshModWorkspaceDialog(USkeletalMesh* SelectedMesh, ENteMeshModWorkspaceAction& OutAction);
}
