// Copyright (c) 2026 NTEBuildTool contributors.

#include "NtePakmodProject.h"

#include "NteJsonFileUtils.h"

#include "Misc/PackageName.h"

namespace NTEBuildTool::Project
{
namespace
{
FString GetString(const FJsonObject& Object, const TCHAR* Name)
{
	FString Value;
	Object.TryGetStringField(Name, Value);
	Value.TrimStartAndEndInline();
	return Value;
}

void SetStringIfNotEmpty(const TSharedRef<FJsonObject>& Object, const TCHAR* Name, const FString& Value)
{
	if (!Value.IsEmpty())
	{
		Object->SetStringField(Name, Value);
	}
}

TArray<TSharedPtr<FJsonValue>> StringsToJson(const TArray<FString>& Values)
{
	return Json::StringArrayToJsonValues(Values);
}

TSharedRef<FJsonObject> SourceToJson(const FNteSourceReference& Source)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("Id"), Source.Id);
	Object->SetStringField(TEXT("Kind"), Source.Kind);
	SetStringIfNotEmpty(Object, TEXT("GamePackagePath"), Source.GamePackagePath);
	SetStringIfNotEmpty(Object, TEXT("Locator"), Source.Locator);
	return Object;
}

TSharedRef<FJsonObject> AssetToJson(const FNteAssetReference& Asset)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("Id"), Asset.Id);
	Object->SetStringField(TEXT("PackagePath"), Asset.PackagePath);
	SetStringIfNotEmpty(Object, TEXT("Class"), Asset.ClassName);
	Object->SetStringField(TEXT("Origin"), AssetOriginToString(Asset.Origin));
	Object->SetStringField(TEXT("Intent"), AssetIntentToString(Asset.Intent));
	SetStringIfNotEmpty(Object, TEXT("OwnerRecipeId"), Asset.OwnerRecipeId);
	return Object;
}

TSharedRef<FJsonObject> RecipeToJson(const FNteAuthoringRecipe& Recipe)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("Id"), Recipe.Id);
	Object->SetStringField(TEXT("Type"), Recipe.Type);
	Object->SetBoolField(TEXT("Enabled"), Recipe.bEnabled);
	Object->SetArrayField(TEXT("Sources"), StringsToJson(Recipe.SourceIds));
	Object->SetArrayField(TEXT("Targets"), StringsToJson(Recipe.TargetAssetIds));
	Object->SetObjectField(TEXT("Deltas"), Recipe.Deltas.IsValid() ? Recipe.Deltas.ToSharedRef() : MakeShared<FJsonObject>());
	Object->SetArrayField(TEXT("Outputs"), StringsToJson(Recipe.OutputAssetIds));
	return Object;
}

FNteSourceReference SourceFromJson(const FJsonObject& Object)
{
	FNteSourceReference Source;
	Source.Id = GetString(Object, TEXT("Id"));
	Source.Kind = GetString(Object, TEXT("Kind"));
	Source.GamePackagePath = NormalizePakmodPackagePath(GetString(Object, TEXT("GamePackagePath")));
	Source.Locator = GetString(Object, TEXT("Locator"));
	return Source;
}

FNteAssetReference AssetFromJson(const FJsonObject& Object)
{
	FNteAssetReference Asset;
	Asset.Id = GetString(Object, TEXT("Id"));
	Asset.PackagePath = NormalizePakmodPackagePath(GetString(Object, TEXT("PackagePath")));
	Asset.ClassName = GetString(Object, TEXT("Class"));
	Asset.Origin = AssetOriginFromString(GetString(Object, TEXT("Origin")));
	Asset.Intent = AssetIntentFromString(GetString(Object, TEXT("Intent")));
	Asset.OwnerRecipeId = GetString(Object, TEXT("OwnerRecipeId"));
	return Asset;
}

FNteAuthoringRecipe RecipeFromJson(const FJsonObject& Object)
{
	FNteAuthoringRecipe Recipe;
	Recipe.Id = GetString(Object, TEXT("Id"));
	Recipe.Type = GetString(Object, TEXT("Type"));
	Object.TryGetBoolField(TEXT("Enabled"), Recipe.bEnabled);
	Recipe.SourceIds = Json::GetStringArrayAny(Object, TEXT("Sources"));
	Recipe.TargetAssetIds = Json::GetStringArrayAny(Object, TEXT("Targets"));
	Recipe.OutputAssetIds = Json::GetStringArrayAny(Object, TEXT("Outputs"));
	const TSharedPtr<FJsonObject>* Deltas = nullptr;
	if (Object.TryGetObjectField(TEXT("Deltas"), Deltas) && Deltas && Deltas->IsValid())
	{
		Recipe.Deltas = *Deltas;
	}
	return Recipe;
}

