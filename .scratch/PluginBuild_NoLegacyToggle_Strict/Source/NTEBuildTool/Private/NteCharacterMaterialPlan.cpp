// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteCharacterMaterialPlan.h"

#include "NteEditorAssetUtils.h"
#include "NteCharacterModSpec.h"
#include "NteJsonFileUtils.h"
#include "NteMaterialInstanceTool.h"

#include "Dom/JsonValue.h"
#include "Misc/PackageName.h"

namespace NTEBuildTool::Character
{
namespace
{
constexpr const TCHAR* MainMeshId = TEXT("main");

bool IsMainMeshId(const FString& MeshId)
{
	return MeshId.IsEmpty() || MeshId.Equals(MainMeshId, ESearchCase::IgnoreCase);
}

const FNteCharacterAttachedMeshSpec* FindAttachedMeshById(const FNteCharacterModSpec& Spec, const FString& Id)
{
	for (const FNteCharacterAttachedMeshSpec& AttachedMesh : Spec.AttachedMeshes)
	{
		if (AttachedMesh.Id == Id)
		{
			return &AttachedMesh;
		}
	}
	return nullptr;
}

TSharedRef<FJsonObject> StringMapToJson(const TMap<FString, FString>& Map)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Pair : Map)
	{
		Object->SetStringField(Pair.Key, Pair.Value);
	}
	return Object;
}

FString SanitizeAssetNamePart(const FString& RawName)
{
	FString Result;
	for (const TCHAR Character : RawName)
	{
		Result.AppendChar(FChar::IsAlnum(Character) ? Character : TEXT('_'));
	}
	return Result.IsEmpty() ? TEXT("Material") : Result;
}

FString DeriveCharacterRootFromMeshPath(const FString& TargetMeshPath)
{
	FString MeshFolder = FPackageName::GetLongPackagePath(TargetMeshPath);
	if (MeshFolder.IsEmpty())
	{
		return FString();
	}

	const int32 ModIndex = MeshFolder.Find(TEXT("/mod/"), ESearchCase::IgnoreCase, ESearchDir::FromStart);
	if (ModIndex != INDEX_NONE)
	{
		MeshFolder = MeshFolder.Left(ModIndex);
	}
	return MeshFolder;
}

FString DeriveOutputMaterialPath(
	const FNteCharacterMaterialOperationPlanItem& Item,
	const FNteCharacterMaterialOperationSpec& Operation)
{
	FString OutputFolder;
	const FString CharacterRoot = DeriveCharacterRootFromMeshPath(Item.TargetMeshPath);
	if (!CharacterRoot.IsEmpty())
	{
		OutputFolder = CharacterRoot / TEXT("mod/Materials");
	}
	else
	{
		OutputFolder = NTEBuildTool::Material::DeriveModMaterialFolderFromParentPath(Item.ParentMaterialPath, TEXT("/Game"));
	}

	FString AssetName;
	if (!Item.SourceMaterialJson.IsEmpty())
	{
		AssetName = NTEBuildTool::Material::MakeModMaterialNameFromFModelJson(Item.SourceMaterialJson);
	}
	else if (!Item.ParentMaterialPath.IsEmpty())
	{
		AssetName = TEXT("MI_mod_") + FPackageName::GetShortName(Item.ParentMaterialPath);
	}
	else
	{
		AssetName = TEXT("MI_mod_") + SanitizeAssetNamePart(Operation.Id);
	}
	return NTEBuildTool::Editor::JoinAssetPath(OutputFolder, AssetName);
}

TMap<FString, FString> NormalizeSourceTextureOverrides(const TMap<FString, FString>& SourceTextureOverrides)
{
	TMap<FString, FString> Result;
	for (const TPair<FString, FString>& Pair : SourceTextureOverrides)
	{
		const FString SourceTexture = NTEBuildTool::Editor::NormalizeAssetPathForText(Pair.Key);
		const FString ReplacementTexture = NTEBuildTool::Editor::NormalizeAssetPathForText(Pair.Value);
		if (!SourceTexture.IsEmpty() && !ReplacementTexture.IsEmpty())
		{
			Result.Add(SourceTexture, ReplacementTexture);
		}
	}
	return Result;
}

