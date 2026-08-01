// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteCharacterModSpec.h"

#include "NteCharacterRuntimeActionPlan.h"

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

TSharedRef<FJsonObject> PlaneToJson(const FPlane& Value)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("X"), Value.X);
	Object->SetNumberField(TEXT("Y"), Value.Y);
	Object->SetNumberField(TEXT("Z"), Value.Z);
	Object->SetNumberField(TEXT("W"), Value.W);
	return Object;
}

FPlane PlaneFromJson(const FJsonObject& Object, const FPlane& DefaultValue)
{
	return FPlane(
		GetFloatField(Object, TEXT("X"), DefaultValue.X),
		GetFloatField(Object, TEXT("Y"), DefaultValue.Y),
		GetFloatField(Object, TEXT("Z"), DefaultValue.Z),
		GetFloatField(Object, TEXT("W"), DefaultValue.W));
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

TSharedRef<FJsonObject> PresentationTargetToJson(const FNteCharacterPresentationTargetSpec& Target)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	SetStringIfNotEmpty(Object, TEXT("Id"), Target.Id);
	SetStringIfNotEmpty(Object, TEXT("BlueprintClassPath"), Target.BlueprintClassPath);
	SetStringIfNotEmpty(Object, TEXT("ParentMeshComponentName"), Target.ParentMeshComponentName);
	SetStringIfNotEmpty(Object, TEXT("MainAnimBlueprintPath"), Target.MainAnimBlueprintPath);
	Object->SetBoolField(TEXT("ConfigureMainMesh"), Target.bConfigureMainMesh);
	return Object;
}

FNteCharacterPresentationTargetSpec PresentationTargetFromJson(const FJsonObject& Object)
{
	FNteCharacterPresentationTargetSpec Target;
	Target.Id = GetStringField(Object, TEXT("Id"));
	Target.BlueprintClassPath = GetStringField(Object, TEXT("BlueprintClassPath"));
	Target.ParentMeshComponentName = GetStringField(Object, TEXT("ParentMeshComponentName"));
	Target.MainAnimBlueprintPath = GetStringField(Object, TEXT("MainAnimBlueprintPath"));
	Target.bConfigureMainMesh = GetBoolField(Object, TEXT("ConfigureMainMesh"), true);
	return Target;
}

TSharedRef<FJsonObject> NPCAppearanceTargetToJson(const FNteCharacterNPCAppearanceTargetSpec& Target)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	SetStringIfNotEmpty(Object, TEXT("AssetPath"), Target.AssetPath);
	SetStringIfNotEmpty(Object, TEXT("MainAnimBlueprintPath"), Target.MainAnimBlueprintPath);
	return Object;
}

FNteCharacterNPCAppearanceTargetSpec NPCAppearanceTargetFromJson(const FJsonObject& Object)
{
	FNteCharacterNPCAppearanceTargetSpec Target;
	Target.AssetPath = GetStringField(Object, TEXT("AssetPath"));
	Target.MainAnimBlueprintPath = GetStringField(Object, TEXT("MainAnimBlueprintPath"));
	return Target;
}

TSharedRef<FJsonObject> AppearanceToJson(const FNteCharacterAppearanceTarget& Appearance)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	SetStringIfNotEmpty(Object, TEXT("AppearanceRowName"), Appearance.AppearanceRowName);
	SetStringIfNotEmpty(Object, TEXT("PlayerAppearanceAssetPath"), Appearance.PlayerAppearanceAssetPath);
	TArray<TSharedPtr<FJsonValue>> NPCAppearanceTargets;
	for (const FNteCharacterNPCAppearanceTargetSpec& Target : Appearance.NPCAppearanceTargets)
	{
		NPCAppearanceTargets.Add(MakeShared<FJsonValueObject>(NPCAppearanceTargetToJson(Target)));
	}
	Object->SetArrayField(TEXT("NPCAppearanceTargets"), NPCAppearanceTargets);
	SetStringIfNotEmpty(Object, TEXT("UIActorClassPath"), Appearance.UIActorClassPath);
	SetStringIfNotEmpty(Object, TEXT("MainUIAnimBlueprintPath"), Appearance.MainUIAnimBlueprintPath);
	TArray<TSharedPtr<FJsonValue>> PresentationTargets;
	for (const FNteCharacterPresentationTargetSpec& Target : Appearance.PresentationTargets)
	{
		PresentationTargets.Add(MakeShared<FJsonValueObject>(PresentationTargetToJson(Target)));
	}
	Object->SetArrayField(TEXT("PresentationTargets"), PresentationTargets);
	if (Appearance.CapsuleHalfHeight.IsSet())
	{
		Object->SetNumberField(TEXT("CapsuleHalfHeight"), Appearance.CapsuleHalfHeight.GetValue());
	}
	if (Appearance.CapsuleRadius.IsSet())
	{
		Object->SetNumberField(TEXT("CapsuleRadius"), Appearance.CapsuleRadius.GetValue());
	}
	if (Appearance.RelativeLocation.IsSet())
	{
		Object->SetObjectField(TEXT("RelativeLocation"), VectorToJson(Appearance.RelativeLocation.GetValue()));
	}
	return Object;
}

FNteCharacterAppearanceTarget AppearanceFromJson(const FJsonObject& Object)
{
	FNteCharacterAppearanceTarget Appearance;
	Appearance.AppearanceRowName = GetStringField(Object, TEXT("AppearanceRowName"));
	Appearance.PlayerAppearanceAssetPath = GetStringField(Object, TEXT("PlayerAppearanceAssetPath"));
	ReadObjectArrayField(Object, TEXT("NPCAppearanceTargets"), [&Appearance](const FJsonObject& Child)
	{
		Appearance.NPCAppearanceTargets.Add(NPCAppearanceTargetFromJson(Child));
	});
	Appearance.UIActorClassPath = GetStringField(Object, TEXT("UIActorClassPath"));
	Appearance.MainUIAnimBlueprintPath = GetStringField(Object, TEXT("MainUIAnimBlueprintPath"));
	ReadObjectArrayField(Object, TEXT("PresentationTargets"), [&Appearance](const FJsonObject& Child)
	{
		Appearance.PresentationTargets.Add(PresentationTargetFromJson(Child));
	});
	double NumberValue = 0.0;
	if (Object.TryGetNumberField(TEXT("CapsuleHalfHeight"), NumberValue))
	{
		Appearance.CapsuleHalfHeight = static_cast<float>(NumberValue);
	}
	if (Object.TryGetNumberField(TEXT("CapsuleRadius"), NumberValue))
	{
		Appearance.CapsuleRadius = static_cast<float>(NumberValue);
	}
	ReadObjectField(Object, TEXT("RelativeLocation"), [&Appearance](const FJsonObject& Child)
	{
		Appearance.RelativeLocation = VectorFromJson(Child, FVector::ZeroVector);
	});
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
	SetStringIfNotEmpty(Object, TEXT("PresentationSocketName"), AttachedMesh.PresentationSocketName);
	Object->SetArrayField(TEXT("MeshComponentOwnedTags"), Json::StringArrayToJsonValues(AttachedMesh.MeshComponentOwnedTags));
	Object->SetArrayField(TEXT("PresentationTargetIds"), Json::StringArrayToJsonValues(AttachedMesh.PresentationTargetIds));
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
	AttachedMesh.PresentationSocketName = GetStringField(Object, TEXT("PresentationSocketName"));
	AttachedMesh.MeshComponentOwnedTags = GetStringArrayField(Object, TEXT("MeshComponentOwnedTags"));
	AttachedMesh.PresentationTargetIds = GetStringArrayField(Object, TEXT("PresentationTargetIds"));
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
	SetStringIfNotEmpty(Object, TEXT("ExistingMaterialPath"), Operation.ExistingMaterialPath);
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
	Operation.ExistingMaterialPath = GetStringField(Object, TEXT("ExistingMaterialPath"));
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
	SetStringIfNotEmpty(Object, TEXT("HostMeshId"), Action.HostMeshId);
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
	Action.HostMeshId = GetStringField(Object, TEXT("HostMeshId"));
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

TSharedRef<FJsonObject> RuntimeUiToJson(const FNteCharacterRuntimeUiSpec& RuntimeUi)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetBoolField(TEXT("EnableUi"), RuntimeUi.bEnableUi);
	SetStringIfNotEmpty(Object, TEXT("ToggleUiHotkey"), RuntimeUi.ToggleUiHotkey);
	SetStringIfNotEmpty(Object, TEXT("Title"), RuntimeUi.Title);
	Object->SetBoolField(TEXT("DefaultVisible"), RuntimeUi.bDefaultVisible);
	SetStringIfNotEmpty(Object, TEXT("StyleProfileId"), RuntimeUi.StyleProfileId);
	SetStringIfNotEmpty(Object, TEXT("FontPath"), RuntimeUi.FontPath);
	SetStringIfNotEmpty(Object, TEXT("BodyFontTypeface"), RuntimeUi.BodyFontTypeface);
	SetStringIfNotEmpty(Object, TEXT("TitleFontTypeface"), RuntimeUi.TitleFontTypeface);
	Object->SetNumberField(TEXT("BodyFontSize"), RuntimeUi.BodyFontSize);
	Object->SetNumberField(TEXT("TitleFontSize"), RuntimeUi.TitleFontSize);
	SetStringIfNotEmpty(Object, TEXT("ButtonNormalTexturePath"), RuntimeUi.ButtonNormalTexturePath);
	SetStringIfNotEmpty(Object, TEXT("ButtonHoveredTexturePath"), RuntimeUi.ButtonHoveredTexturePath);
	SetStringIfNotEmpty(Object, TEXT("ButtonPressedTexturePath"), RuntimeUi.ButtonPressedTexturePath);
	SetStringIfNotEmpty(Object, TEXT("ButtonDisabledTexturePath"), RuntimeUi.ButtonDisabledTexturePath);
	return Object;
}

