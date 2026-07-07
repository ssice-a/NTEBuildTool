// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteMeshToggleConfig.h"

#include "NteEditorAssetUtils.h"
#include "NteJsonFileUtils.h"

#include "Dom/JsonObject.h"

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
	return SaveJsonObjectToFile(Root, ConfigFilename, OutError);
}

bool RunMeshToggleUiSetup(FNteMeshToggleSetupOptions Options, FNteMeshToggleSetupResult& OutResult, FString& OutError)
{
	OutResult = FNteMeshToggleSetupResult();
	OutError = TEXT("Mesh toggle runtime generation has not been migrated into the modular tool yet.");
	return false;
}
}
