// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteModPackagePlan.h"

#include "NteAppearanceAssemblyPlan.h"
#include "NteCharacterMaterialPlan.h"
#include "NteCharacterModSpecMigration.h"
#include "NteCharacterKawaiiPlan.h"
#include "NteCharacterKawaiiWriter.h"
#include "NteCharacterModSpec.h"
#include "NteCharacterRuntimeActionPlan.h"
#include "NteCharacterRuntimeActionWriter.h"
#include "NteEditorAssetUtils.h"
#include "NtePakmodProject.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Animation/Skeleton.h"
#include "Engine/Font.h"
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

FString NormalizePackageName(FString AssetPath)
{
	AssetPath = NTEBuildTool::Editor::NormalizeAssetPathForText(AssetPath);
	int32 DotIndex = INDEX_NONE;
	if (AssetPath.FindLastChar(TEXT('.'), DotIndex))
	{
		AssetPath.LeftInline(DotIndex);
	}
	return AssetPath;
}

void AddReferenceOnlyPackage(TSet<FString>& Packages, const FString& AssetPath)
{
	const FString PackageName = NormalizePackageName(AssetPath);
	if (PackageName.StartsWith(TEXT("/Game/")))
	{
		Packages.Add(PackageName);
	}
}

bool IsUnderModFolder(const FString& PackageName)
{
	return PackageName.Contains(TEXT("/mod/"), ESearchCase::IgnoreCase);
}

bool LooksLikeRuntimeAsset(const FString& PackageName)
{
	return PackageName.Contains(TEXT("/mod/Runtime/"), ESearchCase::IgnoreCase);
}

bool TryFindPackageAssetData(const FString& PackageName, FAssetData& OutAssetData)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> PackageAssets;
	AssetRegistry.GetAssetsByPackageName(FName(*PackageName), PackageAssets, true);
	if (PackageAssets.IsEmpty())
	{
		return false;
	}

	OutAssetData = PackageAssets[0];
	return true;
}

void AddPlanErrors(TArray<FString>& Errors, const TCHAR* PlanName, const TArray<FString>& PlanErrors)
{
	for (const FString& Error : PlanErrors)
	{
		Errors.Add(FString::Printf(TEXT("%s: %s"), PlanName, *Error));
	}
}

bool IsLikelyEditorOnlyProxy(const FString& PackageName)
{
	FAssetData AssetData;
	if (!TryFindPackageAssetData(PackageName, AssetData))
	{
		return false;
	}

	UObject* Asset = AssetData.GetAsset();
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

	if (Cast<UFont>(Asset))
	{
		return !IsUnderModFolder(PackageName);
	}

	return false;
}

bool IsAssetClass(const FString& PackageName, const UClass* ExpectedClass)
{
	FAssetData AssetData;
	if (!TryFindPackageAssetData(PackageName, AssetData))
	{
		return false;
	}

	UObject* Asset = AssetData.GetAsset();
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
	case ENtePackagePlanCandidateKind::ManifestReplacementAsset:
	case ENtePackagePlanCandidateKind::ManifestAddedAsset:
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

bool BuildPackagePlanFromPackagesInternal(
	const TArray<FString>& SeedPackages,
	const FString& SeedReason,
	FNtePackagePlan& OutPlan,
	FString& OutError)
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
		Candidate.Reason = SeedReason;
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
				DependencyCandidate.Reason = SeedReason;
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
}

