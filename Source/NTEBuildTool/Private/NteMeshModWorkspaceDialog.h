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

struct FNteMeshModWorkspaceResult
{
	ENteMeshModWorkspaceAction Action = ENteMeshModWorkspaceAction::None;
	FString MeshPath;
	int32 SlotIndex = INDEX_NONE;
	FString SlotName;
	FString MaterialPath;
};

bool ShowMeshModWorkspaceDialog(USkeletalMesh* SelectedMesh, ENteMeshModWorkspaceAction& OutAction);
bool ShowMeshModWorkspaceDialog(USkeletalMesh* SelectedMesh, FNteMeshModWorkspaceResult& OutResult);
}
