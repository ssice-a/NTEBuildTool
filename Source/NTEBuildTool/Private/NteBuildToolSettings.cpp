// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteBuildToolSettings.h"

#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

UNteBuildToolSettings::UNteBuildToolSettings()
{
	GameMountName = TEXT("HT");
}

FName UNteBuildToolSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FName UNteBuildToolSettings::GetSectionName() const
{
	return TEXT("NTEBuildTool");
}

namespace NTEBuildTool::Settings
{
namespace
{
FString NormalizeAssetPath(FString AssetPath)
{
	AssetPath.TrimStartAndEndInline();
	AssetPath.TrimQuotesInline();

	int32 DotIndex = INDEX_NONE;
	if (AssetPath.FindLastChar(TEXT('.'), DotIndex))
	{
		const FString PackageName = AssetPath.Left(DotIndex);
		const FString ObjectName = AssetPath.Mid(DotIndex + 1);
		if (FPackageName::GetShortName(PackageName) == ObjectName)
		{
			AssetPath = PackageName;
		}
	}

	return AssetPath;
}
}

const UNteBuildToolSettings* Get()
{
	return GetDefault<UNteBuildToolSettings>();
}

FString GetGameMountName()
{
	const UNteBuildToolSettings* Settings = Get();
	FString Value = Settings ? Settings->GameMountName : FString();
	Value.TrimStartAndEndInline();
	if (Value.IsEmpty())
	{
		Value = FApp::GetProjectName();
	}
	return Value;
}

FString GetDefaultModsOutputDirectory()
{
	const UNteBuildToolSettings* Settings = Get();
	FString Value = Settings ? Settings->DefaultModsOutputDirectory.Path : FString();
	Value.TrimStartAndEndInline();
	Value.TrimQuotesInline();
	if (Value.IsEmpty())
	{
		Value = FPaths::ProjectSavedDir() / TEXT("NTEBuildTool/Mods");
	}
	FPaths::NormalizeFilename(Value);
	return Value;
}

FString GetFModelExportRoot()
{
	const UNteBuildToolSettings* Settings = Get();
	FString Value = Settings ? Settings->FModelExportRoot.Path : FString();
	Value.TrimStartAndEndInline();
	Value.TrimQuotesInline();
	FPaths::NormalizeFilename(Value);
	return Value;
}

FString GetDefaultTemplatePostProcessAnimBlueprintPath()
{
	const UNteBuildToolSettings* Settings = Get();
	return Settings ? NormalizeAssetPath(Settings->DefaultTemplatePostProcessAnimBlueprint.GetAssetPathString()) : FString();
}

FString GetDefaultTemplateWidgetBlueprintPath()
{
	const UNteBuildToolSettings* Settings = Get();
	return Settings ? NormalizeAssetPath(Settings->DefaultTemplateWidgetBlueprint.GetAssetPathString()) : FString();
}

FString GetDefaultTemplateSaveGameBlueprintPath()
{
	const UNteBuildToolSettings* Settings = Get();
	return Settings ? NormalizeAssetPath(Settings->DefaultTemplateSaveGameBlueprint.GetAssetPathString()) : FString();
}
}
