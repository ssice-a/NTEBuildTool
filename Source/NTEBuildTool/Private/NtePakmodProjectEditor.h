// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "NteModPackageTypes.h"
#include "NtePakmodProject.h"

class SWidget;
class SWindow;

namespace NTEBuildTool::ProjectEditor
{
class FNtePakmodProjectEditorModel
{
public:
	FNtePakmodProjectEditorModel();

	NTEBuildTool::Project::FNtePakmodProject Project;
	NTEBuildTool::Package::FNtePackagePlan PackagePlan;
	FString Filename;
	FString Status;
	TArray<FString> Diagnostics;
	bool bDirty = false;

	void NewProject();
	bool Load(const FString& ProjectFilename, FString& OutError);
	bool Save(FString& OutError);
	bool SaveAs(const FString& ProjectFilename, FString& OutError);
	bool AddSelectedAssets(NTEBuildTool::Project::ENteAssetIntent Intent, FString& OutError);
	bool AddAssetPath(const FString& PackagePath, const FString& ClassName, NTEBuildTool::Project::ENteAssetOrigin Origin, NTEBuildTool::Project::ENteAssetIntent Intent, FString& OutError);
	bool RemoveAsset(const FString& AssetId, FString& OutError);
	void ToggleManifestAsset(const FString& AssetId);
	bool RefreshPackagePlan(FString& OutError);
	bool Validate(FString& OutError) const;
};

TSharedRef<SWidget> MakePakmodProjectEditorWidget();
void OpenPakmodProjectEditorTab();
void RegisterPakmodProjectEditorTab();
void UnregisterPakmodProjectEditorTab();
}
