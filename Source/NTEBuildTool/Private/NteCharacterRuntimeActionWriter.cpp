// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteCharacterRuntimeActionWriter.h"

#include "NteEditorAssetUtils.h"
#include "NteJsonFileUtils.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/SaveGame.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace NTEBuildTool::Character
{
namespace
{
void AddError(FNteCharacterRuntimeActionWriteResult& Result, const FString& Error)
{
	Result.Errors.Add(Error);
}

void AddError(FNteCharacterRuntimeActionAssetWriteResult& Result, const FString& Error)
{
	Result.Errors.Add(Error);
}

void AddWarning(FNteCharacterRuntimeActionWriteResult& Result, const FString& Warning)
{
	Result.Warnings.Add(Warning);
}

void AddSeed(TArray<FString>& Seeds, FString Path)
{
	Path = NTEBuildTool::Editor::NormalizeAssetPathForText(Path);
	if (Path.StartsWith(TEXT("/Game/")) && !Path.Contains(TEXT(".")))
	{
		Seeds.AddUnique(Path);
	}
}

FString SanitizeVariableSuffix(const FString& RawValue)
{
	FString Result;
	for (const TCHAR Character : RawValue)
	{
		if (FChar::IsAlnum(Character))
		{
			Result.AppendChar(Character);
		}
		else
		{
			Result.AppendChar(TEXT('_'));
		}
	}
	while (Result.Contains(TEXT("__")))
	{
		Result.ReplaceInline(TEXT("__"), TEXT("_"));
	}
	Result.TrimStartAndEndInline();
	Result.RemoveFromStart(TEXT("_"));
	Result.RemoveFromEnd(TEXT("_"));
	return Result.IsEmpty() ? TEXT("Action") : Result;
}

FString MakeActionVariablePrefix(const FNteCharacterRuntimeActionPlanItem& Action)
{
	return TEXT("NTE_Action_") + SanitizeVariableSuffix(Action.Id);
}

FString JoinInts(const TArray<int32>& Values)
{
	TArray<FString> Parts;
	for (const int32 Value : Values)
	{
		Parts.Add(FString::FromInt(Value));
	}
	return FString::Join(Parts, TEXT(","));
}

FString PlanToCondensedJson(const FNteCharacterRuntimeActionPlan& Plan)
{
	FString JsonText;
	const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonText);
	FJsonSerializer::Serialize(CharacterRuntimeActionPlanToJson(Plan), Writer);
	return JsonText;
}

FEdGraphPinType MakePinType(const FName PinCategory)
{
	FEdGraphPinType PinType;
	PinType.PinCategory = PinCategory;
	return PinType;
}

FEdGraphPinType MakeStringPinType()
{
	return MakePinType(UEdGraphSchema_K2::PC_String);
}

FEdGraphPinType MakeBoolPinType()
{
	return MakePinType(UEdGraphSchema_K2::PC_Boolean);
}

FEdGraphPinType MakeIntPinType()
{
	return MakePinType(UEdGraphSchema_K2::PC_Int);
}

bool EnsureBlueprintVariable(UBlueprint& Blueprint, const FName VariableName, const FEdGraphPinType& PinType, const FString& DefaultValue, FNteCharacterRuntimeActionAssetWriteResult& Result)
{
	bool bChanged = false;
	bool bFound = false;
	for (FBPVariableDescription& Variable : Blueprint.NewVariables)
	{
		if (Variable.VarName != VariableName)
		{
			continue;
		}

		bFound = true;
		if (Variable.VarType != PinType)
		{
			Variable.VarType = PinType;
			bChanged = true;
			Result.Actions.Add(FString::Printf(TEXT("changed variable type %s"), *VariableName.ToString()));
		}
		if (Variable.DefaultValue != DefaultValue)
		{
			Variable.DefaultValue = DefaultValue;
			bChanged = true;
			Result.Actions.Add(FString::Printf(TEXT("updated variable default %s"), *VariableName.ToString()));
		}
		break;
	}

	if (!bFound)
	{
		FBlueprintEditorUtils::AddMemberVariable(&Blueprint, VariableName, PinType, DefaultValue);
		bChanged = true;
		Result.Actions.Add(FString::Printf(TEXT("added variable %s"), *VariableName.ToString()));
	}

	if (bChanged)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);
	}
	return bChanged;
}

bool SaveLoadedAssetPackage(UObject& Asset, FNteCharacterRuntimeActionWriteResult& Result, FString& OutError)
{
	UPackage* Package = Asset.GetPackage();
	if (!Package)
	{
		OutError = FString::Printf(TEXT("Asset has no package: %s"), *Asset.GetName());
		return false;
	}

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, &Asset, *PackageFilename, SaveArgs))
	{
		OutError = FString::Printf(TEXT("Could not save package: %s"), *PackageFilename);
		return false;
	}

	Result.SavedPackages.AddUnique(Package->GetName());
	return true;
}