template<typename ItemType, typename WriterType>
TArray<TSharedPtr<FJsonValue>> ObjectsToJson(const TArray<ItemType>& Items, WriterType&& Writer)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	Values.Reserve(Items.Num());
	for (const ItemType& Item : Items)
	{
		Values.Add(MakeShared<FJsonValueObject>(Writer(Item)));
	}
	return Values;
}

template<typename ReaderType>
void ReadObjectArray(const FJsonObject& Object, const TCHAR* Name, ReaderType&& Reader)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.TryGetArrayField(Name, Values) || !Values)
	{
		return;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		const TSharedPtr<FJsonObject> Child = Value.IsValid() ? Value->AsObject() : nullptr;
		if (Child.IsValid())
		{
			Reader(*Child);
		}
	}
}

void AddDuplicateIdErrors(const TCHAR* Kind, const TArray<FString>& Ids, FNtePakmodProjectValidationResult& Result)
{
	TSet<FString> Seen;
	for (const FString& Id : Ids)
	{
		if (Id.IsEmpty())
		{
			Result.Errors.Add(FString::Printf(TEXT("%s id is empty."), Kind));
			continue;
		}
		const FString Key = Id.ToLower();
		if (Seen.Contains(Key))
		{
			Result.Errors.Add(FString::Printf(TEXT("Duplicate %s id: %s"), Kind, *Id));
		}
		Seen.Add(Key);
	}
}
}

FString AssetOriginToString(const ENteAssetOrigin Origin)
{
	switch (Origin)
	{
	case ENteAssetOrigin::GameReference: return TEXT("GameReference");
	case ENteAssetOrigin::UserImported: return TEXT("UserImported");
	case ENteAssetOrigin::ToolGenerated: return TEXT("ToolGenerated");
	default: return TEXT("Invalid");
	}
}

ENteAssetOrigin AssetOriginFromString(const FString& Value)
{
	if (Value.Equals(TEXT("GameReference"), ESearchCase::IgnoreCase)) return ENteAssetOrigin::GameReference;
	if (Value.Equals(TEXT("UserImported"), ESearchCase::IgnoreCase)) return ENteAssetOrigin::UserImported;
	if (Value.Equals(TEXT("ToolGenerated"), ESearchCase::IgnoreCase)) return ENteAssetOrigin::ToolGenerated;
	return ENteAssetOrigin::Invalid;
}

FString AssetIntentToString(const ENteAssetIntent Intent)
{
	switch (Intent)
	{
	case ENteAssetIntent::ExternalReference: return TEXT("ExternalReference");
	case ENteAssetIntent::ReplacementAsset: return TEXT("ReplacementAsset");
	case ENteAssetIntent::AddedAsset: return TEXT("AddedAsset");
	default: return TEXT("Invalid");
	}
}

ENteAssetIntent AssetIntentFromString(const FString& Value)
{
	if (Value.Equals(TEXT("ExternalReference"), ESearchCase::IgnoreCase)) return ENteAssetIntent::ExternalReference;
	if (Value.Equals(TEXT("ReplacementAsset"), ESearchCase::IgnoreCase)) return ENteAssetIntent::ReplacementAsset;
	if (Value.Equals(TEXT("AddedAsset"), ESearchCase::IgnoreCase)) return ENteAssetIntent::AddedAsset;
	return ENteAssetIntent::Invalid;
}

FString NormalizePakmodPackagePath(FString AssetPath)
{
	AssetPath.TrimStartAndEndInline();
	AssetPath.TrimQuotesInline();
	if (AssetPath.IsEmpty())
	{
		return AssetPath;
	}
	if (AssetPath.Contains(TEXT(".")))
	{
		const FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
		if (!PackageName.IsEmpty())
		{
			AssetPath = PackageName;
		}
	}
	return AssetPath;
}

