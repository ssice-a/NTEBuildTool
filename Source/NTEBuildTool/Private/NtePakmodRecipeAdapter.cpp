// Copyright (c) 2026 NTEBuildTool contributors.

#include "NtePakmodRecipeAdapter.h"

#include "FModelPhysicsAssetImporter.h"
#include "NteCharacterKawaiiAssetSync.h"
#include "NteCharacterKawaiiPlan.h"
#include "NteCharacterKawaiiWriter.h"
#include "NteCharacterMaterialPlan.h"
#include "NteCharacterMaterialWriter.h"
#include "NteCharacterModSpec.h"
#include "NteCharacterRuntimeActionPlan.h"
#include "NteCharacterRuntimeActionWriter.h"
#include "NteEditorAssetUtils.h"
#include "NtePakmodProject.h"

#include "Dom/JsonValue.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace NTEBuildTool::Recipes
{
namespace
{
using namespace NTEBuildTool::Project;
using namespace NTEBuildTool::Character;

void AddArrayItems(TArray<TSharedPtr<FJsonValue>>& Target, const TArray<TSharedPtr<FJsonValue>>* Source)
{
	if (Source)
	{
		Target.Append(*Source);
	}
}

bool BuildLegacyProjection(
	const FNtePakmodProject& Project,
	const TArray<FString>& RecipeIds,
	FNteCharacterModSpec& OutSpec,
	TArray<const FNteAuthoringRecipe*>& OutRecipes,
	FString& OutError)
{
	OutSpec = FNteCharacterModSpec();
	OutSpec.WorkspaceName = Project.Project.DisplayName;
	OutSpec.Package.ModName = Project.PackageManifest.ModName;
	OutSpec.Package.ModsDir = Project.PackageManifest.ModsDirOverride;
	OutSpec.Package.JobFilename = Project.PackageManifest.JobFilename;
	for (const FNteAssetReference& Asset : Project.Assets)
	{
		if (Asset.Intent == ENteAssetIntent::ReplacementAsset &&
			(Asset.ClassName.IsEmpty() || Asset.ClassName.Contains(TEXT("SkeletalMesh"), ESearchCase::IgnoreCase)))
		{
			OutSpec.MainMeshPath = Asset.PackagePath;
			break;
		}
	}

	TArray<TSharedPtr<FJsonValue>> MaterialValues;
	TArray<TSharedPtr<FJsonValue>> RuntimeValues;
	TArray<TSharedPtr<FJsonValue>> KawaiiValues;
	TSharedPtr<FJsonObject> RuntimeUi;

	for (const FString& RecipeId : RecipeIds)
	{
		const FNteAuthoringRecipe* Recipe = FindRecipeById(Project, RecipeId);
		if (!Recipe)
		{
			OutError = FString::Printf(TEXT("Recipe '%s' does not exist."), *RecipeId);
			return false;
		}
		if (!Recipe->bEnabled)
		{
			continue;
		}
		OutRecipes.Add(Recipe);
		if (!Recipe->Deltas.IsValid())
		{
			continue;
		}
		if (Recipe->Type.Equals(TEXT("MaterialInstance"), ESearchCase::IgnoreCase))
		{
			MaterialValues.Add(MakeShared<FJsonValueObject>(Recipe->Deltas));
		}
		else if (Recipe->Type.Equals(TEXT("RuntimeActions"), ESearchCase::IgnoreCase))
		{
			const TArray<TSharedPtr<FJsonValue>>* Actions = nullptr;
			AddArrayItems(RuntimeValues, Recipe->Deltas->TryGetArrayField(TEXT("Actions"), Actions) ? Actions : nullptr);
			const TSharedPtr<FJsonObject>* UiObject = nullptr;
			if (Recipe->Deltas->TryGetObjectField(TEXT("RuntimeUi"), UiObject) && UiObject && UiObject->IsValid())
			{
				RuntimeUi = *UiObject;
			}
		}
		else if (Recipe->Type.StartsWith(TEXT("Kawaii"), ESearchCase::IgnoreCase))
		{
			KawaiiValues.Add(MakeShared<FJsonValueObject>(Recipe->Deltas));
			if (OutSpec.MainPostProcessAnimBlueprintPath.IsEmpty())
			{
				Recipe->Deltas->TryGetStringField(TEXT("RuntimeAnimBlueprintPath"), OutSpec.MainPostProcessAnimBlueprintPath);
			}
		}
	}

	// External AnimBPs are evidence only; the generated writers use the explicit paths in recipe deltas.
	for (const FNteAssetReference& Asset : Project.Assets)
	{
		if (Asset.Intent == ENteAssetIntent::ExternalReference && Asset.ClassName.Contains(TEXT("AnimBlueprint"), ESearchCase::IgnoreCase))
		{
			if (OutSpec.MainAnimBlueprintPath.IsEmpty())
			{
				OutSpec.MainAnimBlueprintPath = Asset.PackagePath;
			}
		}
	}
	const TSharedRef<FJsonObject> Root = CharacterModSpecToJson(OutSpec);
	Root->SetArrayField(TEXT("MaterialOperations"), MoveTemp(MaterialValues));
	Root->SetArrayField(TEXT("RuntimeActions"), MoveTemp(RuntimeValues));
	Root->SetArrayField(TEXT("KawaiiPresets"), MoveTemp(KawaiiValues));
	if (RuntimeUi.IsValid())
	{
		Root->SetObjectField(TEXT("RuntimeUi"), RuntimeUi);
	}

	FString ParseError;
	if (!CharacterModSpecFromJson(*Root, OutSpec, ParseError))
	{
		OutError = FString::Printf(TEXT("Recipe projection failed: %s"), *ParseError);
		return false;
	}
	return true;
}

void AppendWriteResult(const FNteCharacterMaterialWriteResult& Result, FNtePakmodRecipeApplyResult& Out)
{
	Out.Warnings.Append(Result.Warnings);
	Out.Errors.Append(Result.Errors);
	for (const FNteCharacterMaterialOperationWriteResult& Operation : Result.Operations)
	{
		if (Operation.bApplied)
		{
			Out.SavedPackages.AddUnique(Operation.OutputMaterialPath);
			Out.SavedPackages.AddUnique(Operation.TargetMeshPath);
		}
		Out.Warnings.Append(Operation.Warnings);
		Out.Errors.Append(Operation.Errors);
	}
}

void AppendWriteResult(const FNteCharacterRuntimeActionWriteResult& Result, FNtePakmodRecipeApplyResult& Out)
{
	Out.Warnings.Append(Result.Warnings);
	Out.Errors.Append(Result.Errors);
	Out.SavedPackages.Append(Result.SavedPackages);
}

void AppendWriteResult(const FNteCharacterKawaiiWriteResult& Result, FNtePakmodRecipeApplyResult& Out)
{
	Out.Warnings.Append(Result.Warnings);
	Out.Errors.Append(Result.Errors);
	Out.SavedPackages.Append(Result.SavedPackages);
	for (const FNteCharacterKawaiiAssetWriteResult& Asset : Result.Assets)
	{
		Out.SavedPackages.AddUnique(Asset.AssetPath);
		Out.Warnings.Append(Asset.Warnings);
		Out.Errors.Append(Asset.Errors);
	}
}

void RegisterGeneratedOutputs(
	FNtePakmodProject& Project,
	const FNteAuthoringRecipe& Recipe,
	const TArray<FString>& PackagePaths)
{
	for (const FString& RawPath : PackagePaths)
	{
		const FString PackagePath = NormalizePakmodPackagePath(RawPath);
		if (!PackagePath.StartsWith(TEXT("/Game/")) || PackagePath.Contains(TEXT(".")))
		{
			continue;
		}
		FNteAssetReference* Existing = Project.Assets.FindByPredicate([&PackagePath](FNteAssetReference& Asset)
		{
			return Asset.PackagePath.Equals(PackagePath, ESearchCase::IgnoreCase);
		});
		if (!Existing)
		{
			FNteAssetReference Asset;
			Asset.Id = TEXT("generated_") + FPackageName::GetShortName(PackagePath).ToLower();
			int32 Suffix = 2;
			const FString BaseId = Asset.Id;
			while (FindAssetById(Project, Asset.Id)) Asset.Id = FString::Printf(TEXT("%s_%d"), *BaseId, Suffix++);
			Asset.PackagePath = PackagePath;
			Asset.Origin = ENteAssetOrigin::ToolGenerated;
			Asset.Intent = ENteAssetIntent::AddedAsset;
			Asset.OwnerRecipeId = Recipe.Id;
			Project.Assets.Add(MoveTemp(Asset));
			Existing = &Project.Assets.Last();
		}
		if (Existing->Origin == ENteAssetOrigin::ToolGenerated && Existing->OwnerRecipeId.IsEmpty())
		{
			Existing->OwnerRecipeId = Recipe.Id;
		}
		if (Existing->Intent != ENteAssetIntent::ExternalReference)
		{
			Project.PackageManifest.AssetIds.AddUnique(Existing->Id);
		}
	}
}
}

bool ApplyPakmodRecipes(FNtePakmodProject& Project, const TArray<FString>& RecipeIds, FNtePakmodRecipeApplyResult& OutResult)
{
	OutResult = FNtePakmodRecipeApplyResult();
	if (RecipeIds.IsEmpty())
	{
		OutResult.Errors.Add(TEXT("No recipes selected for Apply Changes."));
		return false;
	}
	TArray<const FNteAuthoringRecipe*> Recipes;
	FNteCharacterModSpec Spec;
	FString ProjectionError;
	if (!BuildLegacyProjection(Project, RecipeIds, Spec, Recipes, ProjectionError))
	{
		OutResult.Errors.Add(ProjectionError);
		return false;
	}

	bool bDidAnything = false;
	for (const FNteAuthoringRecipe* Recipe : Recipes)
	{
		if (Recipe->Type.Equals(TEXT("MaterialInstance"), ESearchCase::IgnoreCase))
		{
			const FNteCharacterMaterialWriteResult Result = WriteCharacterMaterials(BuildCharacterMaterialPlanFromSpec(Spec));
			AppendWriteResult(Result, OutResult);
			RegisterGeneratedOutputs(Project, *Recipe, OutResult.SavedPackages);
			bDidAnything = true;
			break;
		}
	}
	for (const FNteAuthoringRecipe* Recipe : Recipes)
	{
		if (Recipe->Type.Equals(TEXT("RuntimeActions"), ESearchCase::IgnoreCase))
		{
			const FNteCharacterRuntimeActionWriteResult Result = WriteCharacterRuntimeActions(BuildCharacterRuntimeActionPlanFromSpec(Spec));
			AppendWriteResult(Result, OutResult);
			RegisterGeneratedOutputs(Project, *Recipe, Result.SavedPackages);
			bDidAnything = true;
			break;
		}
	}
	for (const FNteAuthoringRecipe* Recipe : Recipes)
	{
		if (Recipe->Type.StartsWith(TEXT("Kawaii"), ESearchCase::IgnoreCase) || Recipe->Type.Equals(TEXT("PostProcessBlueprint"), ESearchCase::IgnoreCase))
		{
			const FNteCharacterKawaiiWriteResult Result = WriteCharacterKawaiiAssets(BuildCharacterKawaiiPlanFromSpec(Spec));
			AppendWriteResult(Result, OutResult);
			RegisterGeneratedOutputs(Project, *Recipe, Result.SavedPackages);
			bDidAnything = true;
			break;
		}
	}

	for (const FNteAuthoringRecipe* Recipe : Recipes)
	{
		if (!Recipe->Type.Equals(TEXT("PhysicsAsset"), ESearchCase::IgnoreCase) || !Recipe->Deltas.IsValid())
		{
			continue;
		}
		FString TargetPath;
		Recipe->Deltas->TryGetStringField(TEXT("TargetMeshPath"), TargetPath);
		if (TargetPath.IsEmpty() && !Recipe->TargetAssetIds.IsEmpty())
		{
			if (const FNteAssetReference* Target = FindAssetById(Project, Recipe->TargetAssetIds[0]))
			{
				TargetPath = Target->PackagePath;
			}
		}
		USkeletalMesh* Mesh = NTEBuildTool::Editor::LoadAssetByPath<USkeletalMesh>(TargetPath);
		FString JsonPath;
		Recipe->Deltas->TryGetStringField(TEXT("SourcePhysicsAssetJson"), JsonPath);
		if (!Mesh || JsonPath.IsEmpty())
		{
			OutResult.Errors.Add(FString::Printf(TEXT("PhysicsAsset recipe '%s' needs a loaded target SkeletalMesh and SourcePhysicsAssetJson."), *Recipe->Id));
			continue;
		}
		FFModelPhysicsAssetImportOptions Options;
		FString ChainRootBone;
		Recipe->Deltas->TryGetStringField(TEXT("ChainRootBone"), ChainRootBone);
		Options.ChainRootBone = ChainRootBone.IsEmpty() ? NAME_None : FName(*ChainRootBone);
		FFModelPhysicsAssetImportSummary Summary;
		FString Error;
		UPhysicsAsset* PhysicsAsset = FFModelPhysicsAssetImporter::ImportFromJsonFile(*Mesh, JsonPath, Options, Summary, Error);
		if (!PhysicsAsset)
		{
			OutResult.Errors.Add(Error);
			continue;
		}
		Mesh->SetPhysicsAsset(PhysicsAsset);
		Mesh->PostEditChange();
		Mesh->MarkPackageDirty();
		OutResult.SavedPackages.AddUnique(Mesh->GetOutermost()->GetName());
		RegisterGeneratedOutputs(Project, *Recipe, { Mesh->GetOutermost()->GetName() });
		bDidAnything = true;
	}

	if (!bDidAnything)
	{
		OutResult.Warnings.Add(TEXT("Selected recipes do not have an editor writer yet; no assets were changed."));
	}
	for (const FString& RecipeId : RecipeIds)
	{
		if (FindRecipeById(Project, RecipeId))
		{
			OutResult.AppliedRecipeIds.AddUnique(RecipeId);
		}
	}
	return !OutResult.HasErrors();
}

bool SyncPakmodKawaiiRecipe(FNtePakmodProject& Project, const FString& RecipeId, FNtePakmodRecipeApplyResult& OutResult)
{
	OutResult = FNtePakmodRecipeApplyResult();
	FNteAuthoringRecipe* Recipe = Project.Recipes.FindByPredicate([&RecipeId](FNteAuthoringRecipe& Item)
	{
		return Item.Id.Equals(RecipeId, ESearchCase::IgnoreCase);
	});
	if (!Recipe || !Recipe->Type.StartsWith(TEXT("Kawaii"), ESearchCase::IgnoreCase))
	{
		OutResult.Errors.Add(FString::Printf(TEXT("Recipe '%s' is not a Kawaii recipe."), *RecipeId));
		return false;
	}
	FNteCharacterModSpec Spec;
	TArray<const FNteAuthoringRecipe*> Selected;
	FString Error;
	if (!BuildLegacyProjection(Project, { RecipeId }, Spec, Selected, Error))
	{
		OutResult.Errors.Add(Error);
		return false;
	}
	FNteCharacterKawaiiPlan Plan = BuildCharacterKawaiiPlanFromSpec(Spec);
	FString PresetId;
	Recipe->Deltas->TryGetStringField(TEXT("Id"), PresetId);
	FNteCharacterKawaiiPresetPlanItem* PlanItem = Plan.Presets.FindByPredicate([&PresetId](FNteCharacterKawaiiPresetPlanItem& Item)
	{
		return Item.Id == PresetId;
	});
	if (!PlanItem)
	{
		OutResult.Errors.Add(TEXT("Kawaii recipe does not resolve to a preset plan."));
		return false;
	}
	FNteCharacterKawaiiPresetSpec Preset;
	if (Spec.KawaiiPresets.IsEmpty())
	{
		OutResult.Errors.Add(TEXT("Kawaii recipe has no preset data."));
		return false;
	}
	const FNteCharacterKawaiiPresetSpec* PresetPtr = Spec.KawaiiPresets.FindByPredicate([&PresetId](const FNteCharacterKawaiiPresetSpec& Candidate)
	{
		return Candidate.Id == PresetId;
	});
	if (!PresetPtr)
	{
		OutResult.Errors.Add(FString::Printf(TEXT("Kawaii preset '%s' is not present in the projected recipe."), *PresetId));
		return false;
	}
	Preset = *PresetPtr;
	const FNteCharacterKawaiiAssetSyncResult Sync = SyncKawaiiPresetSpecFromGeneratedAssets(*PlanItem, Preset);
	OutResult.Warnings.Append(Sync.Warnings);
	OutResult.Errors.Append(Sync.Errors);
	if (!Sync.HasErrors())
	{
		for (FNteCharacterKawaiiPresetSpec& Candidate : Spec.KawaiiPresets)
		{
			if (Candidate.Id == PresetId)
			{
				Candidate = MoveTemp(Preset);
				break;
			}
		}
		const TSharedRef<FJsonObject> Json = CharacterModSpecToJson(Spec);
		const TArray<TSharedPtr<FJsonValue>>* Presets = nullptr;
		if (Json->TryGetArrayField(TEXT("KawaiiPresets"), Presets) && Presets)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Presets)
			{
				const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
				FString CandidateId;
				if (Object.IsValid() && Object->TryGetStringField(TEXT("Id"), CandidateId) && CandidateId == PresetId)
				{
					Recipe->Deltas = Object;
					break;
				}
			}
		}
		OutResult.AppliedRecipeIds.Add(RecipeId);
	}
	return !OutResult.HasErrors();
}

bool DetachPakmodRecipeOutputs(FNtePakmodProject& Project, const FString& RecipeId, FNtePakmodRecipeApplyResult& OutResult)
{
	OutResult = FNtePakmodRecipeApplyResult();
	FNteAuthoringRecipe* Recipe = Project.Recipes.FindByPredicate([&RecipeId](FNteAuthoringRecipe& Item)
	{
		return Item.Id.Equals(RecipeId, ESearchCase::IgnoreCase);
	});
	if (!Recipe)
	{
		OutResult.Errors.Add(FString::Printf(TEXT("Recipe '%s' does not exist."), *RecipeId));
		return false;
	}
	for (const FString& AssetId : Recipe->OutputAssetIds)
	{
		if (FNteAssetReference* Asset = Project.Assets.FindByPredicate([&AssetId](FNteAssetReference& Item) { return Item.Id == AssetId; }))
		{
			Asset->OwnerRecipeId.Reset();
			Asset->Origin = ENteAssetOrigin::UserImported;
		}
	}
	Recipe->OutputAssetIds.Reset();
	Recipe->bEnabled = false;
	OutResult.AppliedRecipeIds.Add(RecipeId);
	return true;
}
}