UBlueprint* CreateOrLoadBlueprint(
	const FString& BlueprintPath,
	UClass* ParentClass,
	TSubclassOf<UBlueprint> BlueprintClass,
	TSubclassOf<UBlueprintGeneratedClass> GeneratedClass,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	if (BlueprintPath.IsEmpty())
	{
		AddError(AssetResult, TEXT("Blueprint path is empty."));
		return nullptr;
	}
	if (!BlueprintPath.StartsWith(TEXT("/Game/")) || BlueprintPath.Contains(TEXT(".")))
	{
		AddError(AssetResult, FString::Printf(TEXT("Blueprint path must be a /Game package path: %s"), *BlueprintPath));
		return nullptr;
	}

	const FString ObjectName = FPackageName::GetShortName(BlueprintPath);
	if (UPackage* ExistingPackage = FindPackage(nullptr, *BlueprintPath))
	{
		if (UObject* ExistingAsset = FindObject<UObject>(ExistingPackage, *ObjectName))
		{
			UBlueprint* ExistingBlueprint = Cast<UBlueprint>(ExistingAsset);
			if (!ExistingBlueprint)
			{
				AddError(AssetResult, FString::Printf(
					TEXT("Existing asset is %s, not a Blueprint: %s"),
					*ExistingAsset->GetClass()->GetPathName(),
					*BlueprintPath));
				return nullptr;
			}
			AssetResult.bUpdated = true;
			AssetResult.Actions.Add(FString::Printf(TEXT("loaded existing blueprint %s"), *BlueprintPath));
			return ExistingBlueprint;
		}
	}

	FString ExistingPackageFilename;
	if (FPackageName::DoesPackageExist(BlueprintPath, &ExistingPackageFilename))
	{
		if (UObject* ExistingAsset = NTEBuildTool::Editor::LoadAnyAssetByPath(BlueprintPath))
		{
			UBlueprint* ExistingBlueprint = Cast<UBlueprint>(ExistingAsset);
			if (!ExistingBlueprint)
			{
				AddError(AssetResult, FString::Printf(
					TEXT("Existing asset is %s, not a Blueprint: %s"),
					*ExistingAsset->GetClass()->GetPathName(),
					*BlueprintPath));
				return nullptr;
			}
			AssetResult.bUpdated = true;
			AssetResult.Actions.Add(FString::Printf(TEXT("loaded existing blueprint %s"), *BlueprintPath));
			return ExistingBlueprint;
		}

		AddError(AssetResult, FString::Printf(
			TEXT("Package exists but Blueprint asset could not be loaded: %s (%s)"),
			*BlueprintPath,
			*ExistingPackageFilename));
		return nullptr;
	}

	UPackage* Package = CreatePackage(*BlueprintPath);
	if (!Package)
	{
		AddError(AssetResult, FString::Printf(TEXT("Could not create package: %s"), *BlueprintPath));
		return nullptr;
	}

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		ParentClass,
		Package,
		FName(*FPackageName::GetShortName(BlueprintPath)),
		BPTYPE_Normal,
		BlueprintClass,
		GeneratedClass,
		NAME_None);
	if (!Blueprint)
	{
		AddError(AssetResult, FString::Printf(TEXT("Could not create blueprint: %s"), *BlueprintPath));
		return nullptr;
	}

	Blueprint->SetFlags(RF_Public | RF_Standalone);
	FAssetRegistryModule::AssetCreated(Blueprint);
	Package->MarkPackageDirty();
	AssetResult.bCreated = true;
	AssetResult.bUpdated = true;
	AssetResult.Actions.Add(FString::Printf(TEXT("created blueprint %s"), *BlueprintPath));
	return Blueprint;
}

UBlueprint* CreateOrLoadSaveGameBlueprint(const FString& BlueprintPath, FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	return CreateOrLoadBlueprint(
		BlueprintPath,
		USaveGame::StaticClass(),
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		AssetResult);
}

UBlueprint* CreateOrLoadWidgetBlueprint(const FString& BlueprintPath, FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	return CreateOrLoadBlueprint(
		BlueprintPath,
		UUserWidget::StaticClass(),
		UWidgetBlueprint::StaticClass(),
		UWidgetBlueprintGeneratedClass::StaticClass(),
		AssetResult);
}

UBlueprint* CreateOrLoadAnimBlueprint(const FString& BlueprintPath, FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	return CreateOrLoadBlueprint(
		BlueprintPath,
		UAnimInstance::StaticClass(),
		UAnimBlueprint::StaticClass(),
		UAnimBlueprintGeneratedClass::StaticClass(),
		AssetResult);
}

