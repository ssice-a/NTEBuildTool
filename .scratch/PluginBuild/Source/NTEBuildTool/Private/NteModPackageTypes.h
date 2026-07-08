// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"

namespace NTEBuildTool::Package
{
enum class ENteModPackageMode
{
	CookAndPack,
	CookOnly,
	PackOnly
};

struct FNteModPackageJob
{
	FString ProjectRoot;
	FString ProjectFile;
	FString ProjectName;
	FString EngineRoot;
	FString GameMountName = TEXT("HT");
	FString ModsDir;
	FString ModName;
	ENteModPackageMode Mode = ENteModPackageMode::CookAndPack;
	TArray<FString> Packages;
	TArray<FString> NeverPackPackagePrefixes;
	bool bUnversioned = false;
	bool bSkipCook = false;
};

struct FNteModPackageLaunchResult
{
	FString JobFile;
	FString CommandLine;
	FString WorkRoot;
};

struct FNteModPackageJobCreateOptions
{
	FString JobFilename;
	FString ModsDir;
	FString ModName;
	FString GameMountName = TEXT("HT");
	ENteModPackageMode Mode = ENteModPackageMode::CookAndPack;
	TArray<FString> Packages;
	TArray<FString> NeverPackPackagePrefixes;
	bool bUnversioned = false;
	bool bSkipCook = false;
};

struct FNteModPackageJobCreateResult
{
	FString JobFile;
	FString ModName;
	int32 PackageCount = 0;
	FNteModPackageJob Job;
};
}
