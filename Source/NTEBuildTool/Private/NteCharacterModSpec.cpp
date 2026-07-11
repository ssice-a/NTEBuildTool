// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteCharacterModSpec.h"

#include "NteJsonFileUtils.h"

#include "Dom/JsonValue.h"
#include "InputCoreTypes.h"
#include "Misc/PackageName.h"

namespace NTEBuildTool::Character
{
namespace
{
void SetStringIfNotEmpty(const TSharedRef<FJsonObject>& Object, const TCHAR* FieldName, const FString& Value)
{
	if (!Value.IsEmpty())
	{
		Object->SetStringField(FieldName, Value);
	}
}

FString GetStringField(const FJsonObject& Object, const TCHAR* FieldName)
{
	FString Value;
	Object.TryGetStringField(FieldName, Value);
	return Value;
}

bool GetBoolField(const FJsonObject& Object, const TCHAR* FieldName, const bool DefaultValue)
{
	bool Value = DefaultValue;
	Object.TryGetBoolField(FieldName, Value);
	return Value;
}

int32 GetIntField(const FJsonObject& Object, const TCHAR* FieldName, const int32 DefaultValue = INDEX_NONE)
{
	double Value = static_cast<double>(DefaultValue);
	return Object.TryGetNumberField(FieldName, Value) ? static_cast<int32>(Value) : DefaultValue;
}

float GetFloatField(const FJsonObject& Object, const TCHAR* FieldName, const float DefaultValue = 0.0f)
{
	double Value = static_cast<double>(DefaultValue);
	return Object.TryGetNumberField(FieldName, Value) ? static_cast<float>(Value) : DefaultValue;
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

TArray<int32> GetIntArrayField(const FJsonObject& Object, const TCHAR* FieldName)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.TryGetArrayField(FieldName, Values) || !Values)
	{
		return {};
	}

	TArray<int32> Result;
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (Value.IsValid())
		{
			Result.Add(static_cast<int32>(Value->AsNumber()));
		}
	}
	return Result;
}

TArray<FString> GetStringArrayField(const FJsonObject& Object, const TCHAR* FieldName)
{
	return Json::GetStringArrayAny(Object, FieldName);
}

TSharedRef<FJsonObject> VectorToJson(const FVector& Value)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("X"), Value.X);
	Object->SetNumberField(TEXT("Y"), Value.Y);
	Object->SetNumberField(TEXT("Z"), Value.Z);
	return Object;
}

FVector VectorFromJson(const FJsonObject& Object, const FVector& DefaultValue)
{
	return FVector(
		GetFloatField(Object, TEXT("X"), DefaultValue.X),
		GetFloatField(Object, TEXT("Y"), DefaultValue.Y),
		GetFloatField(Object, TEXT("Z"), DefaultValue.Z));
}

TSharedRef<FJsonObject> RotatorToJson(const FRotator& Value)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("Pitch"), Value.Pitch);
	Object->SetNumberField(TEXT("Yaw"), Value.Yaw);
	Object->SetNumberField(TEXT("Roll"), Value.Roll);
	return Object;
}

FRotator RotatorFromJson(const FJsonObject& Object, const FRotator& DefaultValue)
{
	return FRotator(
		GetFloatField(Object, TEXT("Pitch"), DefaultValue.Pitch),
		GetFloatField(Object, TEXT("Yaw"), DefaultValue.Yaw),
		GetFloatField(Object, TEXT("Roll"), DefaultValue.Roll));
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

FLinearColor ColorFromJson(const FJsonObject& Object, const FLinearColor& DefaultValue)
{
	return FLinearColor(
		GetFloatField(Object, TEXT("R"), DefaultValue.R),
		GetFloatField(Object, TEXT("G"), DefaultValue.G),
		GetFloatField(Object, TEXT("B"), DefaultValue.B),
		GetFloatField(Object, TEXT("A"), DefaultValue.A));
}

void ReadObjectField(const FJsonObject& Object, const TCHAR* FieldName, TFunctionRef<void(const FJsonObject&)> Reader)
{
	const TSharedPtr<FJsonObject>* Child = nullptr;
	if (Object.TryGetObjectField(FieldName, Child) && Child && Child->IsValid())
	{
		Reader(*Child->Get());
	}
}

void ReadObjectArrayField(const FJsonObject& Object, const TCHAR* FieldName, TFunctionRef<void(const FJsonObject&)> Reader)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.TryGetArrayField(FieldName, Values) || !Values)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (!Value.IsValid())
		{
			continue;
		}

		const TSharedPtr<FJsonObject> Child = Value->AsObject();
		if (Child.IsValid())
		{
			Reader(*Child);
		}
	}
}

TSharedRef<FJsonObject> AppearanceToJson(const FNteCharacterAppearanceTarget& Appearance)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	SetStringIfNotEmpty(Object, TEXT("AppearanceRowName"), Appearance.AppearanceRowName);
	SetStringIfNotEmpty(Object, TEXT("PlayerAppearanceAssetPath"), Appearance.PlayerAppearanceAssetPath);
	SetStringIfNotEmpty(Object, TEXT("UIActorClassPath"), Appearance.UIActorClassPath);
	return Object;
}