void AddPlanVariablesToBlueprint(
	UBlueprint& Blueprint,
	const FNteCharacterRuntimeActionPlan& Plan,
	const TArray<const FNteCharacterRuntimeActionPlanItem*>& Actions,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	EnsureBlueprintVariable(Blueprint, TEXT("NTE_CharacterActionPlanJson"), MakeStringPinType(), PlanToCondensedJson(Plan), AssetResult);
	EnsureBlueprintVariable(Blueprint, TEXT("NTE_CharacterActionCount"), MakeIntPinType(), FString::FromInt(Actions.Num()), AssetResult);
	for (const FNteCharacterRuntimeActionPlanItem* Action : Actions)
	{
		if (!Action)
		{
			continue;
		}

		const FString Prefix = MakeActionVariablePrefix(*Action);
		EnsureBlueprintVariable(Blueprint, FName(*(Prefix + TEXT("_Id"))), MakeStringPinType(), Action->Id, AssetResult);
		EnsureBlueprintVariable(Blueprint, FName(*(Prefix + TEXT("_Label"))), MakeStringPinType(), Action->Label, AssetResult);
		EnsureBlueprintVariable(Blueprint, FName(*(Prefix + TEXT("_Type"))), MakeStringPinType(), Action->ActionType, AssetResult);
		EnsureBlueprintVariable(Blueprint, FName(*(Prefix + TEXT("_TargetMeshId"))), MakeStringPinType(), Action->TargetMeshId, AssetResult);
		EnsureBlueprintVariable(Blueprint, FName(*(Prefix + TEXT("_MaterialSlots"))), MakeStringPinType(), JoinInts(Action->MaterialSlots), AssetResult);
		EnsureBlueprintVariable(Blueprint, FName(*(Prefix + TEXT("_Enabled"))), MakeBoolPinType(), Action->bDefaultEnabled ? TEXT("true") : TEXT("false"), AssetResult);
	}
}

void CompileAndSaveBlueprint(
	UBlueprint& Blueprint,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult,
	FNteCharacterRuntimeActionWriteResult& Result)
{
	FKismetEditorUtilities::CompileBlueprint(&Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	FString Error;
	if (!SaveLoadedAssetPackage(Blueprint, Result, Error))
	{
		AddError(AssetResult, Error);
		AddError(Result, Error);
		return;
	}
	AssetResult.Actions.Add(FString::Printf(TEXT("saved %s"), *Blueprint.GetPackage()->GetName()));
}

TArray<const FNteCharacterRuntimeActionPlanItem*> CollectAllActionPointers(const FNteCharacterRuntimeActionPlan& Plan)
{
	TArray<const FNteCharacterRuntimeActionPlanItem*> Actions;
	for (const FNteCharacterRuntimeActionPlanItem& Action : Plan.Actions)
	{
		Actions.Add(&Action);
	}
	return Actions;
}

TArray<const FNteCharacterRuntimeActionPlanItem*> CollectHostActionPointers(
	const FNteCharacterRuntimeActionPlan& Plan,
	const FNteCharacterRuntimeActionHostPlan& Host)
{
	TArray<const FNteCharacterRuntimeActionPlanItem*> Actions;
	for (const FString& ActionId : Host.ActionIds)
	{
		for (const FNteCharacterRuntimeActionPlanItem& Action : Plan.Actions)
		{
			if (Action.Id == ActionId)
			{
				Actions.Add(&Action);
				break;
			}
		}
	}
	return Actions;
}

void AppendAssetResult(FNteCharacterRuntimeActionWriteResult& Result, FNteCharacterRuntimeActionAssetWriteResult&& AssetResult)
{
	if (!AssetResult.Errors.IsEmpty())
	{
		Result.Errors.Append(AssetResult.Errors);
	}
	if (!AssetResult.Warnings.IsEmpty())
	{
		Result.Warnings.Append(AssetResult.Warnings);
	}
	Result.Assets.Add(MoveTemp(AssetResult));
}

TSharedRef<FJsonObject> AssetWriteResultToJson(const FNteCharacterRuntimeActionAssetWriteResult& Asset)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("AssetPath"), Asset.AssetPath);
	Object->SetStringField(TEXT("AssetKind"), Asset.AssetKind);
	Object->SetBoolField(TEXT("Created"), Asset.bCreated);
	Object->SetBoolField(TEXT("Updated"), Asset.bUpdated);
	Object->SetArrayField(TEXT("Actions"), Json::StringArrayToJsonValues(Asset.Actions));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Asset.Warnings));
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Asset.Errors));
	return Object;
}
}

TArray<FString> CollectCharacterRuntimeActionPlanPackageSeeds(const FNteCharacterRuntimeActionPlan& Plan)
{
	TArray<FString> Seeds;
	AddSeed(Seeds, Plan.WidgetBlueprintPath);
	AddSeed(Seeds, Plan.SaveGameBlueprintPath);
	for (const FNteCharacterRuntimeActionHostPlan& Host : Plan.Hosts)
	{
		AddSeed(Seeds, Host.AnimBlueprintPath);
	}
	Seeds.Sort();
	return Seeds;
}

