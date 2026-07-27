// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;

namespace NTEBuildTool::Physics
{
struct FNteFModelPhysicsAssetLibrarySelection
{
	FString SourcePhysicsAssetJson;
	FName ChainRootBone = NAME_None;
};

bool ShowFModelPhysicsAssetLibraryDialog(
	const USkeletalMesh& TargetMesh,
	FNteFModelPhysicsAssetLibrarySelection& OutSelection);
}
