// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteMeshToggleConfig.h"

#include "NteEditorAssetUtils.h"
#include "NteJsonFileUtils.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"

namespace NTEBuildTool::Toggle
{
using namespace NTEBuildTool::Editor;
using namespace NTEBuildTool::Json;

namespace
{
FInputChord InputChordFromJsonObject(const FJsonObject& Object)
{
	FInputChord Chord;
	const FString KeyName = GetStringAny(Object, TEXT("Key"));
	if (!KeyName.IsEmpty())
	{
		Chord.Key = FKey(*KeyName);
	}

	bool bShift = Chord.bShift;
	bool bCtrl = Chord.bCtrl;
	bool bAlt = Chord.bAlt;
	bool bCmd = Chord.bCmd;
	GetBoolAny(Object, bShift, TEXT("Shift"));
	GetBoolAny(Object, bCtrl, TEXT("Ctrl"));
	GetBoolAny(Object, bAlt, TEXT("Alt"));
	GetBoolAny(Object, bCmd, TEXT("Cmd"));
	Chord.bShift = bShift;
	Chord.bCtrl = bCtrl;
	Chord.bAlt = bAlt;
	Chord.bCmd = bCmd;
	return Chord;
}

TSharedRef<FJsonObject> InputChordToJsonObject(const FInputChord& Chord)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("Key"), Chord.Key.IsValid() ? Chord.Key.GetFName().ToString() : FString());
	Object->SetBoolField(TEXT("Shift"), Chord.bShift);
	Object->SetBoolField(TEXT("Ctrl"), Chord.bCtrl);
	Object->SetBoolField(TEXT("Alt"), Chord.bAlt);
	Object->SetBoolField(TEXT("Cmd"), Chord.bCmd);
	Object->SetStringField(TEXT("DisplayName"), Chord.GetInputText().ToString());
	return Object;
}

bool SaveAssetPackage(UObject& Asset, FString& OutError)
{
	UPackage* Package = Asset.GetPackage();
	if (!Package)
	{
		OutError = FString::Printf(TEXT("Asset has no package: %s"), *Asset.GetName());
		return false;
	}

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, &Asset, *PackageFilename, SaveArgs))
	{
		OutError = FString::Printf(TEXT("Could not save package: %s"), *PackageFilename);
		return false;
	}

	return true;
}

FString AssetPackageToFilename(const FString& AssetPath, const FString& Extension)
{
	if (!IsGamePackageName(AssetPath))
	{
		return FString();
	}

	FString Filename = FPackageName::LongPackageNameToFilename(AssetPath, Extension);
	FPaths::NormalizeFilename(Filename);
	return Filename;
}

FString DefaultConfigFilenameForOptions(const FNteMeshToggleSetupOptions& Options)
{
	const FString ConfigAssetPath = JoinAssetPath(Options.OutputFolder, Options.ConfigAssetName);
	return AssetPackageToFilename(ConfigAssetPath, TEXT(".json"));
}

void AddSlotBindingFromSkeletalMesh(const USkeletalMesh& SkeletalMesh, const int32 SlotIndex, FNteMeshToggleGroup& Group)
{
	const TArray<FSkeletalMaterial>& Materials = SkeletalMesh.GetMaterials();
	if (!Materials.IsValidIndex(SlotIndex))
	{
		return;
	}

	const FSkeletalMaterial& Material = Materials[SlotIndex];
	FNteMeshToggleSlotBinding Binding;
	Binding.SlotIndex = SlotIndex;
	Binding.SlotName = Material.MaterialSlotName.ToString();
#if WITH_EDITORONLY_DATA
	Binding.ImportedSlotName = Material.ImportedMaterialSlotName.ToString();
#endif
	Binding.MaterialPath = Material.MaterialInterface ? Material.MaterialInterface->GetPackage()->GetName() : FString();
	Group.SlotBindings.Add(Binding);
}