FNteCharacterAppearanceTarget AppearanceFromJson(const FJsonObject& Object)
{
	FNteCharacterAppearanceTarget Appearance;
	Appearance.AppearanceRowName = GetStringField(Object, TEXT("AppearanceRowName"));
	Appearance.PlayerAppearanceAssetPath = GetStringField(Object, TEXT("PlayerAppearanceAssetPath"));
	Appearance.UIActorClassPath = GetStringField(Object, TEXT("UIActorClassPath"));
	return Appearance;
}

TSharedRef<FJsonObject> AttachedMeshToJson(const FNteCharacterAttachedMeshSpec& AttachedMesh)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	SetStringIfNotEmpty(Object, TEXT("Id"), AttachedMesh.Id);
	SetStringIfNotEmpty(Object, TEXT("Label"), AttachedMesh.Label);
	SetStringIfNotEmpty(Object, TEXT("MeshPath"), AttachedMesh.MeshPath);
	SetStringIfNotEmpty(Object, TEXT("AnimBlueprintPath"), AttachedMesh.AnimBlueprintPath);
	SetStringIfNotEmpty(Object, TEXT("MobileAnimBlueprintPath"), AttachedMesh.MobileAnimBlueprintPath);
	SetStringIfNotEmpty(Object, TEXT("UIAnimBlueprintPath"), AttachedMesh.UIAnimBlueprintPath);
	SetStringIfNotEmpty(Object, TEXT("RuntimeAnimBlueprintPath"), AttachedMesh.RuntimeAnimBlueprintPath);
	SetStringIfNotEmpty(Object, TEXT("SocketName"), AttachedMesh.SocketName);
	Object->SetArrayField(TEXT("MeshComponentOwnedTags"), Json::StringArrayToJsonValues(AttachedMesh.MeshComponentOwnedTags));
	Object->SetObjectField(TEXT("RelativeLocation"), VectorToJson(AttachedMesh.RelativeLocation));
	Object->SetObjectField(TEXT("RelativeRotation"), RotatorToJson(AttachedMesh.RelativeRotation));
	Object->SetObjectField(TEXT("RelativeScale"), VectorToJson(AttachedMesh.RelativeScale));
	SetStringIfNotEmpty(Object, TEXT("KawaiiPresetId"), AttachedMesh.KawaiiPresetId);
	Object->SetBoolField(TEXT("SyncToUIShow"), AttachedMesh.bSyncToUIShow);
	Object->SetBoolField(TEXT("EnableRuntimeActions"), AttachedMesh.bEnableRuntimeActions);
	return Object;
}

FNteCharacterAttachedMeshSpec AttachedMeshFromJson(const FJsonObject& Object)
{
	FNteCharacterAttachedMeshSpec AttachedMesh;
	AttachedMesh.Id = GetStringField(Object, TEXT("Id"));
	AttachedMesh.Label = GetStringField(Object, TEXT("Label"));
	AttachedMesh.MeshPath = GetStringField(Object, TEXT("MeshPath"));
	AttachedMesh.AnimBlueprintPath = GetStringField(Object, TEXT("AnimBlueprintPath"));
	AttachedMesh.MobileAnimBlueprintPath = GetStringField(Object, TEXT("MobileAnimBlueprintPath"));
	AttachedMesh.UIAnimBlueprintPath = GetStringField(Object, TEXT("UIAnimBlueprintPath"));
	AttachedMesh.RuntimeAnimBlueprintPath = GetStringField(Object, TEXT("RuntimeAnimBlueprintPath"));
	AttachedMesh.SocketName = GetStringField(Object, TEXT("SocketName"));
	AttachedMesh.MeshComponentOwnedTags = GetStringArrayField(Object, TEXT("MeshComponentOwnedTags"));
	ReadObjectField(Object, TEXT("RelativeLocation"), [&AttachedMesh](const FJsonObject& Child)
	{
		AttachedMesh.RelativeLocation = VectorFromJson(Child, FVector::ZeroVector);
	});
	ReadObjectField(Object, TEXT("RelativeRotation"), [&AttachedMesh](const FJsonObject& Child)
	{
		AttachedMesh.RelativeRotation = RotatorFromJson(Child, FRotator::ZeroRotator);
	});
	ReadObjectField(Object, TEXT("RelativeScale"), [&AttachedMesh](const FJsonObject& Child)
	{
		AttachedMesh.RelativeScale = VectorFromJson(Child, FVector::OneVector);
	});
	AttachedMesh.KawaiiPresetId = GetStringField(Object, TEXT("KawaiiPresetId"));
	AttachedMesh.bSyncToUIShow = GetBoolField(Object, TEXT("SyncToUIShow"), true);
	AttachedMesh.bEnableRuntimeActions = GetBoolField(Object, TEXT("EnableRuntimeActions"), false);
	return AttachedMesh;
}

TSharedRef<FJsonObject> StringMapToJson(const TMap<FString, FString>& Map)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Pair : Map)
	{
		Object->SetStringField(Pair.Key, Pair.Value);
	}
	return Object;
}

