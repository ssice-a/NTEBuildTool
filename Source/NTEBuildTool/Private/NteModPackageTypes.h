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
	FString GameMountName;
	FString ModsDir;
	FString ModName;
	ENteModPackageMode Mode = ENteModPackageMode::CookAndPack;
	TArray<FString> Packages;
	TArray<FString> NeverPackPackagePrefixes;
	bool bUnversioned = false;
	bool bSkipCook = false;
	bool bRequiresHTGameStub = false;
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
	FString GameMountName;
	ENteModPackageMode Mode = ENteModPackageMode::CookAndPack;
	TArray<FString> Packages;
	TArray<FString> NeverPackPackagePrefixes;
	bool bUnversioned = false;
	bool bSkipCook = false;
	bool bCollectContentBrowserSelection = true;
	bool bRequiresHTGameStub = false;
};

struct FNteModPackageJobCreateResult
{
	FString JobFile;
	FString ModName;
	int32 PackageCount = 0;
	FNteModPackageJob Job;
};

enum class ENtePackagePlanCandidateKind
{
	ManifestReplacementAsset,
	ManifestAddedAsset,
	SelectedAsset,
	SelectedFolderAsset,
	HardDependency,
	GeneratedRuntimeAsset,
	ModAuthoredAsset,
	SkeletonAsset,
	PhysicsAsset,
	SourceGameDependency,
	EditorOnlyProxy,
	Unknown
};

struct FNtePackagePlanCandidate
{
	FString PackageName;
	ENtePackagePlanCandidateKind Kind = ENtePackagePlanCandidateKind::Unknown;
	FString Reason;
	bool bDefaultIncluded = true;
};

struct FNtePackagePlan
{
	TArray<FNtePackagePlanCandidate> Candidates;
};

struct FNtePackageManifestResolution
{
	TArray<FString> PackageNames;
	TMap<FString, FString> ReasonsByPackage;
	TArray<FString> Errors;
	TArray<FString> Warnings;

	bool HasErrors() const { return !Errors.IsEmpty(); }
};
}