FNteCharacterRuntimeUiSpec RuntimeUiFromJson(const FJsonObject& Object)
{
	FNteCharacterRuntimeUiSpec RuntimeUi;
	RuntimeUi.bEnableUi = GetBoolField(Object, TEXT("EnableUi"), false);
	RuntimeUi.ToggleUiHotkey = GetStringField(Object, TEXT("ToggleUiHotkey"));
	RuntimeUi.Title = GetStringField(Object, TEXT("Title"));
	RuntimeUi.bDefaultVisible = GetBoolField(Object, TEXT("DefaultVisible"), false);
	const FString StyleProfileId = GetStringField(Object, TEXT("StyleProfileId"));
	if (!StyleProfileId.IsEmpty())
	{
		RuntimeUi.StyleProfileId = StyleProfileId;
	}
	const FString FontPath = GetStringField(Object, TEXT("FontPath"));
	if (!FontPath.IsEmpty())
	{
		RuntimeUi.FontPath = FontPath;
	}
	const FString BodyFontTypeface = GetStringField(Object, TEXT("BodyFontTypeface"));
	if (!BodyFontTypeface.IsEmpty())
	{
		RuntimeUi.BodyFontTypeface = BodyFontTypeface;
	}
	const FString TitleFontTypeface = GetStringField(Object, TEXT("TitleFontTypeface"));
	if (!TitleFontTypeface.IsEmpty())
	{
		RuntimeUi.TitleFontTypeface = TitleFontTypeface;
	}
	RuntimeUi.BodyFontSize = FMath::Max(1, static_cast<int32>(GetFloatField(Object, TEXT("BodyFontSize"), RuntimeUi.BodyFontSize)));
	RuntimeUi.TitleFontSize = FMath::Max(1, static_cast<int32>(GetFloatField(Object, TEXT("TitleFontSize"), RuntimeUi.TitleFontSize)));
	const FString ButtonNormalTexturePath = GetStringField(Object, TEXT("ButtonNormalTexturePath"));
	if (!ButtonNormalTexturePath.IsEmpty())
	{
		RuntimeUi.ButtonNormalTexturePath = ButtonNormalTexturePath;
	}
	const FString ButtonHoveredTexturePath = GetStringField(Object, TEXT("ButtonHoveredTexturePath"));
	if (!ButtonHoveredTexturePath.IsEmpty())
	{
		RuntimeUi.ButtonHoveredTexturePath = ButtonHoveredTexturePath;
	}
	const FString ButtonPressedTexturePath = GetStringField(Object, TEXT("ButtonPressedTexturePath"));
	if (!ButtonPressedTexturePath.IsEmpty())
	{
		RuntimeUi.ButtonPressedTexturePath = ButtonPressedTexturePath;
	}
	const FString ButtonDisabledTexturePath = GetStringField(Object, TEXT("ButtonDisabledTexturePath"));
	if (!ButtonDisabledTexturePath.IsEmpty())
	{
		RuntimeUi.ButtonDisabledTexturePath = ButtonDisabledTexturePath;
	}
	return RuntimeUi;
}

TSharedRef<FJsonObject> KawaiiAdditionalRootBoneToJson(const FNteCharacterKawaiiAdditionalRootBoneSpec& RootBone)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	SetStringIfNotEmpty(Object, TEXT("RootBone"), RootBone.RootBone);
	Object->SetArrayField(TEXT("OverrideExcludeBones"), Json::StringArrayToJsonValues(RootBone.OverrideExcludeBones));
	Object->SetBoolField(TEXT("UseOverrideExcludeBones"), RootBone.bUseOverrideExcludeBones);
	return Object;
}

FNteCharacterKawaiiAdditionalRootBoneSpec KawaiiAdditionalRootBoneFromJson(const FJsonObject& Object)
{
	FNteCharacterKawaiiAdditionalRootBoneSpec RootBone;
	RootBone.RootBone = GetStringField(Object, TEXT("RootBone"));
	RootBone.OverrideExcludeBones = GetStringArrayField(Object, TEXT("OverrideExcludeBones"));
	RootBone.bUseOverrideExcludeBones = GetBoolField(Object, TEXT("UseOverrideExcludeBones"), false);
	return RootBone;
}

TArray<TSharedPtr<FJsonValue>> KawaiiAdditionalRootBonesToJsonValues(const TArray<FNteCharacterKawaiiAdditionalRootBoneSpec>& RootBones)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FNteCharacterKawaiiAdditionalRootBoneSpec& RootBone : RootBones)
	{
		Values.Add(MakeShared<FJsonValueObject>(KawaiiAdditionalRootBoneToJson(RootBone)));
	}
	return Values;
}

TSharedRef<FJsonObject> KawaiiBoneRemapToJson(const FNteCharacterKawaiiBoneRemapSpec& Remap)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	SetStringIfNotEmpty(Object, TEXT("SourceBone"), Remap.SourceBone);
	SetStringIfNotEmpty(Object, TEXT("TargetBone"), Remap.TargetBone);
	return Object;
}

FNteCharacterKawaiiBoneRemapSpec KawaiiBoneRemapFromJson(const FJsonObject& Object)
{
	FNteCharacterKawaiiBoneRemapSpec Remap;
	Remap.SourceBone = GetStringField(Object, TEXT("SourceBone"));
	Remap.TargetBone = GetStringField(Object, TEXT("TargetBone"));
	return Remap;
}