TSharedRef<FJsonObject> SlotBindingToJsonObject(const FNteMeshToggleSlotBinding& Binding)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("SlotIndex"), Binding.SlotIndex);
	Object->SetStringField(TEXT("SlotName"), Binding.SlotName);
	Object->SetStringField(TEXT("ImportedSlotName"), Binding.ImportedSlotName);
	Object->SetStringField(TEXT("MaterialPath"), Binding.MaterialPath);
	return Object;
}

FNteMeshToggleSlotBinding SlotBindingFromJsonObject(const FJsonObject& Object)
{
	FNteMeshToggleSlotBinding Binding;
	GetIntAny(Object, Binding.SlotIndex, TEXT("SlotIndex"), TEXT("slotIndex"));
	Binding.SlotName = GetStringAny(Object, TEXT("SlotName"), TEXT("slotName"));
	Binding.ImportedSlotName = GetStringAny(Object, TEXT("ImportedSlotName"), TEXT("importedSlotName"));
	Binding.MaterialPath = NormalizeAssetPathForText(GetStringAny(Object, TEXT("MaterialPath"), TEXT("materialPath")));
	return Binding;
}

bool ValidateToggleGroupsAgainstMesh(const USkeletalMesh& SkeletalMesh, const TArray<FNteMeshToggleGroup>& Groups, FString& OutError)
{
	const int32 MaterialCount = SkeletalMesh.GetMaterials().Num();
	for (const FNteMeshToggleGroup& Group : Groups)
	{
		if (Group.GroupId.IsEmpty())
		{
			OutError = TEXT("Toggle group is missing GroupId.");
			return false;
		}
		if (Group.Slots.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Toggle group has no MaterialSlots: %s"), *Group.GroupId);
			return false;
		}
		for (const int32 SlotIndex : Group.Slots)
		{
			if (SlotIndex < 0 || SlotIndex >= MaterialCount)
			{
				OutError = FString::Printf(
					TEXT("Material slot %d in group %s is out of range for %s, which has %d slots."),
					SlotIndex,
					*Group.GroupId,
					*SkeletalMesh.GetPackage()->GetName(),
					MaterialCount);
				return false;
			}
		}
	}

	return true;
}

void FillMissingSlotBindings(const USkeletalMesh& SkeletalMesh, TArray<FNteMeshToggleGroup>& Groups)
{
	for (FNteMeshToggleGroup& Group : Groups)
	{
		if (!Group.SlotBindings.IsEmpty())
		{
			continue;
		}

		for (const int32 SlotIndex : Group.Slots)
		{
			AddSlotBindingFromSkeletalMesh(SkeletalMesh, SlotIndex, Group);
		}
	}
}

bool LoadRequiredAsset(UObject*& OutAsset, const FString& AssetPath, const TCHAR* Label, FString& OutError)
{
	if (AssetPath.IsEmpty())
	{
		OutError = FString::Printf(TEXT("%s path is empty."), Label);
		return false;
	}

	OutAsset = LoadAnyAssetByPath(AssetPath);
	if (!OutAsset)
	{
		OutError = FString::Printf(TEXT("Could not load %s: %s"), Label, *AssetPath);
		return false;
	}

	return true;
}

bool BlueprintLooksLikeGeneratedHardcodedRuntime(const UAnimBlueprint& AnimBlueprint)
{
	FString PackageFilename;
	if (!FPackageName::TryConvertLongPackageNameToFilename(AnimBlueprint.GetPackage()->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		return false;
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *PackageFilename))
	{
		return false;
	}

	const auto ContainsAscii = [&Bytes](const ANSICHAR* Needle)
	{
		const int32 NeedleLen = FCStringAnsi::Strlen(Needle);
		if (NeedleLen <= 0 || Bytes.Num() < NeedleLen)
		{
			return false;
		}

		for (int32 Index = 0; Index <= Bytes.Num() - NeedleLen; ++Index)
		{
			if (FMemory::Memcmp(Bytes.GetData() + Index, Needle, NeedleLen) == 0)
			{
				return true;
			}
		}

		return false;
	};

	return ContainsAscii("ShowMaterialSection") && ContainsAscii("IsInputKeyDown");
}
}

