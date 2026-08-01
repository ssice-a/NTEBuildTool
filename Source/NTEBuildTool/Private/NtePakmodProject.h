// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace NTEBuildTool::Project
{
enum class ENteAssetOrigin : uint8
{
	GameReference,
	UserImported,
	ToolGenerated,
	Invalid
};

enum class ENteAssetIntent : uint8
{
	ExternalReference,
	ReplacementAsset,
	AddedAsset,
	Invalid
};

struct FNtePakmodProjectIdentity
{
	FString Id;
	FString DisplayName;
	FString GameProfileId = TEXT("nte-default");
};

struct FNteSourceReference
{
	FString Id;
	FString Kind;
	FString GamePackagePath;
	FString Locator;
};

struct FNteAssetReference
{
	FString Id;
	FString PackagePath;
	FString ClassName;
	ENteAssetOrigin Origin = ENteAssetOrigin::UserImported;
	ENteAssetIntent Intent = ENteAssetIntent::AddedAsset;
	FString OwnerRecipeId;
};

struct FNteAuthoringRecipe
{
	FString Id;
	FString Type;
	bool bEnabled = true;
	TArray<FString> SourceIds;
	TArray<FString> TargetAssetIds;
	TSharedPtr<FJsonObject> Deltas = MakeShared<FJsonObject>();
	TArray<FString> OutputAssetIds;
};

struct FNtePackageManifest
{
	FString ModName;
	FString OutputProfileId = TEXT("nte-client-mods");
	FString ModsDirOverride;
	FString JobFilename;
	FString GameMountNameOverride;
	TArray<FString> AssetIds;
	TArray<FString> ExplicitExclusions;
	bool bUnversioned = false;
	bool bRequiresHTGameStub = false;
};

struct FNtePakmodProject
{
	FString Format = TEXT("NTE.PakmodProject");
	int32 Version = 1;
	FNtePakmodProjectIdentity Project;
	TArray<FNteSourceReference> Sources;
	TArray<FNteAssetReference> Assets;
	TArray<FNteAuthoringRecipe> Recipes;
	FNtePackageManifest PackageManifest;
};

struct FNtePakmodProjectValidationResult
{
	TArray<FString> Errors;
	TArray<FString> Warnings;

	bool HasErrors() const { return !Errors.IsEmpty(); }
};

FString AssetOriginToString(ENteAssetOrigin Origin);
ENteAssetOrigin AssetOriginFromString(const FString& Value);
FString AssetIntentToString(ENteAssetIntent Intent);
ENteAssetIntent AssetIntentFromString(const FString& Value);
FString NormalizePakmodPackagePath(FString AssetPath);

const FNteAssetReference* FindAssetById(const FNtePakmodProject& Project, const FString& AssetId);
const FNteAuthoringRecipe* FindRecipeById(const FNtePakmodProject& Project, const FString& RecipeId);

TSharedRef<FJsonObject> PakmodProjectToJson(const FNtePakmodProject& Project);
bool PakmodProjectFromJson(const FJsonObject& Object, FNtePakmodProject& OutProject, FString& OutError);
bool LoadPakmodProjectFromJsonFile(const FString& Filename, FNtePakmodProject& OutProject, FString& OutError);
bool SavePakmodProjectToJsonFile(const FNtePakmodProject& Project, const FString& Filename, FString& OutError);
FNtePakmodProjectValidationResult ValidatePakmodProject(const FNtePakmodProject& Project);
}
