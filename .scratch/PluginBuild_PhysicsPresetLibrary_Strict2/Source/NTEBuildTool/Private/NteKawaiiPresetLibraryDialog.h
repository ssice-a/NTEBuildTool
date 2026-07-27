// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;

namespace NTEBuildTool::Kawaii
{
struct FNteFModelKawaiiPresetLibrarySelection
{
	FString SourceAnimLayerJson;
	FString SourceNodeName;
	FString SuggestedPresetPrefix;
};

bool ShowFModelKawaiiPresetLibraryDialog(
	const USkeletalMesh& TargetMesh,
	FNteFModelKawaiiPresetLibrarySelection& OutSelection);
}
