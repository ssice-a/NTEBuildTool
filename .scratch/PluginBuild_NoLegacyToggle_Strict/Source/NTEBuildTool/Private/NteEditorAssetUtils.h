// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

class UMaterialInstanceConstant;
class UObject;
class USkeletalMesh;

namespace NTEBuildTool::Editor
{
TArray<FAssetData> GetSelectedContentBrowserAssets();
TArray<FString> GetSelectedContentBrowserPaths();
FString GetSelectedContentBrowserPath();

USkeletalMesh* GetSingleSelectedSkeletalMesh();
UMaterialInstanceConstant* GetSingleSelectedMaterialInstanceConstant();

bool IsGamePackageName(const FString& PackageName);
bool IsGameContentPath(const FString& ContentPath);
FString NormalizeAssetPathForText(FString AssetPath);
FString ToObjectPath(const FString& AssetPath);
FString JoinAssetPath(const FString& PackagePath, const FString& AssetName);
FString TryConvertFilenameToGamePackagePath(const FString& Filename);
FString GetAssetPackagePath(UObject* Asset);
UObject* LoadAnyAssetByPath(const FString& AssetPath);
bool OpenAssetEditorByPath(const FString& AssetPath, FString& OutError);

template <typename AssetType>
AssetType* LoadAssetByPath(const FString& AssetPath)
{
	return Cast<AssetType>(LoadAnyAssetByPath(AssetPath));
}
}