TMap<FString, FString> StringMapFromJson(const FJsonObject& Object)
{
	TMap<FString, FString> Result;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object.Values)
	{
		if (Pair.Value.IsValid())
		{
			Result.Add(Pair.Key, Pair.Value->AsString());
		}
	}
	return Result;
}

TSharedRef<FJsonObject> MaterialOperationToJson(const FNteCharacterMaterialOperationSpec& Operation)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	SetStringIfNotEmpty(Object, TEXT("Id"), Operation.Id);
	SetStringIfNotEmpty(Object, TEXT("TargetMeshId"), Operation.TargetMeshId);
	Object->SetNumberField(TEXT("SlotIndex"), Operation.SlotIndex);
	SetStringIfNotEmpty(Object, TEXT("SlotName"), Operation.SlotName);
	SetStringIfNotEmpty(Object, TEXT("SourceMaterialJson"), Operation.SourceMaterialJson);
	SetStringIfNotEmpty(Object, TEXT("ParentMaterialPath"), Operation.ParentMaterialPath);
	SetStringIfNotEmpty(Object, TEXT("OutputMaterialPath"), Operation.OutputMaterialPath);
	Object->SetObjectField(TEXT("SourceTextureOverrides"), StringMapToJson(Operation.SourceTextureOverrides));
	Object->SetBoolField(TEXT("AssignToSlot"), Operation.bAssignToSlot);
	return Object;
}

FNteCharacterMaterialOperationSpec MaterialOperationFromJson(const FJsonObject& Object)
{
	FNteCharacterMaterialOperationSpec Operation;
	Operation.Id = GetStringField(Object, TEXT("Id"));
	Operation.TargetMeshId = GetStringField(Object, TEXT("TargetMeshId"));
	Operation.SlotIndex = GetIntField(Object, TEXT("SlotIndex"));
	Operation.SlotName = GetStringField(Object, TEXT("SlotName"));
	Operation.SourceMaterialJson = GetStringField(Object, TEXT("SourceMaterialJson"));
	Operation.ParentMaterialPath = GetStringField(Object, TEXT("ParentMaterialPath"));
	Operation.OutputMaterialPath = GetStringField(Object, TEXT("OutputMaterialPath"));
	ReadObjectField(Object, TEXT("SourceTextureOverrides"), [&Operation](const FJsonObject& Child)
	{
		Operation.SourceTextureOverrides = StringMapFromJson(Child);
	});
	Operation.bAssignToSlot = GetBoolField(Object, TEXT("AssignToSlot"), true);
	return Operation;
}

TSharedRef<FJsonObject> RuntimeActionToJson(const FNteCharacterRuntimeActionSpec& Action)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	SetStringIfNotEmpty(Object, TEXT("Id"), Action.Id);
	SetStringIfNotEmpty(Object, TEXT("Label"), Action.Label);
	SetStringIfNotEmpty(Object, TEXT("Hotkey"), Action.Hotkey);
	SetStringIfNotEmpty(Object, TEXT("TargetMeshId"), Action.TargetMeshId);
	SetStringIfNotEmpty(Object, TEXT("ActionType"), Action.ActionType);
	Object->SetArrayField(TEXT("TargetComponentTags"), Json::StringArrayToJsonValues(Action.TargetComponentTags));
	Object->SetArrayField(TEXT("MaterialSlots"), IntArrayToJsonValues(Action.MaterialSlots));
	SetStringIfNotEmpty(Object, TEXT("MaterialPath"), Action.MaterialPath);
	SetStringIfNotEmpty(Object, TEXT("ParameterName"), Action.ParameterName);
	Object->SetNumberField(TEXT("ScalarValue"), Action.ScalarValue);
	Object->SetObjectField(TEXT("VectorValue"), ColorToJson(Action.VectorValue));
	SetStringIfNotEmpty(Object, TEXT("MorphTargetName"), Action.MorphTargetName);
	Object->SetNumberField(TEXT("MorphValue"), Action.MorphValue);
	Object->SetBoolField(TEXT("DefaultEnabled"), Action.bDefaultEnabled);
	return Object;
}

FNteCharacterRuntimeActionSpec RuntimeActionFromJson(const FJsonObject& Object)
{
	FNteCharacterRuntimeActionSpec Action;
	Action.Id = GetStringField(Object, TEXT("Id"));
	Action.Label = GetStringField(Object, TEXT("Label"));
	Action.Hotkey = GetStringField(Object, TEXT("Hotkey"));
	Action.TargetMeshId = GetStringField(Object, TEXT("TargetMeshId"));
	Action.ActionType = GetStringField(Object, TEXT("ActionType"));
	if (Action.ActionType.IsEmpty())
	{
		Action.ActionType = TEXT("MaterialSlotVisibility");
	}
	Action.TargetComponentTags = GetStringArrayField(Object, TEXT("TargetComponentTags"));
	Action.MaterialSlots = GetIntArrayField(Object, TEXT("MaterialSlots"));
	Action.MaterialPath = GetStringField(Object, TEXT("MaterialPath"));
	Action.ParameterName = GetStringField(Object, TEXT("ParameterName"));
	Action.ScalarValue = GetFloatField(Object, TEXT("ScalarValue"));
	ReadObjectField(Object, TEXT("VectorValue"), [&Action](const FJsonObject& Child)
	{
		Action.VectorValue = ColorFromJson(Child, FLinearColor::White);
	});
	Action.MorphTargetName = GetStringField(Object, TEXT("MorphTargetName"));
	Action.MorphValue = GetFloatField(Object, TEXT("MorphValue"));
	Action.bDefaultEnabled = GetBoolField(Object, TEXT("DefaultEnabled"), true);
	return Action;
}