const FNteAssetReference* FindAssetById(const FNtePakmodProject& Project, const FString& AssetId)
{
	return Project.Assets.FindByPredicate([&AssetId](const FNteAssetReference& Asset)
	{
		return Asset.Id.Equals(AssetId, ESearchCase::IgnoreCase);
	});
}

const FNteAuthoringRecipe* FindRecipeById(const FNtePakmodProject& Project, const FString& RecipeId)
{
	return Project.Recipes.FindByPredicate([&RecipeId](const FNteAuthoringRecipe& Recipe)
	{
		return Recipe.Id.Equals(RecipeId, ESearchCase::IgnoreCase);
	});
}

TSharedRef<FJsonObject> PakmodProjectToJson(const FNtePakmodProject& Project)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("Format"), Project.Format);
	Root->SetNumberField(TEXT("Version"), Project.Version);

	const TSharedRef<FJsonObject> Identity = MakeShared<FJsonObject>();
	Identity->SetStringField(TEXT("Id"), Project.Project.Id);
	SetStringIfNotEmpty(Identity, TEXT("DisplayName"), Project.Project.DisplayName);
	SetStringIfNotEmpty(Identity, TEXT("GameProfileId"), Project.Project.GameProfileId);
	Root->SetObjectField(TEXT("Project"), Identity);

	Root->SetArrayField(TEXT("Sources"), ObjectsToJson(Project.Sources, SourceToJson));
	Root->SetArrayField(TEXT("Assets"), ObjectsToJson(Project.Assets, AssetToJson));
	Root->SetArrayField(TEXT("Recipes"), ObjectsToJson(Project.Recipes, RecipeToJson));

	const TSharedRef<FJsonObject> Manifest = MakeShared<FJsonObject>();
	Manifest->SetStringField(TEXT("ModName"), Project.PackageManifest.ModName);
	SetStringIfNotEmpty(Manifest, TEXT("OutputProfileId"), Project.PackageManifest.OutputProfileId);
	SetStringIfNotEmpty(Manifest, TEXT("ModsDirOverride"), Project.PackageManifest.ModsDirOverride);
	SetStringIfNotEmpty(Manifest, TEXT("JobFilename"), Project.PackageManifest.JobFilename);
	SetStringIfNotEmpty(Manifest, TEXT("GameMountNameOverride"), Project.PackageManifest.GameMountNameOverride);
	Manifest->SetArrayField(TEXT("AssetIds"), StringsToJson(Project.PackageManifest.AssetIds));
	Manifest->SetArrayField(TEXT("ExplicitExclusions"), StringsToJson(Project.PackageManifest.ExplicitExclusions));
	const TSharedRef<FJsonObject> Cook = MakeShared<FJsonObject>();
	Cook->SetBoolField(TEXT("Unversioned"), Project.PackageManifest.bUnversioned);
	Cook->SetBoolField(TEXT("RequiresHTGameStub"), Project.PackageManifest.bRequiresHTGameStub);
	Manifest->SetObjectField(TEXT("Cook"), Cook);
	Root->SetObjectField(TEXT("PackageManifest"), Manifest);
	return Root;
}

