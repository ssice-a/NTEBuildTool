// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace NTEBuildTool::Character
{
struct FNteCharacterAppearanceTarget
{
	FString AppearanceRowName;
	FString PlayerAppearanceAssetPath;
	FString UIActorClassPath;
};

struct FNteCharacterAttachedMeshSpec
{
	FString Id;
	FString Label;
	FString MeshPath;
	FString AnimBlueprintPath;
	FString MobileAnimBlueprintPath;
	FString UIAnimBlueprintPath;
	FString RuntimeAnimBlueprintPath;
	FString SocketName;
	TArray<FString> MeshComponentOwnedTags;
	FVector RelativeLocation = FVector::ZeroVector;
	FRotator RelativeRotation = FRotator::ZeroRotator;
	FVector RelativeScale = FVector::OneVector;
	FString KawaiiPresetId;
	bool bSyncToUIShow = true;
	bool bEnableRuntimeActions = false;
};

struct FNteCharacterMaterialOperationSpec
{
	FString Id;
	FString TargetMeshId;
	int32 SlotIndex = INDEX_NONE;
	FString SlotName;
	FString SourceMaterialJson;
	FString ParentMaterialPath;
	FString OutputMaterialPath;
	TMap<FString, FString> SourceTextureOverrides;
	bool bAssignToSlot = true;
};

struct FNteCharacterRuntimeActionSpec
{
	FString Id;
	FString Label;
	FString Hotkey;
	FString TargetMeshId;
	FString ActionType = TEXT("MaterialSlotVisibility");
	TArray<FString> TargetComponentTags;
	TArray<int32> MaterialSlots;
	FString MaterialPath;
	FString ParameterName;
	float ScalarValue = 0.0f;
	FLinearColor VectorValue = FLinearColor::White;
	FString MorphTargetName;
	float MorphValue = 0.0f;
	bool bDefaultEnabled = true;
};

struct FNteCharacterKawaiiAdditionalRootBoneSpec
{
	FString RootBone;
	TArray<FString> OverrideExcludeBones;
	bool bUseOverrideExcludeBones = false;
};

struct FNteCharacterKawaiiPhysicsSettingsSpec
{
	float Damping = 0.0f;
	float Stiffness = 0.0f;
	float WorldDampingLocation = 0.0f;
	float WorldDampingRotation = 0.0f;
	float Radius = 0.0f;
	float LimitAngle = 0.0f;
	float ForwardMoveOffset = 0.0f;
	bool bHasForwardMoveOffset = false;
};

struct FNteCharacterKawaiiLimitSpec
{
	FString LimitKind;
	FString DrivingBone;
	FVector OffsetLocation = FVector::ZeroVector;
	FRotator OffsetRotation = FRotator::ZeroRotator;
	float Radius = 0.0f;
	float Length = 0.0f;
	float SphereRadius = 0.0f;
	FString LimitType;
	FString SourceType;
	bool bEnable = true;
};

struct FNteCharacterKawaiiPresetSpec
{
	FString Id;
	FString Label;
	FString TargetMeshId;
	FString SourceAnimBlueprintJson;
	FString SourceNodeName;
	FString SchemaStatus;
	FString RootBone;
	TArray<FString> ExcludeBones;
	TArray<FNteCharacterKawaiiAdditionalRootBoneSpec> AdditionalRootBones;
	FNteCharacterKawaiiPhysicsSettingsSpec PhysicsSettings;
	float DummyBoneLength = 0.0f;
	FString BoneForwardAxis;
	FString PlanarConstraint;
	FString LimitsDataAssetPath;
	FString PhysicsAssetForLimitsPath;
	FString BoneConstraintsDataAssetPath;
	TArray<FNteCharacterKawaiiLimitSpec> CollisionLimits;
	FVector Gravity = FVector::ZeroVector;
	bool bEnableWind = false;
	float WindScale = 1.0f;
	bool bUseRelativeMove = false;
	TArray<FString> IgnoreBones;
	TArray<FString> IgnoreBoneNamePrefix;
	FString KawaiiPhysicsTag;
	TArray<FString> UnsupportedSourceFields;
};

struct FNteCharacterPackageSpec
{
	FString ModName;
	FString ModsDir;
	FString JobFilename;
	bool bBuildAfterCreate = false;
};

struct FNteCharacterModSpec
{
	FString Format = TEXT("NTE.CharacterModSpec");
	int32 Version = 1;
	FString WorkspaceName;
	FNteCharacterAppearanceTarget Appearance;
	FString MainMeshPath;
	FString MainAnimBlueprintPath;
	TArray<FNteCharacterAttachedMeshSpec> AttachedMeshes;
	TArray<FNteCharacterMaterialOperationSpec> MaterialOperations;
	TArray<FNteCharacterRuntimeActionSpec> RuntimeActions;
	TArray<FNteCharacterKawaiiPresetSpec> KawaiiPresets;
	FNteCharacterPackageSpec Package;
};

struct FNteCharacterModSpecValidationResult
{
	TArray<FString> Errors;
	TArray<FString> Warnings;

	bool HasErrors() const { return !Errors.IsEmpty(); }
	bool HasWarnings() const { return !Warnings.IsEmpty(); }
};

TSharedRef<FJsonObject> CharacterModSpecToJson(const FNteCharacterModSpec& Spec);
bool CharacterModSpecFromJson(const FJsonObject& Object, FNteCharacterModSpec& OutSpec, FString& OutError);
bool LoadCharacterModSpecFromJsonFile(const FString& Filename, FNteCharacterModSpec& OutSpec, FString& OutError);
bool SaveCharacterModSpecToJsonFile(const FNteCharacterModSpec& Spec, const FString& Filename, FString& OutError);
FNteCharacterModSpecValidationResult ValidateCharacterModSpec(const FNteCharacterModSpec& Spec);
TArray<FString> CollectCharacterModSpecPackageSeeds(const FNteCharacterModSpec& Spec);
}
