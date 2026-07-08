// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UMaterialInstanceConstant;
class UObject;

namespace NTEBuildTool::Material
{
struct FNteMaterialSourceTextureUsage
{
	FString SourceTexturePath;
	TArray<FString> ParameterNames;
};

struct FNteMaterialApplySummary
{
	int32 TextureOverrides = 0;
	int32 SourceTextureOverrideGroups = 0;
	int32 ScalarOverrides = 0;
	int32 VectorOverrides = 0;
	int32 StaticSwitchOverrides = 0;
	TArray<FString> MissingTextures;
	TArray<FString> InvalidVectors;
	TArray<FString> UnmatchedSourceTextureOverrides;
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
	TSharedPtr<FJsonObject> SourceTextureOverrides;
	TArray<FName> ManagedTextureParameters;
};

struct FNteMaterialInstanceCreateResult
{
	UMaterialInstanceConstant* MaterialInstance = nullptr;
	UObject* TargetMesh = nullptr;
	FString AssignedMaterialPath;
	FNteMaterialApplySummary ApplySummary;
	bool bCreatedParentPlaceholder = false;
	bool bCreatedMaterialProxy = false;
};

struct FNteMaterialConfigApplyResult
{
	FString SourceMaterialJson;
	FString OutputMaterialPath;
	FString ParentMaterialPath;
	FString ReportFilename;
	TArray<FNteMaterialSourceTextureUsage> SourceTextureUsage;
	FNteMaterialInstanceCreateResult CreateResult;
};

FString DeriveParentMaterialPathFromFModelJson(const FString& SourceMaterialJson);
FString MakeModMaterialNameFromFModelJson(const FString& SourceMaterialJson);
FString DeriveModMaterialFolderFromParentPath(const FString& ParentMaterialPath, const FString& FallbackPath);
const FJsonObject* FindSourceMaterialParameterObject(const FJsonObject* SourceObject, const TCHAR* SectionName);
TArray<FNteMaterialSourceTextureUsage> BuildSourceTextureUsage(const FJsonObject* SourceTextures);
TSharedRef<FJsonObject> ExpandSourceTextureOverridesToParameters(
	const TArray<FNteMaterialSourceTextureUsage>& SourceTextureUsage,
	const FJsonObject* SourceTextureOverrides,
	int32& OutMatchedGroups,
	TArray<FString>* OutUnmatchedSourceTextures = nullptr);

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

bool ApplyModMaterialConfig(
	const FNteMaterialInstanceOptions& Options,
	const FJsonObject* SourceTextureOverrides,
	const FJsonObject* TextureOverrides,
	const FJsonObject* ScalarOverrides,
	const FJsonObject* VectorOverrides,
	const FJsonObject* StaticSwitchOverrides,
	FNteMaterialConfigApplyResult& OutResult,
	FString& OutError);
bool ApplyModMaterialConfigFromFile(const FString& ConfigFilename, FNteMaterialConfigApplyResult& OutResult, FString& OutError);
bool SaveMaterialInstanceOverrideReport(UMaterialInstanceConstant& MaterialInstance, FString& OutFilename, FString& OutError);
}
