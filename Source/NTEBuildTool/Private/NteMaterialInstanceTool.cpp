// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteMaterialInstanceTool.h"

#include "NteEditorAssetUtils.h"
#include "NteJsonFileUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/Texture.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"

namespace NTEBuildTool::Material
{
using namespace NTEBuildTool::Editor;
using namespace NTEBuildTool::Json;

FString DeriveParentMaterialPathFromFModelJson(const FString& SourceMaterialJson)
{
	FString Normalized = SourceMaterialJson;
	FPaths::NormalizeFilename(Normalized);

	const FString ContentMarker = TEXT("/Content/");
	const int32 ContentIndex = Normalized.Find(ContentMarker, ESearchCase::IgnoreCase, ESearchDir::FromStart);
	if (ContentIndex == INDEX_NONE)
	{
		return FString();
	}

	FString RelativePath = Normalized.Mid(ContentIndex + ContentMarker.Len());
	RelativePath.RemoveFromEnd(TEXT(".json"), ESearchCase::IgnoreCase);
	return TEXT("/Game/") + RelativePath;
}

FString MakeModMaterialNameFromFModelJson(const FString& SourceMaterialJson)
{
	return TEXT("MI_mod_") + FPaths::GetBaseFilename(SourceMaterialJson);
}

FString DeriveModMaterialFolderFromParentPath(const FString& ParentMaterialPath, const FString& FallbackPath)
{
	const FString ParentFolder = FPackageName::GetLongPackagePath(ParentMaterialPath);
	const TArray<FString> SourceMarkers = {
		TEXT("/ter/"),
		TEXT("/ter_new/"),
		TEXT("/materials/"),
		TEXT("/Materials/")
	};

	for (const FString& Marker : SourceMarkers)
	{
		const int32 MarkerIndex = ParentFolder.Find(Marker, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if (MarkerIndex != INDEX_NONE)
		{
			return ParentFolder.Left(MarkerIndex) / TEXT("mod/Materials");
		}
	}

	return IsGameContentPath(FallbackPath) ? FallbackPath : TEXT("/Game");
}

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
	FString& OutError)
{
	OutResult = FNteMaterialInstanceCreateResult();
	OutError = TEXT("Material instance creation has not been migrated into the modular tool yet.");
	return false;
}

bool ApplyModMaterialConfigFromFile(const FString& ConfigFilename, FNteMaterialConfigApplyResult& OutResult, FString& OutError)
{
	OutResult = FNteMaterialConfigApplyResult();

	TSharedPtr<FJsonObject> Config;
	if (!LoadJsonObjectFromFile(ConfigFilename, Config, OutError))
	{
		return false;
	}

	OutResult.SourceMaterialJson = GetStringAny(*Config, TEXT("SourceMaterialJson"), TEXT("sourceMaterialJson"), TEXT("FModelMaterialJson"));
	OutResult.OutputMaterialPath = NormalizeAssetPathForText(GetStringAny(*Config, TEXT("OutputMaterial"), TEXT("outputMaterial")));
	OutResult.ParentMaterialPath = NormalizeAssetPathForText(GetStringAny(*Config, TEXT("ParentMaterial"), TEXT("parentMaterial")));
	if (OutResult.ParentMaterialPath.IsEmpty() && !OutResult.SourceMaterialJson.IsEmpty())
	{
		OutResult.ParentMaterialPath = DeriveParentMaterialPathFromFModelJson(OutResult.SourceMaterialJson);
	}

	OutError = TEXT("Material recipe parsing succeeded, but material writing is not migrated yet.");
	return false;
}

bool SaveMaterialInstanceOverrideReport(UMaterialInstanceConstant& MaterialInstance, FString& OutFilename, FString& OutError)
{
	OutFilename = FPaths::ProjectSavedDir() / TEXT("NTEBuildTool/MaterialReports") / (MaterialInstance.GetName() + TEXT(".json"));
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("MaterialInstance"), MaterialInstance.GetPackage()->GetName());
	Root->SetNumberField(TEXT("TextureOverrideCount"), MaterialInstance.TextureParameterValues.Num());
	Root->SetNumberField(TEXT("ScalarOverrideCount"), MaterialInstance.ScalarParameterValues.Num());
	Root->SetNumberField(TEXT("VectorOverrideCount"), MaterialInstance.VectorParameterValues.Num());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutFilename), true);
	return SaveJsonObjectToFile(Root, OutFilename, OutError);
}
}
