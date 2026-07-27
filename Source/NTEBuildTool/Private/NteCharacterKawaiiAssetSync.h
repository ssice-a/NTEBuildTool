// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"

#include "NteCharacterKawaiiPlan.h"
#include "NteCharacterModSpec.h"

namespace NTEBuildTool::Character
{
struct FNteCharacterKawaiiAssetSyncResult
{
	FString PresetId;
	FString RuntimeAnimBlueprintPath;
	TArray<FString> UpdatedFields;
	TArray<FString> Warnings;
	TArray<FString> Errors;

	bool HasErrors() const { return !Errors.IsEmpty(); }
};

FNteCharacterKawaiiAssetSyncResult SyncKawaiiPresetSpecFromGeneratedAssets(
	const FNteCharacterKawaiiPresetPlanItem& PlanItem,
	FNteCharacterKawaiiPresetSpec& InOutPreset);
}
