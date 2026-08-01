// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"

namespace NTEBuildTool::Project
{
struct FNtePakmodProject;
}

namespace NTEBuildTool::Recipes
{
struct FNtePakmodRecipeApplyResult
{
	TArray<FString> AppliedRecipeIds;
	TArray<FString> SavedPackages;
	TArray<FString> Warnings;
	TArray<FString> Errors;

	bool HasErrors() const { return !Errors.IsEmpty(); }
};

/** Projects lightweight recipes into the proven Character writers. */
bool ApplyPakmodRecipes(
	NTEBuildTool::Project::FNtePakmodProject& Project,
	const TArray<FString>& RecipeIds,
	FNtePakmodRecipeApplyResult& OutResult);

/** Reads supported generated Kawaii fields back into its recipe delta. */
bool SyncPakmodKawaiiRecipe(
	NTEBuildTool::Project::FNtePakmodProject& Project,
	const FString& RecipeId,
	FNtePakmodRecipeApplyResult& OutResult);

/** Releases generated ownership without deleting the user asset. */
bool DetachPakmodRecipeOutputs(
	NTEBuildTool::Project::FNtePakmodProject& Project,
	const FString& RecipeId,
	FNtePakmodRecipeApplyResult& OutResult);
}
