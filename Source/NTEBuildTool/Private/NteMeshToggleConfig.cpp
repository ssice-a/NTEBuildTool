// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteMeshToggleConfig.h"

#include "NteEditorAssetUtils.h"
#include "NteJsonFileUtils.h"
#include "NteMeshToggleBlueprintBuilder.h"

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

void AddSlotBindingFromStaticMesh(const UStaticMesh& StaticMesh, const int32 SlotIndex, FNteMeshToggleGroup& Group)
{
	const TArray<FStaticMaterial>& Materials = StaticMesh.GetStaticMaterials();
	if (!Materials.IsValidIndex(SlotIndex))
	{
		return;
	}

	const FStaticMaterial& Material = Materials[SlotIndex];
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

bool ValidateToggleGroupsAgainstMaterialCount(const FString& TargetMeshPath, const int32 MaterialCount, const TArray<FNteMeshToggleGroup>& Groups, FString& OutError)
{
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
					*TargetMeshPath,
					MaterialCount);
				return false;
			}
		}
	}

	return true;
}

bool ValidateToggleGroupsAgainstMesh(const UObject& MeshAsset, const TArray<FNteMeshToggleGroup>& Groups, FString& OutError)
{
	if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(&MeshAsset))
	{
		return ValidateToggleGroupsAgainstMaterialCount(SkeletalMesh->GetPackage()->GetName(), SkeletalMesh->GetMaterials().Num(), Groups, OutError);
	}
	if (const UStaticMesh* StaticMesh = Cast<UStaticMesh>(&MeshAsset))
	{
		return ValidateToggleGroupsAgainstMaterialCount(StaticMesh->GetPackage()->GetName(), StaticMesh->GetStaticMaterials().Num(), Groups, OutError);
	}

	OutError = FString::Printf(TEXT("Target asset is neither a SkeletalMesh nor a StaticMesh: %s (%s)"), *MeshAsset.GetPackage()->GetName(), *MeshAsset.GetClass()->GetName());
	return false;
}

