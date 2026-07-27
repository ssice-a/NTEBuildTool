// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "NteModPackageTypes.h"

namespace NTEBuildTool::Character
{
struct FNteCharacterModSpec;
}

namespace NTEBuildTool::Package
{
FString PackagePlanCandidateKindToString(ENtePackagePlanCandidateKind Kind);
bool BuildPackagePlanFromSelection(FNtePackagePlan& OutPlan, FString& OutError);
bool BuildPackagePlanFromPackages(const TArray<FString>& SeedPackages, FNtePackagePlan& OutPlan, FString& OutError);
bool BuildPackagePlanFromCharacterModSpec(const NTEBuildTool::Character::FNteCharacterModSpec& Spec, FNtePackagePlan& OutPlan, FString& OutError);
TArray<FString> GetIncludedPackageNames(const FNtePackagePlan& Plan);
}