bool PakmodProjectFromJson(const FJsonObject& Object, FNtePakmodProject& OutProject, FString& OutError)
{
	const FString Format = GetString(Object, TEXT("Format"));
	if (Format != TEXT("NTE.PakmodProject"))
	{
		OutError = FString::Printf(TEXT("Unsupported pakmod project format: %s"), *Format);
		return false;
	}
	double VersionNumber = 0.0;
	if (!Object.TryGetNumberField(TEXT("Version"), VersionNumber) || static_cast<int32>(VersionNumber) != 1)
	{
		OutError = FString::Printf(TEXT("Unsupported pakmod project version: %d"), static_cast<int32>(VersionNumber));
		return false;
	}

	OutProject = FNtePakmodProject();
	const TSharedPtr<FJsonObject>* Identity = nullptr;
	if (!Object.TryGetObjectField(TEXT("Project"), Identity) || !Identity || !Identity->IsValid())
	{
		OutError = TEXT("Pakmod project is missing Project identity.");
		return false;
	}
	OutProject.Project.Id = GetString(**Identity, TEXT("Id"));
	OutProject.Project.DisplayName = GetString(**Identity, TEXT("DisplayName"));
	const FString GameProfileId = GetString(**Identity, TEXT("GameProfileId"));
	if (!GameProfileId.IsEmpty())
	{
		OutProject.Project.GameProfileId = GameProfileId;
	}

	ReadObjectArray(Object, TEXT("Sources"), [&OutProject](const FJsonObject& Child) { OutProject.Sources.Add(SourceFromJson(Child)); });
	ReadObjectArray(Object, TEXT("Assets"), [&OutProject](const FJsonObject& Child) { OutProject.Assets.Add(AssetFromJson(Child)); });
	ReadObjectArray(Object, TEXT("Recipes"), [&OutProject](const FJsonObject& Child) { OutProject.Recipes.Add(RecipeFromJson(Child)); });

	const TSharedPtr<FJsonObject>* Manifest = nullptr;
	if (!Object.TryGetObjectField(TEXT("PackageManifest"), Manifest) || !Manifest || !Manifest->IsValid())
	{
		OutError = TEXT("Pakmod project is missing PackageManifest.");
		return false;
	}
	OutProject.PackageManifest.ModName = GetString(**Manifest, TEXT("ModName"));
	const FString OutputProfileId = GetString(**Manifest, TEXT("OutputProfileId"));
	if (!OutputProfileId.IsEmpty())
	{
		OutProject.PackageManifest.OutputProfileId = OutputProfileId;
	}
	OutProject.PackageManifest.ModsDirOverride = GetString(**Manifest, TEXT("ModsDirOverride"));
	OutProject.PackageManifest.JobFilename = GetString(**Manifest, TEXT("JobFilename"));
	OutProject.PackageManifest.GameMountNameOverride = GetString(**Manifest, TEXT("GameMountNameOverride"));
	OutProject.PackageManifest.AssetIds = Json::GetStringArrayAny(**Manifest, TEXT("AssetIds"));
	OutProject.PackageManifest.ExplicitExclusions = Json::GetStringArrayAny(**Manifest, TEXT("ExplicitExclusions"));
	const TSharedPtr<FJsonObject>* Cook = nullptr;
	if ((*Manifest)->TryGetObjectField(TEXT("Cook"), Cook) && Cook && Cook->IsValid())
	{
		(*Cook)->TryGetBoolField(TEXT("Unversioned"), OutProject.PackageManifest.bUnversioned);
		(*Cook)->TryGetBoolField(TEXT("RequiresHTGameStub"), OutProject.PackageManifest.bRequiresHTGameStub);
	}
	return true;
}

bool LoadPakmodProjectFromJsonFile(const FString& Filename, FNtePakmodProject& OutProject, FString& OutError)
{
	TSharedPtr<FJsonObject> Object;
	return Json::LoadJsonObjectFromFile(Filename, Object, OutError)
		&& PakmodProjectFromJson(*Object, OutProject, OutError);
}

bool SavePakmodProjectToJsonFile(const FNtePakmodProject& Project, const FString& Filename, FString& OutError)
{
	const FNtePakmodProjectValidationResult Validation = ValidatePakmodProject(Project);
	if (Validation.HasErrors())
	{
		OutError = FString::Join(Validation.Errors, LINE_TERMINATOR);
		return false;
	}
	return Json::SaveJsonObjectToFile(PakmodProjectToJson(Project), Filename, OutError);
}

