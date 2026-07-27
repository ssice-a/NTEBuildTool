// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteCharacterRuntimeActionPlan.h"

#include "NteCharacterModSpec.h"
#include "NteEditorAssetUtils.h"
#include "NteJsonFileUtils.h"

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

bool IsFirstSliceRuntimeActionType(const FString& ActionType)
{
	return ActionType.Equals(TEXT("AttachedMeshVisibility"), ESearchCase::IgnoreCase)
		|| ActionType.Equals(TEXT("MaterialSlotVisibility"), ESearchCase::IgnoreCase);
}

FString NormalizeRuntimeRootPath(FString RootPath)
{
	RootPath = NTEBuildTool::Editor::NormalizeAssetPathForText(RootPath);
	if (!RootPath.StartsWith(TEXT("/Game/")))
	{
		return FString();
	}
	while (RootPath.EndsWith(TEXT("/")))
	{
		RootPath.LeftChopInline(1);
	}
	return RootPath;
}

FString GetAssetDirectory(FString AssetPath)
{
	AssetPath = NTEBuildTool::Editor::NormalizeAssetPathForText(AssetPath);
	if (!AssetPath.StartsWith(TEXT("/Game/")))
	{
		return FString();
	}
	return NormalizeRuntimeRootPath(FPackageName::GetLongPackagePath(AssetPath));
}

FString DeriveModRuntimeRootFromAssetPath(FString AssetPath)
{
	AssetPath = NTEBuildTool::Editor::NormalizeAssetPathForText(AssetPath);
	if (!AssetPath.StartsWith(TEXT("/Game/")))
	{
		return FString();
	}

	const FString ModMarker = TEXT("/mod/");
	const int32 ModMarkerIndex = AssetPath.Find(ModMarker, ESearchCase::IgnoreCase, ESearchDir::FromStart);
	if (ModMarkerIndex != INDEX_NONE)
	{
		return NormalizeRuntimeRootPath(AssetPath.Left(ModMarkerIndex) / TEXT("mod/Runtime"));
	}

	const FString AssetDirectory = GetAssetDirectory(AssetPath);
	return AssetDirectory.IsEmpty() ? FString() : NormalizeRuntimeRootPath(AssetDirectory / TEXT("mod/Runtime"));
}

FString DeriveRuntimeRootPath(const FNteCharacterModSpec& Spec)
{
	for (const FNteCharacterAttachedMeshSpec& AttachedMesh : Spec.AttachedMeshes)
	{
		const FString RuntimeAnimDirectory = GetAssetDirectory(AttachedMesh.RuntimeAnimBlueprintPath);
		if (!RuntimeAnimDirectory.IsEmpty())
		{
			return RuntimeAnimDirectory;
		}
	}

	const FString MainAnimDirectory = Spec.MainAnimBlueprintPath.Contains(TEXT("/mod/"), ESearchCase::IgnoreCase)
		? GetAssetDirectory(Spec.MainAnimBlueprintPath)
		: FString();
	if (!MainAnimDirectory.IsEmpty())
	{
		return MainAnimDirectory;
	}

	for (const FNteCharacterAttachedMeshSpec& AttachedMesh : Spec.AttachedMeshes)
	{
		const FString AttachedAnimDirectory = AttachedMesh.AnimBlueprintPath.Contains(TEXT("/mod/"), ESearchCase::IgnoreCase)
			? GetAssetDirectory(AttachedMesh.AnimBlueprintPath)
			: FString();
		if (!AttachedAnimDirectory.IsEmpty())
		{
			return AttachedAnimDirectory;
		}
	}

	for (const FNteCharacterAttachedMeshSpec& AttachedMesh : Spec.AttachedMeshes)
	{
		const FString AttachedMeshRuntimeRoot = DeriveModRuntimeRootFromAssetPath(AttachedMesh.MeshPath);
		if (!AttachedMeshRuntimeRoot.IsEmpty())
		{
			return AttachedMeshRuntimeRoot;
		}
	}

	const FString AppearanceRuntimeRoot = DeriveModRuntimeRootFromAssetPath(Spec.Appearance.PlayerAppearanceAssetPath);
	if (!AppearanceRuntimeRoot.IsEmpty())
	{
		return AppearanceRuntimeRoot;
	}

	const FString MainMeshRuntimeRoot = DeriveModRuntimeRootFromAssetPath(Spec.MainMeshPath);
	if (!MainMeshRuntimeRoot.IsEmpty())
	{
		return MainMeshRuntimeRoot;
	}

	return FString();
}

