// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPath.h"

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

	UPROPERTY(config, EditAnywhere, Category="Toggle Runtime", meta=(DisplayName="Default Post Process Template"))
	FSoftObjectPath DefaultTemplatePostProcessAnimBlueprint;

	UPROPERTY(config, EditAnywhere, Category="Toggle Runtime", meta=(DisplayName="Default Widget Template"))
	FSoftObjectPath DefaultTemplateWidgetBlueprint;

	UPROPERTY(config, EditAnywhere, Category="Toggle Runtime", meta=(DisplayName="Default SaveGame Template"))
	FSoftObjectPath DefaultTemplateSaveGameBlueprint;
};

namespace NTEBuildTool::Settings
{
const UNteBuildToolSettings* Get();
FString GetGameMountName();
FString GetDefaultModsOutputDirectory();
FString GetFModelExportRoot();
FString GetDefaultTemplatePostProcessAnimBlueprintPath();
FString GetDefaultTemplateWidgetBlueprintPath();
FString GetDefaultTemplateSaveGameBlueprintPath();
}
