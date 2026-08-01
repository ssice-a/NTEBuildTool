// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteEditorAssetUtils.h"

#include "HTAttachedMeshAnimInstance.h"

#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SkeletalMesh.h"
#include "IContentBrowserSingleton.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "Subsystems/AssetEditorSubsystem.h"

namespace NTEBuildTool::Editor
{
TArray<FAssetData> GetSelectedContentBrowserAssets()
{
	TArray<FAssetData> SelectedAssets;
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);
	return SelectedAssets;
}

TArray<FString> GetSelectedContentBrowserPaths()
{
	TArray<FString> SelectedPaths;
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowserModule.Get().GetSelectedPathViewFolders(SelectedPaths);
	return SelectedPaths;
}

FString GetSelectedContentBrowserPath()
{
	const TArray<FString> SelectedPaths = GetSelectedContentBrowserPaths();
	return SelectedPaths.Num() == 1 && IsGameContentPath(SelectedPaths[0]) ? SelectedPaths[0] : TEXT("/Game");
}

USkeletalMesh* GetSingleSelectedSkeletalMesh()
{
	USkeletalMesh* SelectedSkeletalMesh = nullptr;
	for (const FAssetData& AssetData : GetSelectedContentBrowserAssets())
	{
		if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(AssetData.GetAsset()))
		{
			if (SelectedSkeletalMesh)
			{
				return nullptr;
			}

			SelectedSkeletalMesh = SkeletalMesh;
		}
	}

	return SelectedSkeletalMesh;
}

UMaterialInstanceConstant* GetSingleSelectedMaterialInstanceConstant()
{
	UMaterialInstanceConstant* SelectedMaterialInstance = nullptr;
	for (const FAssetData& AssetData : GetSelectedContentBrowserAssets())
	{
		if (UMaterialInstanceConstant* MaterialInstance = Cast<UMaterialInstanceConstant>(AssetData.GetAsset()))
		{
			if (SelectedMaterialInstance)
			{
				return nullptr;
			}

			SelectedMaterialInstance = MaterialInstance;
		}
	}

	return SelectedMaterialInstance;
}

bool IsGamePackageName(const FString& PackageName)
{
	return PackageName.StartsWith(TEXT("/Game/")) && !PackageName.Contains(TEXT("."));
}

bool IsGameContentPath(const FString& ContentPath)
{
	return ContentPath == TEXT("/Game") || ContentPath.StartsWith(TEXT("/Game/"));
}

FString NormalizeAssetPathForText(FString AssetPath)
{
	AssetPath.TrimStartAndEndInline();
	AssetPath.TrimQuotesInline();

	int32 DotIndex = INDEX_NONE;
	if (AssetPath.FindLastChar(TEXT('.'), DotIndex))
	{
		const FString PackageName = AssetPath.Left(DotIndex);
		const FString ObjectName = AssetPath.Mid(DotIndex + 1);
		const bool bFModelExportIndex = !ObjectName.IsEmpty() && ObjectName.IsNumeric();
		if (FPackageName::GetShortName(PackageName) == ObjectName || bFModelExportIndex)
		{
			AssetPath = PackageName;
		}
	}

	return AssetPath;
}

FString ToObjectPath(const FString& AssetPath)
{
	if (AssetPath.Contains(TEXT(".")))
	{
		return AssetPath;
	}

	return AssetPath + TEXT(".") + FPackageName::GetShortName(AssetPath);
}

FString JoinAssetPath(const FString& PackagePath, const FString& AssetName)
{
	FString NormalizedPackagePath = PackagePath;
	NormalizedPackagePath.TrimStartAndEndInline();
	NormalizedPackagePath.RemoveFromEnd(TEXT("/"));
	return NormalizedPackagePath / AssetName;
}

FString TryConvertFilenameToGamePackagePath(const FString& Filename)
{
	FString NormalizedFilename = FPaths::ConvertRelativePathToFull(Filename);
	FPaths::NormalizeFilename(NormalizedFilename);

	FString ContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	FPaths::NormalizeFilename(ContentDir);
	ContentDir.RemoveFromEnd(TEXT("/"));

	if (!NormalizedFilename.StartsWith(ContentDir / TEXT("")) || !NormalizedFilename.EndsWith(TEXT(".uasset"), ESearchCase::IgnoreCase))
	{
		return FString();
	}

	FString RelativePath = NormalizedFilename.Mid((ContentDir / TEXT("")).Len());
	RelativePath.RemoveFromEnd(TEXT(".uasset"), ESearchCase::IgnoreCase);
	return TEXT("/Game/") + RelativePath;
}

FString GetAssetPackagePath(UObject* Asset)
{
	return Asset ? Asset->GetPackage()->GetName() : FString();
}

UObject* LoadAnyAssetByPath(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return nullptr;
	}

	return StaticLoadObject(UObject::StaticClass(), nullptr, *ToObjectPath(NormalizeAssetPathForText(AssetPath)));
}

bool OpenAssetEditorByPath(const FString& AssetPath, FString& OutError)
{
	OutError.Reset();

	UObject* Asset = LoadAnyAssetByPath(AssetPath);
	if (!Asset)
	{
		OutError = FString::Printf(TEXT("Could not load asset: %s"), *AssetPath);
		return false;
	}
	if (!GEditor)
	{
		OutError = TEXT("GEditor is unavailable; asset editors can only be opened in the editor.");
		return false;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		OutError = TEXT("AssetEditorSubsystem is unavailable.");
		return false;
	}
	if (!AssetEditorSubsystem->OpenEditorForAsset(Asset))
	{
		OutError = FString::Printf(TEXT("Could not open editor for asset: %s"), *AssetPath);
		return false;
	}
	return true;
}

UClass* GetAttachedMeshAnimInstanceParentClass()
{
	return UHTAttachedMeshAnimInstance::StaticClass();
}

bool EnsureBlueprintParentClass(
	UBlueprint& Blueprint,
	UClass& ExpectedParentClass,
	bool& OutChanged,
	FString& OutError)
{
	OutChanged = false;
	OutError.Reset();
	if (Blueprint.ParentClass && Blueprint.ParentClass->IsChildOf(&ExpectedParentClass))
	{
		return true;
	}

	Blueprint.Modify();
	Blueprint.ParentClass = &ExpectedParentClass;
	if (UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(Blueprint.GeneratedClass))
	{
		GeneratedClass->PrepareToConformSparseClassData(ExpectedParentClass.GetSparseClassDataStruct());
	}
	FBlueprintEditorUtils::RefreshAllNodes(&Blueprint);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);
	if (!Blueprint.ParentClass || !Blueprint.ParentClass->IsChildOf(&ExpectedParentClass))
	{
		OutError = FString::Printf(
			TEXT("Could not reparent Blueprint %s to %s."),
			*Blueprint.GetPathName(),
			*ExpectedParentClass.GetPathName());
		return false;
	}

	OutChanged = true;
	return true;
}
}