FString JoinRuntimeAssetPath(const FString& RuntimeRootPath, const FString& AssetName)
{
	if (RuntimeRootPath.IsEmpty() || AssetName.IsEmpty())
	{
		return FString();
	}
	return NTEBuildTool::Editor::JoinAssetPath(RuntimeRootPath, AssetName);
}

FString MakeRuntimeSaveSlotName(const FString& RuntimeRootPath)
{
	FString Suffix = RuntimeRootPath;
	Suffix.RemoveFromStart(TEXT("/Game/"));
	for (TCHAR& Character : Suffix)
	{
		if (!FChar::IsAlnum(Character))
		{
			Character = TEXT('_');
		}
	}
	while (Suffix.Contains(TEXT("__")))
	{
		Suffix.ReplaceInline(TEXT("__"), TEXT("_"));
	}
	Suffix.TrimStartAndEndInline();
	Suffix.RemoveFromStart(TEXT("_"));
	Suffix.RemoveFromEnd(TEXT("_"));
	return Suffix.IsEmpty()
		? TEXT("NTE_CharacterActionState")
		: FString::Printf(TEXT("NTE_CharacterActionState_%s"), *Suffix);
}

TArray<TSharedPtr<FJsonValue>> IntArrayToJsonValues(const TArray<int32>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	for (const int32 Value : Values)
	{
		Result.Add(MakeShared<FJsonValueNumber>(Value));
	}
	return Result;
}

TSharedRef<FJsonObject> ColorToJson(const FLinearColor& Value)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("R"), Value.R);
	Object->SetNumberField(TEXT("G"), Value.G);
	Object->SetNumberField(TEXT("B"), Value.B);
	Object->SetNumberField(TEXT("A"), Value.A);
	return Object;
}

void AddStringIfNotEmpty(const TSharedRef<FJsonObject>& Object, const TCHAR* FieldName, const FString& Value)
{
	if (!Value.IsEmpty())
	{
		Object->SetStringField(FieldName, Value);
	}
}

FString ResolveAttachedHostAnimBlueprintPath(const FNteCharacterAttachedMeshSpec& AttachedMesh)
{
	if (!AttachedMesh.RuntimeAnimBlueprintPath.IsEmpty())
	{
		return AttachedMesh.RuntimeAnimBlueprintPath;
	}
	return AttachedMesh.AnimBlueprintPath;
}

FNteCharacterRuntimeActionHostPlan& FindOrAddHost(
	FNteCharacterRuntimeActionPlan& Plan,
	const FString& MeshId,
	const FString& HostKind,
	const FString& MeshPath,
	const FString& AnimBlueprintPath)
{
	for (FNteCharacterRuntimeActionHostPlan& Host : Plan.Hosts)
	{
		if (Host.MeshId == MeshId)
		{
			if (Host.HostKind.IsEmpty())
			{
				Host.HostKind = HostKind;
			}
			if (Host.MeshPath.IsEmpty())
			{
				Host.MeshPath = MeshPath;
			}
			if (Host.AnimBlueprintPath.IsEmpty())
			{
				Host.AnimBlueprintPath = AnimBlueprintPath;
			}
			return Host;
		}
	}

	FNteCharacterRuntimeActionHostPlan& Host = Plan.Hosts.AddDefaulted_GetRef();
	Host.MeshId = MeshId;
	Host.HostKind = HostKind;
	Host.MeshPath = MeshPath;
	Host.AnimBlueprintPath = AnimBlueprintPath;
	return Host;
}

