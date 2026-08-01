// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace NTEBuildTool::Character
{
struct FNteCharacterModSpec;

struct FNteCharacterMaterialOperationPlanItem
{
	FString Id;
	FString TargetMeshId;
	FString TargetKind;
	FString TargetMeshPath;
	int32 SlotIndex = INDEX_NONE;
	FString SlotName;
	FString ExistingMaterialPath;
	FString SourceMaterialJson;
	FString ParentMaterialPath;
	FString OutputMaterialPath;
	bool bDerivedParentMaterialPath = false;
	bool bDerivedOutputMaterialPath = false;
	TMap<FString, FString> SourceTextureOverrides;
	bool bAssignToSlot = true;
	TArray<FString> Errors;
	TArray<FString> Warnings;
};

struct FNteCharacterMaterialPlan
{
	TArray<FNteCharacterMaterialOperationPlanItem> Operations;
	TArray<FString> Errors;
	TArray<FString> Warnings;
};

FNteCharacterMaterialPlan BuildCharacterMaterialPlanFromSpec(const FNteCharacterModSpec& Spec);
TSharedRef<FJsonObject> CharacterMaterialPlanToJson(const FNteCharacterMaterialPlan& Plan);
TArray<FString> CollectCharacterMaterialPlanPackageSeeds(const FNteCharacterMaterialPlan& Plan);
}