TSharedRef<FJsonObject> KawaiiPresetToJson(const FNteCharacterKawaiiPresetSpec& Preset)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	SetStringIfNotEmpty(Object, TEXT("Id"), Preset.Id);
	SetStringIfNotEmpty(Object, TEXT("Label"), Preset.Label);
	SetStringIfNotEmpty(Object, TEXT("TargetMeshId"), Preset.TargetMeshId);
	SetStringIfNotEmpty(Object, TEXT("SourceAnimBlueprintJson"), Preset.SourceAnimBlueprintJson);
	SetStringIfNotEmpty(Object, TEXT("RootBone"), Preset.RootBone);
	Object->SetArrayField(TEXT("AdditionalRootBones"), Json::StringArrayToJsonValues(Preset.AdditionalRootBones));
	Object->SetArrayField(TEXT("ExcludeBones"), Json::StringArrayToJsonValues(Preset.ExcludeBones));
	return Object;
}

FNteCharacterKawaiiPresetSpec KawaiiPresetFromJson(const FJsonObject& Object)
{
	FNteCharacterKawaiiPresetSpec Preset;
	Preset.Id = GetStringField(Object, TEXT("Id"));
	Preset.Label = GetStringField(Object, TEXT("Label"));
	Preset.TargetMeshId = GetStringField(Object, TEXT("TargetMeshId"));
	Preset.SourceAnimBlueprintJson = GetStringField(Object, TEXT("SourceAnimBlueprintJson"));
	Preset.RootBone = GetStringField(Object, TEXT("RootBone"));
	Preset.AdditionalRootBones = GetStringArrayField(Object, TEXT("AdditionalRootBones"));
	Preset.ExcludeBones = GetStringArrayField(Object, TEXT("ExcludeBones"));
	return Preset;
}

TSharedRef<FJsonObject> PackageSpecToJson(const FNteCharacterPackageSpec& Package)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	SetStringIfNotEmpty(Object, TEXT("ModName"), Package.ModName);
	SetStringIfNotEmpty(Object, TEXT("ModsDir"), Package.ModsDir);
	SetStringIfNotEmpty(Object, TEXT("JobFilename"), Package.JobFilename);
	Object->SetBoolField(TEXT("BuildAfterCreate"), Package.bBuildAfterCreate);
	return Object;
}

FNteCharacterPackageSpec PackageSpecFromJson(const FJsonObject& Object)
{
	FNteCharacterPackageSpec Package;
	Package.ModName = GetStringField(Object, TEXT("ModName"));
	Package.ModsDir = GetStringField(Object, TEXT("ModsDir"));
	Package.JobFilename = GetStringField(Object, TEXT("JobFilename"));
	Package.bBuildAfterCreate = GetBoolField(Object, TEXT("BuildAfterCreate"), false);
	return Package;
}

template <typename SpecType>
TArray<TSharedPtr<FJsonValue>> ObjectArrayToJsonValues(const TArray<SpecType>& Values, TFunctionRef<TSharedRef<FJsonObject>(const SpecType&)> Writer)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	for (const SpecType& Value : Values)
	{
		Result.Add(MakeShared<FJsonValueObject>(Writer(Value)));
	}
	return Result;
}

void AddDuplicateIdErrors(const TCHAR* Label, const TArray<FString>& Ids, FNteCharacterModSpecValidationResult& Result)
{
	TSet<FString> Seen;
	for (const FString& Id : Ids)
	{
		if (Id.IsEmpty())
		{
			continue;
		}
		if (Seen.Contains(Id))
		{
			Result.Errors.Add(FString::Printf(TEXT("Duplicate %s id: %s"), Label, *Id));
		}
		Seen.Add(Id);
	}
}

bool IsMainMeshId(const FString& Id)
{
	return Id.IsEmpty() || Id == TEXT("Main") || Id == TEXT("main");
}