FNteCharacterRuntimeActionWriteResult WriteCharacterRuntimeActions(const FNteCharacterRuntimeActionPlan& Plan)
{
	FNteCharacterRuntimeActionWriteResult Result;
	Result.RuntimeAssetRootPath = Plan.RuntimeAssetRootPath;
	Result.WidgetBlueprintPath = Plan.WidgetBlueprintPath;
	Result.SaveGameBlueprintPath = Plan.SaveGameBlueprintPath;

	if (Plan.Actions.IsEmpty())
	{
		AddWarning(Result, TEXT("CharacterModSpec has no RuntimeActions; nothing to write."));
		return Result;
	}
	if (!Plan.Errors.IsEmpty())
	{
		Result.Errors.Append(Plan.Errors);
		return Result;
	}

	const TArray<const FNteCharacterRuntimeActionPlanItem*> AllActions = CollectAllActionPointers(Plan);

	FNteCharacterRuntimeActionAssetWriteResult SaveGameResult;
	SaveGameResult.AssetPath = Plan.SaveGameBlueprintPath;
	SaveGameResult.AssetKind = TEXT("SaveGameBlueprint");
	if (UBlueprint* SaveGameBlueprint = CreateOrLoadSaveGameBlueprint(Plan.SaveGameBlueprintPath, SaveGameResult))
	{
		AddPlanVariablesToBlueprint(*SaveGameBlueprint, Plan, AllActions, SaveGameResult);
		CompileAndSaveBlueprint(*SaveGameBlueprint, SaveGameResult, Result);
	}
	AppendAssetResult(Result, MoveTemp(SaveGameResult));

	FNteCharacterRuntimeActionAssetWriteResult WidgetResult;
	WidgetResult.AssetPath = Plan.WidgetBlueprintPath;
	WidgetResult.AssetKind = TEXT("WidgetBlueprint");
	if (UBlueprint* WidgetBlueprint = CreateOrLoadWidgetBlueprint(Plan.WidgetBlueprintPath, WidgetResult))
	{
		AddPlanVariablesToBlueprint(*WidgetBlueprint, Plan, AllActions, WidgetResult);
		CompileAndSaveBlueprint(*WidgetBlueprint, WidgetResult, Result);
	}
	AppendAssetResult(Result, MoveTemp(WidgetResult));

	for (const FNteCharacterRuntimeActionHostPlan& Host : Plan.Hosts)
	{
		FNteCharacterRuntimeActionAssetWriteResult HostResult;
		HostResult.AssetPath = Host.AnimBlueprintPath;
		HostResult.AssetKind = FString::Printf(TEXT("%s:%s"), *Host.HostKind, *Host.MeshId);
		if (Host.AnimBlueprintPath.IsEmpty())
		{
			AddError(HostResult, FString::Printf(TEXT("Runtime action host '%s' has no AnimBlueprintPath."), *Host.MeshId));
			AppendAssetResult(Result, MoveTemp(HostResult));
			continue;
		}

		if (UBlueprint* HostBlueprint = CreateOrLoadAnimBlueprint(Host.AnimBlueprintPath, HostResult))
		{
			AddPlanVariablesToBlueprint(*HostBlueprint, Plan, CollectHostActionPointers(Plan, Host), HostResult);
			CompileAndSaveBlueprint(*HostBlueprint, HostResult, Result);
		}
		AppendAssetResult(Result, MoveTemp(HostResult));
	}

	return Result;
}

TSharedRef<FJsonObject> CharacterRuntimeActionWriteResultToJson(const FNteCharacterRuntimeActionWriteResult& Result)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("RuntimeAssetRootPath"), Result.RuntimeAssetRootPath);
	Object->SetStringField(TEXT("WidgetBlueprintPath"), Result.WidgetBlueprintPath);
	Object->SetStringField(TEXT("SaveGameBlueprintPath"), Result.SaveGameBlueprintPath);

	TArray<TSharedPtr<FJsonValue>> Assets;
	for (const FNteCharacterRuntimeActionAssetWriteResult& Asset : Result.Assets)
	{
		Assets.Add(MakeShared<FJsonValueObject>(AssetWriteResultToJson(Asset)));
	}
	Object->SetArrayField(TEXT("Assets"), Assets);
	Object->SetNumberField(TEXT("AssetCount"), Result.Assets.Num());
	Object->SetArrayField(TEXT("SavedPackages"), Json::StringArrayToJsonValues(Result.SavedPackages));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Result.Warnings));
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Result.Errors));
	return Object;
}
}
