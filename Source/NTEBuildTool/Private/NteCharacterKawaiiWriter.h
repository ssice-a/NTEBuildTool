// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "NteCharacterKawaiiPlan.h"

namespace NTEBuildTool::Character
{
struct FNteCharacterKawaiiAssetWriteResult
{
	FString AssetPath;
	FString AssetKind;
	bool bCreated = false;
	bool bUpdated = false;
	bool bSkipped = false;
	TArray<FString> Actions;
	TArray<FString> Warnings;
	TArray<FString> Errors;
};

struct FNteCharacterKawaiiSchemaProbeResult
{
	bool bKawaiiPhysicsModuleAvailable = false;
	bool bKawaiiPhysicsEditorModuleAvailable = false;
	bool bNteCompatible = false;
	TArray<FString> PresentTypes;
	TArray<FString> MissingTypes;
	TArray<FString> PresentFields;
	TArray<FString> MissingFields;
	TArray<FString> PublicOnlyFields;
	TArray<FString> Warnings;
	TArray<FString> Errors;
};

struct FNteCharacterKawaiiWriteResult
{
	FString KawaiiAssetRootPath;
	FNteCharacterKawaiiSchemaProbeResult SchemaProbe;
	TArray<FNteCharacterKawaiiAssetWriteResult> Assets;
	TArray<FString> SavedPackages;
	TArray<FString> Warnings;
	TArray<FString> Errors;

	bool HasErrors() const { return !Errors.IsEmpty(); }
};

struct FNteCharacterKawaiiAssetPreflightResult
{
	TArray<FString> CheckedAnimBlueprints;
	TArray<FString> Errors;
	TArray<FString> Warnings;

	bool HasErrors() const { return !Errors.IsEmpty(); }
};

FNteCharacterKawaiiSchemaProbeResult ProbeNteKawaiiSchemaCompatibility();
FNteCharacterKawaiiAssetPreflightResult ValidateCharacterKawaiiAssetsForPackage(const FNteCharacterKawaiiPlan& Plan);
FNteCharacterKawaiiWriteResult WriteAttachedMeshCopyPoseAnimBlueprint(
	const FString& RuntimeAnimBlueprintPath,
	const FString& TargetMeshPath);
FNteCharacterKawaiiWriteResult WriteCharacterKawaiiAssets(const FNteCharacterKawaiiPlan& Plan);
TSharedRef<FJsonObject> CharacterKawaiiSchemaProbeToJson(const FNteCharacterKawaiiSchemaProbeResult& Result);
TSharedRef<FJsonObject> CharacterKawaiiWriteResultToJson(const FNteCharacterKawaiiWriteResult& Result);
}