bool HasAttachedMeshId(const FNteCharacterModSpec& Spec, const FString& Id)
{
	for (const FNteCharacterAttachedMeshSpec& AttachedMesh : Spec.AttachedMeshes)
	{
		if (AttachedMesh.Id == Id)
		{
			return true;
		}
	}
	return false;
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

bool HasMeshTargetId(const FNteCharacterModSpec& Spec, const FString& Id)
{
	return IsMainMeshId(Id) || HasAttachedMeshId(Spec, Id);
}

bool IsSupportedRuntimeActionType(const FString& ActionType)
{
	return ActionType == TEXT("MaterialSlotVisibility")
		|| ActionType == TEXT("AttachedMeshVisibility")
		|| ActionType == TEXT("MaterialSwap")
		|| ActionType == TEXT("ScalarParameter")
		|| ActionType == TEXT("VectorParameter")
		|| ActionType == TEXT("MorphTarget");
}

FString NormalizeRuntimeHotkey(FString Hotkey)
{
	Hotkey.TrimStartAndEndInline();
	Hotkey.ReplaceInline(TEXT(" "), TEXT(""));
	return Hotkey;
}

bool ValidateRuntimeHotkey(const FString& RawHotkey, FString& OutNormalizedHotkey, FString& OutError)
{
	OutNormalizedHotkey = NormalizeRuntimeHotkey(RawHotkey);
	if (OutNormalizedHotkey.IsEmpty())
	{
		return true;
	}

	TArray<FString> Tokens;
	OutNormalizedHotkey.ParseIntoArray(Tokens, TEXT("+"), true);
	if (Tokens.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Runtime hotkey '%s' is empty after parsing."), *RawHotkey);
		return false;
	}

	FString KeyName;
	TSet<FString> Modifiers;
	for (FString Token : Tokens)
	{
		Token.TrimStartAndEndInline();
		if (Token.IsEmpty())
		{
			continue;
		}

		if (Token.Equals(TEXT("Ctrl"), ESearchCase::IgnoreCase)
			|| Token.Equals(TEXT("Control"), ESearchCase::IgnoreCase)
			|| Token.Equals(TEXT("Alt"), ESearchCase::IgnoreCase)
			|| Token.Equals(TEXT("Shift"), ESearchCase::IgnoreCase)
			|| Token.Equals(TEXT("Cmd"), ESearchCase::IgnoreCase)
			|| Token.Equals(TEXT("Command"), ESearchCase::IgnoreCase))
		{
			Modifiers.Add(Token.ToLower());
			continue;
		}

		if (!KeyName.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Runtime hotkey '%s' contains more than one key token."), *RawHotkey);
			return false;
		}
		KeyName = Token;
	}

	if (KeyName.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Runtime hotkey '%s' has modifiers but no key."), *RawHotkey);
		return false;
	}

	const FKey Key(*KeyName);
	if (!Key.IsValid())
	{
		OutError = FString::Printf(TEXT("Runtime hotkey '%s' uses unknown key '%s'. Use an Unreal key name such as M, Slash, F9, NumPadOne, or leave it empty for UI-only actions."), *RawHotkey, *KeyName);
		return false;
	}

	TArray<FString> NormalizedParts;
	if (Modifiers.Contains(TEXT("control")) || Modifiers.Contains(TEXT("ctrl")))
	{
		NormalizedParts.Add(TEXT("Ctrl"));
	}
	if (Modifiers.Contains(TEXT("alt")))
	{
		NormalizedParts.Add(TEXT("Alt"));
	}
	if (Modifiers.Contains(TEXT("shift")))
	{
		NormalizedParts.Add(TEXT("Shift"));
	}
	if (Modifiers.Contains(TEXT("command")) || Modifiers.Contains(TEXT("cmd")))
	{
		NormalizedParts.Add(TEXT("Cmd"));
	}
	NormalizedParts.Add(Key.GetFName().ToString());
	OutNormalizedHotkey = FString::Join(NormalizedParts, TEXT("+"));
	return true;
}

bool HasDuplicateInts(const TArray<int32>& Values)
{
	TSet<int32> Seen;
	for (const int32 Value : Values)
	{
		if (Seen.Contains(Value))
		{
			return true;
		}
		Seen.Add(Value);
	}
	return false;
}

bool HasAnyRuntimeComponentTag(const FNteCharacterRuntimeActionSpec& Action, const FNteCharacterAttachedMeshSpec* AttachedMesh)
{
	if (!Action.TargetComponentTags.IsEmpty())
	{
		return true;
	}
	return AttachedMesh && !AttachedMesh->MeshComponentOwnedTags.IsEmpty();
}

FString NormalizeSpecPackagePath(FString Path)
{
	Path.TrimStartAndEndInline();
	Path.TrimQuotesInline();
	Path.RemoveFromStart(TEXT("BlueprintGeneratedClass'"));
	Path.RemoveFromStart(TEXT("Class'"));
	Path.RemoveFromStart(TEXT("AnimBlueprintGeneratedClass'"));
	Path.RemoveFromEnd(TEXT("'"));
	Path.TrimStartAndEndInline();

	int32 DotIndex = INDEX_NONE;
	if (Path.FindLastChar(TEXT('.'), DotIndex))
	{
		const FString PackageName = Path.Left(DotIndex);
		const FString ObjectName = Path.Mid(DotIndex + 1);
		if (FPackageName::GetShortName(PackageName) == ObjectName || ObjectName.EndsWith(TEXT("_C")))
		{
			Path = PackageName;
		}
	}

	return Path;
}

void AddPackageSeed(TArray<FString>& Seeds, const FString& Path)
{
	const FString NormalizedPath = NormalizeSpecPackagePath(Path);
	if (NormalizedPath.StartsWith(TEXT("/Game/")) && !NormalizedPath.Contains(TEXT(".")))
	{
		Seeds.AddUnique(NormalizedPath);
	}
}
}

