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
struct FNteCharacterPackagePreflight
{
	TArray<FString> RequiredPackages;
	TArray<FString> Errors;
	TArray<FString> Warnings;

	bool HasErrors() const { return !Errors.IsEmpty(); }
};

FString PackagePlanCandidateKindToString(ENtePackagePlanCandidateKind Kind);
bool BuildPackagePlanFromSelection(FNtePackagePlan& OutPlan, FString& OutError);
bool BuildPackagePlanFromPackages(const TArray<FString>& SeedPackages, FNtePackagePlan& OutPlan, FString& OutError);
FNtePackageManifestResolution ResolvePackageManifest(const NTEBuildTool::Project::FNtePakmodProject& Project);
bool BuildPackagePlanFromManifest(const NTEBuildTool::Project::FNtePakmodProject& Project, FNtePackagePlan& OutPlan, FString& OutError);
bool BuildPackagePlanFromCharacterModSpec(const NTEBuildTool::Character::FNteCharacterModSpec& Spec, FNtePackagePlan& OutPlan, FString& OutError);
TArray<FString> CollectEffectiveCharacterPackageSeeds(const NTEBuildTool::Character::FNteCharacterModSpec& Spec);
FNteCharacterPackagePreflight BuildCharacterModSpecPackagePreflight(const NTEBuildTool::Character::FNteCharacterModSpec& Spec);
TArray<FString> GetIncludedPackageNames(const FNtePackagePlan& Plan);
}