bool LoadMeshToggleSetupOptionsFromJsonFile(const FString& ConfigFilename, FNteMeshToggleSetupOptions& OutOptions, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	if (!LoadJsonObjectFromFile(ConfigFilename, Root, OutError))
	{
		return false;
	}

	const FString Format = GetStringAny(*Root, TEXT("Format"), TEXT("format"));
	if (!Format.IsEmpty() && Format != TEXT("NTE.ModToggleSetup"))
	{
		OutError = FString::Printf(TEXT("Unsupported mesh toggle config format: %s"), *Format);
		return false;
	}

	OutOptions.MeshPath = NormalizeAssetPathForText(GetStringAny(*Root, TEXT("Mesh"), TEXT("mesh")));
	OutOptions.OutputFolder = TEXT("/Game");
	const FString PostProcessAnimBlueprintPath = NormalizeAssetPathForText(GetStringAny(*Root, TEXT("PostProcessAnimBlueprint")));
	const FString ControllerBlueprintPath = NormalizeAssetPathForText(GetStringAny(*Root, TEXT("ControllerBlueprint")));
	const FString WidgetBlueprintPath = NormalizeAssetPathForText(GetStringAny(*Root, TEXT("WidgetBlueprint")));
	const FString SaveGameBlueprintPath = NormalizeAssetPathForText(GetStringAny(*Root, TEXT("SaveGameBlueprint")));
	OutOptions.SaveSlotName = GetStringAny(*Root, TEXT("SaveSlot"), TEXT("saveSlot"));

	if (!PostProcessAnimBlueprintPath.IsEmpty())
	{
		OutOptions.OutputFolder = FPackageName::GetLongPackagePath(PostProcessAnimBlueprintPath);
		OutOptions.PostProcessAnimBlueprintName = FPackageName::GetShortName(PostProcessAnimBlueprintPath);
	}
	if (!ControllerBlueprintPath.IsEmpty())
	{
		OutOptions.ControllerBlueprintName = FPackageName::GetShortName(ControllerBlueprintPath);
	}
	if (!WidgetBlueprintPath.IsEmpty())
	{
		OutOptions.WidgetBlueprintName = FPackageName::GetShortName(WidgetBlueprintPath);
	}
	if (!SaveGameBlueprintPath.IsEmpty())
	{
		OutOptions.SaveGameBlueprintName = FPackageName::GetShortName(SaveGameBlueprintPath);
	}

	const TSharedPtr<FJsonObject>* UiChordObject = nullptr;
	if (Root->TryGetObjectField(TEXT("UIInputChord"), UiChordObject) && UiChordObject && UiChordObject->IsValid())
	{
		OutOptions.UiChord = InputChordFromJsonObject(**UiChordObject);
	}

	OutOptions.ToggleGroups.Reset();
	const TArray<TSharedPtr<FJsonValue>>* GroupValues = nullptr;
	if (Root->TryGetArrayField(TEXT("Groups"), GroupValues) && GroupValues)
	{
		for (const TSharedPtr<FJsonValue>& GroupValue : *GroupValues)
		{
			const TSharedPtr<FJsonObject> GroupObject = GroupValue.IsValid() && GroupValue->Type == EJson::Object ? GroupValue->AsObject() : nullptr;
			if (!GroupObject.IsValid())
			{
				continue;
			}

			FNteMeshToggleGroup Group;
			Group.GroupId = GetStringAny(*GroupObject, TEXT("GroupId"), TEXT("groupId"));
			Group.Label = GetStringAny(*GroupObject, TEXT("Label"), TEXT("label"));
			Group.bDefaultVisible = true;
			GetBoolAny(*GroupObject, Group.bDefaultVisible, TEXT("DefaultVisible"), TEXT("defaultVisible"));

			const TSharedPtr<FJsonObject>* InputChordObject = nullptr;
			if (GroupObject->TryGetObjectField(TEXT("InputChord"), InputChordObject) && InputChordObject && InputChordObject->IsValid())
			{
				Group.Chord = InputChordFromJsonObject(**InputChordObject);
			}
			else
			{
				const FString KeyName = GetStringAny(*GroupObject, TEXT("Key"));
				if (!KeyName.IsEmpty())
				{
					Group.Chord.Key = FKey(*KeyName);
				}
			}

			const TArray<TSharedPtr<FJsonValue>>* SlotValues = nullptr;
			if (GroupObject->TryGetArrayField(TEXT("MaterialSlots"), SlotValues) && SlotValues)
			{
				for (const TSharedPtr<FJsonValue>& SlotValue : *SlotValues)
				{
					Group.Slots.Add(static_cast<int32>(SlotValue->AsNumber()));
				}
			}

			const TArray<TSharedPtr<FJsonValue>>* BindingValues = nullptr;
			if (GroupObject->TryGetArrayField(TEXT("MaterialSlotBindings"), BindingValues) && BindingValues)
			{
				for (const TSharedPtr<FJsonValue>& BindingValue : *BindingValues)
				{
					const TSharedPtr<FJsonObject> BindingObject = BindingValue.IsValid() && BindingValue->Type == EJson::Object ? BindingValue->AsObject() : nullptr;
					if (BindingObject.IsValid())
					{
						Group.SlotBindings.Add(SlotBindingFromJsonObject(*BindingObject));
					}
				}
			}

			OutOptions.ToggleGroups.Add(Group);
		}
	}

	return true;
}

