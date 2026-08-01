// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "NteBuildToolSettings.generated.h"

UCLASS(config=EditorPerProjectUserSettings, defaultconfig, meta=(DisplayName="NTE Build Tool"))
class NTEBUILDTOOL_API UNteBuildToolSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UNteBuildToolSettings();

	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;

	UPROPERTY(config, EditAnywhere, Category="Package", meta=(DisplayName="Game Mount Name"))
	FString GameMountName;

	UPROPERTY(config, EditAnywhere, Category="Package", meta=(DisplayName="Default Mods Output Directory"))
	FDirectoryPath DefaultModsOutputDirectory;

	UPROPERTY(config, EditAnywhere, Category="Source", meta=(DisplayName="FModel Export Root"))
	FDirectoryPath FModelExportRoot;

	UPROPERTY(config, VisibleAnywhere, Category="Pakmod Project", meta=(DisplayName="Last Pakmod Project"))
	FFilePath LastPakmodProject;

	UPROPERTY(config, VisibleAnywhere, Category="Pakmod Project", meta=(DisplayName="Recent Pakmod Projects"))
	TArray<FFilePath> RecentPakmodProjects;

};

namespace NTEBuildTool::Settings
{
const UNteBuildToolSettings* Get();
FString GetGameMountName();
FString GetDefaultModsOutputDirectory();
FString GetFModelExportRoot();
FString GetLastPakmodProject();
TArray<FString> GetRecentPakmodProjects();
void RememberPakmodProject(const FString& Filename);
}
