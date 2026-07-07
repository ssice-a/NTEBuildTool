// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UMaterialInstanceConstant;
class USkeletalMesh;

namespace NTEBuildTool::Material
{
struct FNteMaterialApplySummary
{
	int32 TextureOverrides = 0;
	int32 ScalarOverrides = 0;
	int32 VectorOverrides = 0;
	int32 StaticSwitchOverrides = 0;
	TArray<FString> MissingTextures;
	TArray<FString> InvalidVectors;
};

struct FNteMaterialInstanceOptions
{
	FString SourceMaterialJson;
	FString ParentMaterialPath;
	FString OutputMaterialPath;
	FString MeshPath;
	int32 SlotIndex = INDEX_NONE;
	bool bAssignToMeshSlot = false;
	bool bCopySourceParameters = false;
	bool bCopySourceTextures = false;
	bool bResetForPakTextureOnly = true;
	bool bAllowStaticSwitchOverrides = false;
	bool bEnsureParentPlaceholder = true;
	bool bReplaceWrongParentPlaceholder = true;
	TSharedPtr<FJsonObject> TextureOverrides;
	TArray<FName> ManagedTextureParameters;
};

struct FNteMaterialInstanceCreateResult
{
	UMaterialInstanceConstant* MaterialInstance = nullptr;
	USkeletalMesh* TargetMesh = nullptr;
	FString AssignedMaterialPath;
	FNteMaterialApplySummary ApplySummary;
	bool bCreatedParentPlaceholder = false;
};

struct FNteMaterialConfigApplyResult
{
	FString SourceMaterialJson;
	FString OutputMaterialPath;
	FString ParentMaterialPath;
	FString ReportFilename;
	FNteMaterialInstanceCreateResult CreateResult;
};

FString DeriveParentMaterialPathFromFModelJson(const FString& SourceMaterialJson);
FString MakeModMaterialNameFromFModelJson(const FString& SourceMaterialJson);
FString DeriveModMaterialFolderFromParentPath(const FString& ParentMaterialPath, const FString& FallbackPath);

bool CreateOrUpdateModMaterialInstance(
	const FNteMaterialInstanceOptions& Options,
	const FJsonObject* SourceTextures,
	const FJsonObject* SourceScalars,
	const FJsonObject* SourceColors,
	const FJsonObject* SourceSwitches,
	const FJsonObject* TextureOverrides,
	const FJsonObject* ScalarOverrides,
	const FJsonObject* VectorOverrides,
	const FJsonObject* StaticSwitchOverrides,
	FNteMaterialInstanceCreateResult& OutResult,
	FString& OutError);

bool ApplyModMaterialConfigFromFile(const FString& ConfigFilename, FNteMaterialConfigApplyResult& OutResult, FString& OutError);
bool SaveMaterialInstanceOverrideReport(UMaterialInstanceConstant& MaterialInstance, FString& OutFilename, FString& OutError);
}
