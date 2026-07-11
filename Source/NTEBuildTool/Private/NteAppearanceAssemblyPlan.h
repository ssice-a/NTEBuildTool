// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace NTEBuildTool::Character
{
struct FNteCharacterModSpec;

struct FNteAppearanceMeshDataPlan
{
	FString Id;
	FString Label;
	FString CharacterMeshPath;
	FString AnimInstancePath;
	FString MobileAnimInstancePath;
	FString UIAnimInstancePath;
	FString SocketName;
	FVector RelativeLocation = FVector::ZeroVector;
	FRotator RelativeRotation = FRotator::ZeroRotator;
	FVector RelativeScale3D = FVector::OneVector;
	bool bSyncToUIShow = true;
	bool bRuntimeActionsEnabled = false;
};

struct FNteAppearanceAssemblyPlan
{
	FString PlayerAppearanceAssetPath;
	FString UIActorClassPath;
	FNteAppearanceMeshDataPlan MainMesh;
	TArray<FNteAppearanceMeshDataPlan> AttachedMeshes;
	TArray<FString> Errors;
	TArray<FString> Warnings;
};

FNteAppearanceAssemblyPlan BuildAppearanceAssemblyPlanFromSpec(const FNteCharacterModSpec& Spec);
TSharedRef<FJsonObject> AppearanceAssemblyPlanToJson(const FNteAppearanceAssemblyPlan& Plan);
}
