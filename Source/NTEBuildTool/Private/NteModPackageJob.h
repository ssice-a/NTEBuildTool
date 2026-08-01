// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "NteModPackageTypes.h"

namespace NTEBuildTool::Character
{
struct FNteCharacterModSpec;
}

namespace NTEBuildTool::Project
{
struct FNtePakmodProject;
}

namespace NTEBuildTool::Package
{
FString SanitizeModPackageName(FString ModName);
FString DeriveModPackageNameFromPackages(const TArray<FString>& Packages);

bool LoadModPackageJobJson(const FString& JobFilename, FNteModPackageJob& OutJob, FString& OutError);
bool SaveModPackageJobJson(const FNteModPackageJob& Job, const FString& JobFilename, FString& OutError);
bool CreateModPackageJobFromSelection(const FNteModPackageJobCreateOptions& Options, FNteModPackageJobCreateResult& OutResult, FString& OutError);
bool CreateModPackageJobFromManifest(const NTEBuildTool::Project::FNtePakmodProject& Project, const FNtePackagePlan& Plan, FNteModPackageJobCreateResult& OutResult, FString& OutError);
bool CreateModPackageJobFromCharacterModSpec(const NTEBuildTool::Character::FNteCharacterModSpec& Spec, const FNtePackagePlan& Plan, FNteModPackageJobCreateResult& OutResult, FString& OutError);
bool LaunchModPackageBuildJob(const FString& JobFilename, FNteModPackageLaunchResult& OutResult, FString& OutError);
}
