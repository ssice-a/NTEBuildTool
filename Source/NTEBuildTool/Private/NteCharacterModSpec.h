// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace NTEBuildTool::Character
{
struct FNteCharacterPresentationTargetSpec
{
	FString Id;
	FString BlueprintClassPath;
	FString ParentMeshComponentName;
	FString MainAnimBlueprintPath;
	bool bConfigureMainMesh = true;
};

struct FNteCharacterNPCAppearanceTargetSpec
{
	FString AssetPath;
	FString MainAnimBlueprintPath;
};

struct FNteCharacterAppearanceTarget
{
	FString AppearanceRowName;
	FString PlayerAppearanceAssetPath;
	TArray<FNteCharacterNPCAppearanceTargetSpec> NPCAppearanceTargets;
	TArray<FNteCharacterPresentationTargetSpec> PresentationTargets;
	// Legacy JSON compatibility. New specs should use PresentationTargets.
	FString UIActorClassPath;
	FString MainUIAnimBlueprintPath;
	TOptional<float> CapsuleHalfHeight;
	TOptional<float> CapsuleRadius;
	TOptional<FVector> RelativeLocation;
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
	FString PresentationSocketName;
	TArray<FString> MeshComponentOwnedTags;
	TArray<FString> PresentationTargetIds;
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
	FString ExistingMaterialPath;
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
	FString HostMeshId;
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

struct FNteCharacterRuntimeUiSpec
{
	bool bEnableUi = false;
	FString ToggleUiHotkey;
	FString Title;
	bool bDefaultVisible = false;
	FString StyleProfileId = TEXT("NTE.Common.DarkButton");
	FString FontPath;
	FString BodyFontTypeface = TEXT("Light");
	FString TitleFontTypeface = TEXT("Heavy");
	int32 BodyFontSize = 20;
	int32 TitleFontSize = 24;
	FString ButtonNormalTexturePath;
	FString ButtonHoveredTexturePath;
	FString ButtonPressedTexturePath;
	FString ButtonDisabledTexturePath;
};

struct FNteCharacterKawaiiAdditionalRootBoneSpec
{
	FString RootBone;
	TArray<FString> OverrideExcludeBones;
	bool bUseOverrideExcludeBones = false;
};

struct FNteCharacterKawaiiBoneRemapSpec
{
	FString SourceBone;
	FString TargetBone;
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
	FVector Extent = FVector::ZeroVector;
	FPlane Plane = FPlane(0, 0, 0, 0);
	FString LimitType;
	FString SourceType;
	bool bEnable = true;
};

struct FNteCharacterKawaiiCurveSpec
{
	FString CurveKind;
	FString ExternalCurveObjectName;
	FString ExternalCurveObjectPath;
	int32 InlineKeyCount = 0;
	FString OutputCurvePath;
};

struct FNteCharacterKawaiiPresetSpec
{
	FString Id;
	FString Label;
	FString TargetMeshId;
	FString SourceKind;
	FString SourceAnimBlueprintJson;
	FString SourceGeneratedClassName;
	FString SourceClassDefaultObjectName;
	FString SourceNodeName;
	FString ReferencedPresetId;
	FString TemplateKind;
	FString SchemaStatus;
	TArray<FNteCharacterKawaiiBoneRemapSpec> BoneRemaps;
	FString RootBone;
	TArray<FString> ExcludeBones;
	TArray<FNteCharacterKawaiiAdditionalRootBoneSpec> AdditionalRootBones;
	FNteCharacterKawaiiPhysicsSettingsSpec PhysicsSettings;
	float DummyBoneLength = 0.0f;
	FString BoneForwardAxis;
	int32 TargetFramerate = 60;
	bool bOverrideTargetFramerate = false;
	int32 WarmUpFrames = 0;
	bool bUseWarmUpWhenResetDynamics = true;
	bool bNeedWarmUp = false;
	float TeleportDistanceThreshold = 0.0f;
	float TeleportRotationThreshold = 0.0f;
	FString PlanarConstraint;
	bool bResetBoneTransformWhenBoneNotFound = false;
	TArray<FNteCharacterKawaiiCurveSpec> Curves;
	FString RuntimeAnimBlueprintPath;
	FString LimitsDataAssetPath;
	FString PhysicsAssetForLimitsPath;
	FString OutputLimitsDataAssetPath;
	FString BoneConstraintsDataAssetPath;
	FString OutputBoneConstraintsDataAssetPath;
	int32 SphericalLimitsDataCount = 0;
	int32 CapsuleLimitsDataCount = 0;
	int32 BoxLimitsDataCount = 0;
	int32 PlanarLimitsDataCount = 0;
	TArray<FNteCharacterKawaiiLimitSpec> CollisionLimits;
	FString BoneConstraintGlobalComplianceType;
	int32 BoneConstraintIterationCountBeforeCollision = 0;
	int32 BoneConstraintIterationCountAfterCollision = 0;
	bool bAutoAddChildDummyBoneConstraint = true;
	int32 BoneConstraintCount = 0;
	int32 BoneConstraintsDataCount = 0;
	FVector Gravity = FVector::ZeroVector;
	bool bEnableWind = false;
	float WindScale = 1.0f;
	bool bHasUseRelativeMove = false;
	bool bUseRelativeMove = false;
	FVector MovementReferenceDisplacement = FVector::ZeroVector;
	bool bAllowWorldCollision = false;
	bool bOverrideCollisionParams = false;
	bool bIgnoreSelfComponent = true;
	TArray<FString> IgnoreBones;
	TArray<FString> IgnoreBoneNamePrefix;
	FString KawaiiPhysicsTag;
	TArray<FString> UnsupportedSourceFields;
};

enum class ENteCharacterPackageAssetIntent : uint8
{
	ExternalReference,
	GeneratedAsset,
	ReplacementAsset,
	Invalid
};

struct FNteCharacterPackageAssetSpec
{
	FString AssetPath;
	ENteCharacterPackageAssetIntent Intent = ENteCharacterPackageAssetIntent::ExternalReference;
};

struct FNteCharacterPackageSpec
{
	FString ModName;
	FString ModsDir;
	FString JobFilename;
	TArray<FNteCharacterPackageAssetSpec> Assets;
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
	FString MainPostProcessAnimBlueprintPath;
	TArray<FNteCharacterAttachedMeshSpec> AttachedMeshes;
	TArray<FNteCharacterMaterialOperationSpec> MaterialOperations;
	TArray<FNteCharacterRuntimeActionSpec> RuntimeActions;
	FNteCharacterRuntimeUiSpec RuntimeUi;
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
FString PackageAssetIntentToString(ENteCharacterPackageAssetIntent Intent);
ENteCharacterPackageAssetIntent PackageAssetIntentFromString(const FString& Intent);
TOptional<ENteCharacterPackageAssetIntent> FindPackageAssetIntent(const FNteCharacterModSpec& Spec, const FString& AssetPath);
}