TArray<TSharedPtr<FJsonValue>> KawaiiBoneRemapsToJsonValues(const TArray<FNteCharacterKawaiiBoneRemapSpec>& Remaps)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FNteCharacterKawaiiBoneRemapSpec& Remap : Remaps)
	{
		Values.Add(MakeShared<FJsonValueObject>(KawaiiBoneRemapToJson(Remap)));
	}
	return Values;
}

TSharedRef<FJsonObject> KawaiiPhysicsSettingsToJson(const FNteCharacterKawaiiPhysicsSettingsSpec& Settings)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("Damping"), Settings.Damping);
	Object->SetNumberField(TEXT("Stiffness"), Settings.Stiffness);
	Object->SetNumberField(TEXT("WorldDampingLocation"), Settings.WorldDampingLocation);
	Object->SetNumberField(TEXT("WorldDampingRotation"), Settings.WorldDampingRotation);
	Object->SetNumberField(TEXT("Radius"), Settings.Radius);
	Object->SetNumberField(TEXT("LimitAngle"), Settings.LimitAngle);
	Object->SetNumberField(TEXT("ForwardMoveOffset"), Settings.ForwardMoveOffset);
	Object->SetBoolField(TEXT("HasForwardMoveOffset"), Settings.bHasForwardMoveOffset);
	return Object;
}

FNteCharacterKawaiiPhysicsSettingsSpec KawaiiPhysicsSettingsFromJson(const FJsonObject& Object)
{
	FNteCharacterKawaiiPhysicsSettingsSpec Settings;
	Settings.Damping = GetFloatField(Object, TEXT("Damping"));
	Settings.Stiffness = GetFloatField(Object, TEXT("Stiffness"));
	Settings.WorldDampingLocation = GetFloatField(Object, TEXT("WorldDampingLocation"));
	Settings.WorldDampingRotation = GetFloatField(Object, TEXT("WorldDampingRotation"));
	Settings.Radius = GetFloatField(Object, TEXT("Radius"));
	Settings.LimitAngle = GetFloatField(Object, TEXT("LimitAngle"));
	Settings.ForwardMoveOffset = GetFloatField(Object, TEXT("ForwardMoveOffset"));
	Settings.bHasForwardMoveOffset = GetBoolField(Object, TEXT("HasForwardMoveOffset"), false);
	return Settings;
}

TSharedRef<FJsonObject> KawaiiLimitToJson(const FNteCharacterKawaiiLimitSpec& Limit)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	SetStringIfNotEmpty(Object, TEXT("LimitKind"), Limit.LimitKind);
	SetStringIfNotEmpty(Object, TEXT("DrivingBone"), Limit.DrivingBone);
	Object->SetObjectField(TEXT("OffsetLocation"), VectorToJson(Limit.OffsetLocation));
	Object->SetObjectField(TEXT("OffsetRotation"), RotatorToJson(Limit.OffsetRotation));
	Object->SetNumberField(TEXT("Radius"), Limit.Radius);
	Object->SetNumberField(TEXT("Length"), Limit.Length);
	Object->SetNumberField(TEXT("SphereRadius"), Limit.SphereRadius);
	Object->SetObjectField(TEXT("Extent"), VectorToJson(Limit.Extent));
	Object->SetObjectField(TEXT("Plane"), PlaneToJson(Limit.Plane));
	SetStringIfNotEmpty(Object, TEXT("LimitType"), Limit.LimitType);
	SetStringIfNotEmpty(Object, TEXT("SourceType"), Limit.SourceType);
	Object->SetBoolField(TEXT("Enable"), Limit.bEnable);
	return Object;
}

FNteCharacterKawaiiLimitSpec KawaiiLimitFromJson(const FJsonObject& Object)
{
	FNteCharacterKawaiiLimitSpec Limit;
	Limit.LimitKind = GetStringField(Object, TEXT("LimitKind"));
	Limit.DrivingBone = GetStringField(Object, TEXT("DrivingBone"));
	ReadObjectField(Object, TEXT("OffsetLocation"), [&Limit](const FJsonObject& Child)
	{
		Limit.OffsetLocation = VectorFromJson(Child, FVector::ZeroVector);
	});
	ReadObjectField(Object, TEXT("OffsetRotation"), [&Limit](const FJsonObject& Child)
	{
		Limit.OffsetRotation = RotatorFromJson(Child, FRotator::ZeroRotator);
	});
	Limit.Radius = GetFloatField(Object, TEXT("Radius"));
	Limit.Length = GetFloatField(Object, TEXT("Length"));
	Limit.SphereRadius = GetFloatField(Object, TEXT("SphereRadius"));
	ReadObjectField(Object, TEXT("Extent"), [&Limit](const FJsonObject& Child)
	{
		Limit.Extent = VectorFromJson(Child, FVector::ZeroVector);
	});
	ReadObjectField(Object, TEXT("Plane"), [&Limit](const FJsonObject& Child)
	{
		Limit.Plane = PlaneFromJson(Child, FPlane(0, 0, 0, 0));
	});
	Limit.LimitType = GetStringField(Object, TEXT("LimitType"));
	Limit.SourceType = GetStringField(Object, TEXT("SourceType"));
	Limit.bEnable = GetBoolField(Object, TEXT("Enable"), true);
	return Limit;
}

TArray<TSharedPtr<FJsonValue>> KawaiiLimitsToJsonValues(const TArray<FNteCharacterKawaiiLimitSpec>& Limits)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FNteCharacterKawaiiLimitSpec& Limit : Limits)
	{
		Values.Add(MakeShared<FJsonValueObject>(KawaiiLimitToJson(Limit)));
	}
	return Values;
}

TSharedRef<FJsonObject> KawaiiCurveToJson(const FNteCharacterKawaiiCurveSpec& Curve)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	SetStringIfNotEmpty(Object, TEXT("CurveKind"), Curve.CurveKind);
	SetStringIfNotEmpty(Object, TEXT("ExternalCurveObjectName"), Curve.ExternalCurveObjectName);
	SetStringIfNotEmpty(Object, TEXT("ExternalCurveObjectPath"), Curve.ExternalCurveObjectPath);
	Object->SetNumberField(TEXT("InlineKeyCount"), Curve.InlineKeyCount);
	SetStringIfNotEmpty(Object, TEXT("OutputCurvePath"), Curve.OutputCurvePath);
	return Object;
}

FNteCharacterKawaiiCurveSpec KawaiiCurveFromJson(const FJsonObject& Object)
{
	FNteCharacterKawaiiCurveSpec Curve;
	Curve.CurveKind = GetStringField(Object, TEXT("CurveKind"));
	Curve.ExternalCurveObjectName = GetStringField(Object, TEXT("ExternalCurveObjectName"));
	Curve.ExternalCurveObjectPath = GetStringField(Object, TEXT("ExternalCurveObjectPath"));
	Curve.InlineKeyCount = GetIntField(Object, TEXT("InlineKeyCount"), 0);
	Curve.OutputCurvePath = GetStringField(Object, TEXT("OutputCurvePath"));
	return Curve;
}

TArray<TSharedPtr<FJsonValue>> KawaiiCurvesToJsonValues(const TArray<FNteCharacterKawaiiCurveSpec>& Curves)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FNteCharacterKawaiiCurveSpec& Curve : Curves)
	{
		Values.Add(MakeShared<FJsonValueObject>(KawaiiCurveToJson(Curve)));
	}
	return Values;
}

