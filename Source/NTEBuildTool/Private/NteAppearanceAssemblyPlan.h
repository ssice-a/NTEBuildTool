// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace NTEBuildTool::Character
{
struct FNteCharacterModSpec;

struct FNteAppearancePresentationTargetPlan
{
	FString Id;
	FString BlueprintClassPath;
	FString ParentMeshComponentName;
	FString MainAnimInstancePath;
	bool bConfigureMainMesh = true;
};

struct FNteNPCAppearanceTargetPlan
{
	FString AssetPath;
	FString MainAnimInstancePath;
};

struct FNteAppearanceMeshDataPlan
{
	FString Id;
	FString Label;
	FString CharacterMeshPath;
	FString AnimInstancePath;
	FString MobileAnimInstancePath;
	FString UIAnimInstancePath;
	FString SocketName;
	FString PresentationSocketName;
	TArray<FString> MeshComponentOwnedTags;
	TArray<FString> PresentationTargetIds;
	FVector RelativeLocation = FVector::ZeroVector;
	FRotator RelativeRotation = FRotator::ZeroRotator;
	FVector RelativeScale3D = FVector::OneVector;
	bool bSyncToUIShow = true;
	bool bRuntimeActionsEnabled = false;
};

struct FNteAppearanceAssemblyPlan
{
	FString PlayerAppearanceAssetPath;
	TArray<FNteNPCAppearanceTargetPlan> NPCAppearanceTargets;
	TArray<FNteAppearancePresentationTargetPlan> PresentationTargets;
	TOptional<float> CapsuleHalfHeight;
	TOptional<float> CapsuleRadius;
	TOptional<FVector> RelativeLocation;
	FNteAppearanceMeshDataPlan MainMesh;
	TArray<FNteAppearanceMeshDataPlan> AttachedMeshes;
	TArray<FString> Errors;
	TArray<FString> Warnings;
};

FNteAppearanceAssemblyPlan BuildAppearanceAssemblyPlanFromSpec(const FNteCharacterModSpec& Spec);
TSharedRef<FJsonObject> AppearanceAssemblyPlanToJson(const FNteAppearanceAssemblyPlan& Plan);
}
