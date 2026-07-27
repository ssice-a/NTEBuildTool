// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

namespace NTEBuildTool
{
struct FFModelKawaiiCurveReference
{
	FString ExternalCurveObjectName;
	FString ExternalCurveObjectPath;
	int32 InlineKeyCount = 0;
};

struct FFModelKawaiiPhysicsSettings
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

struct FFModelKawaiiRootBoneSetting
{
	FName RootBone;
	TArray<FName> OverrideExcludeBones;
	bool bUseOverrideExcludeBones = false;
};

struct FFModelKawaiiCollisionLimit
{
	FString LimitKind;
	FName DrivingBone;
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

struct FFModelKawaiiNodeAnalysis
{
	FString GraphNodeName;
	FName RootBone;
	TArray<FName> ExcludeBones;
	TArray<FFModelKawaiiRootBoneSetting> AdditionalRootBones;

	float DummyBoneLength = 0.0f;
	FString BoneForwardAxis;
	FFModelKawaiiPhysicsSettings PhysicsSettings;

	int32 TargetFramerate = 60;
	bool bOverrideTargetFramerate = false;
	int32 WarmUpFrames = 0;
	bool bUseWarmUpWhenResetDynamics = true;
	bool bNeedWarmUp = false;
	float TeleportDistanceThreshold = 0.0f;
	float TeleportRotationThreshold = 0.0f;
	FString PlanarConstraint;
	bool bResetBoneTransformWhenBoneNotFound = false;

	FFModelKawaiiCurveReference DampingCurve;
	FFModelKawaiiCurveReference StiffnessCurve;
	FFModelKawaiiCurveReference WorldDampingLocationCurve;
	FFModelKawaiiCurveReference WorldDampingRotationCurve;
	FFModelKawaiiCurveReference RadiusCurve;
	FFModelKawaiiCurveReference LimitAngleCurve;

	TArray<FFModelKawaiiCollisionLimit> SphericalLimits;
	TArray<FFModelKawaiiCollisionLimit> CapsuleLimits;
	TArray<FFModelKawaiiCollisionLimit> BoxLimits;
	TArray<FFModelKawaiiCollisionLimit> PlanarLimits;

	FString LimitsDataAssetPath;
	FString PhysicsAssetForLimitsPath;
	int32 SphericalLimitsDataCount = 0;
	int32 CapsuleLimitsDataCount = 0;
	int32 BoxLimitsDataCount = 0;
	int32 PlanarLimitsDataCount = 0;

	FString BoneConstraintGlobalComplianceType;
	int32 BoneConstraintIterationCountBeforeCollision = 0;
	int32 BoneConstraintIterationCountAfterCollision = 0;
	bool bAutoAddChildDummyBoneConstraint = true;
	int32 BoneConstraintCount = 0;
	FString BoneConstraintsDataAssetPath;
	int32 BoneConstraintsDataCount = 0;

	FVector Gravity = FVector::ZeroVector;
	bool bEnableWind = false;
	float WindScale = 1.0f;
	bool bUseRelativeMove = false;
	bool bHasUseRelativeMove = false;
	FVector MovementReferenceDisplacement = FVector::ZeroVector;
	bool bAllowWorldCollision = false;
	bool bOverrideCollisionParams = false;
	bool bIgnoreSelfComponent = true;
	TArray<FName> IgnoreBones;
	TArray<FName> IgnoreBoneNamePrefix;
	FString KawaiiPhysicsTag;

	TArray<FString> UnsupportedFields;
};

struct FFModelKawaiiAnimLayerAnalysis
{
	FString GeneratedClassName;
	FString ClassDefaultObjectName;
	TArray<FFModelKawaiiNodeAnalysis> KawaiiNodes;
};

bool AnalyzeFModelKawaiiAnimLayerJson(const TArray<TSharedPtr<FJsonValue>>& RootArray, FFModelKawaiiAnimLayerAnalysis& OutAnalysis, FString& OutError);
}