TSharedRef<FJsonObject> KawaiiPresetToJson(const FNteCharacterKawaiiPresetSpec& Preset)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	SetStringIfNotEmpty(Object, TEXT("Id"), Preset.Id);
	SetStringIfNotEmpty(Object, TEXT("Label"), Preset.Label);
	SetStringIfNotEmpty(Object, TEXT("TargetMeshId"), Preset.TargetMeshId);
	SetStringIfNotEmpty(Object, TEXT("SourceKind"), Preset.SourceKind);
	SetStringIfNotEmpty(Object, TEXT("SourceAnimBlueprintJson"), Preset.SourceAnimBlueprintJson);
	SetStringIfNotEmpty(Object, TEXT("SourceGeneratedClassName"), Preset.SourceGeneratedClassName);
	SetStringIfNotEmpty(Object, TEXT("SourceClassDefaultObjectName"), Preset.SourceClassDefaultObjectName);
	SetStringIfNotEmpty(Object, TEXT("SourceNodeName"), Preset.SourceNodeName);
	SetStringIfNotEmpty(Object, TEXT("ReferencedPresetId"), Preset.ReferencedPresetId);
	SetStringIfNotEmpty(Object, TEXT("TemplateKind"), Preset.TemplateKind);
	SetStringIfNotEmpty(Object, TEXT("SchemaStatus"), Preset.SchemaStatus);
	Object->SetArrayField(TEXT("BoneRemaps"), KawaiiBoneRemapsToJsonValues(Preset.BoneRemaps));
	SetStringIfNotEmpty(Object, TEXT("RootBone"), Preset.RootBone);
	Object->SetArrayField(TEXT("ExcludeBones"), Json::StringArrayToJsonValues(Preset.ExcludeBones));
	Object->SetArrayField(TEXT("AdditionalRootBones"), KawaiiAdditionalRootBonesToJsonValues(Preset.AdditionalRootBones));
	Object->SetObjectField(TEXT("PhysicsSettings"), KawaiiPhysicsSettingsToJson(Preset.PhysicsSettings));
	Object->SetNumberField(TEXT("DummyBoneLength"), Preset.DummyBoneLength);
	SetStringIfNotEmpty(Object, TEXT("BoneForwardAxis"), Preset.BoneForwardAxis);
	Object->SetNumberField(TEXT("TargetFramerate"), Preset.TargetFramerate);
	Object->SetBoolField(TEXT("OverrideTargetFramerate"), Preset.bOverrideTargetFramerate);
	Object->SetNumberField(TEXT("WarmUpFrames"), Preset.WarmUpFrames);
	Object->SetBoolField(TEXT("UseWarmUpWhenResetDynamics"), Preset.bUseWarmUpWhenResetDynamics);
	Object->SetBoolField(TEXT("NeedWarmUp"), Preset.bNeedWarmUp);
	Object->SetNumberField(TEXT("TeleportDistanceThreshold"), Preset.TeleportDistanceThreshold);
	Object->SetNumberField(TEXT("TeleportRotationThreshold"), Preset.TeleportRotationThreshold);
	SetStringIfNotEmpty(Object, TEXT("PlanarConstraint"), Preset.PlanarConstraint);
	Object->SetBoolField(TEXT("ResetBoneTransformWhenBoneNotFound"), Preset.bResetBoneTransformWhenBoneNotFound);
	Object->SetArrayField(TEXT("Curves"), KawaiiCurvesToJsonValues(Preset.Curves));
	SetStringIfNotEmpty(Object, TEXT("RuntimeAnimBlueprintPath"), Preset.RuntimeAnimBlueprintPath);
	SetStringIfNotEmpty(Object, TEXT("LimitsDataAssetPath"), Preset.LimitsDataAssetPath);
	SetStringIfNotEmpty(Object, TEXT("PhysicsAssetForLimitsPath"), Preset.PhysicsAssetForLimitsPath);
	SetStringIfNotEmpty(Object, TEXT("OutputLimitsDataAssetPath"), Preset.OutputLimitsDataAssetPath);
	SetStringIfNotEmpty(Object, TEXT("BoneConstraintsDataAssetPath"), Preset.BoneConstraintsDataAssetPath);
	SetStringIfNotEmpty(Object, TEXT("OutputBoneConstraintsDataAssetPath"), Preset.OutputBoneConstraintsDataAssetPath);
	Object->SetNumberField(TEXT("SphericalLimitsDataCount"), Preset.SphericalLimitsDataCount);
	Object->SetNumberField(TEXT("CapsuleLimitsDataCount"), Preset.CapsuleLimitsDataCount);
	Object->SetNumberField(TEXT("BoxLimitsDataCount"), Preset.BoxLimitsDataCount);
	Object->SetNumberField(TEXT("PlanarLimitsDataCount"), Preset.PlanarLimitsDataCount);
	Object->SetArrayField(TEXT("CollisionLimits"), KawaiiLimitsToJsonValues(Preset.CollisionLimits));
	SetStringIfNotEmpty(Object, TEXT("BoneConstraintGlobalComplianceType"), Preset.BoneConstraintGlobalComplianceType);
	Object->SetNumberField(TEXT("BoneConstraintIterationCountBeforeCollision"), Preset.BoneConstraintIterationCountBeforeCollision);
	Object->SetNumberField(TEXT("BoneConstraintIterationCountAfterCollision"), Preset.BoneConstraintIterationCountAfterCollision);
	Object->SetBoolField(TEXT("AutoAddChildDummyBoneConstraint"), Preset.bAutoAddChildDummyBoneConstraint);
	Object->SetNumberField(TEXT("BoneConstraintCount"), Preset.BoneConstraintCount);
	Object->SetNumberField(TEXT("BoneConstraintsDataCount"), Preset.BoneConstraintsDataCount);
	Object->SetObjectField(TEXT("Gravity"), VectorToJson(Preset.Gravity));
	Object->SetBoolField(TEXT("EnableWind"), Preset.bEnableWind);
	Object->SetNumberField(TEXT("WindScale"), Preset.WindScale);
	Object->SetBoolField(TEXT("HasUseRelativeMove"), Preset.bHasUseRelativeMove);
	Object->SetBoolField(TEXT("UseRelativeMove"), Preset.bUseRelativeMove);
	Object->SetObjectField(TEXT("MovementReferenceDisplacement"), VectorToJson(Preset.MovementReferenceDisplacement));
	Object->SetBoolField(TEXT("AllowWorldCollision"), Preset.bAllowWorldCollision);
	Object->SetBoolField(TEXT("OverrideCollisionParams"), Preset.bOverrideCollisionParams);
	Object->SetBoolField(TEXT("IgnoreSelfComponent"), Preset.bIgnoreSelfComponent);
	Object->SetArrayField(TEXT("IgnoreBones"), Json::StringArrayToJsonValues(Preset.IgnoreBones));
	Object->SetArrayField(TEXT("IgnoreBoneNamePrefix"), Json::StringArrayToJsonValues(Preset.IgnoreBoneNamePrefix));
	SetStringIfNotEmpty(Object, TEXT("KawaiiPhysicsTag"), Preset.KawaiiPhysicsTag);
	Object->SetArrayField(TEXT("UnsupportedSourceFields"), Json::StringArrayToJsonValues(Preset.UnsupportedSourceFields));
	return Object;
}