bool SaveMeshToggleSetupOptionsToJsonFile(const FNteMeshToggleSetupOptions& Options, const FString& ConfigFilename, FString& OutError)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("Format"), TEXT("NTE.ModToggleSetup"));
	Root->SetNumberField(TEXT("Version"), 2.0);
	Root->SetStringField(TEXT("Mesh"), Options.MeshPath);
	Root->SetStringField(TEXT("PostProcessAnimBlueprint"), JoinAssetPath(Options.OutputFolder, Options.PostProcessAnimBlueprintName));
	Root->SetStringField(TEXT("ControllerBlueprint"), JoinAssetPath(Options.OutputFolder, Options.ControllerBlueprintName));
	Root->SetStringField(TEXT("WidgetBlueprint"), JoinAssetPath(Options.OutputFolder, Options.WidgetBlueprintName));
	Root->SetStringField(TEXT("SaveGameBlueprint"), JoinAssetPath(Options.OutputFolder, Options.SaveGameBlueprintName));
	Root->SetStringField(TEXT("SaveSlot"), Options.SaveSlotName);
	Root->SetObjectField(TEXT("UIInputChord"), InputChordToJsonObject(Options.UiChord));

	TArray<TSharedPtr<FJsonValue>> Groups;
	for (const FNteMeshToggleGroup& Group : Options.ToggleGroups)
	{
		const TSharedRef<FJsonObject> GroupObject = MakeShared<FJsonObject>();
		GroupObject->SetStringField(TEXT("GroupId"), Group.GroupId);
		GroupObject->SetStringField(TEXT("Key"), Group.Chord.Key.IsValid() ? Group.Chord.Key.GetFName().ToString() : FString());
		GroupObject->SetStringField(TEXT("Label"), Group.Label);
		GroupObject->SetBoolField(TEXT("DefaultVisible"), Group.bDefaultVisible);
		GroupObject->SetObjectField(TEXT("InputChord"), InputChordToJsonObject(Group.Chord));

		TArray<TSharedPtr<FJsonValue>> Slots;
		for (const int32 SlotIndex : Group.Slots)
		{
			Slots.Add(MakeShared<FJsonValueNumber>(SlotIndex));
		}
		GroupObject->SetArrayField(TEXT("MaterialSlots"), Slots);

		TArray<TSharedPtr<FJsonValue>> Bindings;
		for (const FNteMeshToggleSlotBinding& Binding : Group.SlotBindings)
		{
			Bindings.Add(MakeShared<FJsonValueObject>(SlotBindingToJsonObject(Binding)));
		}
		GroupObject->SetArrayField(TEXT("MaterialSlotBindings"), Bindings);
		Groups.Add(MakeShared<FJsonValueObject>(GroupObject));
	}
	Root->SetArrayField(TEXT("Groups"), Groups);
	return SaveJsonObjectToFile(Root, ConfigFilename, OutError);
}

