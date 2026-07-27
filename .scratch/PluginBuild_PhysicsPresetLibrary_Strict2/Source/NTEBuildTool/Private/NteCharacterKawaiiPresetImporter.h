// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "NteCharacterModSpec.h"

namespace NTEBuildTool::Character
{
struct FNteCharacterKawaiiImportOptions
{
	FString SourceJsonPath;
	FString TargetMeshId = TEXT("main");
	FString PresetIdPrefix;
	/** Empty imports every Kawaii node in SourceJsonPath. Non-empty imports named nodes only. */
	TArray<FString> SourceNodeNames;
	bool bReplaceExistingById = true;
};

struct FNteCharacterKawaiiImportResult
{
	FString SourceJsonPath;
	FString TargetMeshId;
	FString SourceGeneratedClassName;
	FString SourceClassDefaultObjectName;
	TArray<FString> Errors;
	TArray<FString> Warnings;
	TArray<FNteCharacterKawaiiPresetSpec> ImportedPresets;
	int32 ReplacedPresetCount = 0;
	int32 AddedPresetCount = 0;

	bool HasErrors() const { return !Errors.IsEmpty(); }
};

FNteCharacterKawaiiImportResult ImportKawaiiPresetsFromFModelJson(const FNteCharacterKawaiiImportOptions& Options);
void UpsertKawaiiPresets(FNteCharacterModSpec& Spec, const TArray<FNteCharacterKawaiiPresetSpec>& Presets, bool bReplaceExistingById, FNteCharacterKawaiiImportResult& InOutResult);

TSharedRef<FJsonObject> CharacterKawaiiImportResultToJson(const FNteCharacterKawaiiImportResult& Result);
TSharedRef<FJsonObject> CharacterKawaiiPresetPlanToJson(const FNteCharacterModSpec& Spec);
}