FNteCharacterKawaiiPresetSpec KawaiiPresetFromJson(const FJsonObject& Object)
{
	FNteCharacterKawaiiPresetSpec Preset;
	Preset.Id = GetStringField(Object, TEXT("Id"));
	Preset.Label = GetStringField(Object, TEXT("Label"));
	Preset.TargetMeshId = GetStringField(Object, TEXT("TargetMeshId"));
	Preset.SourceKind = GetStringField(Object, TEXT("SourceKind"));
	Preset.SourceAnimBlueprintJson = GetStringField(Object, TEXT("SourceAnimBlueprintJson"));
	Preset.SourceGeneratedClassName = GetStringField(Object, TEXT("SourceGeneratedClassName"));
	Preset.SourceClassDefaultObjectName = GetStringField(Object, TEXT("SourceClassDefaultObjectName"));
	Preset.SourceNodeName = GetStringField(Object, TEXT("SourceNodeName"));
	Preset.ReferencedPresetId = GetStringField(Object, TEXT("ReferencedPresetId"));
	Preset.TemplateKind = GetStringField(Object, TEXT("TemplateKind"));
	Preset.SchemaStatus = GetStringField(Object, TEXT("SchemaStatus"));
	ReadObjectArrayField(Object, TEXT("BoneRemaps"), [&Preset](const FJsonObject& Child)
	{
		Preset.BoneRemaps.Add(KawaiiBoneRemapFromJson(Child));
	});
	Preset.RootBone = GetStringField(Object, TEXT("RootBone"));
	Preset.ExcludeBones = GetStringArrayField(Object, TEXT("ExcludeBones"));
	ReadObjectArrayField(Object, TEXT("AdditionalRootBones"), [&Preset](const FJsonObject& Child)
	{
		Preset.AdditionalRootBones.Add(KawaiiAdditionalRootBoneFromJson(Child));
	});
	ReadObjectField(Object, TEXT("PhysicsSettings"), [&Preset](const FJsonObject& Child)
	{
		Preset.PhysicsSettings = KawaiiPhysicsSettingsFromJson(Child);
	});
	Preset.DummyBoneLength = GetFloatField(Object, TEXT("DummyBoneLength"));
	Preset.BoneForwardAxis = GetStringField(Object, TEXT("BoneForwardAxis"));
	Preset.TargetFramerate = GetIntField(Object, TEXT("TargetFramerate"), 60);
	Preset.bOverrideTargetFramerate = GetBoolField(Object, TEXT("OverrideTargetFramerate"), false);
	Preset.WarmUpFrames = GetIntField(Object, TEXT("WarmUpFrames"), 0);
	Preset.bUseWarmUpWhenResetDynamics = GetBoolField(Object, TEXT("UseWarmUpWhenResetDynamics"), true);
	Preset.bNeedWarmUp = GetBoolField(Object, TEXT("NeedWarmUp"), false);
	Preset.TeleportDistanceThreshold = GetFloatField(Object, TEXT("TeleportDistanceThreshold"));
	Preset.TeleportRotationThreshold = GetFloatField(Object, TEXT("TeleportRotationThreshold"));
	Preset.PlanarConstraint = GetStringField(Object, TEXT("PlanarConstraint"));
	Preset.bResetBoneTransformWhenBoneNotFound = GetBoolField(Object, TEXT("ResetBoneTransformWhenBoneNotFound"), false);
	ReadObjectArrayField(Object, TEXT("Curves"), [&Preset](const FJsonObject& Child)
	{
		Preset.Curves.Add(KawaiiCurveFromJson(Child));
	});
	Preset.RuntimeAnimBlueprintPath = GetStringField(Object, TEXT("RuntimeAnimBlueprintPath"));
	Preset.LimitsDataAssetPath = GetStringField(Object, TEXT("LimitsDataAssetPath"));
	Preset.PhysicsAssetForLimitsPath = GetStringField(Object, TEXT("PhysicsAssetForLimitsPath"));
	Preset.OutputLimitsDataAssetPath = GetStringField(Object, TEXT("OutputLimitsDataAssetPath"));
	Preset.BoneConstraintsDataAssetPath = GetStringField(Object, TEXT("BoneConstraintsDataAssetPath"));
	Preset.OutputBoneConstraintsDataAssetPath = GetStringField(Object, TEXT("OutputBoneConstraintsDataAssetPath"));
	Preset.SphericalLimitsDataCount = GetIntField(Object, TEXT("SphericalLimitsDataCount"), 0);
	Preset.CapsuleLimitsDataCount = GetIntField(Object, TEXT("CapsuleLimitsDataCount"), 0);
	Preset.BoxLimitsDataCount = GetIntField(Object, TEXT("BoxLimitsDataCount"), 0);
	Preset.PlanarLimitsDataCount = GetIntField(Object, TEXT("PlanarLimitsDataCount"), 0);
	ReadObjectArrayField(Object, TEXT("CollisionLimits"), [&Preset](const FJsonObject& Child)
	{
		Preset.CollisionLimits.Add(KawaiiLimitFromJson(Child));
	});
	Preset.BoneConstraintGlobalComplianceType = GetStringField(Object, TEXT("BoneConstraintGlobalComplianceType"));
	Preset.BoneConstraintIterationCountBeforeCollision = GetIntField(Object, TEXT("BoneConstraintIterationCountBeforeCollision"), 0);
	Preset.BoneConstraintIterationCountAfterCollision = GetIntField(Object, TEXT("BoneConstraintIterationCountAfterCollision"), 0);
	Preset.bAutoAddChildDummyBoneConstraint = GetBoolField(Object, TEXT("AutoAddChildDummyBoneConstraint"), true);
	Preset.BoneConstraintCount = GetIntField(Object, TEXT("BoneConstraintCount"), 0);
	Preset.BoneConstraintsDataCount = GetIntField(Object, TEXT("BoneConstraintsDataCount"), 0);
	ReadObjectField(Object, TEXT("Gravity"), [&Preset](const FJsonObject& Child)
	{
		Preset.Gravity = VectorFromJson(Child, FVector::ZeroVector);
	});
	Preset.bEnableWind = GetBoolField(Object, TEXT("EnableWind"), false);
	Preset.WindScale = GetFloatField(Object, TEXT("WindScale"), 1.0f);
	Preset.bHasUseRelativeMove = GetBoolField(Object, TEXT("HasUseRelativeMove"), false);
	Preset.bUseRelativeMove = GetBoolField(Object, TEXT("UseRelativeMove"), false);
	ReadObjectField(Object, TEXT("MovementReferenceDisplacement"), [&Preset](const FJsonObject& Child)
	{
		Preset.MovementReferenceDisplacement = VectorFromJson(Child, FVector::ZeroVector);
	});
	Preset.bAllowWorldCollision = GetBoolField(Object, TEXT("AllowWorldCollision"), false);
	Preset.bOverrideCollisionParams = GetBoolField(Object, TEXT("OverrideCollisionParams"), false);
	Preset.bIgnoreSelfComponent = GetBoolField(Object, TEXT("IgnoreSelfComponent"), true);
	Preset.IgnoreBones = GetStringArrayField(Object, TEXT("IgnoreBones"));
	Preset.IgnoreBoneNamePrefix = GetStringArrayField(Object, TEXT("IgnoreBoneNamePrefix"));
	Preset.KawaiiPhysicsTag = GetStringField(Object, TEXT("KawaiiPhysicsTag"));
	Preset.UnsupportedSourceFields = GetStringArrayField(Object, TEXT("UnsupportedSourceFields"));
	return Preset;
}

TSharedRef<FJsonObject> PackageSpecToJson(const FNteCharacterPackageSpec& Package)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	SetStringIfNotEmpty(Object, TEXT("ModName"), Package.ModName);
	SetStringIfNotEmpty(Object, TEXT("ModsDir"), Package.ModsDir);
	SetStringIfNotEmpty(Object, TEXT("JobFilename"), Package.JobFilename);
	TArray<TSharedPtr<FJsonValue>> Assets;
	for (const FNteCharacterPackageAssetSpec& Asset : Package.Assets)
	{
		TSharedRef<FJsonObject> AssetObject = MakeShared<FJsonObject>();
		SetStringIfNotEmpty(AssetObject, TEXT("AssetPath"), Asset.AssetPath);
		AssetObject->SetStringField(TEXT("Intent"), PackageAssetIntentToString(Asset.Intent));
		Assets.Add(MakeShared<FJsonValueObject>(AssetObject));
	}
	Object->SetArrayField(TEXT("Assets"), Assets);
	Object->SetBoolField(TEXT("BuildAfterCreate"), Package.bBuildAfterCreate);
	return Object;
}