bool RunMeshToggleUiSetup(FNteMeshToggleSetupOptions Options, FNteMeshToggleSetupResult& OutResult, FString& OutError)
{
	OutResult = FNteMeshToggleSetupResult();
	if (Options.MeshPath.IsEmpty())
	{
		OutError = TEXT("Mesh toggle setup requires a Mesh path.");
		return false;
	}

	if (Options.OutputFolder.IsEmpty())
	{
		Options.OutputFolder = FPackageName::GetLongPackagePath(Options.MeshPath) / TEXT("mod/Runtime");
	}
	if (Options.SaveSlotName.IsEmpty())
	{
		Options.SaveSlotName = FPackageName::GetShortName(Options.MeshPath) + TEXT("_NTE_ModToggle");
	}

	UObject* MeshAsset = LoadAnyAssetByPath(Options.MeshPath);
	if (!MeshAsset)
	{
		OutError = FString::Printf(TEXT("Could not load mesh: %s"), *Options.MeshPath);
		return false;
	}

	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(MeshAsset))
	{
		OutError = FString::Printf(
			TEXT("%s is a StaticMesh. Pure-pak hotkey/UI toggles need a Runtime Anchor, and this module can only assign a PostProcessAnimBlueprint to a SkeletalMesh. Reimport the PSK as a SkeletalMesh or choose another loaded SkeletalMesh anchor before generating runtime toggles."),
			*StaticMesh->GetPackage()->GetName());
		return false;
	}

	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(MeshAsset);
	if (!SkeletalMesh)
	{
		OutError = FString::Printf(TEXT("Target asset is not a SkeletalMesh: %s (%s)"), *Options.MeshPath, *MeshAsset->GetClass()->GetName());
		return false;
	}
	OutResult.TargetMesh = SkeletalMesh;

	if (!ValidateToggleGroupsAgainstMesh(*SkeletalMesh, Options.ToggleGroups, OutError))
	{
		return false;
	}
	FillMissingSlotBindings(*SkeletalMesh, Options.ToggleGroups);

	OutResult.ConfigAssetPath = JoinAssetPath(Options.OutputFolder, Options.ConfigAssetName);
	OutResult.ConfigFilename = DefaultConfigFilenameForOptions(Options);
	OutResult.PostProcessAnimBlueprintPath = JoinAssetPath(Options.OutputFolder, Options.PostProcessAnimBlueprintName);
	OutResult.ControllerBlueprintPath = Options.ControllerBlueprintName.IsEmpty() ? FString() : JoinAssetPath(Options.OutputFolder, Options.ControllerBlueprintName);
	OutResult.WidgetBlueprintPath = JoinAssetPath(Options.OutputFolder, Options.WidgetBlueprintName);
	OutResult.SaveGameBlueprintPath = JoinAssetPath(Options.OutputFolder, Options.SaveGameBlueprintName);

	if (!OutResult.ConfigFilename.IsEmpty())
	{
		if (!SaveMeshToggleSetupOptionsToJsonFile(Options, OutResult.ConfigFilename, OutError))
		{
			return false;
		}
	}

	UObject* LoadedAsset = nullptr;
	if (!LoadRequiredAsset(LoadedAsset, OutResult.PostProcessAnimBlueprintPath, TEXT("PostProcessAnimBlueprint"), OutError))
	{
		if (Options.bCreateBlueprintAssets)
		{
			OutError += LINE_TERMINATOR TEXT("Runtime Blueprint graph generation has not been migrated yet. Existing target-specific runtime assets are required for now.");
		}
		return false;
	}

	UAnimBlueprint* PostProcessAnimBlueprint = Cast<UAnimBlueprint>(LoadedAsset);
	if (!PostProcessAnimBlueprint || !PostProcessAnimBlueprint->GeneratedClass)
	{
		OutError = FString::Printf(TEXT("PostProcessAnimBlueprint is not a compiled AnimBlueprint: %s"), *OutResult.PostProcessAnimBlueprintPath);
		return false;
	}
	if (!PostProcessAnimBlueprint->GeneratedClass->IsChildOf(UAnimInstance::StaticClass()))
	{
		OutError = FString::Printf(TEXT("PostProcessAnimBlueprint generated class is not an AnimInstance: %s"), *OutResult.PostProcessAnimBlueprintPath);
		return false;
	}
	OutResult.PostProcessAnimBlueprint = PostProcessAnimBlueprint;

	if (!BlueprintLooksLikeGeneratedHardcodedRuntime(*PostProcessAnimBlueprint))
	{
		OutResult.Warnings.Add(TEXT("PostProcessAnimBlueprint does not contain the old hardcoded IsInputKeyDown/ShowMaterialSection runtime pattern. If this is a new thin-anchor controller asset, add a dedicated inspector before treating it as runtime-ready."));
	}

	if (!OutResult.WidgetBlueprintPath.IsEmpty())
	{
		UObject* WidgetAsset = LoadAnyAssetByPath(OutResult.WidgetBlueprintPath);
		OutResult.WidgetBlueprint = Cast<UBlueprint>(WidgetAsset);
		if (!OutResult.WidgetBlueprint)
		{
			OutError = FString::Printf(TEXT("Could not load WidgetBlueprint: %s"), *OutResult.WidgetBlueprintPath);
			return false;
		}
	}

	if (!OutResult.SaveGameBlueprintPath.IsEmpty())
	{
		UObject* SaveGameAsset = LoadAnyAssetByPath(OutResult.SaveGameBlueprintPath);
		OutResult.SaveGameBlueprint = Cast<UBlueprint>(SaveGameAsset);
		if (!OutResult.SaveGameBlueprint)
		{
			OutError = FString::Printf(TEXT("Could not load SaveGameBlueprint: %s"), *OutResult.SaveGameBlueprintPath);
			return false;
		}
	}

	if (!OutResult.ControllerBlueprintPath.IsEmpty())
	{
		UObject* ControllerAsset = LoadAnyAssetByPath(OutResult.ControllerBlueprintPath);
		OutResult.ControllerBlueprint = Cast<UBlueprint>(ControllerAsset);
		if (!OutResult.ControllerBlueprint)
		{
			OutResult.Warnings.Add(TEXT("ControllerBlueprint is not present. This matches the old hardcoded PostProcess runtime, but the planned thin-anchor/controller model is not implemented for this asset yet."));
		}
	}

	if (Options.bAssignPostProcessAnimBlueprint)
	{
		SkeletalMesh->Modify();
		SkeletalMesh->SetPostProcessAnimBlueprint(TSubclassOf<UAnimInstance>(PostProcessAnimBlueprint->GeneratedClass.Get()));
		SkeletalMesh->PostEditChange();
		SkeletalMesh->MarkPackageDirty();

		if (Options.bSaveDirtyAssetsAfterCreate && !SaveAssetPackage(*SkeletalMesh, OutError))
		{
			return false;
		}
	}

	return true;
}
}