TSharedRef<FJsonObject> CharacterModSpecToJson(const FNteCharacterModSpec& Spec)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("Format"), Spec.Format);
	Object->SetNumberField(TEXT("Version"), Spec.Version);
	SetStringIfNotEmpty(Object, TEXT("WorkspaceName"), Spec.WorkspaceName);
	Object->SetObjectField(TEXT("Appearance"), AppearanceToJson(Spec.Appearance));
	SetStringIfNotEmpty(Object, TEXT("MainMeshPath"), Spec.MainMeshPath);
	SetStringIfNotEmpty(Object, TEXT("MainAnimBlueprintPath"), Spec.MainAnimBlueprintPath);
	Object->SetArrayField(TEXT("AttachedMeshes"), ObjectArrayToJsonValues<FNteCharacterAttachedMeshSpec>(Spec.AttachedMeshes, AttachedMeshToJson));
	Object->SetArrayField(TEXT("MaterialOperations"), ObjectArrayToJsonValues<FNteCharacterMaterialOperationSpec>(Spec.MaterialOperations, MaterialOperationToJson));
	Object->SetArrayField(TEXT("RuntimeActions"), ObjectArrayToJsonValues<FNteCharacterRuntimeActionSpec>(Spec.RuntimeActions, RuntimeActionToJson));
	Object->SetArrayField(TEXT("KawaiiPresets"), ObjectArrayToJsonValues<FNteCharacterKawaiiPresetSpec>(Spec.KawaiiPresets, KawaiiPresetToJson));
	Object->SetObjectField(TEXT("Package"), PackageSpecToJson(Spec.Package));
	return Object;
}

bool CharacterModSpecFromJson(const FJsonObject& Object, FNteCharacterModSpec& OutSpec, FString& OutError)
{
	FString Format = GetStringField(Object, TEXT("Format"));
	if (!Format.IsEmpty() && Format != TEXT("NTE.CharacterModSpec"))
	{
		OutError = FString::Printf(TEXT("Unsupported character mod spec format: %s"), *Format);
		return false;
	}

	OutSpec = FNteCharacterModSpec();
	if (!Format.IsEmpty())
	{
		OutSpec.Format = Format;
	}
	OutSpec.Version = GetIntField(Object, TEXT("Version"), 1);
	OutSpec.WorkspaceName = GetStringField(Object, TEXT("WorkspaceName"));
	ReadObjectField(Object, TEXT("Appearance"), [&OutSpec](const FJsonObject& Child)
	{
		OutSpec.Appearance = AppearanceFromJson(Child);
	});
	OutSpec.MainMeshPath = GetStringField(Object, TEXT("MainMeshPath"));
	OutSpec.MainAnimBlueprintPath = GetStringField(Object, TEXT("MainAnimBlueprintPath"));

	ReadObjectArrayField(Object, TEXT("AttachedMeshes"), [&OutSpec](const FJsonObject& Child)
	{
		OutSpec.AttachedMeshes.Add(AttachedMeshFromJson(Child));
	});
	ReadObjectArrayField(Object, TEXT("MaterialOperations"), [&OutSpec](const FJsonObject& Child)
	{
		OutSpec.MaterialOperations.Add(MaterialOperationFromJson(Child));
	});
	ReadObjectArrayField(Object, TEXT("RuntimeActions"), [&OutSpec](const FJsonObject& Child)
	{
		OutSpec.RuntimeActions.Add(RuntimeActionFromJson(Child));
	});
	ReadObjectArrayField(Object, TEXT("KawaiiPresets"), [&OutSpec](const FJsonObject& Child)
	{
		OutSpec.KawaiiPresets.Add(KawaiiPresetFromJson(Child));
	});
	ReadObjectField(Object, TEXT("Package"), [&OutSpec](const FJsonObject& Child)
	{
		OutSpec.Package = PackageSpecFromJson(Child);
	});
	return true;
}

bool LoadCharacterModSpecFromJsonFile(const FString& Filename, FNteCharacterModSpec& OutSpec, FString& OutError)
{
	TSharedPtr<FJsonObject> Object;
	if (!Json::LoadJsonObjectFromFile(Filename, Object, OutError))
	{
		return false;
	}

	return CharacterModSpecFromJson(*Object, OutSpec, OutError);
}

bool SaveCharacterModSpecToJsonFile(const FNteCharacterModSpec& Spec, const FString& Filename, FString& OutError)
{
	return Json::SaveJsonObjectToFile(CharacterModSpecToJson(Spec), Filename, OutError);
}

