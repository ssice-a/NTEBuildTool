// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace NTEBuildTool::Character
{
struct FNteCharacterModSpec;

struct FNteCharacterRuntimeActionPlanItem
{
	FString Id;
	FString Label;
	FString ActionType;
	FString Hotkey;
	FString TargetMeshId;
	FString TargetKind;
	FString TargetMeshPath;
	FString HostMeshId;
	FString HostKind;
	FString HostAnimBlueprintPath;
	FString TargetLookupMode;
	TArray<FString> TargetComponentTags;
	TArray<int32> MaterialSlots;
	FString MaterialPath;
	FString ParameterName;
	float ScalarValue = 0.0f;
	FLinearColor VectorValue = FLinearColor::White;
	FString MorphTargetName;
	float MorphValue = 0.0f;
	bool bDefaultEnabled = true;
	bool bFirstSliceBlueprintSupported = false;
	TArray<FString> Errors;
	TArray<FString> Warnings;
};

struct FNteCharacterRuntimeActionHostPlan
{
	FString MeshId;
	FString HostKind;
	FString AnimBlueprintPath;
	TArray<FString> ActionIds;
	TArray<FString> Errors;
	TArray<FString> Warnings;
};

struct FNteCharacterRuntimeActionPlan
{
	FString RuntimeAssetRootPath;
	FString WidgetBlueprintPath;
	FString SaveGameBlueprintPath;
	TArray<FNteCharacterRuntimeActionPlanItem> Actions;
	TArray<FNteCharacterRuntimeActionHostPlan> Hosts;
	TArray<FString> Errors;
	TArray<FString> Warnings;
};

FNteCharacterRuntimeActionPlan BuildCharacterRuntimeActionPlanFromSpec(const FNteCharacterModSpec& Spec);
TSharedRef<FJsonObject> CharacterRuntimeActionPlanToJson(const FNteCharacterRuntimeActionPlan& Plan);
}
