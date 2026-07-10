// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteModPackagePlan.h"

#include "NteEditorAssetUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Animation/Skeleton.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/PhysicsAsset.h"

namespace NTEBuildTool::Package
{
namespace
{
void AddUniquePackage(TArray<FString>& Packages, const FString& PackageName)
{
	if (PackageName.StartsWith(TEXT("/Game/")) && !PackageName.Contains(TEXT(".")))
	{
		Packages.AddUnique(PackageName);
	}
}

bool IsUnderModFolder(const FString& PackageName)
{
	return PackageName.Contains(TEXT("/mod/"), ESearchCase::IgnoreCase);
}

bool LooksLikeRuntimeAsset(const FString& PackageName)
{
	const FString AssetName = FPackageName::GetShortName(PackageName);
	return AssetName.StartsWith(TEXT("ABP_NTE_ModToggle_"))
		|| AssetName.StartsWith(TEXT("WBP_NTE_ModToggle"))
		|| AssetName.StartsWith(TEXT("BP_NTE_ModToggle"))
		|| PackageName.Contains(TEXT("/mod/Runtime/"), ESearchCase::IgnoreCase);
}

bool IsLikelyEditorOnlyProxy(const FString& PackageName)
{
	UObject* Asset = NTEBuildTool::Editor::LoadAnyAssetByPath(PackageName);
	if (!Asset)
	{
		return false;
	}

	if (UMaterial* Material = Cast<UMaterial>(Asset))
	{
		return !IsUnderModFolder(PackageName) && Material->GetName().StartsWith(TEXT("M_"), ESearchCase::IgnoreCase);
	}

	if (UMaterialInstanceConstant* MaterialInstance = Cast<UMaterialInstanceConstant>(Asset))
	{
		return !IsUnderModFolder(PackageName) && MaterialInstance->GetName().StartsWith(TEXT("MI_"), ESearchCase::IgnoreCase);
	}

	return false;
}

bool IsAssetClass(const FString& PackageName, const UClass* ExpectedClass)
{
	UObject* Asset = NTEBuildTool::Editor::LoadAnyAssetByPath(PackageName);
	return Asset && ExpectedClass && Asset->IsA(ExpectedClass);
}

ENtePackagePlanCandidateKind ClassifyPackage(const FString& PackageName, const bool bSeed)
{
	if (IsLikelyEditorOnlyProxy(PackageName))
	{
		return ENtePackagePlanCandidateKind::EditorOnlyProxy;
	}
	if (LooksLikeRuntimeAsset(PackageName))
	{
		return ENtePackagePlanCandidateKind::GeneratedRuntimeAsset;
	}
	if (IsUnderModFolder(PackageName))
	{
		return ENtePackagePlanCandidateKind::ModAuthoredAsset;
	}
	if (bSeed)
	{
		return ENtePackagePlanCandidateKind::SelectedAsset;
	}
	if (IsAssetClass(PackageName, USkeleton::StaticClass()))
	{
		return ENtePackagePlanCandidateKind::SkeletonAsset;
	}
	if (IsAssetClass(PackageName, UPhysicsAsset::StaticClass()))
	{
		return ENtePackagePlanCandidateKind::PhysicsAsset;
	}
	return ENtePackagePlanCandidateKind::SourceGameDependency;
}

bool ShouldIncludeByDefault(ENtePackagePlanCandidateKind Kind)
{
	switch (Kind)
	{
	case ENtePackagePlanCandidateKind::SelectedAsset:
	case ENtePackagePlanCandidateKind::SelectedFolderAsset:
	case ENtePackagePlanCandidateKind::GeneratedRuntimeAsset:
	case ENtePackagePlanCandidateKind::ModAuthoredAsset:
		return true;
	case ENtePackagePlanCandidateKind::SkeletonAsset:
	case ENtePackagePlanCandidateKind::PhysicsAsset:
	case ENtePackagePlanCandidateKind::EditorOnlyProxy:
	case ENtePackagePlanCandidateKind::SourceGameDependency:
	case ENtePackagePlanCandidateKind::HardDependency:
	case ENtePackagePlanCandidateKind::Unknown:
	default:
		return false;
	}
}

void AddOrMergeCandidate(TArray<FNtePackagePlanCandidate>& Candidates, FNtePackagePlanCandidate Candidate)
{
	for (FNtePackagePlanCandidate& Existing : Candidates)
	{
		if (Existing.PackageName != Candidate.PackageName)
		{
			continue;
		}

		if (!Existing.Reason.Contains(Candidate.Reason))
		{
			Existing.Reason += TEXT("; ") + Candidate.Reason;
		}
		Existing.bDefaultIncluded = Existing.bDefaultIncluded || Candidate.bDefaultIncluded;
		if (Existing.Kind == ENtePackagePlanCandidateKind::SourceGameDependency && Candidate.Kind != ENtePackagePlanCandidateKind::SourceGameDependency)
		{
			Existing.Kind = Candidate.Kind;
		}
		return;
	}

	Candidates.Add(MoveTemp(Candidate));
}

TArray<FString> CollectSelectedPackages()
{
	TArray<FString> Packages;
	for (const FAssetData& AssetData : NTEBuildTool::Editor::GetSelectedContentBrowserAssets())
	{
		AddUniquePackage(Packages, AssetData.PackageName.ToString());
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	for (const FString& Folder : NTEBuildTool::Editor::GetSelectedContentBrowserPaths())
	{
		if (!NTEBuildTool::Editor::IsGameContentPath(Folder))
		{
			continue;
		}

		TArray<FAssetData> FolderAssets;
		AssetRegistry.GetAssetsByPath(FName(*Folder), FolderAssets, true);
		for (const FAssetData& AssetData : FolderAssets)
		{
			AddUniquePackage(Packages, AssetData.PackageName.ToString());
		}
	}
	Packages.Sort();
	return Packages;
}
}

FString PackagePlanCandidateKindToString(const ENtePackagePlanCandidateKind Kind)
{
	switch (Kind)
	{
	case ENtePackagePlanCandidateKind::SelectedAsset:
		return TEXT("Selected");
	case ENtePackagePlanCandidateKind::SelectedFolderAsset:
		return TEXT("Folder");
	case ENtePackagePlanCandidateKind::HardDependency:
		return TEXT("Dependency");
	case ENtePackagePlanCandidateKind::GeneratedRuntimeAsset:
		return TEXT("Runtime");
	case ENtePackagePlanCandidateKind::ModAuthoredAsset:
		return TEXT("Mod");
	case ENtePackagePlanCandidateKind::SkeletonAsset:
		return TEXT("Skeleton");
	case ENtePackagePlanCandidateKind::PhysicsAsset:
		return TEXT("Physics");
	case ENtePackagePlanCandidateKind::SourceGameDependency:
		return TEXT("Source");
	case ENtePackagePlanCandidateKind::EditorOnlyProxy:
		return TEXT("Proxy");
	default:
		return TEXT("Unknown");
	}
}

bool BuildPackagePlanFromSelection(FNtePackagePlan& OutPlan, FString& OutError)
{
	return BuildPackagePlanFromPackages(CollectSelectedPackages(), OutPlan, OutError);
}

bool BuildPackagePlanFromPackages(const TArray<FString>& SeedPackages, FNtePackagePlan& OutPlan, FString& OutError)
{
	OutPlan = FNtePackagePlan();
	if (SeedPackages.IsEmpty())
	{
		OutError = TEXT("Select at least one Content Browser asset or /Game folder before creating a package plan.");
		return false;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	TSet<FString> SeedSet(SeedPackages);

	for (const FString& SeedPackage : SeedPackages)
	{
		FNtePackagePlanCandidate Candidate;
		Candidate.PackageName = SeedPackage;
		Candidate.Kind = ClassifyPackage(SeedPackage, true);
		Candidate.Reason = TEXT("selected by user");
		Candidate.bDefaultIncluded = ShouldIncludeByDefault(Candidate.Kind);
		AddOrMergeCandidate(OutPlan.Candidates, MoveTemp(Candidate));

		TArray<FName> DependencyNames;
		AssetRegistry.GetDependencies(FName(*SeedPackage), DependencyNames, UE::AssetRegistry::EDependencyCategory::Package);
		for (const FName& DependencyName : DependencyNames)
		{
			const FString DependencyPackage = DependencyName.ToString();
			if (!DependencyPackage.StartsWith(TEXT("/Game/")) || DependencyPackage == SeedPackage)
			{
				continue;
			}

			FNtePackagePlanCandidate DependencyCandidate;
			DependencyCandidate.PackageName = DependencyPackage;
			DependencyCandidate.Kind = ClassifyPackage(DependencyPackage, SeedSet.Contains(DependencyPackage));
			if (DependencyCandidate.Kind == ENtePackagePlanCandidateKind::SelectedAsset)
			{
				DependencyCandidate.Reason = TEXT("selected by user");
			}
			else
			{
				DependencyCandidate.Reason = FString::Printf(TEXT("hard dependency of %s"), *SeedPackage);
			}
			DependencyCandidate.bDefaultIncluded = ShouldIncludeByDefault(DependencyCandidate.Kind);
			AddOrMergeCandidate(OutPlan.Candidates, MoveTemp(DependencyCandidate));
		}
	}

	OutPlan.Candidates.Sort([](const FNtePackagePlanCandidate& A, const FNtePackagePlanCandidate& B)
	{
		if (A.bDefaultIncluded != B.bDefaultIncluded)
		{
			return A.bDefaultIncluded;
		}
		if (A.Kind != B.Kind)
		{
			return static_cast<int32>(A.Kind) < static_cast<int32>(B.Kind);
		}
		return A.PackageName < B.PackageName;
	});
	return true;
}

TArray<FString> GetIncludedPackageNames(const FNtePackagePlan& Plan)
{
	TArray<FString> Packages;
	for (const FNtePackagePlanCandidate& Candidate : Plan.Candidates)
	{
		if (Candidate.bDefaultIncluded)
		{
			AddUniquePackage(Packages, Candidate.PackageName);
		}
	}
	Packages.Sort();
	return Packages;
}
}