void AddUniqueStrings(TArray<FString>& Target, const TArray<FString>& Values)
{
	for (const FString& Value : Values)
	{
		if (!Value.IsEmpty())
		{
			Target.AddUnique(Value);
		}
	}
}

FNteCharacterRuntimeActionPlanItem BuildActionPlanItem(
	const FNteCharacterModSpec& Spec,
	const FNteCharacterRuntimeActionSpec& Action)
{
	FNteCharacterRuntimeActionPlanItem Item;
	Item.Id = Action.Id;
	Item.Label = Action.Label;
	Item.ActionType = Action.ActionType;
	Item.Hotkey = Action.Hotkey;
	Item.TargetMeshId = IsMainMeshId(Action.TargetMeshId) ? MainMeshId : Action.TargetMeshId;
	Item.HostMeshId = IsMainMeshId(Action.HostMeshId) ? MainMeshId : Action.HostMeshId;
	Item.TargetComponentTags = Action.TargetComponentTags;
	Item.MaterialSlots = Action.MaterialSlots;
	Item.MaterialPath = Action.MaterialPath;
	Item.ParameterName = Action.ParameterName;
	Item.ScalarValue = Action.ScalarValue;
	Item.VectorValue = Action.VectorValue;
	Item.MorphTargetName = Action.MorphTargetName;
	Item.MorphValue = Action.MorphValue;
	Item.bDefaultEnabled = Action.bDefaultEnabled;
	Item.bFirstSliceBlueprintSupported = IsFirstSliceRuntimeActionType(Action.ActionType);

	if (Item.TargetMeshId == MainMeshId)
	{
		Item.TargetKind = TEXT("MainMesh");
		Item.TargetMeshPath = Spec.MainMeshPath;
	}
	else if (const FNteCharacterAttachedMeshSpec* AttachedMesh = FindAttachedMeshById(Spec, Action.TargetMeshId))
	{
		Item.TargetKind = TEXT("AttachedMesh");
		Item.TargetMeshPath = AttachedMesh->MeshPath;
		AddUniqueStrings(Item.TargetComponentTags, AttachedMesh->MeshComponentOwnedTags);
	}
	else
	{
		Item.TargetKind = TEXT("Unknown");
		Item.Errors.Add(FString::Printf(TEXT("Runtime action targets unknown mesh id '%s'."), *Action.TargetMeshId));
	}

	if (Item.HostMeshId == MainMeshId)
	{
		Item.HostKind = TEXT("MainAnimBlueprint");
		Item.HostMeshPath = Spec.MainMeshPath;
		Item.HostAnimBlueprintPath = Spec.MainAnimBlueprintPath;
		if (Item.HostAnimBlueprintPath.IsEmpty())
		{
			Item.Warnings.Add(TEXT("Runtime action host is main mesh, but MainAnimBlueprintPath is empty."));
		}
	}
	else if (const FNteCharacterAttachedMeshSpec* HostMesh = FindAttachedMeshById(Spec, Item.HostMeshId))
	{
		Item.HostKind = TEXT("AttachedMeshAnimBlueprint");
		Item.HostMeshPath = HostMesh->MeshPath;
		Item.HostAnimBlueprintPath = ResolveAttachedHostAnimBlueprintPath(*HostMesh);
		if (Item.HostAnimBlueprintPath.IsEmpty())
		{
			Item.Warnings.Add(FString::Printf(TEXT("Runtime action host attached mesh '%s' has no RuntimeAnimBlueprintPath or AnimBlueprintPath."), *HostMesh->Id));
		}
	}
	else
	{
		Item.HostKind = TEXT("Unknown");
		Item.Errors.Add(FString::Printf(TEXT("Runtime action uses unknown host mesh id '%s'."), *Action.HostMeshId));
	}

	Item.TargetLookupMode = Item.HostMeshId == Item.TargetMeshId ? TEXT("OwningComponent") : TEXT("OwnerComponentByTags");
	if (Item.TargetLookupMode == TEXT("OwnerComponentByTags") && Item.TargetComponentTags.IsEmpty())
	{
		Item.Errors.Add(FString::Printf(
			TEXT("Runtime action needs TargetComponentTags or target attached mesh MeshComponentOwnedTags because host '%s' differs from target '%s'."),
			*Item.HostMeshId,
			*Item.TargetMeshId));
	}

	if (!Item.bFirstSliceBlueprintSupported)
	{
		Item.Warnings.Add(FString::Printf(
			TEXT("ActionType '%s' is schema-supported but not part of the first runtime Blueprint generation slice."),
			*Item.ActionType));
	}
	return Item;
}