FNteCharacterPackageSpec PackageSpecFromJson(const FJsonObject& Object)
{
	FNteCharacterPackageSpec Package;
	Package.ModName = GetStringField(Object, TEXT("ModName"));
	Package.ModsDir = GetStringField(Object, TEXT("ModsDir"));
	Package.JobFilename = GetStringField(Object, TEXT("JobFilename"));
	ReadObjectArrayField(Object, TEXT("Assets"), [&Package](const FJsonObject& Child)
	{
		FNteCharacterPackageAssetSpec Asset;
		Asset.AssetPath = GetStringField(Child, TEXT("AssetPath"));
		Asset.Intent = PackageAssetIntentFromString(GetStringField(Child, TEXT("Intent")));
		Package.Assets.Add(MoveTemp(Asset));
	});
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

bool IsSupportedKawaiiSourceKind(const FString& SourceKind)
{
	return SourceKind.IsEmpty()
		|| SourceKind == TEXT("Manual")
		|| SourceKind == TEXT("ImportedJson")
		|| SourceKind == TEXT("ReferencedPreset")
		|| SourceKind == TEXT("Template");
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

FString PackageAssetIntentToString(const ENteCharacterPackageAssetIntent Intent)
{
	switch (Intent)
	{
	case ENteCharacterPackageAssetIntent::ExternalReference:
		return TEXT("ExternalReference");
	case ENteCharacterPackageAssetIntent::GeneratedAsset:
		return TEXT("GeneratedAsset");
	case ENteCharacterPackageAssetIntent::ReplacementAsset:
		return TEXT("ReplacementAsset");
	case ENteCharacterPackageAssetIntent::Invalid:
	default:
		return TEXT("Invalid");
	}
}

ENteCharacterPackageAssetIntent PackageAssetIntentFromString(const FString& Intent)
{
	if (Intent.Equals(TEXT("ExternalReference"), ESearchCase::IgnoreCase))
	{
		return ENteCharacterPackageAssetIntent::ExternalReference;
	}
	if (Intent.Equals(TEXT("GeneratedAsset"), ESearchCase::IgnoreCase))
	{
		return ENteCharacterPackageAssetIntent::GeneratedAsset;
	}
	if (Intent.Equals(TEXT("ReplacementAsset"), ESearchCase::IgnoreCase))
	{
		return ENteCharacterPackageAssetIntent::ReplacementAsset;
	}
	return ENteCharacterPackageAssetIntent::Invalid;
}

TOptional<ENteCharacterPackageAssetIntent> FindPackageAssetIntent(const FNteCharacterModSpec& Spec, const FString& AssetPath)
{
	const FString PackagePath = NormalizeSpecPackagePath(AssetPath);
	for (const FNteCharacterPackageAssetSpec& Asset : Spec.Package.Assets)
	{
		if (NormalizeSpecPackagePath(Asset.AssetPath).Equals(PackagePath, ESearchCase::IgnoreCase))
		{
			return Asset.Intent;
		}
	}
	return {};
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
	SetStringIfNotEmpty(Object, TEXT("MainPostProcessAnimBlueprintPath"), Spec.MainPostProcessAnimBlueprintPath);
	Object->SetArrayField(TEXT("AttachedMeshes"), ObjectArrayToJsonValues<FNteCharacterAttachedMeshSpec>(Spec.AttachedMeshes, AttachedMeshToJson));
	Object->SetArrayField(TEXT("MaterialOperations"), ObjectArrayToJsonValues<FNteCharacterMaterialOperationSpec>(Spec.MaterialOperations, MaterialOperationToJson));
	Object->SetArrayField(TEXT("RuntimeActions"), ObjectArrayToJsonValues<FNteCharacterRuntimeActionSpec>(Spec.RuntimeActions, RuntimeActionToJson));
	Object->SetObjectField(TEXT("RuntimeUi"), RuntimeUiToJson(Spec.RuntimeUi));
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
	if (OutSpec.Appearance.PresentationTargets.IsEmpty() && !OutSpec.Appearance.UIActorClassPath.IsEmpty())
	{
		FNteCharacterPresentationTargetSpec LegacyUiTarget;
		LegacyUiTarget.Id = TEXT("ui");
		LegacyUiTarget.BlueprintClassPath = OutSpec.Appearance.UIActorClassPath;
		LegacyUiTarget.ParentMeshComponentName = TEXT("Mesh");
		LegacyUiTarget.MainAnimBlueprintPath = OutSpec.Appearance.MainUIAnimBlueprintPath;
		OutSpec.Appearance.PresentationTargets.Add(MoveTemp(LegacyUiTarget));
	}
	OutSpec.MainMeshPath = GetStringField(Object, TEXT("MainMeshPath"));
	OutSpec.MainAnimBlueprintPath = GetStringField(Object, TEXT("MainAnimBlueprintPath"));
	OutSpec.MainPostProcessAnimBlueprintPath = GetStringField(Object, TEXT("MainPostProcessAnimBlueprintPath"));

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
	ReadObjectField(Object, TEXT("RuntimeUi"), [&OutSpec](const FJsonObject& Child)
	{
		OutSpec.RuntimeUi = RuntimeUiFromJson(Child);
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
	TArray<FString> NPCAppearanceAssetPaths;
	for (const FNteCharacterNPCAppearanceTargetSpec& Target : Spec.Appearance.NPCAppearanceTargets)
	{
		NPCAppearanceAssetPaths.Add(Target.AssetPath);
		if (Target.AssetPath.IsEmpty())
		{
			Result.Errors.Add(TEXT("NPC appearance target is missing AssetPath."));
		}
		if (Target.MainAnimBlueprintPath.IsEmpty())
		{
			Result.Errors.Add(FString::Printf(TEXT("NPC appearance target '%s' is missing MainAnimBlueprintPath."), *Target.AssetPath));
		}
	}
	AddDuplicateIdErrors(TEXT("NPC appearance target"), NPCAppearanceAssetPaths, Result);
	if (Spec.Appearance.PresentationTargets.IsEmpty() && Spec.Appearance.UIActorClassPath.IsEmpty())
	{
		Result.Warnings.Add(TEXT("Appearance.PresentationTargets is empty; no UI or world Blueprint components will be synchronized."));
	}
	TArray<FString> PresentationTargetIds;
	for (const FNteCharacterPresentationTargetSpec& Target : Spec.Appearance.PresentationTargets)
	{
		PresentationTargetIds.Add(Target.Id);
		if (Target.Id.IsEmpty())
		{
			Result.Errors.Add(TEXT("Presentation target is missing Id."));
		}
		if (Target.BlueprintClassPath.IsEmpty())
		{
			Result.Errors.Add(FString::Printf(TEXT("Presentation target '%s' is missing BlueprintClassPath."), *Target.Id));
		}
		if (Target.ParentMeshComponentName.IsEmpty())
		{
			Result.Errors.Add(FString::Printf(TEXT("Presentation target '%s' is missing ParentMeshComponentName."), *Target.Id));
		}
	}
	AddDuplicateIdErrors(TEXT("presentation target"), PresentationTargetIds, Result);
	TSet<FString> PresentationTargetIdSet(PresentationTargetIds);
	if (Spec.Appearance.PresentationTargets.IsEmpty() && !Spec.Appearance.UIActorClassPath.IsEmpty())
	{
		PresentationTargetIdSet.Add(TEXT("ui"));
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
			Result.Errors.Add(FString::Printf(TEXT("Attached mesh '%s' has no SocketName; native ArrayFashionAttachedMeshData entries require an explicit mount socket."), *AttachedMesh.Id));
		}
		if (AttachedMesh.bEnableRuntimeActions && AttachedMesh.RuntimeAnimBlueprintPath.IsEmpty())
		{
			Result.Warnings.Add(FString::Printf(TEXT("Attached mesh '%s' enables runtime actions but has no RuntimeAnimBlueprintPath."), *AttachedMesh.Id));
		}
		TSet<FString> SeenPresentationTargetIds;
		for (const FString& TargetId : AttachedMesh.PresentationTargetIds)
		{
			if (!PresentationTargetIdSet.Contains(TargetId))
			{
				Result.Errors.Add(FString::Printf(TEXT("Attached mesh '%s' references unknown presentation target '%s'."), *AttachedMesh.Id, *TargetId));
			}
			if (SeenPresentationTargetIds.Contains(TargetId))
			{
				Result.Errors.Add(FString::Printf(TEXT("Attached mesh '%s' lists presentation target '%s' more than once."), *AttachedMesh.Id, *TargetId));
			}
			SeenPresentationTargetIds.Add(TargetId);
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
		if (Operation.ExistingMaterialPath.IsEmpty() && Operation.SourceMaterialJson.IsEmpty() && Operation.ParentMaterialPath.IsEmpty())
		{
			Result.Errors.Add(FString::Printf(TEXT("Material operation '%s' needs ExistingMaterialPath, SourceMaterialJson, or ParentMaterialPath."), *Operation.Id));
		}
		if (Operation.ExistingMaterialPath.IsEmpty() && Operation.OutputMaterialPath.IsEmpty())
		{
			Result.Warnings.Add(FString::Printf(TEXT("Material operation '%s' has no OutputMaterialPath."), *Operation.Id));
		}
		if (!Operation.ExistingMaterialPath.IsEmpty() && (!Operation.SourceTextureOverrides.IsEmpty() || !Operation.OutputMaterialPath.IsEmpty()))
		{
			Result.Errors.Add(FString::Printf(TEXT("Material operation '%s' cannot combine ExistingMaterialPath with generated-material output or texture overrides."), *Operation.Id));
		}
	}
	AddDuplicateIdErrors(TEXT("material operation"), MaterialOperationIds, Result);

	TArray<FString> RuntimeActionIds;
	TSet<FString> Hotkeys;
	if (Spec.RuntimeUi.bEnableUi)
	{
		if (!Spec.RuntimeUi.ToggleUiHotkey.IsEmpty())
		{
			FString NormalizedUiHotkey;
			FString UiHotkeyError;
			if (!ValidateRuntimeHotkey(Spec.RuntimeUi.ToggleUiHotkey, NormalizedUiHotkey, UiHotkeyError))
			{
				Result.Errors.Add(FString::Printf(TEXT("Runtime UI: %s"), *UiHotkeyError));
			}
			else if (!NormalizedUiHotkey.IsEmpty())
			{
				Hotkeys.Add(NormalizedUiHotkey);
			}
		}
	}
	else if (!Spec.RuntimeUi.ToggleUiHotkey.IsEmpty())
	{
		Result.Warnings.Add(TEXT("RuntimeUi.ToggleUiHotkey is set but RuntimeUi.EnableUi=false; the UI hotkey will be ignored."));
	}

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
		if (!Action.HostMeshId.IsEmpty() && !HasMeshTargetId(Spec, Action.HostMeshId))
		{
			Result.Errors.Add(FString::Printf(TEXT("Runtime action '%s' uses unknown HostMeshId '%s'."), *Action.Id, *Action.HostMeshId));
		}
		const ENteCharacterRuntimeActionType RuntimeActionType = RuntimeActionTypeFromString(Action.ActionType);
		if (!IsSupportedRuntimeActionType(RuntimeActionType))
		{
			Result.Errors.Add(FString::Printf(
				TEXT("Runtime action '%s' uses unsupported ActionType '%s'. Supported values: MaterialSlotVisibility, AttachedMeshVisibility."),
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

		if (RuntimeActionType == ENteCharacterRuntimeActionType::MaterialSlotVisibility && Action.MaterialSlots.IsEmpty())
		{
			Result.Errors.Add(FString::Printf(TEXT("Runtime action '%s' has no MaterialSlots."), *Action.Id));
		}
		const FString EffectiveHostMeshId = Action.HostMeshId.IsEmpty() ? TEXT("main") : Action.HostMeshId;
		const bool bTargetIsAttachedMesh = !IsMainMeshId(Action.TargetMeshId);
		const bool bHostDiffersFromTarget = !EffectiveHostMeshId.Equals(Action.TargetMeshId, ESearchCase::IgnoreCase)
			&& !(IsMainMeshId(EffectiveHostMeshId) && IsMainMeshId(Action.TargetMeshId));
		const bool bGeneratedActionNeedsComponentLookup = IsSupportedRuntimeActionType(RuntimeActionType);
		if (bTargetIsAttachedMesh && bHostDiffersFromTarget && bGeneratedActionNeedsComponentLookup)
		{
			const FNteCharacterAttachedMeshSpec* AttachedMesh = FindAttachedMeshById(Spec, Action.TargetMeshId);
			if (AttachedMesh && !HasAnyRuntimeComponentTag(Action, AttachedMesh))
			{
				Result.Errors.Add(FString::Printf(
					TEXT("Runtime action '%s' targets attached mesh '%s' from host '%s', but neither TargetComponentTags nor the attached mesh MeshComponentOwnedTags are set."),
					*Action.Id,
					*Action.TargetMeshId,
					*EffectiveHostMeshId));
			}
		}
		if (RuntimeActionType == ENteCharacterRuntimeActionType::AttachedMeshVisibility)
		{
			const FNteCharacterAttachedMeshSpec* AttachedMesh = FindAttachedMeshById(Spec, Action.TargetMeshId);
			if (!AttachedMesh)
			{
				Result.Errors.Add(FString::Printf(TEXT("Runtime action '%s' is AttachedMeshVisibility but TargetMeshId '%s' is not an attached mesh id."), *Action.Id, *Action.TargetMeshId));
			}
		}
	}
	AddDuplicateIdErrors(TEXT("runtime action"), RuntimeActionIds, Result);

	TArray<FString> KawaiiPresetIds;
	for (const FNteCharacterKawaiiPresetSpec& Preset : Spec.KawaiiPresets)
	{
		KawaiiPresetIds.Add(Preset.Id);
	}
	for (const FNteCharacterKawaiiPresetSpec& Preset : Spec.KawaiiPresets)
	{
		if (Preset.Id.IsEmpty())
		{
			Result.Errors.Add(TEXT("Kawaii preset is missing Id."));
		}
		if (!HasMeshTargetId(Spec, Preset.TargetMeshId))
		{
			Result.Errors.Add(FString::Printf(TEXT("Kawaii preset '%s' targets unknown mesh id '%s'."), *Preset.Id, *Preset.TargetMeshId));
		}
		if (!IsSupportedKawaiiSourceKind(Preset.SourceKind))
		{
			Result.Errors.Add(FString::Printf(TEXT("Kawaii preset '%s' uses unsupported SourceKind '%s'. Supported values: Manual, ImportedJson, ReferencedPreset, Template."), *Preset.Id, *Preset.SourceKind));
		}
		if (Preset.SourceKind == TEXT("ReferencedPreset"))
		{
			if (Preset.ReferencedPresetId.IsEmpty())
			{
				Result.Errors.Add(FString::Printf(TEXT("Kawaii preset '%s' has SourceKind=ReferencedPreset but no ReferencedPresetId."), *Preset.Id));
			}
			else if (Preset.ReferencedPresetId == Preset.Id)
			{
				Result.Errors.Add(FString::Printf(TEXT("Kawaii preset '%s' cannot reference itself."), *Preset.Id));
			}
			else if (!KawaiiPresetIds.Contains(Preset.ReferencedPresetId))
			{
				Result.Errors.Add(FString::Printf(TEXT("Kawaii preset '%s' references unknown preset id '%s'."), *Preset.Id, *Preset.ReferencedPresetId));
			}
		}
		if (Preset.SourceKind == TEXT("Template") && Preset.TemplateKind.IsEmpty())
		{
			Result.Warnings.Add(FString::Printf(TEXT("Kawaii preset '%s' has SourceKind=Template but no TemplateKind."), *Preset.Id));
		}
		if ((Preset.SourceKind == TEXT("ImportedJson") || !Preset.SourceAnimBlueprintJson.IsEmpty()) && Preset.SourceNodeName.IsEmpty())
		{
			Result.Warnings.Add(FString::Printf(TEXT("Kawaii preset '%s' was imported from JSON but has no SourceNodeName."), *Preset.Id));
		}
		if (Preset.RootBone.IsEmpty())
		{
			Result.Warnings.Add(FString::Printf(TEXT("Kawaii preset '%s' has no RootBone."), *Preset.Id));
		}
		TSet<FString> RemappedSourceBones;
		TSet<FString> RemappedTargetBones;
		for (const FNteCharacterKawaiiBoneRemapSpec& Remap : Preset.BoneRemaps)
		{
			if (Remap.SourceBone.IsEmpty() || Remap.TargetBone.IsEmpty())
			{
				Result.Errors.Add(FString::Printf(TEXT("Kawaii preset '%s' has a BoneRemaps entry with an empty SourceBone or TargetBone."), *Preset.Id));
				continue;
			}
			if (RemappedSourceBones.Contains(Remap.SourceBone))
			{
				Result.Errors.Add(FString::Printf(TEXT("Kawaii preset '%s' maps source bone '%s' more than once."), *Preset.Id, *Remap.SourceBone));
			}
			if (RemappedTargetBones.Contains(Remap.TargetBone))
			{
				Result.Errors.Add(FString::Printf(TEXT("Kawaii preset '%s' maps more than one source bone to target bone '%s'."), *Preset.Id, *Remap.TargetBone));
			}
			RemappedSourceBones.Add(Remap.SourceBone);
			RemappedTargetBones.Add(Remap.TargetBone);
		}
		for (const FNteCharacterKawaiiAdditionalRootBoneSpec& AdditionalRootBone : Preset.AdditionalRootBones)
		{
			if (AdditionalRootBone.RootBone.IsEmpty())
			{
				Result.Errors.Add(FString::Printf(TEXT("Kawaii preset '%s' has an AdditionalRootBones entry with no RootBone."), *Preset.Id));
			}
			if (AdditionalRootBone.bUseOverrideExcludeBones && AdditionalRootBone.OverrideExcludeBones.IsEmpty())
			{
				Result.Warnings.Add(FString::Printf(TEXT("Kawaii preset '%s' additional root '%s' enables override exclude bones but has no OverrideExcludeBones."), *Preset.Id, *AdditionalRootBone.RootBone));
			}
		}
		for (const FNteCharacterKawaiiLimitSpec& Limit : Preset.CollisionLimits)
		{
			if (Limit.LimitKind.IsEmpty())
			{
				Result.Errors.Add(FString::Printf(TEXT("Kawaii preset '%s' has a CollisionLimits entry with no LimitKind."), *Preset.Id));
			}
			if (Limit.DrivingBone.IsEmpty())
			{
				Result.Warnings.Add(FString::Printf(TEXT("Kawaii preset '%s' %s limit has no DrivingBone."), *Preset.Id, *Limit.LimitKind));
			}
		}
		for (const FNteCharacterKawaiiCurveSpec& Curve : Preset.Curves)
		{
			if (Curve.CurveKind.IsEmpty())
			{
				Result.Errors.Add(FString::Printf(TEXT("Kawaii preset '%s' has a Curves entry with no CurveKind."), *Preset.Id));
			}
		}
	}
	AddDuplicateIdErrors(TEXT("Kawaii preset"), KawaiiPresetIds, Result);

	TMap<FString, ENteCharacterPackageAssetIntent> PackageAssetIntents;
	for (const FNteCharacterPackageAssetSpec& Asset : Spec.Package.Assets)
	{
		const FString PackagePath = NormalizeSpecPackagePath(Asset.AssetPath);
		if (!PackagePath.StartsWith(TEXT("/Game/")) || PackagePath.Contains(TEXT(".")))
		{
			Result.Errors.Add(FString::Printf(TEXT("Package asset has an invalid /Game package path: %s"), *Asset.AssetPath));
			continue;
		}
		if (Asset.Intent == ENteCharacterPackageAssetIntent::Invalid)
		{
			Result.Errors.Add(FString::Printf(TEXT("Package asset '%s' has an invalid Intent."), *Asset.AssetPath));
			continue;
		}
		if (const ENteCharacterPackageAssetIntent* ExistingIntent = PackageAssetIntents.Find(PackagePath))
		{
			if (*ExistingIntent != Asset.Intent)
			{
				Result.Errors.Add(FString::Printf(TEXT("Package asset '%s' has conflicting intents."), *PackagePath));
			}
			else
			{
				Result.Warnings.Add(FString::Printf(TEXT("Package asset '%s' is listed more than once."), *PackagePath));
			}
		}
		else
		{
			PackageAssetIntents.Add(PackagePath, Asset.Intent);
		}
	}

	return Result;
}

TArray<FString> CollectCharacterModSpecPackageSeeds(const FNteCharacterModSpec& Spec)
{
	TArray<FString> Seeds;
	const auto AddOwnedAsset = [&Spec, &Seeds](const FString& AssetPath)
	{
		const TOptional<ENteCharacterPackageAssetIntent> Intent = FindPackageAssetIntent(Spec, AssetPath);
		if (!Intent.IsSet() || Intent.GetValue() != ENteCharacterPackageAssetIntent::ExternalReference)
		{
			AddPackageSeed(Seeds, AssetPath);
		}
	};

	AddOwnedAsset(Spec.Appearance.PlayerAppearanceAssetPath);
	for (const FNteCharacterNPCAppearanceTargetSpec& Target : Spec.Appearance.NPCAppearanceTargets)
	{
		AddOwnedAsset(Target.AssetPath);
	}
	for (const FNteCharacterPresentationTargetSpec& Target : Spec.Appearance.PresentationTargets)
	{
		AddOwnedAsset(Target.BlueprintClassPath);
	}
	if (Spec.Appearance.PresentationTargets.IsEmpty())
	{
		AddOwnedAsset(Spec.Appearance.UIActorClassPath);
	}
	AddOwnedAsset(Spec.MainMeshPath);

	for (const FNteCharacterAttachedMeshSpec& AttachedMesh : Spec.AttachedMeshes)
	{
		AddOwnedAsset(AttachedMesh.MeshPath);
	}

	for (const FNteCharacterPackageAssetSpec& Asset : Spec.Package.Assets)
	{
		if (Asset.Intent == ENteCharacterPackageAssetIntent::GeneratedAsset
			|| Asset.Intent == ENteCharacterPackageAssetIntent::ReplacementAsset)
		{
			AddPackageSeed(Seeds, Asset.AssetPath);
		}
	}

	Seeds.Sort();
	return Seeds;
}
}
