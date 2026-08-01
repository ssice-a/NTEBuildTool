// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteBuildToolSettings.h"

#include "Misc/App.h"
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

FString GetLastPakmodProject()
{
	const UNteBuildToolSettings* Settings = Get();
	FString Value = Settings ? Settings->LastPakmodProject.FilePath : FString();
	Value.TrimStartAndEndInline();
	Value.TrimQuotesInline();
	FPaths::NormalizeFilename(Value);
	return Value;
}

TArray<FString> GetRecentPakmodProjects()
{
	TArray<FString> Result;
	const UNteBuildToolSettings* Settings = Get();
	if (!Settings)
	{
		return Result;
	}
	for (const FFilePath& Entry : Settings->RecentPakmodProjects)
	{
		FString Filename = Entry.FilePath;
		Filename.TrimStartAndEndInline();
		Filename.TrimQuotesInline();
		FPaths::NormalizeFilename(Filename);
		if (!Filename.IsEmpty())
		{
			Result.AddUnique(Filename);
		}
	}
	return Result;
}

void RememberPakmodProject(const FString& Filename)
{
	FString Normalized = Filename;
	Normalized.TrimStartAndEndInline();
	Normalized.TrimQuotesInline();
	FPaths::NormalizeFilename(Normalized);
	if (Normalized.IsEmpty())
	{
		return;
	}

	UNteBuildToolSettings* Settings = GetMutableDefault<UNteBuildToolSettings>();
	Settings->LastPakmodProject.FilePath = Normalized;
	Settings->RecentPakmodProjects.RemoveAll([&Normalized](const FFilePath& Entry)
	{
		return Entry.FilePath.Equals(Normalized, ESearchCase::IgnoreCase);
	});
	FFilePath Entry;
	Entry.FilePath = Normalized;
	Settings->RecentPakmodProjects.Insert(MoveTemp(Entry), 0);
	if (Settings->RecentPakmodProjects.Num() > 10)
	{
		Settings->RecentPakmodProjects.SetNum(10);
	}
	Settings->SaveConfig();
}

}
