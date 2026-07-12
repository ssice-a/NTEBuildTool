// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "NteCharacterRuntimeActionPlan.h"

class UBlueprint;

namespace NTEBuildTool::Character
{
struct FNteCharacterRuntimeActionAssetWriteResult
{
	FString AssetPath;
	FString AssetKind;
	bool bCreated = false;
	bool bUpdated = false;
	TArray<FString> Actions;
	TArray<FString> Warnings;
	TArray<FString> Errors;
};

struct FNteCharacterRuntimeActionWriteResult
{
	FString RuntimeAssetRootPath;
	FString WidgetBlueprintPath;
	FString SaveGameBlueprintPath;
	TArray<FNteCharacterRuntimeActionAssetWriteResult> Assets;
	TArray<FString> SavedPackages;
	TArray<FString> Warnings;
	TArray<FString> Errors;

	bool HasErrors() const { return !Errors.IsEmpty(); }
};

TArray<FString> CollectCharacterRuntimeActionPlanPackageSeeds(const FNteCharacterRuntimeActionPlan& Plan);
FNteCharacterRuntimeActionWriteResult WriteCharacterRuntimeActions(const FNteCharacterRuntimeActionPlan& Plan);
TSharedRef<FJsonObject> CharacterRuntimeActionWriteResultToJson(const FNteCharacterRuntimeActionWriteResult& Result);
}
