// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/InputChord.h"

class UAnimBlueprint;
class UBlueprint;
class USkeletalMesh;
class UObject;
class UWidgetBlueprint;

namespace NTEBuildTool::Toggle
{
struct FNteMeshToggleSlotBinding
{
	int32 SlotIndex = INDEX_NONE;
	FString SlotName;
	FString ImportedSlotName;
	FString MaterialPath;
};

struct FNteMeshToggleGroup
{
	FString GroupId;
	FInputChord Chord;
	FString Label;
	TArray<int32> Slots;
	TArray<FNteMeshToggleSlotBinding> SlotBindings;
	bool bDefaultVisible = true;
};

struct FNteMeshToggleSetupOptions
{
	// Deprecated input name kept for v1/v2 setup JSON compatibility. Prefer TargetMeshPath.
	FString MeshPath;
	FString TargetMeshPath;
	FString RuntimeAnchorMeshPath;
	FString OutputFolder;
	FString ConfigAssetName = TEXT("NTE_ModToggleSetup");
	FString PostProcessAnimBlueprintName = TEXT("ABP_NTE_ModToggle_PostProcess");
	FString ControllerBlueprintName;
	FString WidgetBlueprintName = TEXT("WBP_NTE_ModToggleMenu");
	FString SaveGameBlueprintName = TEXT("BP_NTE_ModToggleSaveGame");
	FString TemplatePostProcessAnimBlueprintPath;
	FString TemplateWidgetBlueprintPath;
	FString TemplateSaveGameBlueprintPath;
	FString SaveSlotName;
	FString RuntimeMode = TEXT("StandardPostProcessTemplate");
	FString StaticMeshVisibilityAdapter = TEXT("MaterialSwap");
	FString HiddenMaterialPath;
	FInputChord UiChord = FInputChord(EKeys::Slash, false, true, false, false);
	TArray<FNteMeshToggleGroup> ToggleGroups;
	bool bAssignPostProcessAnimBlueprint = true;
	bool bCreateBlueprintAssets = true;
	bool bSaveDirtyAssetsAfterCreate = true;
	bool bOverwriteExistingRuntimeAssets = true;
	bool bValidateOnly = false;
};

struct FNteMeshToggleSetupResult
{
	UObject* TargetMesh = nullptr;
	USkeletalMesh* RuntimeAnchorMesh = nullptr;
	UAnimBlueprint* PostProcessAnimBlueprint = nullptr;
	UBlueprint* ControllerBlueprint = nullptr;
	UBlueprint* WidgetBlueprint = nullptr;
	UBlueprint* SaveGameBlueprint = nullptr;
	FString ConfigAssetPath;
	FString ConfigFilename;
	FString PostProcessAnimBlueprintPath;
	FString ControllerBlueprintPath;
	FString WidgetBlueprintPath;
	FString SaveGameBlueprintPath;
	TArray<FString> Warnings;
};
}
