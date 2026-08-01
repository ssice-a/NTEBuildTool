// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteCharacterModSpecMigration.h"

#include "NteCharacterKawaiiPlan.h"
#include "NteCharacterMaterialPlan.h"
#include "NteCharacterModSpec.h"
#include "NteCharacterRuntimeActionPlan.h"
#include "NteCharacterRuntimeActionWriter.h"

#include "Misc/PackageName.h"

namespace NTEBuildTool::Character
{
namespace
{
FString MakeStableId(FString Prefix, const FString& Value, const int32 FallbackIndex)
{
	FString Name = FPackageName::GetShortName(Value);
	if (Name.IsEmpty())
	{
		Name = FString::Printf(TEXT("item_%d"), FallbackIndex);
	}
	for (TCHAR& Character : Name)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
		{
			Character = TEXT('_');
		}
	}
	return Prefix + Name.ToLower();
}

NTEBuildTool::Project::FNteAssetReference& AddOrMergeAsset(
	NTEBuildTool::Project::FNtePakmodProject& Project,
	const FString& PackagePath,
	NTEBuildTool::Project::ENteAssetOrigin Origin,
	NTEBuildTool::Project::ENteAssetIntent Intent,
	const FString& OwnerRecipeId = FString())
{
	const FString NormalizedPath = NTEBuildTool::Project::NormalizePakmodPackagePath(PackagePath);
	if (NTEBuildTool::Project::FNteAssetReference* Existing = Project.Assets.FindByPredicate([&NormalizedPath](const NTEBuildTool::Project::FNteAssetReference& Asset)
	{
		return Asset.PackagePath.Equals(NormalizedPath, ESearchCase::IgnoreCase);
	}))
	{
		if (Existing->Intent == NTEBuildTool::Project::ENteAssetIntent::ExternalReference
			&& Intent != NTEBuildTool::Project::ENteAssetIntent::ExternalReference)
		{
			Existing->Intent = Intent;
		}
		if (Origin == NTEBuildTool::Project::ENteAssetOrigin::ToolGenerated)
		{
			Existing->Origin = Origin;
		}
		if (!OwnerRecipeId.IsEmpty())
		{
			Existing->OwnerRecipeId = OwnerRecipeId;
		}
		return *Existing;
	}

	NTEBuildTool::Project::FNteAssetReference Asset;
	Asset.Id = MakeStableId(TEXT("asset_"), NormalizedPath, Project.Assets.Num());
	while (NTEBuildTool::Project::FindAssetById(Project, Asset.Id))
	{
		Asset.Id += TEXT("_1");
	}
	Asset.PackagePath = NormalizedPath;
	Asset.Origin = Origin;
	Asset.Intent = Intent;
	Asset.OwnerRecipeId = OwnerRecipeId;
	return Project.Assets.Add_GetRef(MoveTemp(Asset));
}

NTEBuildTool::Project::ENteAssetIntent ConvertIntent(const ENteCharacterPackageAssetIntent Intent)
{
	switch (Intent)
	{
	case ENteCharacterPackageAssetIntent::ExternalReference:
		return NTEBuildTool::Project::ENteAssetIntent::ExternalReference;
	case ENteCharacterPackageAssetIntent::ReplacementAsset:
		return NTEBuildTool::Project::ENteAssetIntent::ReplacementAsset;
	case ENteCharacterPackageAssetIntent::GeneratedAsset:
		return NTEBuildTool::Project::ENteAssetIntent::AddedAsset;
	default:
		return NTEBuildTool::Project::ENteAssetIntent::Invalid;
	}
}

TSharedPtr<FJsonObject> FindArrayObjectById(const TSharedRef<FJsonObject>& Root, const TCHAR* ArrayName, const FString& Id)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Root->TryGetArrayField(ArrayName, Values) || !Values)
	{
		return MakeShared<FJsonObject>();
	}
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		FString CandidateId;
		if (Object.IsValid() && Object->TryGetStringField(TEXT("Id"), CandidateId) && CandidateId == Id)
		{
			return Object;
		}
	}
	return MakeShared<FJsonObject>();
}
}

