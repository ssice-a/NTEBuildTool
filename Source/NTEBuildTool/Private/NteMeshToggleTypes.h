// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/InputChord.h"

class UAnimBlueprint;
class UBlueprint;
class USkeletalMesh;
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
	FString MeshPath;
	FString OutputFolder;
	FString ConfigAssetName = TEXT("NTE_ModToggleSetup");
	FString PostProcessAnimBlueprintName = TEXT("ABP_NTE_ModToggle_PostProcess");
	FString ControllerBlueprintName = TEXT("BP_NTE_ModToggleController");
	FString WidgetBlueprintName = TEXT("WBP_NTE_ModToggleMenu");
	FString SaveGameBlueprintName = TEXT("BP_NTE_ModToggleSaveGame");
	FString SaveSlotName;
	FInputChord UiChord = FInputChord(EKeys::Slash, false, true, false, false);
	TArray<FNteMeshToggleGroup> ToggleGroups;
	bool bAssignPostProcessAnimBlueprint = true;
	bool bCreateBlueprintAssets = true;
	bool bSaveDirtyAssetsAfterCreate = true;
};

struct FNteMeshToggleSetupResult
{
	USkeletalMesh* TargetMesh = nullptr;
	UAnimBlueprint* PostProcessAnimBlueprint = nullptr;
	UBlueprint* ControllerBlueprint = nullptr;
	UWidgetBlueprint* WidgetBlueprint = nullptr;
	UBlueprint* SaveGameBlueprint = nullptr;
	FString ConfigAssetPath;
	FString ConfigFilename;
	FString PostProcessAnimBlueprintPath;
	FString ControllerBlueprintPath;
	FString WidgetBlueprintPath;
	FString SaveGameBlueprintPath;
};
}