bool IsGamePackageName(const FString& PackagePath)
{
	return PackagePath.StartsWith(TEXT("/Game/")) && !PackagePath.Contains(TEXT("."));
}

void AddPackageSeed(TArray<FString>& Seeds, const FString& PackagePath)
{
	if (IsGamePackageName(PackagePath))
	{
		Seeds.AddUnique(PackagePath);
	}
}

void ResolveDerivedMaterialPaths(
	FNteCharacterMaterialOperationPlanItem& Item,
	const FNteCharacterMaterialOperationSpec& Operation)
{
	if (Item.ParentMaterialPath.IsEmpty() && !Item.SourceMaterialJson.IsEmpty())
	{
		Item.ParentMaterialPath = NTEBuildTool::Material::DeriveParentMaterialPathFromFModelJson(Item.SourceMaterialJson);
		Item.bDerivedParentMaterialPath = !Item.ParentMaterialPath.IsEmpty();
	}

	if (Item.OutputMaterialPath.IsEmpty())
	{
		Item.OutputMaterialPath = DeriveOutputMaterialPath(Item, Operation);
		Item.bDerivedOutputMaterialPath = !Item.OutputMaterialPath.IsEmpty();
	}
}

void AddStringIfNotEmpty(const TSharedRef<FJsonObject>& Object, const TCHAR* FieldName, const FString& Value)
{
	if (!Value.IsEmpty())
	{
		Object->SetStringField(FieldName, Value);
	}
}

FNteCharacterMaterialOperationPlanItem BuildOperationPlanItem(
	const FNteCharacterModSpec& Spec,
	const FNteCharacterMaterialOperationSpec& Operation)
{
	FNteCharacterMaterialOperationPlanItem Item;
	Item.Id = Operation.Id;
	Item.TargetMeshId = IsMainMeshId(Operation.TargetMeshId) ? MainMeshId : Operation.TargetMeshId;
	Item.SlotIndex = Operation.SlotIndex;
	Item.SlotName = Operation.SlotName;
	Item.SourceMaterialJson = Operation.SourceMaterialJson;
	Item.ParentMaterialPath = NTEBuildTool::Editor::NormalizeAssetPathForText(Operation.ParentMaterialPath);
	Item.OutputMaterialPath = NTEBuildTool::Editor::NormalizeAssetPathForText(Operation.OutputMaterialPath);
	Item.SourceTextureOverrides = NormalizeSourceTextureOverrides(Operation.SourceTextureOverrides);
	Item.bAssignToSlot = Operation.bAssignToSlot;

	if (IsMainMeshId(Operation.TargetMeshId))
	{
		Item.TargetKind = TEXT("MainMesh");
		Item.TargetMeshPath = NTEBuildTool::Editor::NormalizeAssetPathForText(Spec.MainMeshPath);
	}
	else if (const FNteCharacterAttachedMeshSpec* AttachedMesh = FindAttachedMeshById(Spec, Operation.TargetMeshId))
	{
		Item.TargetKind = TEXT("AttachedMesh");
		Item.TargetMeshPath = NTEBuildTool::Editor::NormalizeAssetPathForText(AttachedMesh->MeshPath);
	}
	else
	{
		Item.TargetKind = TEXT("Unknown");
		Item.Errors.Add(FString::Printf(TEXT("Material operation targets unknown mesh id '%s'."), *Operation.TargetMeshId));
	}

	ResolveDerivedMaterialPaths(Item, Operation);

	if (Item.SourceMaterialJson.IsEmpty() && Item.ParentMaterialPath.IsEmpty())
	{
		Item.Errors.Add(TEXT("Material operation needs SourceMaterialJson or ParentMaterialPath."));
	}
	if (Item.OutputMaterialPath.IsEmpty())
	{
		Item.Errors.Add(TEXT("Material operation has no OutputMaterialPath and one could not be derived."));
	}
	else if (!IsGamePackageName(Item.OutputMaterialPath))
	{
		Item.Errors.Add(FString::Printf(TEXT("OutputMaterialPath must be a /Game package path: %s"), *Item.OutputMaterialPath));
	}
	else if (Item.bDerivedOutputMaterialPath)
	{
		Item.Warnings.Add(FString::Printf(TEXT("OutputMaterialPath was derived as %s."), *Item.OutputMaterialPath));
	}
	if (Item.bAssignToSlot)
	{
		if (Item.TargetMeshPath.IsEmpty())
		{
			Item.Errors.Add(TEXT("AssignToSlot is true but target mesh path is empty."));
		}
		if (Item.SlotIndex == INDEX_NONE)
		{
			Item.Errors.Add(TEXT("AssignToSlot is true but SlotIndex is missing."));
		}
	}
	if (!Item.SourceTextureOverrides.IsEmpty() && Item.SourceMaterialJson.IsEmpty())
	{
		Item.Warnings.Add(TEXT("SourceTextureOverrides are set but SourceMaterialJson is empty; source texture group expansion cannot run."));
	}

	return Item;
}

