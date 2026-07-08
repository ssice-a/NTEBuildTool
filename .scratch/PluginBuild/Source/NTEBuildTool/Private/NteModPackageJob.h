// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "NteModPackageTypes.h"

namespace NTEBuildTool::Package
{
bool LoadModPackageJobJson(const FString& JobFilename, FNteModPackageJob& OutJob, FString& OutError);
bool SaveModPackageJobJson(const FNteModPackageJob& Job, const FString& JobFilename, FString& OutError);
bool CreateModPackageJobFromSelection(const FNteModPackageJobCreateOptions& Options, FNteModPackageJobCreateResult& OutResult, FString& OutError);
bool LaunchModPackageBuildJob(const FString& JobFilename, FNteModPackageLaunchResult& OutResult, FString& OutError);
}
