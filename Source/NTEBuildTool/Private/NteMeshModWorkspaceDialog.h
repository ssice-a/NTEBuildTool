// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "NteCharacterModSpec.h"

class USkeletalMesh;

namespace NTEBuildTool::Workspace
{
enum class ENteMeshModWorkspaceAction
{
	None,
	ApplyMaterialOperation,
	ConfigureToggleRuntime,
	BuildPackage
};

struct FNteMeshModWorkspaceResult
{
	ENteMeshModWorkspaceAction Action = ENteMeshModWorkspaceAction::None;
	NTEBuildTool::Character::FNteCharacterModSpec CharacterSpec;
	FString SpecFilename;
	FString MeshPath;
	int32 SlotIndex = INDEX_NONE;
	FString SlotName;
	FString MaterialPath;
};

bool ShowMeshModWorkspaceDialog(USkeletalMesh* SelectedMesh, ENteMeshModWorkspaceAction& OutAction);
bool ShowMeshModWorkspaceDialog(USkeletalMesh* SelectedMesh, FNteMeshModWorkspaceResult& OutResult);
}
