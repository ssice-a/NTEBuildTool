// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace NTEBuildTool::Character
{
struct FNteCharacterMaterialPlan;

struct FNteCharacterMaterialSourceTextureUsageItem
{
	FString SourceTexturePath;
	TArray<FString> ParameterNames;
};

struct FNteCharacterMaterialOperationWriteResult
{
	FString Id;
	FString TargetMeshPath;
	int32 SlotIndex = INDEX_NONE;
	FString SourceMaterialJson;
	FString ParentMaterialPath;
	FString OutputMaterialPath;
	FString AssignedMaterialPath;
	FString ReportFilename;
	int32 TextureOverrides = 0;
	int32 SourceTextureOverrideGroups = 0;
	int32 ScalarOverrides = 0;
	int32 VectorOverrides = 0;
	int32 StaticSwitchOverrides = 0;
	bool bCreatedParentPlaceholder = false;
	bool bCreatedMaterialProxy = false;
	bool bApplied = false;
	TArray<FString> MissingTextures;
	TArray<FString> UnmatchedSourceTextureOverrides;
	TArray<FNteCharacterMaterialSourceTextureUsageItem> SourceTextureUsage;
	TArray<FString> Errors;
	TArray<FString> Warnings;
};

struct FNteCharacterMaterialWriteResult
{
	TArray<FNteCharacterMaterialOperationWriteResult> Operations;
	TArray<FString> Errors;
	TArray<FString> Warnings;

	bool HasErrors() const { return !Errors.IsEmpty(); }
	bool HasWarnings() const { return !Warnings.IsEmpty(); }
};

FNteCharacterMaterialWriteResult WriteCharacterMaterials(const FNteCharacterMaterialPlan& Plan);
TSharedRef<FJsonObject> CharacterMaterialWriteResultToJson(const FNteCharacterMaterialWriteResult& Result);
}
