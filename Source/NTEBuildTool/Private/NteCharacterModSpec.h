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
	TArray<int32> MaterialSlots;
	FString MaterialPath;
	FString ParameterName;
	float ScalarValue = 0.0f;
	bool bDefaultEnabled = true;
};

struct FNteCharacterKawaiiPresetSpec
{
	FString Id;
	FString Label;
	FString TargetMeshId;
	FString SourceAnimBlueprintJson;
	FString RootBone;
	TArray<FString> AdditionalRootBones;
	TArray<FString> ExcludeBones;
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