TSharedRef<FJsonObject> ActionPlanItemToJson(const FNteCharacterRuntimeActionPlanItem& Item)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	AddStringIfNotEmpty(Object, TEXT("Id"), Item.Id);
	AddStringIfNotEmpty(Object, TEXT("Label"), Item.Label);
	AddStringIfNotEmpty(Object, TEXT("ActionType"), Item.ActionType);
	AddStringIfNotEmpty(Object, TEXT("Hotkey"), Item.Hotkey);
	AddStringIfNotEmpty(Object, TEXT("TargetMeshId"), Item.TargetMeshId);
	AddStringIfNotEmpty(Object, TEXT("TargetKind"), Item.TargetKind);
	AddStringIfNotEmpty(Object, TEXT("TargetMeshPath"), Item.TargetMeshPath);
	AddStringIfNotEmpty(Object, TEXT("HostMeshId"), Item.HostMeshId);
	AddStringIfNotEmpty(Object, TEXT("HostKind"), Item.HostKind);
	AddStringIfNotEmpty(Object, TEXT("HostMeshPath"), Item.HostMeshPath);
	AddStringIfNotEmpty(Object, TEXT("HostAnimBlueprintPath"), Item.HostAnimBlueprintPath);
	AddStringIfNotEmpty(Object, TEXT("TargetLookupMode"), Item.TargetLookupMode);
	Object->SetArrayField(TEXT("TargetComponentTags"), Json::StringArrayToJsonValues(Item.TargetComponentTags));
	Object->SetArrayField(TEXT("MaterialSlots"), IntArrayToJsonValues(Item.MaterialSlots));
	AddStringIfNotEmpty(Object, TEXT("MaterialPath"), Item.MaterialPath);
	AddStringIfNotEmpty(Object, TEXT("ParameterName"), Item.ParameterName);
	Object->SetNumberField(TEXT("ScalarValue"), Item.ScalarValue);
	Object->SetObjectField(TEXT("VectorValue"), ColorToJson(Item.VectorValue));
	AddStringIfNotEmpty(Object, TEXT("MorphTargetName"), Item.MorphTargetName);
	Object->SetNumberField(TEXT("MorphValue"), Item.MorphValue);
	Object->SetBoolField(TEXT("DefaultEnabled"), Item.bDefaultEnabled);
	Object->SetBoolField(TEXT("FirstSliceBlueprintSupported"), Item.bFirstSliceBlueprintSupported);
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Item.Errors));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Item.Warnings));
	return Object;
}

TSharedRef<FJsonObject> HostPlanToJson(const FNteCharacterRuntimeActionHostPlan& Host)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	AddStringIfNotEmpty(Object, TEXT("MeshId"), Host.MeshId);
	AddStringIfNotEmpty(Object, TEXT("HostKind"), Host.HostKind);
	AddStringIfNotEmpty(Object, TEXT("MeshPath"), Host.MeshPath);
	AddStringIfNotEmpty(Object, TEXT("AnimBlueprintPath"), Host.AnimBlueprintPath);
	Object->SetArrayField(TEXT("ActionIds"), Json::StringArrayToJsonValues(Host.ActionIds));
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Host.Errors));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Host.Warnings));
	return Object;
}