void FillMissingSlotBindings(const UObject& MeshAsset, TArray<FNteMeshToggleGroup>& Groups)
{
	for (FNteMeshToggleGroup& Group : Groups)
	{
		if (!Group.SlotBindings.IsEmpty())
		{
			continue;
		}

		for (const int32 SlotIndex : Group.Slots)
		{
			if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(&MeshAsset))
			{
				AddSlotBindingFromSkeletalMesh(*SkeletalMesh, SlotIndex, Group);
			}
			else if (const UStaticMesh* StaticMesh = Cast<UStaticMesh>(&MeshAsset))
			{
				AddSlotBindingFromStaticMesh(*StaticMesh, SlotIndex, Group);
			}
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

bool BlueprintLooksLikeStandardPostProcessTemplateRuntime(const UAnimBlueprint& AnimBlueprint)
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
	OutOptions.TargetMeshPath = NormalizeAssetPathForText(GetStringAny(*Root, TEXT("TargetMesh"), TEXT("targetMesh")));
	OutOptions.RuntimeAnchorMeshPath = NormalizeAssetPathForText(GetStringAny(*Root, TEXT("RuntimeAnchorMesh"), TEXT("runtimeAnchorMesh"), TEXT("AnchorMesh")));
	OutOptions.OutputFolder = TEXT("/Game");
	const FString PostProcessAnimBlueprintPath = NormalizeAssetPathForText(GetStringAny(*Root, TEXT("PostProcessAnimBlueprint")));
	const FString ControllerBlueprintPath = NormalizeAssetPathForText(GetStringAny(*Root, TEXT("ControllerBlueprint")));
	const FString WidgetBlueprintPath = NormalizeAssetPathForText(GetStringAny(*Root, TEXT("WidgetBlueprint")));
	const FString SaveGameBlueprintPath = NormalizeAssetPathForText(GetStringAny(*Root, TEXT("SaveGameBlueprint")));
	OutOptions.TemplatePostProcessAnimBlueprintPath = NormalizeAssetPathForText(GetStringAny(*Root, TEXT("TemplatePostProcessAnimBlueprint"), TEXT("TemplatePostProcess")));
	OutOptions.TemplateWidgetBlueprintPath = NormalizeAssetPathForText(GetStringAny(*Root, TEXT("TemplateWidgetBlueprint"), TEXT("TemplateWidget")));
	OutOptions.TemplateSaveGameBlueprintPath = NormalizeAssetPathForText(GetStringAny(*Root, TEXT("TemplateSaveGameBlueprint"), TEXT("TemplateSaveGame")));
	OutOptions.SaveSlotName = GetStringAny(*Root, TEXT("SaveSlot"), TEXT("saveSlot"));
	OutOptions.RuntimeMode = GetStringAny(*Root, TEXT("RuntimeMode"), TEXT("runtimeMode"));
	OutOptions.StaticMeshVisibilityAdapter = GetStringAny(*Root, TEXT("StaticMeshVisibilityAdapter"), TEXT("staticMeshVisibilityAdapter"));
	OutOptions.HiddenMaterialPath = NormalizeAssetPathForText(GetStringAny(*Root, TEXT("HiddenMaterial"), TEXT("hiddenMaterial")));
	GetBoolAny(*Root, OutOptions.bValidateOnly, TEXT("ValidateOnly"), TEXT("validateOnly"));
	GetBoolAny(*Root, OutOptions.bAssignPostProcessAnimBlueprint, TEXT("AssignPostProcess"), TEXT("AssignPostProcessAnimBlueprint"));
	GetBoolAny(*Root, OutOptions.bAssignPostProcessAnimBlueprint, TEXT("assignPostProcess"));
	GetBoolAny(*Root, OutOptions.bCreateBlueprintAssets, TEXT("CreateBlueprintAssets"), TEXT("createBlueprintAssets"));
	GetBoolAny(*Root, OutOptions.bSaveDirtyAssetsAfterCreate, TEXT("SaveDirtyAssetsAfterCreate"), TEXT("saveDirtyAssetsAfterCreate"));
	GetBoolAny(*Root, OutOptions.bOverwriteExistingRuntimeAssets, TEXT("OverwriteExistingRuntimeAssets"), TEXT("overwriteExistingRuntimeAssets"));

	if (OutOptions.TargetMeshPath.IsEmpty())
	{
		OutOptions.TargetMeshPath = OutOptions.MeshPath;
	}
	if (OutOptions.RuntimeAnchorMeshPath.IsEmpty())
	{
		OutOptions.RuntimeAnchorMeshPath = OutOptions.TargetMeshPath;
	}
	if (OutOptions.RuntimeMode.IsEmpty())
	{
		OutOptions.RuntimeMode = TEXT("StandardPostProcessTemplate");
	}
	if (OutOptions.StaticMeshVisibilityAdapter.IsEmpty())
	{
		OutOptions.StaticMeshVisibilityAdapter = TEXT("MaterialSwap");
	}

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
	const FString TargetMeshPath = !Options.TargetMeshPath.IsEmpty() ? Options.TargetMeshPath : Options.MeshPath;
	const FString RuntimeAnchorMeshPath = !Options.RuntimeAnchorMeshPath.IsEmpty() ? Options.RuntimeAnchorMeshPath : TargetMeshPath;
	Root->SetStringField(TEXT("Mesh"), TargetMeshPath);
	Root->SetStringField(TEXT("TargetMesh"), TargetMeshPath);
	Root->SetStringField(TEXT("RuntimeAnchorMesh"), RuntimeAnchorMeshPath);
	Root->SetStringField(TEXT("PostProcessAnimBlueprint"), JoinAssetPath(Options.OutputFolder, Options.PostProcessAnimBlueprintName));
	Root->SetStringField(TEXT("ControllerBlueprint"), Options.ControllerBlueprintName.IsEmpty() ? FString() : JoinAssetPath(Options.OutputFolder, Options.ControllerBlueprintName));
	Root->SetStringField(TEXT("WidgetBlueprint"), JoinAssetPath(Options.OutputFolder, Options.WidgetBlueprintName));
	Root->SetStringField(TEXT("SaveGameBlueprint"), JoinAssetPath(Options.OutputFolder, Options.SaveGameBlueprintName));
	Root->SetStringField(TEXT("TemplatePostProcessAnimBlueprint"), Options.TemplatePostProcessAnimBlueprintPath);
	Root->SetStringField(TEXT("TemplateWidgetBlueprint"), Options.TemplateWidgetBlueprintPath);
	Root->SetStringField(TEXT("TemplateSaveGameBlueprint"), Options.TemplateSaveGameBlueprintPath);
	Root->SetStringField(TEXT("SaveSlot"), Options.SaveSlotName);
	Root->SetStringField(TEXT("RuntimeMode"), Options.RuntimeMode);
	Root->SetStringField(TEXT("StaticMeshVisibilityAdapter"), Options.StaticMeshVisibilityAdapter);
	Root->SetStringField(TEXT("HiddenMaterial"), Options.HiddenMaterialPath);
	Root->SetBoolField(TEXT("AssignPostProcess"), Options.bAssignPostProcessAnimBlueprint);
	Root->SetBoolField(TEXT("CreateBlueprintAssets"), Options.bCreateBlueprintAssets);
	Root->SetBoolField(TEXT("SaveDirtyAssetsAfterCreate"), Options.bSaveDirtyAssetsAfterCreate);
	Root->SetBoolField(TEXT("OverwriteExistingRuntimeAssets"), Options.bOverwriteExistingRuntimeAssets);
	Root->SetBoolField(TEXT("ValidateOnly"), Options.bValidateOnly);
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
	if (Options.TargetMeshPath.IsEmpty())
	{
		Options.TargetMeshPath = Options.MeshPath;
	}
	if (Options.RuntimeAnchorMeshPath.IsEmpty())
	{
		Options.RuntimeAnchorMeshPath = Options.TargetMeshPath;
	}
	if (Options.MeshPath.IsEmpty())
	{
		Options.MeshPath = Options.TargetMeshPath;
	}
	if (Options.TargetMeshPath.IsEmpty())
	{
		OutError = TEXT("Mesh toggle setup requires a TargetMesh path.");
		return false;
	}

	if (Options.OutputFolder.IsEmpty())
	{
		Options.OutputFolder = FPackageName::GetLongPackagePath(Options.TargetMeshPath) / TEXT("mod/Runtime");
	}
	if (Options.SaveSlotName.IsEmpty())
	{
		Options.SaveSlotName = FPackageName::GetShortName(Options.TargetMeshPath) + TEXT("_NTE_ModToggle");
	}
	if (Options.RuntimeMode.IsEmpty())
	{
		Options.RuntimeMode = TEXT("StandardPostProcessTemplate");
	}
	if (Options.StaticMeshVisibilityAdapter.IsEmpty())
	{
		Options.StaticMeshVisibilityAdapter = TEXT("MaterialSwap");
	}

	UObject* TargetMeshAsset = LoadAnyAssetByPath(Options.TargetMeshPath);
	if (!TargetMeshAsset)
	{
		OutError = FString::Printf(TEXT("Could not load target mesh: %s"), *Options.TargetMeshPath);
		return false;
	}
	OutResult.TargetMesh = TargetMeshAsset;

	UObject* AnchorMeshAsset = LoadAnyAssetByPath(Options.RuntimeAnchorMeshPath);
	if (!AnchorMeshAsset)
	{
		OutError = FString::Printf(TEXT("Could not load runtime anchor mesh: %s"), *Options.RuntimeAnchorMeshPath);
		return false;
	}
	USkeletalMesh* AnchorSkeletalMesh = Cast<USkeletalMesh>(AnchorMeshAsset);
	if (!AnchorSkeletalMesh)
	{
		OutError = FString::Printf(
			TEXT("RuntimeAnchorMesh must be a SkeletalMesh because PostProcessAnimBlueprint can only tick through a SkinnedMeshComponent. Asset=%s Class=%s. A Skeleton asset cannot be used as a runtime anchor."),
			*Options.RuntimeAnchorMeshPath,
			*AnchorMeshAsset->GetClass()->GetName());
		return false;
	}
	OutResult.RuntimeAnchorMesh = AnchorSkeletalMesh;

	if (!ValidateToggleGroupsAgainstMesh(*TargetMeshAsset, Options.ToggleGroups, OutError))
	{
		return false;
	}
	FillMissingSlotBindings(*TargetMeshAsset, Options.ToggleGroups);

	if (Cast<UStaticMesh>(TargetMeshAsset))
	{
		OutResult.Warnings.Add(TEXT("TargetMesh is a StaticMesh. This setup can use the RuntimeAnchorMesh for input/UI ticking, but StaticMesh visibility needs the runtime controller to use the configured StaticMeshVisibilityAdapter instead of SkinnedMeshComponent::ShowMaterialSection."));
		if (Options.StaticMeshVisibilityAdapter.Equals(TEXT("MaterialSwap"), ESearchCase::IgnoreCase) && Options.HiddenMaterialPath.IsEmpty())
		{
			OutResult.Warnings.Add(TEXT("StaticMeshVisibilityAdapter is MaterialSwap but HiddenMaterial is empty. Hidden-state generation will need a transparent/hidden material package before this setup is runtime-complete."));
		}
	}

	if (Options.bValidateOnly)
	{
		OutResult.ConfigAssetPath = JoinAssetPath(Options.OutputFolder, Options.ConfigAssetName);
		OutResult.ConfigFilename = DefaultConfigFilenameForOptions(Options);
		OutResult.PostProcessAnimBlueprintPath = JoinAssetPath(Options.OutputFolder, Options.PostProcessAnimBlueprintName);
		OutResult.ControllerBlueprintPath = Options.ControllerBlueprintName.IsEmpty() ? FString() : JoinAssetPath(Options.OutputFolder, Options.ControllerBlueprintName);
		OutResult.WidgetBlueprintPath = JoinAssetPath(Options.OutputFolder, Options.WidgetBlueprintName);
		OutResult.SaveGameBlueprintPath = JoinAssetPath(Options.OutputFolder, Options.SaveGameBlueprintName);
		return true;
	}

	OutResult.ConfigAssetPath = JoinAssetPath(Options.OutputFolder, Options.ConfigAssetName);
	OutResult.ConfigFilename = DefaultConfigFilenameForOptions(Options);
	OutResult.PostProcessAnimBlueprintPath = JoinAssetPath(Options.OutputFolder, Options.PostProcessAnimBlueprintName);
	OutResult.ControllerBlueprintPath = Options.ControllerBlueprintName.IsEmpty() ? FString() : JoinAssetPath(Options.OutputFolder, Options.ControllerBlueprintName);
	OutResult.WidgetBlueprintPath = JoinAssetPath(Options.OutputFolder, Options.WidgetBlueprintName);
	OutResult.SaveGameBlueprintPath = JoinAssetPath(Options.OutputFolder, Options.SaveGameBlueprintName);

	const bool bHasAllTemplatePaths = !Options.TemplatePostProcessAnimBlueprintPath.IsEmpty()
		&& !Options.TemplateWidgetBlueprintPath.IsEmpty()
		&& !Options.TemplateSaveGameBlueprintPath.IsEmpty();
	if (Options.bCreateBlueprintAssets && !bHasAllTemplatePaths)
	{
		OutError = TEXT("CreateBlueprintAssets=true requires TemplatePostProcessAnimBlueprint, TemplateWidgetBlueprint, and TemplateSaveGameBlueprint. Set CreateBlueprintAssets=false only when reusing existing runtime assets.");
		return false;
	}

	if (!OutResult.ConfigFilename.IsEmpty())
	{
		if (!SaveMeshToggleSetupOptionsToJsonFile(Options, OutResult.ConfigFilename, OutError))
		{
			return false;
		}
	}

	if (Options.bCreateBlueprintAssets)
	{
		FNteMeshToggleBlueprintBuildResult BuildResult;
		if (!BuildMeshToggleRuntimeBlueprints(Options, OutResult, BuildResult, OutError))
		{
			return false;
		}
		OutResult.Warnings.Append(BuildResult.Warnings);
	}

	UObject* LoadedAsset = nullptr;
	if (!OutResult.PostProcessAnimBlueprint)
	{
		if (!LoadRequiredAsset(LoadedAsset, OutResult.PostProcessAnimBlueprintPath, TEXT("PostProcessAnimBlueprint"), OutError))
		{
			if (Options.bCreateBlueprintAssets)
			{
				OutError += LINE_TERMINATOR TEXT("Add TemplatePostProcessAnimBlueprint, TemplateWidgetBlueprint, and TemplateSaveGameBlueprint to generate runtime assets from a template.");
			}
			return false;
		}
		OutResult.PostProcessAnimBlueprint = Cast<UAnimBlueprint>(LoadedAsset);
	}

	UAnimBlueprint* PostProcessAnimBlueprint = OutResult.PostProcessAnimBlueprint;
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

	if (!BlueprintLooksLikeStandardPostProcessTemplateRuntime(*PostProcessAnimBlueprint))
	{
		OutResult.Warnings.Add(TEXT("PostProcessAnimBlueprint does not contain the standard template IsInputKeyDown/ShowMaterialSection runtime markers. Add a dedicated inspector before treating a different runtime contract as ready."));
	}

	if (!OutResult.WidgetBlueprint && !OutResult.WidgetBlueprintPath.IsEmpty())
	{
		UObject* WidgetAsset = LoadAnyAssetByPath(OutResult.WidgetBlueprintPath);
		OutResult.WidgetBlueprint = Cast<UBlueprint>(WidgetAsset);
		if (!OutResult.WidgetBlueprint)
		{
			OutError = FString::Printf(TEXT("Could not load WidgetBlueprint: %s"), *OutResult.WidgetBlueprintPath);
			return false;
		}
	}

	if (!OutResult.SaveGameBlueprint && !OutResult.SaveGameBlueprintPath.IsEmpty())
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
			OutResult.Warnings.Add(TEXT("ControllerBlueprint is not present. This is expected for StandardPostProcessTemplate runtime; ThinAnchorController generation is a separate future runtime contract."));
		}
	}

	if (Options.bAssignPostProcessAnimBlueprint)
	{
		AnchorSkeletalMesh->Modify();
		AnchorSkeletalMesh->SetPostProcessAnimBlueprint(TSubclassOf<UAnimInstance>(PostProcessAnimBlueprint->GeneratedClass.Get()));
		AnchorSkeletalMesh->PostEditChange();
		AnchorSkeletalMesh->MarkPackageDirty();

		if (Options.bSaveDirtyAssetsAfterCreate && !SaveAssetPackage(*AnchorSkeletalMesh, OutError))
		{
			return false;
		}
	}

	return true;
}
}