FString PackagePlanCandidateKindToString(const ENtePackagePlanCandidateKind Kind)
{
	switch (Kind)
	{
	case ENtePackagePlanCandidateKind::ManifestReplacementAsset:
		return TEXT("Replacement");
	case ENtePackagePlanCandidateKind::ManifestAddedAsset:
		return TEXT("Added");
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
	return BuildPackagePlanFromPackagesInternal(SeedPackages, TEXT("selected by user"), OutPlan, OutError);
}

FNtePackageManifestResolution ResolvePackageManifest(const NTEBuildTool::Project::FNtePakmodProject& Project)
{
	FNtePackageManifestResolution Result;
	TSet<FString> SeenPackageNames;
	TSet<FString> ExcludedPackages;
	for (const FString& Exclusion : Project.PackageManifest.ExplicitExclusions)
	{
		const FString PackageName = NTEBuildTool::Project::NormalizePakmodPackagePath(Exclusion);
		if (PackageName.StartsWith(TEXT("/Game/")))
		{
			ExcludedPackages.Add(PackageName.ToLower());
		}
		else
		{
			Result.Warnings.Add(FString::Printf(TEXT("Ignored invalid explicit exclusion: %s"), *Exclusion));
		}
	}

	for (const FString& AssetId : Project.PackageManifest.AssetIds)
	{
		const NTEBuildTool::Project::FNteAssetReference* Asset = NTEBuildTool::Project::FindAssetById(Project, AssetId);
		if (!Asset)
		{
			Result.Errors.Add(FString::Printf(TEXT("PackageManifest references missing asset '%s'."), *AssetId));
			continue;
		}
		if (Asset->Intent == NTEBuildTool::Project::ENteAssetIntent::ExternalReference)
		{
			Result.Errors.Add(FString::Printf(TEXT("PackageManifest cannot package ExternalReference asset '%s'."), *AssetId));
			continue;
		}
		const FString PackageName = NTEBuildTool::Project::NormalizePakmodPackagePath(Asset->PackagePath);
		if (!PackageName.StartsWith(TEXT("/Game/")))
		{
			Result.Errors.Add(FString::Printf(TEXT("Asset '%s' has invalid package path '%s'."), *AssetId, *Asset->PackagePath));
			continue;
		}
		if (ExcludedPackages.Contains(PackageName.ToLower()))
		{
			Result.Warnings.Add(FString::Printf(TEXT("PackageManifest explicitly excludes asset '%s' (%s)."), *AssetId, *PackageName));
			continue;
		}
		const FString Key = PackageName.ToLower();
		if (SeenPackageNames.Contains(Key))
		{
			continue;
		}
		SeenPackageNames.Add(Key);
		Result.PackageNames.Add(PackageName);
		Result.ReasonsByPackage.Add(PackageName, FString::Printf(
			TEXT("manifest %s (%s)"),
			*NTEBuildTool::Project::AssetIntentToString(Asset->Intent),
			*NTEBuildTool::Project::AssetOriginToString(Asset->Origin)));
	}
	return Result;
}

bool BuildPackagePlanFromManifest(
	const NTEBuildTool::Project::FNtePakmodProject& Project,
	FNtePackagePlan& OutPlan,
	FString& OutError)
{
	const NTEBuildTool::Project::FNtePakmodProjectValidationResult Validation =
		NTEBuildTool::Project::ValidatePakmodProject(Project);
	if (Validation.HasErrors())
	{
		OutError = FString::Join(Validation.Errors, LINE_TERMINATOR);
		return false;
	}

	const FNtePackageManifestResolution Resolution = ResolvePackageManifest(Project);
	if (Resolution.HasErrors())
	{
		OutError = FString::Join(Resolution.Errors, LINE_TERMINATOR);
		return false;
	}
	if (Resolution.PackageNames.IsEmpty())
	{
		OutError = TEXT("PackageManifest contains no package-owned assets.");
		return false;
	}
	if (!BuildPackagePlanFromPackagesInternal(Resolution.PackageNames, TEXT("from PackageManifest"), OutPlan, OutError))
	{
		return false;
	}

	for (FNtePackagePlanCandidate& Candidate : OutPlan.Candidates)
	{
		const NTEBuildTool::Project::FNteAssetReference* Asset = Project.Assets.FindByPredicate([&Candidate](const NTEBuildTool::Project::FNteAssetReference& Item)
		{
			return NTEBuildTool::Project::NormalizePakmodPackagePath(Item.PackagePath).Equals(Candidate.PackageName, ESearchCase::IgnoreCase);
		});
		if (!Asset || Asset->Intent == NTEBuildTool::Project::ENteAssetIntent::ExternalReference)
		{
			continue;
		}
		Candidate.Kind = Asset->Intent == NTEBuildTool::Project::ENteAssetIntent::ReplacementAsset
			? ENtePackagePlanCandidateKind::ManifestReplacementAsset
			: ENtePackagePlanCandidateKind::ManifestAddedAsset;
		Candidate.bDefaultIncluded = true;
		if (const FString* Reason = Resolution.ReasonsByPackage.Find(Candidate.PackageName))
		{
			Candidate.Reason = *Reason;
		}
	}
	return true;
}

bool BuildPackagePlanFromCharacterModSpec(
	const NTEBuildTool::Character::FNteCharacterModSpec& Spec,
	FNtePackagePlan& OutPlan,
	FString& OutError)
{
	const NTEBuildTool::Character::FNteCharacterModSpecMigrationResult Migration =
		NTEBuildTool::Character::MigrateCharacterModSpecToPakmodProject(Spec);
	if (Migration.HasErrors())
	{
		OutError = FString::Join(Migration.Errors, LINE_TERMINATOR);
		return false;
	}
	return BuildPackagePlanFromManifest(Migration.Project, OutPlan, OutError);
}

TArray<FString> CollectEffectiveCharacterPackageSeeds(const NTEBuildTool::Character::FNteCharacterModSpec& Spec)
{
	TArray<FString> EffectiveSeedPackages = NTEBuildTool::Character::CollectCharacterModSpecPackageSeeds(Spec);
	TSet<FString> GeneratedKawaiiPackages;
	const NTEBuildTool::Character::FNteCharacterMaterialPlan MaterialPlan =
		NTEBuildTool::Character::BuildCharacterMaterialPlanFromSpec(Spec);
	for (const FString& MaterialSeed : NTEBuildTool::Character::CollectCharacterMaterialPlanPackageSeeds(MaterialPlan))
	{
		EffectiveSeedPackages.AddUnique(MaterialSeed);
	}
	const NTEBuildTool::Character::FNteCharacterRuntimeActionPlan RuntimeActionPlan =
		NTEBuildTool::Character::BuildCharacterRuntimeActionPlanFromSpec(Spec);
	for (const FString& RuntimeSeed : NTEBuildTool::Character::CollectCharacterRuntimeActionPlanPackageSeeds(RuntimeActionPlan))
	{
		EffectiveSeedPackages.AddUnique(RuntimeSeed);
	}
	const NTEBuildTool::Character::FNteCharacterKawaiiPlan KawaiiPlan =
		NTEBuildTool::Character::BuildCharacterKawaiiPlanFromSpec(Spec);
	for (const FString& KawaiiSeed : NTEBuildTool::Character::CollectCharacterKawaiiPlanPackageSeeds(KawaiiPlan))
	{
		EffectiveSeedPackages.AddUnique(KawaiiSeed);
		GeneratedKawaiiPackages.Add(KawaiiSeed);
	}
	TSet<FString> ReferenceOnlyPackages;
	AddReferenceOnlyPackage(ReferenceOnlyPackages, Spec.MainAnimBlueprintPath);
	AddReferenceOnlyPackage(ReferenceOnlyPackages, Spec.Appearance.MainUIAnimBlueprintPath);
	for (const NTEBuildTool::Character::FNteCharacterPresentationTargetSpec& Target : Spec.Appearance.PresentationTargets)
	{
		AddReferenceOnlyPackage(ReferenceOnlyPackages, Target.MainAnimBlueprintPath);
	}
	for (const NTEBuildTool::Character::FNteCharacterAttachedMeshSpec& AttachedMesh : Spec.AttachedMeshes)
	{
		AddReferenceOnlyPackage(ReferenceOnlyPackages, AttachedMesh.AnimBlueprintPath);
		AddReferenceOnlyPackage(ReferenceOnlyPackages, AttachedMesh.MobileAnimBlueprintPath);
		AddReferenceOnlyPackage(ReferenceOnlyPackages, AttachedMesh.UIAnimBlueprintPath);
	}
	EffectiveSeedPackages.RemoveAll([&Spec, &GeneratedKawaiiPackages, &ReferenceOnlyPackages](const FString& PackageName)
	{
		const TOptional<NTEBuildTool::Character::ENteCharacterPackageAssetIntent> Intent =
			NTEBuildTool::Character::FindPackageAssetIntent(Spec, PackageName);
		if (Intent.IsSet())
		{
			return Intent.GetValue() == NTEBuildTool::Character::ENteCharacterPackageAssetIntent::ExternalReference;
		}
		return ReferenceOnlyPackages.Contains(PackageName) && !GeneratedKawaiiPackages.Contains(PackageName);
	});
	EffectiveSeedPackages.Sort();
	return EffectiveSeedPackages;
}

FNteCharacterPackagePreflight BuildCharacterModSpecPackagePreflight(const NTEBuildTool::Character::FNteCharacterModSpec& Spec)
{
	FNteCharacterPackagePreflight Result;
	const NTEBuildTool::Character::FNteCharacterModSpecValidationResult Validation =
		NTEBuildTool::Character::ValidateCharacterModSpec(Spec);
	Result.Errors.Append(Validation.Errors);
	Result.Warnings.Append(Validation.Warnings);

	const NTEBuildTool::Character::FNteAppearanceAssemblyPlan AppearancePlan =
		NTEBuildTool::Character::BuildAppearanceAssemblyPlanFromSpec(Spec);
	const NTEBuildTool::Character::FNteCharacterMaterialPlan MaterialPlan =
		NTEBuildTool::Character::BuildCharacterMaterialPlanFromSpec(Spec);
	const NTEBuildTool::Character::FNteCharacterRuntimeActionPlan RuntimeActionPlan =
		NTEBuildTool::Character::BuildCharacterRuntimeActionPlanFromSpec(Spec);
	const NTEBuildTool::Character::FNteCharacterKawaiiPlan KawaiiPlan =
		NTEBuildTool::Character::BuildCharacterKawaiiPlanFromSpec(Spec);
	AddPlanErrors(Result.Errors, TEXT("AppearanceAssemblyPlan"), AppearancePlan.Errors);
	AddPlanErrors(Result.Errors, TEXT("MaterialPlan"), MaterialPlan.Errors);
	AddPlanErrors(Result.Errors, TEXT("RuntimeActionPlan"), RuntimeActionPlan.Errors);
	AddPlanErrors(Result.Errors, TEXT("KawaiiPlan"), KawaiiPlan.Errors);
	if (!KawaiiPlan.Presets.IsEmpty())
	{
		const NTEBuildTool::Character::FNteCharacterKawaiiAssetPreflightResult KawaiiAssetPreflight =
			NTEBuildTool::Character::ValidateCharacterKawaiiAssetsForPackage(KawaiiPlan);
		AddPlanErrors(Result.Errors, TEXT("KawaiiAssetPreflight"), KawaiiAssetPreflight.Errors);
		Result.Warnings.Append(KawaiiAssetPreflight.Warnings);

		for (const NTEBuildTool::Character::FNteCharacterKawaiiPresetPlanItem& Preset : KawaiiPlan.Presets)
		{
			if (Preset.TargetKind.Equals(TEXT("MainMesh"), ESearchCase::IgnoreCase))
			{
				if (!Preset.SourcePoseStrategy.Equals(TEXT("PostProcessInputPose"), ESearchCase::IgnoreCase))
				{
					Result.Errors.Add(FString::Printf(
						TEXT("Main mesh Kawaii preset '%s' must use PostProcessInputPose, resolved strategy is %s."),
						*Preset.Id,
						*Preset.SourcePoseStrategy));
				}
				if (NormalizePackageName(Spec.MainPostProcessAnimBlueprintPath) != NormalizePackageName(Preset.RuntimeAnimBlueprintPath))
				{
					Result.Errors.Add(FString::Printf(
						TEXT("Main mesh Kawaii preset '%s' does not reference MainPostProcessAnimBlueprintPath %s; resolved RuntimeAnimBlueprintPath is %s."),
						*Preset.Id,
						*Spec.MainPostProcessAnimBlueprintPath,
						*Preset.RuntimeAnimBlueprintPath));
				}
				continue;
			}

			const NTEBuildTool::Character::FNteAppearanceMeshDataPlan* AttachedMesh = AppearancePlan.AttachedMeshes.FindByPredicate(
				[&Preset](const NTEBuildTool::Character::FNteAppearanceMeshDataPlan& Candidate)
				{
					return Candidate.Id == Preset.TargetMeshId;
				});
			if (!AttachedMesh)
			{
				Result.Errors.Add(FString::Printf(
					TEXT("Kawaii preset '%s' target '%s' is not an attached mesh in AppearanceAssemblyPlan."),
					*Preset.Id,
					*Preset.TargetMeshId));
				continue;
			}
			if (NormalizePackageName(AttachedMesh->AnimInstancePath) != NormalizePackageName(Preset.RuntimeAnimBlueprintPath))
			{
				Result.Errors.Add(FString::Printf(
					TEXT("Attached mesh '%s' does not reference Kawaii Runtime AnimBlueprint %s; resolved AnimInstancePath is %s."),
					*AttachedMesh->Id,
					*Preset.RuntimeAnimBlueprintPath,
					*AttachedMesh->AnimInstancePath));
			}

			bool bUsedByPresentationTarget = false;
			for (const NTEBuildTool::Character::FNteAppearancePresentationTargetPlan& Target : AppearancePlan.PresentationTargets)
			{
				const bool bExplicitTargetList = !AttachedMesh->PresentationTargetIds.IsEmpty();
				const bool bIncluded = bExplicitTargetList
					? AttachedMesh->PresentationTargetIds.Contains(Target.Id)
					: (!Target.Id.Equals(TEXT("ui"), ESearchCase::IgnoreCase) || AttachedMesh->bSyncToUIShow);
				bUsedByPresentationTarget |= bIncluded;
			}
			if (!AppearancePlan.PresentationTargets.IsEmpty() && !bUsedByPresentationTarget)
			{
				Result.Errors.Add(FString::Printf(
					TEXT("Attached mesh '%s' with Kawaii preset '%s' is not included by any presentation target."),
					*AttachedMesh->Id,
					*Preset.Id));
			}
		}

		for (const FString& KawaiiSeed : NTEBuildTool::Character::CollectCharacterKawaiiPlanPackageSeeds(KawaiiPlan))
		{
			const TOptional<NTEBuildTool::Character::ENteCharacterPackageAssetIntent> Intent =
				NTEBuildTool::Character::FindPackageAssetIntent(Spec, KawaiiSeed);
			if (Intent.IsSet() && Intent.GetValue() == NTEBuildTool::Character::ENteCharacterPackageAssetIntent::ExternalReference)
			{
				Result.Errors.Add(FString::Printf(
					TEXT("Generated Kawaii asset is marked ExternalReference and would be omitted from the package: %s"),
					*KawaiiSeed));
			}
		}
	}
	Result.RequiredPackages = CollectEffectiveCharacterPackageSeeds(Spec);
	if (Result.RequiredPackages.IsEmpty())
	{
		Result.Errors.Add(TEXT("CharacterModSpec produced no /Game package seeds."));
		return Result;
	}

	for (const FString& PackageName : Result.RequiredPackages)
	{
		FAssetData AssetData;
		if (!TryFindPackageAssetData(PackageName, AssetData))
		{
			Result.Errors.Add(FString::Printf(
				TEXT("Required package is not present in the project: %s. Apply the generating step before creating a package job."),
				*PackageName));
		}
	}
	return Result;
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