FNteCharacterModSpecMigrationResult MigrateCharacterModSpecToPakmodProject(const FNteCharacterModSpec& Spec)
{
	FNteCharacterModSpecMigrationResult Result;
	NTEBuildTool::Project::FNtePakmodProject& Project = Result.Project;
	Project.Project.Id = MakeStableId(TEXT(""), !Spec.WorkspaceName.IsEmpty() ? Spec.WorkspaceName : Spec.Package.ModName, 0);
	Project.Project.DisplayName = !Spec.WorkspaceName.IsEmpty() ? Spec.WorkspaceName : Spec.Package.ModName;
	Project.PackageManifest.ModName = Spec.Package.ModName;
	Project.PackageManifest.ModsDirOverride = Spec.Package.ModsDir;
	Project.PackageManifest.JobFilename = Spec.Package.JobFilename;
	Project.PackageManifest.bRequiresHTGameStub = false;

	const TSharedRef<FJsonObject> LegacyJson = CharacterModSpecToJson(Spec);

	const auto AddExternal = [&Project](const FString& Path)
	{
		if (!NTEBuildTool::Project::NormalizePakmodPackagePath(Path).StartsWith(TEXT("/Game/")))
		{
			return;
		}
		AddOrMergeAsset(Project, Path, NTEBuildTool::Project::ENteAssetOrigin::GameReference, NTEBuildTool::Project::ENteAssetIntent::ExternalReference);
	};

	AddExternal(Spec.MainAnimBlueprintPath);
	AddExternal(Spec.Appearance.MainUIAnimBlueprintPath);
	for (const FNteCharacterPresentationTargetSpec& Target : Spec.Appearance.PresentationTargets)
	{
		AddExternal(Target.MainAnimBlueprintPath);
	}
	for (const FNteCharacterAttachedMeshSpec& Mesh : Spec.AttachedMeshes)
	{
		AddExternal(Mesh.AnimBlueprintPath);
		AddExternal(Mesh.MobileAnimBlueprintPath);
		AddExternal(Mesh.UIAnimBlueprintPath);
	}

	const auto AddSourceReference = [&Project](const FString& Id, const FString& Kind, const FString& GamePackagePath, const FString& Locator)
	{
		if (GamePackagePath.IsEmpty() && Locator.IsEmpty())
		{
			return;
		}
		if (Project.Sources.ContainsByPredicate([&Id](const NTEBuildTool::Project::FNteSourceReference& Source) { return Source.Id.Equals(Id, ESearchCase::IgnoreCase); }))
		{
			return;
		}
		NTEBuildTool::Project::FNteSourceReference Source;
		Source.Id = Id;
		Source.Kind = Kind;
		Source.GamePackagePath = NTEBuildTool::Project::NormalizePakmodPackagePath(GamePackagePath);
		Source.Locator = Locator;
		Project.Sources.Add(MoveTemp(Source));
	};
	if (!Spec.MainAnimBlueprintPath.IsEmpty()) AddSourceReference(TEXT("source_main_animbp"), TEXT("GameAnimBlueprint"), Spec.MainAnimBlueprintPath, FString());
	if (!Spec.Appearance.PlayerAppearanceAssetPath.IsEmpty()) AddSourceReference(TEXT("source_player_appearance"), TEXT("HTPlayerAppearance"), Spec.Appearance.PlayerAppearanceAssetPath, FString());
	for (const FNteCharacterPresentationTargetSpec& Target : Spec.Appearance.PresentationTargets)
	{
		AddSourceReference(TEXT("source_") + Target.Id, TEXT("PresentationBlueprint"), Target.BlueprintClassPath, FString());
	}

	for (const FNteCharacterPackageAssetSpec& LegacyAsset : Spec.Package.Assets)
	{
		const NTEBuildTool::Project::ENteAssetIntent Intent = ConvertIntent(LegacyAsset.Intent);
		if (Intent == NTEBuildTool::Project::ENteAssetIntent::Invalid)
		{
			Result.Warnings.Add(FString::Printf(TEXT("Skipped invalid legacy package asset intent: %s"), *LegacyAsset.AssetPath));
			continue;
		}
		const NTEBuildTool::Project::ENteAssetOrigin Origin = LegacyAsset.Intent == ENteCharacterPackageAssetIntent::GeneratedAsset
			? NTEBuildTool::Project::ENteAssetOrigin::ToolGenerated
			: (LegacyAsset.Intent == ENteCharacterPackageAssetIntent::ExternalReference
				? NTEBuildTool::Project::ENteAssetOrigin::GameReference
				: NTEBuildTool::Project::ENteAssetOrigin::UserImported);
		AddOrMergeAsset(Project, LegacyAsset.AssetPath, Origin, Intent);
	}

	if (!Spec.MainMeshPath.IsEmpty())
	{
		AddOrMergeAsset(Project, Spec.MainMeshPath, NTEBuildTool::Project::ENteAssetOrigin::UserImported, NTEBuildTool::Project::ENteAssetIntent::ReplacementAsset);
	}
	if (!Spec.MainPostProcessAnimBlueprintPath.IsEmpty())
	{
		AddOrMergeAsset(Project, Spec.MainPostProcessAnimBlueprintPath, NTEBuildTool::Project::ENteAssetOrigin::ToolGenerated, NTEBuildTool::Project::ENteAssetIntent::AddedAsset);
	}

	const FNteCharacterMaterialPlan MaterialPlan = BuildCharacterMaterialPlanFromSpec(Spec);
	for (const FNteCharacterMaterialOperationPlanItem& Operation : MaterialPlan.Operations)
	{
		NTEBuildTool::Project::FNteAuthoringRecipe Recipe;
		Recipe.Id = TEXT("material_") + Operation.Id;
		Recipe.Type = TEXT("MaterialInstance");
		Recipe.Deltas = FindArrayObjectById(LegacyJson, TEXT("MaterialOperations"), Operation.Id);
		if (!Operation.TargetMeshPath.IsEmpty())
		{
			Recipe.TargetAssetIds.Add(AddOrMergeAsset(Project, Operation.TargetMeshPath, NTEBuildTool::Project::ENteAssetOrigin::UserImported, NTEBuildTool::Project::ENteAssetIntent::ReplacementAsset).Id);
		}
		if (!Operation.OutputMaterialPath.IsEmpty())
		{
			NTEBuildTool::Project::FNteAssetReference& Output = AddOrMergeAsset(Project, Operation.OutputMaterialPath, NTEBuildTool::Project::ENteAssetOrigin::ToolGenerated, NTEBuildTool::Project::ENteAssetIntent::AddedAsset, Recipe.Id);
			Recipe.OutputAssetIds.Add(Output.Id);
		}
		Project.Recipes.Add(MoveTemp(Recipe));
	}

	if (!Spec.RuntimeActions.IsEmpty() || Spec.RuntimeUi.bEnableUi)
	{
		NTEBuildTool::Project::FNteAuthoringRecipe Recipe;
		Recipe.Id = TEXT("runtime_actions");
		Recipe.Type = TEXT("RuntimeActions");
		Recipe.Deltas = MakeShared<FJsonObject>();
		if (const TArray<TSharedPtr<FJsonValue>>* Actions = nullptr; LegacyJson->TryGetArrayField(TEXT("RuntimeActions"), Actions) && Actions)
		{
			Recipe.Deltas->SetArrayField(TEXT("Actions"), *Actions);
		}
		if (const TSharedPtr<FJsonObject>* RuntimeUi = nullptr; LegacyJson->TryGetObjectField(TEXT("RuntimeUi"), RuntimeUi) && RuntimeUi && RuntimeUi->IsValid())
		{
			Recipe.Deltas->SetObjectField(TEXT("RuntimeUi"), *RuntimeUi);
		}
		if (!Spec.MainMeshPath.IsEmpty())
		{
			Recipe.TargetAssetIds.Add(AddOrMergeAsset(Project, Spec.MainMeshPath, NTEBuildTool::Project::ENteAssetOrigin::UserImported, NTEBuildTool::Project::ENteAssetIntent::ReplacementAsset).Id);
		}
		const FNteCharacterRuntimeActionPlan RuntimePlan = BuildCharacterRuntimeActionPlanFromSpec(Spec);
		for (const FString& PackagePath : CollectCharacterRuntimeActionPlanPackageSeeds(RuntimePlan))
		{
			NTEBuildTool::Project::FNteAssetReference& Output = AddOrMergeAsset(Project, PackagePath, NTEBuildTool::Project::ENteAssetOrigin::ToolGenerated, NTEBuildTool::Project::ENteAssetIntent::AddedAsset, Recipe.Id);
			Recipe.OutputAssetIds.AddUnique(Output.Id);
		}
		Project.Recipes.Add(MoveTemp(Recipe));
	}

	const FNteCharacterKawaiiPlan KawaiiPlan = BuildCharacterKawaiiPlanFromSpec(Spec);
	for (const FNteCharacterKawaiiPresetPlanItem& Preset : KawaiiPlan.Presets)
	{
		NTEBuildTool::Project::FNteAuthoringRecipe Recipe;
		Recipe.Id = TEXT("kawaii_") + Preset.Id;
		Recipe.Type = Preset.TargetKind.Equals(TEXT("MainMesh"), ESearchCase::IgnoreCase) ? TEXT("KawaiiPostProcess") : TEXT("KawaiiAttachedMesh");
		Recipe.Deltas = FindArrayObjectById(LegacyJson, TEXT("KawaiiPresets"), Preset.Id);
		if (!Preset.TargetMeshPath.IsEmpty())
		{
			Recipe.TargetAssetIds.Add(AddOrMergeAsset(Project, Preset.TargetMeshPath, NTEBuildTool::Project::ENteAssetOrigin::UserImported, NTEBuildTool::Project::ENteAssetIntent::ReplacementAsset).Id);
		}
		for (const FString& PackagePath : Preset.PackageSeeds)
		{
			NTEBuildTool::Project::FNteAssetReference& Output = AddOrMergeAsset(Project, PackagePath, NTEBuildTool::Project::ENteAssetOrigin::ToolGenerated, NTEBuildTool::Project::ENteAssetIntent::AddedAsset, Recipe.Id);
			Recipe.OutputAssetIds.AddUnique(Output.Id);
		}
		Project.Recipes.Add(MoveTemp(Recipe));
	}

	for (const NTEBuildTool::Project::FNteAssetReference& Asset : Project.Assets)
	{
		if (Asset.Intent != NTEBuildTool::Project::ENteAssetIntent::ExternalReference)
		{
			Project.PackageManifest.AssetIds.AddUnique(Asset.Id);
		}
	}

	const NTEBuildTool::Project::FNtePakmodProjectValidationResult Validation = NTEBuildTool::Project::ValidatePakmodProject(Project);
	Result.Errors.Append(Validation.Errors);
	Result.Warnings.Append(Validation.Warnings);
	return Result;
}

bool MigrateCharacterModSpecFileToPakmodProjectFile(
	const FString& CharacterSpecFilename,
	const FString& PakmodProjectFilename,
	FNteCharacterModSpecMigrationResult& OutResult,
	FString& OutError)
{
	FNteCharacterModSpec Spec;
	if (!LoadCharacterModSpecFromJsonFile(CharacterSpecFilename, Spec, OutError))
	{
		return false;
	}
	OutResult = MigrateCharacterModSpecToPakmodProject(Spec);
	if (OutResult.HasErrors())
	{
		OutError = FString::Join(OutResult.Errors, LINE_TERMINATOR);
		return false;
	}
	return NTEBuildTool::Project::SavePakmodProjectToJsonFile(OutResult.Project, PakmodProjectFilename, OutError);
}
}