FNteCharacterModSpecValidationResult ValidateCharacterModSpec(const FNteCharacterModSpec& Spec)
{
	FNteCharacterModSpecValidationResult Result;

	if (Spec.MainMeshPath.IsEmpty())
	{
		Result.Errors.Add(TEXT("MainMeshPath is required."));
	}
	if (Spec.Appearance.PlayerAppearanceAssetPath.IsEmpty())
	{
		Result.Warnings.Add(TEXT("PlayerAppearanceAssetPath is empty; appearance assembly cannot update MeshAsset yet."));
	}
	if (Spec.Appearance.UIActorClassPath.IsEmpty())
	{
		Result.Warnings.Add(TEXT("UIActorClassPath is empty; UI preview sync cannot update PlayerUIShow yet."));
	}

	TArray<FString> AttachedMeshIds;
	for (const FNteCharacterAttachedMeshSpec& AttachedMesh : Spec.AttachedMeshes)
	{
		AttachedMeshIds.Add(AttachedMesh.Id);
		if (AttachedMesh.Id.IsEmpty())
		{
			Result.Errors.Add(TEXT("Attached mesh is missing Id."));
		}
		if (AttachedMesh.MeshPath.IsEmpty())
		{
			Result.Errors.Add(FString::Printf(TEXT("Attached mesh '%s' is missing MeshPath."), *AttachedMesh.Id));
		}
		if (AttachedMesh.SocketName.IsEmpty())
		{
			Result.Warnings.Add(FString::Printf(TEXT("Attached mesh '%s' has no SocketName; it cannot be mounted precisely."), *AttachedMesh.Id));
		}
		if (AttachedMesh.bEnableRuntimeActions && AttachedMesh.RuntimeAnimBlueprintPath.IsEmpty())
		{
			Result.Warnings.Add(FString::Printf(TEXT("Attached mesh '%s' enables runtime actions but has no RuntimeAnimBlueprintPath."), *AttachedMesh.Id));
		}
	}
	AddDuplicateIdErrors(TEXT("attached mesh"), AttachedMeshIds, Result);

	TArray<FString> MaterialOperationIds;
	for (const FNteCharacterMaterialOperationSpec& Operation : Spec.MaterialOperations)
	{
		MaterialOperationIds.Add(Operation.Id);
		if (Operation.Id.IsEmpty())
		{
			Result.Errors.Add(TEXT("Material operation is missing Id."));
		}
		if (!HasMeshTargetId(Spec, Operation.TargetMeshId))
		{
			Result.Errors.Add(FString::Printf(TEXT("Material operation '%s' targets unknown mesh id '%s'."), *Operation.Id, *Operation.TargetMeshId));
		}
		if (Operation.SlotIndex == INDEX_NONE)
		{
			Result.Errors.Add(FString::Printf(TEXT("Material operation '%s' is missing SlotIndex."), *Operation.Id));
		}
		if (Operation.OutputMaterialPath.IsEmpty())
		{
			Result.Warnings.Add(FString::Printf(TEXT("Material operation '%s' has no OutputMaterialPath."), *Operation.Id));
		}
	}
	AddDuplicateIdErrors(TEXT("material operation"), MaterialOperationIds, Result);

	TArray<FString> RuntimeActionIds;
	TSet<FString> Hotkeys;
	for (const FNteCharacterRuntimeActionSpec& Action : Spec.RuntimeActions)
	{
		RuntimeActionIds.Add(Action.Id);
		if (Action.Id.IsEmpty())
		{
			Result.Errors.Add(TEXT("Runtime action is missing Id."));
		}
		if (Action.Label.IsEmpty())
		{
			Result.Errors.Add(FString::Printf(TEXT("Runtime action '%s' is missing Label."), *Action.Id));
		}
		if (!HasMeshTargetId(Spec, Action.TargetMeshId))
		{
			Result.Errors.Add(FString::Printf(TEXT("Runtime action '%s' targets unknown mesh id '%s'."), *Action.Id, *Action.TargetMeshId));
		}
		if (!IsSupportedRuntimeActionType(Action.ActionType))
		{
			Result.Errors.Add(FString::Printf(
				TEXT("Runtime action '%s' uses unsupported ActionType '%s'. Supported values: MaterialSlotVisibility, AttachedMeshVisibility, MaterialSwap, ScalarParameter, VectorParameter, MorphTarget."),
				*Action.Id,
				*Action.ActionType));
		}
		if (!Action.Hotkey.IsEmpty())
		{
			FString NormalizedHotkey;
			FString HotkeyError;
			if (!ValidateRuntimeHotkey(Action.Hotkey, NormalizedHotkey, HotkeyError))
			{
				Result.Errors.Add(FString::Printf(TEXT("Runtime action '%s': %s"), *Action.Id, *HotkeyError));
			}
			else if (!NormalizedHotkey.IsEmpty() && Hotkeys.Contains(NormalizedHotkey))
			{
				Result.Errors.Add(FString::Printf(TEXT("Duplicate runtime hotkey: %s"), *NormalizedHotkey));
			}
			else if (!NormalizedHotkey.IsEmpty())
			{
				Hotkeys.Add(NormalizedHotkey);
			}
		}

		if (HasDuplicateInts(Action.MaterialSlots))
		{
			Result.Errors.Add(FString::Printf(TEXT("Runtime action '%s' lists a material slot more than once."), *Action.Id));
		}
		for (const int32 SlotIndex : Action.MaterialSlots)
		{
			if (SlotIndex < 0)
			{
				Result.Errors.Add(FString::Printf(TEXT("Runtime action '%s' has invalid negative material slot %d."), *Action.Id, SlotIndex));
			}
		}

		if ((Action.ActionType == TEXT("MaterialSlotVisibility") || Action.ActionType == TEXT("MaterialSwap")) && Action.MaterialSlots.IsEmpty())
		{
			Result.Errors.Add(FString::Printf(TEXT("Runtime action '%s' has no MaterialSlots."), *Action.Id));
		}
		if (Action.ActionType == TEXT("MaterialSwap") && Action.MaterialPath.IsEmpty())
		{
			Result.Errors.Add(FString::Printf(TEXT("Runtime action '%s' is MaterialSwap but has no MaterialPath."), *Action.Id));
		}
		if ((Action.ActionType == TEXT("ScalarParameter") || Action.ActionType == TEXT("VectorParameter")) && Action.ParameterName.IsEmpty())
		{
			Result.Errors.Add(FString::Printf(TEXT("Runtime action '%s' requires ParameterName."), *Action.Id));
		}
		if (Action.ActionType == TEXT("MorphTarget") && Action.MorphTargetName.IsEmpty())
		{
			Result.Errors.Add(FString::Printf(TEXT("Runtime action '%s' is MorphTarget but has no MorphTargetName."), *Action.Id));
		}
		if (Action.ActionType == TEXT("AttachedMeshVisibility"))
		{
			const FNteCharacterAttachedMeshSpec* AttachedMesh = FindAttachedMeshById(Spec, Action.TargetMeshId);
			if (!AttachedMesh)
			{
				Result.Errors.Add(FString::Printf(TEXT("Runtime action '%s' is AttachedMeshVisibility but TargetMeshId '%s' is not an attached mesh id."), *Action.Id, *Action.TargetMeshId));
			}
			else
			{
				if (!HasAnyRuntimeComponentTag(Action, AttachedMesh))
				{
					Result.Errors.Add(FString::Printf(TEXT("Runtime action '%s' cannot locate attached mesh '%s' at runtime because neither TargetComponentTags nor the attached mesh MeshComponentOwnedTags are set."), *Action.Id, *Action.TargetMeshId));
				}
				if (!AttachedMesh->bEnableRuntimeActions)
				{
					Result.Warnings.Add(FString::Printf(TEXT("Runtime action '%s' targets attached mesh '%s', but that attached mesh has EnableRuntimeActions=false."), *Action.Id, *Action.TargetMeshId));
				}
			}
		}
	}
	AddDuplicateIdErrors(TEXT("runtime action"), RuntimeActionIds, Result);

	TArray<FString> KawaiiPresetIds;
	for (const FNteCharacterKawaiiPresetSpec& Preset : Spec.KawaiiPresets)
	{
		KawaiiPresetIds.Add(Preset.Id);
		if (Preset.Id.IsEmpty())
		{
			Result.Errors.Add(TEXT("Kawaii preset is missing Id."));
		}
		if (!HasMeshTargetId(Spec, Preset.TargetMeshId))
		{
			Result.Errors.Add(FString::Printf(TEXT("Kawaii preset '%s' targets unknown mesh id '%s'."), *Preset.Id, *Preset.TargetMeshId));
		}
		if (Preset.RootBone.IsEmpty())
		{
			Result.Warnings.Add(FString::Printf(TEXT("Kawaii preset '%s' has no RootBone."), *Preset.Id));
		}
	}
	AddDuplicateIdErrors(TEXT("Kawaii preset"), KawaiiPresetIds, Result);

	return Result;
}

