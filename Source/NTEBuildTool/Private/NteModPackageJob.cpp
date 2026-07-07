// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteModPackageJob.h"

#include "NteJsonFileUtils.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/Paths.h"

namespace NTEBuildTool::Package
{
using namespace NTEBuildTool::Json;

namespace
{
FString PackageModeToString(const ENteModPackageMode Mode)
{
	switch (Mode)
	{
	case ENteModPackageMode::CookOnly:
		return TEXT("CookOnly");
	case ENteModPackageMode::PackOnly:
		return TEXT("PackOnly");
	default:
		return TEXT("CookAndPack");
	}
}

ENteModPackageMode PackageModeFromString(const FString& Value)
{
	if (Value.Equals(TEXT("CookOnly"), ESearchCase::IgnoreCase))
	{
		return ENteModPackageMode::CookOnly;
	}
	if (Value.Equals(TEXT("PackOnly"), ESearchCase::IgnoreCase))
	{
		return ENteModPackageMode::PackOnly;
	}
	return ENteModPackageMode::CookAndPack;
}
}

bool LoadModPackageJobJson(const FString& JobFilename, FNteModPackageJob& OutJob, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	if (!LoadJsonObjectFromFile(JobFilename, Root, OutError))
	{
		return false;
	}

	const FString Format = GetStringAny(*Root, TEXT("Format"), TEXT("format"));
	if (!Format.IsEmpty() && Format != TEXT("NTE.ModPackageJob") && Format != TEXT("NTE.ModPackageProfile"))
	{
		OutError = FString::Printf(TEXT("Unsupported package job format: %s"), *Format);
		return false;
	}

	OutJob.ProjectRoot = GetStringAny(*Root, TEXT("ProjectRoot"), TEXT("projectRoot"));
	OutJob.ProjectFile = GetStringAny(*Root, TEXT("ProjectFile"), TEXT("projectFile"));
	OutJob.ProjectName = GetStringAny(*Root, TEXT("ProjectName"), TEXT("projectName"));
	OutJob.EngineRoot = GetStringAny(*Root, TEXT("EngineRoot"), TEXT("engineRoot"));
	OutJob.GameMountName = GetStringAny(*Root, TEXT("GameMountName"), TEXT("GameMount"), TEXT("gameMountName"));
	OutJob.ModsDir = GetStringAny(*Root, TEXT("ModsDir"), TEXT("modsDir"));
	OutJob.ModName = GetStringAny(*Root, TEXT("ModName"), TEXT("modName"));
	OutJob.Mode = PackageModeFromString(GetStringAny(*Root, TEXT("Mode"), TEXT("mode")));
	OutJob.Packages = GetStringArrayAny(*Root, TEXT("Packages"), TEXT("packages"));
	OutJob.NeverPackPackagePrefixes = GetStringArrayAny(*Root, TEXT("NeverPackPackagePrefixes"), TEXT("neverPackPackagePrefixes"));
	GetBoolAny(*Root, OutJob.bUnversioned, TEXT("Unversioned"), TEXT("unversioned"));

	if (OutJob.GameMountName.IsEmpty())
	{
		OutJob.GameMountName = TEXT("HT");
	}

	return true;
}

bool SaveModPackageJobJson(const FNteModPackageJob& Job, const FString& JobFilename, FString& OutError)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("Format"), TEXT("NTE.ModPackageJob"));
	Root->SetNumberField(TEXT("Version"), 1.0);
	Root->SetStringField(TEXT("ProjectRoot"), Job.ProjectRoot);
	Root->SetStringField(TEXT("ProjectFile"), Job.ProjectFile);
	Root->SetStringField(TEXT("ProjectName"), Job.ProjectName);
	Root->SetStringField(TEXT("EngineRoot"), Job.EngineRoot);
	Root->SetStringField(TEXT("GameMountName"), Job.GameMountName);
	Root->SetStringField(TEXT("ModsDir"), Job.ModsDir);
	Root->SetStringField(TEXT("ModName"), Job.ModName);
	Root->SetStringField(TEXT("Mode"), PackageModeToString(Job.Mode));
	Root->SetArrayField(TEXT("Packages"), StringArrayToJsonValues(Job.Packages));
	Root->SetArrayField(TEXT("NeverPackPackagePrefixes"), StringArrayToJsonValues(Job.NeverPackPackagePrefixes));
	Root->SetBoolField(TEXT("Unversioned"), Job.bUnversioned);
	return SaveJsonObjectToFile(Root, JobFilename, OutError);
}

bool LaunchModPackageBuildJob(const FString& JobFilename, FNteModPackageLaunchResult& OutResult, FString& OutError)
{
	OutResult = FNteModPackageLaunchResult();
	OutError = TEXT("Package pipeline launch has not been migrated into the modular tool yet.");
	return false;
}
}