FNteCharacterRuntimeUiPlan BuildRuntimeUiPlan(const FNteCharacterModSpec& Spec)
{
	FNteCharacterRuntimeUiPlan RuntimeUi;
	RuntimeUi.bEnableUi = Spec.RuntimeUi.bEnableUi;
	RuntimeUi.ToggleUiHotkey = Spec.RuntimeUi.ToggleUiHotkey;
	RuntimeUi.Title = Spec.RuntimeUi.Title;
	RuntimeUi.bDefaultVisible = Spec.RuntimeUi.bDefaultVisible;
	RuntimeUi.FontPath = Spec.RuntimeUi.FontPath;
	RuntimeUi.BodyFontTypeface = Spec.RuntimeUi.BodyFontTypeface;
	RuntimeUi.TitleFontTypeface = Spec.RuntimeUi.TitleFontTypeface;
	RuntimeUi.BodyFontSize = Spec.RuntimeUi.BodyFontSize;
	RuntimeUi.TitleFontSize = Spec.RuntimeUi.TitleFontSize;
	if (RuntimeUi.bEnableUi && Spec.RuntimeActions.IsEmpty())
	{
		RuntimeUi.Warnings.Add(TEXT("Runtime UI is enabled but CharacterModSpec.RuntimeActions is empty; the generated UI would have no action buttons."));
	}
	if (RuntimeUi.bEnableUi && !RuntimeUi.FontPath.StartsWith(TEXT("/Game/")))
	{
		RuntimeUi.Errors.Add(FString::Printf(
			TEXT("RuntimeUi.FontPath must be a /Game package path: %s"),
			*RuntimeUi.FontPath));
	}
	return RuntimeUi;
}

TSharedRef<FJsonObject> RuntimeUiPlanToJson(const FNteCharacterRuntimeUiPlan& RuntimeUi)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetBoolField(TEXT("EnableUi"), RuntimeUi.bEnableUi);
	AddStringIfNotEmpty(Object, TEXT("ToggleUiHotkey"), RuntimeUi.ToggleUiHotkey);
	AddStringIfNotEmpty(Object, TEXT("Title"), RuntimeUi.Title);
	Object->SetBoolField(TEXT("DefaultVisible"), RuntimeUi.bDefaultVisible);
	AddStringIfNotEmpty(Object, TEXT("FontPath"), RuntimeUi.FontPath);
	AddStringIfNotEmpty(Object, TEXT("BodyFontTypeface"), RuntimeUi.BodyFontTypeface);
	AddStringIfNotEmpty(Object, TEXT("TitleFontTypeface"), RuntimeUi.TitleFontTypeface);
	Object->SetNumberField(TEXT("BodyFontSize"), RuntimeUi.BodyFontSize);
	Object->SetNumberField(TEXT("TitleFontSize"), RuntimeUi.TitleFontSize);
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(RuntimeUi.Errors));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(RuntimeUi.Warnings));
	return Object;
}
}