TArray<FString> CollectCharacterModSpecPackageSeeds(const FNteCharacterModSpec& Spec)
{
	TArray<FString> Seeds;
	AddPackageSeed(Seeds, Spec.Appearance.PlayerAppearanceAssetPath);
	AddPackageSeed(Seeds, Spec.Appearance.UIActorClassPath);
	AddPackageSeed(Seeds, Spec.MainMeshPath);
	AddPackageSeed(Seeds, Spec.MainAnimBlueprintPath);

	for (const FNteCharacterAttachedMeshSpec& AttachedMesh : Spec.AttachedMeshes)
	{
		AddPackageSeed(Seeds, AttachedMesh.MeshPath);
		AddPackageSeed(Seeds, AttachedMesh.AnimBlueprintPath);
		AddPackageSeed(Seeds, AttachedMesh.MobileAnimBlueprintPath);
		AddPackageSeed(Seeds, AttachedMesh.UIAnimBlueprintPath);
		AddPackageSeed(Seeds, AttachedMesh.RuntimeAnimBlueprintPath);
	}

	for (const FNteCharacterMaterialOperationSpec& Operation : Spec.MaterialOperations)
	{
		AddPackageSeed(Seeds, Operation.OutputMaterialPath);
		for (const TPair<FString, FString>& Pair : Operation.SourceTextureOverrides)
		{
			AddPackageSeed(Seeds, Pair.Value);
		}
	}

	for (const FNteCharacterRuntimeActionSpec& Action : Spec.RuntimeActions)
	{
		AddPackageSeed(Seeds, Action.MaterialPath);
	}

	Seeds.Sort();
	return Seeds;
}
}