TSharedRef<FJsonObject> MaterialOperationPlanItemToJson(const FNteCharacterMaterialOperationPlanItem& Item)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	AddStringIfNotEmpty(Object, TEXT("Id"), Item.Id);
	AddStringIfNotEmpty(Object, TEXT("TargetMeshId"), Item.TargetMeshId);
	AddStringIfNotEmpty(Object, TEXT("TargetKind"), Item.TargetKind);
	AddStringIfNotEmpty(Object, TEXT("TargetMeshPath"), Item.TargetMeshPath);
	Object->SetNumberField(TEXT("SlotIndex"), Item.SlotIndex);
	AddStringIfNotEmpty(Object, TEXT("SlotName"), Item.SlotName);
	AddStringIfNotEmpty(Object, TEXT("SourceMaterialJson"), Item.SourceMaterialJson);
	AddStringIfNotEmpty(Object, TEXT("ParentMaterialPath"), Item.ParentMaterialPath);
	AddStringIfNotEmpty(Object, TEXT("OutputMaterialPath"), Item.OutputMaterialPath);
	Object->SetBoolField(TEXT("DerivedParentMaterialPath"), Item.bDerivedParentMaterialPath);
	Object->SetBoolField(TEXT("DerivedOutputMaterialPath"), Item.bDerivedOutputMaterialPath);
	Object->SetObjectField(TEXT("SourceTextureOverrides"), StringMapToJson(Item.SourceTextureOverrides));
	Object->SetBoolField(TEXT("AssignToSlot"), Item.bAssignToSlot);
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Item.Errors));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Item.Warnings));
	return Object;
}
}

FNteCharacterMaterialPlan BuildCharacterMaterialPlanFromSpec(const FNteCharacterModSpec& Spec)
{
	FNteCharacterMaterialPlan Plan;
	for (const FNteCharacterMaterialOperationSpec& Operation : Spec.MaterialOperations)
	{
		FNteCharacterMaterialOperationPlanItem Item = BuildOperationPlanItem(Spec, Operation);
		Plan.Errors.Append(Item.Errors);
		Plan.Warnings.Append(Item.Warnings);
		Plan.Operations.Add(MoveTemp(Item));
	}
	return Plan;
}

TSharedRef<FJsonObject> CharacterMaterialPlanToJson(const FNteCharacterMaterialPlan& Plan)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Operations;
	for (const FNteCharacterMaterialOperationPlanItem& Operation : Plan.Operations)
	{
		Operations.Add(MakeShared<FJsonValueObject>(MaterialOperationPlanItemToJson(Operation)));
	}
	Object->SetArrayField(TEXT("Operations"), Operations);
	Object->SetNumberField(TEXT("OperationCount"), Plan.Operations.Num());
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Plan.Errors));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Plan.Warnings));
	return Object;
}

TArray<FString> CollectCharacterMaterialPlanPackageSeeds(const FNteCharacterMaterialPlan& Plan)
{
	TArray<FString> Seeds;
	for (const FNteCharacterMaterialOperationPlanItem& Operation : Plan.Operations)
	{
		AddPackageSeed(Seeds, Operation.OutputMaterialPath);
		for (const TPair<FString, FString>& Pair : Operation.SourceTextureOverrides)
		{
			AddPackageSeed(Seeds, Pair.Value);
		}
	}
	Seeds.Sort();
	return Seeds;
}
}
