// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "NteModPackageTypes.h"

namespace NTEBuildTool::Package
{
struct FNtePackagePlanDialogResult
{
	FString ModsDir;
	FString ModName;
	FString JobFilename;
	bool bLaunchBuild = true;
};

bool ShowPackagePlanDialog(FNtePackagePlan& Plan);
bool ShowPackagePlanDialog(FNtePackagePlan& Plan, FNtePackagePlanDialogResult& OutResult);
}