FNtePakmodProjectValidationResult ValidatePakmodProject(const FNtePakmodProject& Project)
{
	FNtePakmodProjectValidationResult Result;
	if (Project.Format != TEXT("NTE.PakmodProject"))
	{
		Result.Errors.Add(FString::Printf(TEXT("Unsupported Format: %s"), *Project.Format));
	}
	if (Project.Version != 1)
	{
		Result.Errors.Add(FString::Printf(TEXT("Unsupported Version: %d"), Project.Version));
	}
	if (Project.Project.Id.TrimStartAndEnd().IsEmpty())
	{
		Result.Errors.Add(TEXT("Project.Id is required."));
	}
	if (Project.PackageManifest.ModName.TrimStartAndEnd().IsEmpty())
	{
		Result.Errors.Add(TEXT("PackageManifest.ModName is required."));
	}

	TArray<FString> SourceIds;
	for (const FNteSourceReference& Source : Project.Sources)
	{
		SourceIds.Add(Source.Id);
		if (!Source.GamePackagePath.IsEmpty() && !FPackageName::IsValidLongPackageName(Source.GamePackagePath))
		{
			Result.Errors.Add(FString::Printf(TEXT("Source '%s' has invalid GamePackagePath: %s"), *Source.Id, *Source.GamePackagePath));
		}
	}
	AddDuplicateIdErrors(TEXT("source"), SourceIds, Result);

	TArray<FString> AssetIds;
	for (const FNteAssetReference& Asset : Project.Assets)
	{
		AssetIds.Add(Asset.Id);
		if (!FPackageName::IsValidLongPackageName(Asset.PackagePath))
		{
			Result.Errors.Add(FString::Printf(TEXT("Asset '%s' has invalid PackagePath: %s"), *Asset.Id, *Asset.PackagePath));
		}
		if (Asset.Origin == ENteAssetOrigin::Invalid)
		{
			Result.Errors.Add(FString::Printf(TEXT("Asset '%s' has invalid Origin."), *Asset.Id));
		}
		if (Asset.Intent == ENteAssetIntent::Invalid)
		{
			Result.Errors.Add(FString::Printf(TEXT("Asset '%s' has invalid Intent."), *Asset.Id));
		}
	}
	AddDuplicateIdErrors(TEXT("asset"), AssetIds, Result);

	TArray<FString> RecipeIds;
	for (const FNteAuthoringRecipe& Recipe : Project.Recipes)
	{
		RecipeIds.Add(Recipe.Id);
		if (Recipe.Type.TrimStartAndEnd().IsEmpty())
		{
			Result.Errors.Add(FString::Printf(TEXT("Recipe '%s' has no Type."), *Recipe.Id));
		}
	}
	AddDuplicateIdErrors(TEXT("recipe"), RecipeIds, Result);

	for (const FNteAssetReference& Asset : Project.Assets)
	{
		if (!Asset.OwnerRecipeId.IsEmpty() && !FindRecipeById(Project, Asset.OwnerRecipeId))
		{
			Result.Errors.Add(FString::Printf(TEXT("Asset '%s' references missing OwnerRecipeId '%s'."), *Asset.Id, *Asset.OwnerRecipeId));
		}
	}
	for (const FNteAuthoringRecipe& Recipe : Project.Recipes)
	{
		for (const FString& SourceId : Recipe.SourceIds)
		{
			if (!Project.Sources.ContainsByPredicate([&SourceId](const FNteSourceReference& Source) { return Source.Id.Equals(SourceId, ESearchCase::IgnoreCase); }))
			{
				Result.Errors.Add(FString::Printf(TEXT("Recipe '%s' references missing source '%s'."), *Recipe.Id, *SourceId));
			}
		}
		for (const FString& AssetId : Recipe.TargetAssetIds)
		{
			if (!FindAssetById(Project, AssetId))
			{
				Result.Errors.Add(FString::Printf(TEXT("Recipe '%s' references missing target asset '%s'."), *Recipe.Id, *AssetId));
			}
		}
		for (const FString& AssetId : Recipe.OutputAssetIds)
		{
			const FNteAssetReference* Asset = FindAssetById(Project, AssetId);
			if (!Asset)
			{
				Result.Errors.Add(FString::Printf(TEXT("Recipe '%s' references missing output asset '%s'."), *Recipe.Id, *AssetId));
			}
			else if (!Asset->OwnerRecipeId.Equals(Recipe.Id, ESearchCase::IgnoreCase))
			{
				Result.Warnings.Add(FString::Printf(TEXT("Recipe '%s' output '%s' is not owned by that recipe."), *Recipe.Id, *AssetId));
			}
		}
	}

	TSet<FString> ManifestIds;
	for (const FString& AssetId : Project.PackageManifest.AssetIds)
	{
		const FString Key = AssetId.ToLower();
		if (ManifestIds.Contains(Key))
		{
			Result.Warnings.Add(FString::Printf(TEXT("PackageManifest lists asset '%s' more than once."), *AssetId));
		}
		ManifestIds.Add(Key);
		const FNteAssetReference* Asset = FindAssetById(Project, AssetId);
		if (!Asset)
		{
			Result.Errors.Add(FString::Printf(TEXT("PackageManifest references missing asset '%s'."), *AssetId));
		}
		else if (Asset->Intent == ENteAssetIntent::ExternalReference)
		{
			Result.Errors.Add(FString::Printf(TEXT("PackageManifest cannot package ExternalReference asset '%s'."), *AssetId));
		}
	}
	return Result;
}
}