FNteCharacterRuntimeActionPlan BuildCharacterRuntimeActionPlanFromSpec(const FNteCharacterModSpec& Spec)
{
	FNteCharacterRuntimeActionPlan Plan;
	Plan.RuntimeAssetRootPath = DeriveRuntimeRootPath(Spec);
	Plan.RuntimeUi = BuildRuntimeUiPlan(Spec);

	if (Spec.RuntimeActions.IsEmpty() && !Plan.RuntimeUi.bEnableUi)
	{
		return Plan;
	}

	if (Plan.RuntimeAssetRootPath.IsEmpty())
	{
		Plan.Errors.Add(TEXT("Could not derive a /Game runtime asset root path from PlayerAppearanceAssetPath or MainMeshPath."));
	}
	if (!Plan.RuntimeUi.Errors.IsEmpty())
	{
		Plan.Errors.Append(Plan.RuntimeUi.Errors);
	}
	if (!Plan.RuntimeUi.Warnings.IsEmpty())
	{
		Plan.Warnings.Append(Plan.RuntimeUi.Warnings);
	}

	// A runtime action always needs persisted state. The Widget is only an
	// optional presentation asset, so avoid creating an unused empty package.
	Plan.SaveGameBlueprintPath = JoinRuntimeAssetPath(Plan.RuntimeAssetRootPath, TEXT("BP_NTE_CharacterActionSaveGame"));
	Plan.SaveSlotName = MakeRuntimeSaveSlotName(Plan.RuntimeAssetRootPath);
	if (Plan.RuntimeUi.bEnableUi)
	{
		Plan.WidgetBlueprintPath = JoinRuntimeAssetPath(Plan.RuntimeAssetRootPath, TEXT("WBP_NTE_CharacterActions"));
	}

	for (const FNteCharacterRuntimeActionSpec& Action : Spec.RuntimeActions)
	{
		FNteCharacterRuntimeActionPlanItem Item = BuildActionPlanItem(Spec, Action);
		if (!Item.HostMeshId.IsEmpty())
		{
			FNteCharacterRuntimeActionHostPlan& Host = FindOrAddHost(Plan, Item.HostMeshId, Item.HostKind, Item.HostMeshPath, Item.HostAnimBlueprintPath);
			Host.ActionIds.AddUnique(Item.Id);
			if (Item.HostAnimBlueprintPath.IsEmpty())
			{
				Host.Warnings.AddUnique(FString::Printf(TEXT("Action '%s' has no host AnimBlueprint path yet."), *Item.Id));
			}
		}
		if (!Item.Errors.IsEmpty())
		{
			Plan.Errors.Append(Item.Errors);
		}
		if (!Item.Warnings.IsEmpty())
		{
			Plan.Warnings.Append(Item.Warnings);
		}
		Plan.Actions.Add(MoveTemp(Item));
	}

	return Plan;
}

TSharedRef<FJsonObject> CharacterRuntimeActionPlanToJson(const FNteCharacterRuntimeActionPlan& Plan)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	AddStringIfNotEmpty(Object, TEXT("RuntimeAssetRootPath"), Plan.RuntimeAssetRootPath);
	AddStringIfNotEmpty(Object, TEXT("WidgetBlueprintPath"), Plan.WidgetBlueprintPath);
	AddStringIfNotEmpty(Object, TEXT("SaveGameBlueprintPath"), Plan.SaveGameBlueprintPath);
	AddStringIfNotEmpty(Object, TEXT("SaveSlotName"), Plan.SaveSlotName);
	Object->SetObjectField(TEXT("RuntimeUi"), RuntimeUiPlanToJson(Plan.RuntimeUi));

	TArray<TSharedPtr<FJsonValue>> Hosts;
	for (const FNteCharacterRuntimeActionHostPlan& Host : Plan.Hosts)
	{
		Hosts.Add(MakeShared<FJsonValueObject>(HostPlanToJson(Host)));
	}
	Object->SetArrayField(TEXT("Hosts"), Hosts);
	Object->SetNumberField(TEXT("HostCount"), Plan.Hosts.Num());

	TArray<TSharedPtr<FJsonValue>> Actions;
	for (const FNteCharacterRuntimeActionPlanItem& Action : Plan.Actions)
	{
		Actions.Add(MakeShared<FJsonValueObject>(ActionPlanItemToJson(Action)));
	}
	Object->SetArrayField(TEXT("Actions"), Actions);
	Object->SetNumberField(TEXT("ActionCount"), Plan.Actions.Num());
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Plan.Errors));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Plan.Warnings));
	return Object;
}
}
