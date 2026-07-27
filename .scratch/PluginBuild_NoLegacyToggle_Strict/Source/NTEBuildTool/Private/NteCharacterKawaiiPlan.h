// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace NTEBuildTool::Character
{
struct FNteCharacterModSpec;

struct FNteCharacterKawaiiCurvePlanItem
{
	FString CurveKind;
	FString ExternalCurveObjectName;
	FString ExternalCurveObjectPath;
	int32 InlineKeyCount = 0;
	FString OutputCurvePath;
	bool bDerivedOutputCurvePath = false;
	TArray<FString> Errors;
	TArray<FString> Warnings;
};

struct FNteCharacterKawaiiAdditionalRootBonePlanItem
{
	FString RootBone;
	TArray<FString> OverrideExcludeBones;
	bool bUseOverrideExcludeBones = false;
};

struct FNteCharacterKawaiiPhysicsSettingsPlanItem
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

struct FNteCharacterKawaiiLimitPlanItem
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

struct FNteCharacterKawaiiPresetPlanItem
{
	FString Id;
	FString Label;
	FString TargetMeshId;
	FString TargetKind;
	FString TargetMeshPath;
	bool bTargetMeshLoaded = false;
	FString TargetSkeletonPath;
	TArray<FString> ReferencedBones;
	TArray<FString> MissingBones;
	FString SourceKind;
	FString SourceAnimBlueprintJson;
	FString SourceGeneratedClassName;
	FString SourceClassDefaultObjectName;
	FString SourceNodeName;
	FString ReferencedPresetId;
	FString TemplateKind;
	FString SchemaStatus;
	FString RootBone;
	TArray<FString> ExcludeBones;
	TArray<FNteCharacterKawaiiAdditionalRootBonePlanItem> AdditionalRootBones;
	FNteCharacterKawaiiPhysicsSettingsPlanItem PhysicsSettings;
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
	int32 AdditionalRootBoneCount = 0;
	int32 CollisionLimitCount = 0;
	int32 CurveCount = 0;
	int32 SphericalLimitsDataCount = 0;
	int32 CapsuleLimitsDataCount = 0;
	int32 BoxLimitsDataCount = 0;
	int32 PlanarLimitsDataCount = 0;
	int32 BoneConstraintCount = 0;
	int32 BoneConstraintsDataCount = 0;
	FString KawaiiAssetRootPath;
	FString RuntimeAnimBlueprintPath;
	bool bDerivedRuntimeAnimBlueprintPath = false;
	FString LimitsDataAssetPath;
	FString PhysicsAssetForLimitsPath;
	FString OutputLimitsDataAssetPath;
	bool bDerivedOutputLimitsDataAssetPath = false;
	FString BoneConstraintsDataAssetPath;
	FString OutputBoneConstraintsDataAssetPath;
	bool bDerivedOutputBoneConstraintsDataAssetPath = false;
	TArray<FNteCharacterKawaiiCurvePlanItem> Curves;
	TArray<FNteCharacterKawaiiLimitPlanItem> CollisionLimits;
	FString BoneConstraintGlobalComplianceType;
	int32 BoneConstraintIterationCountBeforeCollision = 0;
	int32 BoneConstraintIterationCountAfterCollision = 0;
	bool bAutoAddChildDummyBoneConstraint = true;
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
	bool bKawaiiPhysicsTagChecked = false;
	bool bKawaiiPhysicsTagValid = false;
	TArray<FString> PackageSeeds;
	TArray<FString> Errors;
	TArray<FString> Warnings;
};

struct FNteCharacterKawaiiPlan
{
	FString KawaiiAssetRootPath;
	TArray<FNteCharacterKawaiiPresetPlanItem> Presets;
	TArray<FString> PackageSeeds;
	TArray<FString> Errors;
	TArray<FString> Warnings;
};

FNteCharacterKawaiiPlan BuildCharacterKawaiiPlanFromSpec(const FNteCharacterModSpec& Spec);
TSharedRef<FJsonObject> CharacterKawaiiPlanToJson(const FNteCharacterKawaiiPlan& Plan);
TArray<FString> CollectCharacterKawaiiPlanPackageSeeds(const FNteCharacterKawaiiPlan& Plan);
}
