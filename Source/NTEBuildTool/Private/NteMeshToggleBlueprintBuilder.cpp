// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteMeshToggleBlueprintBuilder.h"

#include "NTEBuildTool.h"
#include "NteEditorAssetUtils.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Fonts/CompositeFont.h"
#include "IAssetTools.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Variable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

namespace NTEBuildTool::Toggle
{
using namespace NTEBuildTool::Editor;

namespace
{
struct FRuntimeBuildContext
{
	FNteMeshToggleSetupOptions Options;
	FString TargetPostProcessAnimBlueprintPath;
	FString TargetWidgetBlueprintPath;
	FString TargetSaveGameBlueprintPath;
	UAnimBlueprint* PostProcessAnimBlueprint = nullptr;
	UBlueprint* WidgetBlueprint = nullptr;
	UBlueprint* SaveGameBlueprint = nullptr;
	FNteMeshToggleBlueprintBuildResult* Result = nullptr;
	TMap<int32, int32> ShowMaterialSectionNodeOrdinalByGroup;
	TMap<int32, int32> PatchedShowMaterialSectionNodeCountByGroup;
};

FString ToGeneratedClassObjectPath(const FString& BlueprintPackagePath)
{
	const FString ShortName = FPackageName::GetShortName(BlueprintPackagePath);
	return BlueprintPackagePath + TEXT(".") + ShortName + TEXT("_C");
}

UClass* LoadGeneratedClassFromPackagePath(const FString& BlueprintPackagePath)
{
	return LoadObject<UClass>(nullptr, *ToGeneratedClassObjectPath(BlueprintPackagePath));
}

bool SaveLoadedAssetPackage(UObject& Asset, FString& OutError)
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

UObject* DuplicateOrLoadAsset(const FString& SourcePackagePath, const FString& TargetPackagePath, const bool bOverwriteExisting, TArray<FString>& Actions, FString& OutError)
{
	if (SourcePackagePath.IsEmpty())
	{
		OutError = TEXT("Template asset path is empty.");
		return nullptr;
	}
	if (TargetPackagePath.IsEmpty())
	{
		OutError = TEXT("Target asset path is empty.");
		return nullptr;
	}

	if (SourcePackagePath == TargetPackagePath)
	{
		UObject* SourceAsset = LoadAnyAssetByPath(SourcePackagePath);
		if (!SourceAsset)
		{
			OutError = FString::Printf(TEXT("Could not load runtime asset for in-place patch: %s"), *SourcePackagePath);
			return nullptr;
		}
		Actions.Add(FString::Printf(TEXT("loaded %s for in-place patch"), *SourcePackagePath));
		return SourceAsset;
	}

	if (UObject* ExistingAsset = LoadAnyAssetByPath(TargetPackagePath))
	{
		if (!bOverwriteExisting)
		{
			Actions.Add(FString::Printf(TEXT("loaded existing %s"), *TargetPackagePath));
			return ExistingAsset;
		}

		const TArray<UObject*> ExistingAssets = { ExistingAsset };
		if (ObjectTools::ForceDeleteObjects(ExistingAssets, false) <= 0)
		{
			OutError = FString::Printf(TEXT("Could not delete existing target asset before duplicate: %s"), *TargetPackagePath);
			return nullptr;
		}
		Actions.Add(FString::Printf(TEXT("deleted existing %s"), *TargetPackagePath));
	}

	UObject* SourceAsset = LoadAnyAssetByPath(SourcePackagePath);
	if (!SourceAsset)
	{
		OutError = FString::Printf(TEXT("Could not load template asset: %s"), *SourcePackagePath);
		return nullptr;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	UObject* DuplicatedAsset = AssetToolsModule.Get().DuplicateAsset(
		FPackageName::GetShortName(TargetPackagePath),
		FPackageName::GetLongPackagePath(TargetPackagePath),
		SourceAsset);
	if (!DuplicatedAsset)
	{
		OutError = FString::Printf(TEXT("Could not duplicate %s to %s"), *SourcePackagePath, *TargetPackagePath);
		return nullptr;
	}

	Actions.Add(FString::Printf(TEXT("duplicated %s -> %s"), *SourcePackagePath, *TargetPackagePath));
	return DuplicatedAsset;
}

UEdGraphPin* FindPinByName(UEdGraphNode& Node, const TCHAR* PinName)
{
	for (UEdGraphPin* Pin : Node.Pins)
	{
		if (Pin && Pin->PinName == PinName)
		{
			return Pin;
		}
	}
	return nullptr;
}

FString GetLinkedVariableName(const UEdGraphPin& Pin)
{
	if (Pin.LinkedTo.Num() != 1 || !Pin.LinkedTo[0])
	{
		return FString();
	}

	const UEdGraphNode* LinkedNode = Pin.LinkedTo[0]->GetOwningNode();
	const UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(LinkedNode);
	return VariableNode ? VariableNode->GetVarNameString() : FString();
}

bool ParseStandardGroupOrdinalFromVisibleVar(const FString& VariableName, int32& OutOrdinal)
{
	OutOrdinal = INDEX_NONE;
	if (!VariableName.StartsWith(TEXT("NTE_Toggle_")) || !VariableName.Contains(TEXT("_toggle_group_")) || !VariableName.EndsWith(TEXT("_Visible")))
	{
		return false;
	}

	FString Remaining = VariableName.RightChop(FCString::Strlen(TEXT("NTE_Toggle_")));
	FString OrdinalText;
	if (!Remaining.Split(TEXT("_"), &OrdinalText, &Remaining))
	{
		return false;
	}

	OutOrdinal = FCString::Atoi(*OrdinalText);
	return OutOrdinal > 0;
}

bool ParseStandardGroupOrdinalFromInputVar(const FString& VariableName, int32& OutOrdinal)
{
	OutOrdinal = INDEX_NONE;
	if (!VariableName.StartsWith(TEXT("NTE_Toggle_Input_")))
	{
		return false;
	}

	FString Tail = VariableName.RightChop(FCString::Strlen(TEXT("NTE_Toggle_Input_")));
	FString OrdinalText;
	if (!Tail.Split(TEXT("_"), &OrdinalText, &Tail))
	{
		return false;
	}

	OutOrdinal = FCString::Atoi(*OrdinalText);
	return OutOrdinal > 0;
}

bool IsCallFunctionNodeWithTitle(const UEdGraphNode& Node, const TCHAR* TitleNeedle)
{
	const UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(&Node);
	if (!CallFunctionNode)
	{
		return false;
	}

	const FString MemberName = CallFunctionNode->FunctionReference.GetMemberName().ToString();
	return MemberName.Contains(TitleNeedle) || Node.GetNodeTitle(ENodeTitleType::FullTitle).ToString().Contains(TitleNeedle);
}

void SetPinDefaultValue(FRuntimeBuildContext& Context, UEdGraphNode& Node, UEdGraphPin& Pin, const FString& NewValue)
{
	if (Pin.DefaultValue == NewValue)
	{
		return;
	}

	Pin.Modify();
	Pin.DefaultValue = NewValue;
	Node.PinDefaultValueChanged(&Pin);
	if (Context.Result)
	{
		Context.Result->Actions.Add(FString::Printf(TEXT("patched %s.%s=%s"), *Node.GetName(), *Pin.PinName.ToString(), *NewValue));
	}
}

void SetPinDefaultObject(FRuntimeBuildContext& Context, UEdGraphNode& Node, UEdGraphPin& Pin, UObject* NewObject)
{
	if (Pin.DefaultObject == NewObject)
	{
		return;
	}

	Pin.Modify();
	Pin.DefaultObject = NewObject;
	Node.PinDefaultValueChanged(&Pin);
	if (Context.Result && NewObject)
	{
		Context.Result->Actions.Add(FString::Printf(TEXT("patched %s.%s=%s"), *Node.GetName(), *Pin.PinName.ToString(), *NewObject->GetPathName()));
	}
}

TArray<int32> GetTargetSlotsForTemplateGroup(const FRuntimeBuildContext& Context, const int32 TemplateGroupOrdinal)
{
	const int32 GroupIndex = TemplateGroupOrdinal - 1;
	return Context.Options.ToggleGroups.IsValidIndex(GroupIndex) ? Context.Options.ToggleGroups[GroupIndex].Slots : TArray<int32>();
}

FString GetTargetKeyForTemplateGroup(const FRuntimeBuildContext& Context, const int32 TemplateGroupOrdinal)
{
	const int32 GroupIndex = TemplateGroupOrdinal - 1;
	if (!Context.Options.ToggleGroups.IsValidIndex(GroupIndex))
	{
		return FString();
	}

	const FInputChord& Chord = Context.Options.ToggleGroups[GroupIndex].Chord;
	return Chord.Key.IsValid() ? Chord.Key.GetFName().ToString() : TEXT("None");
}

void PatchShowMaterialSectionNode(FRuntimeBuildContext& Context, UEdGraphNode& Node)
{
	UEdGraphPin* ShowPin = FindPinByName(Node, TEXT("bShow"));
	if (!ShowPin)
	{
		return;
	}

	int32 TemplateGroupOrdinal = INDEX_NONE;
	if (!ParseStandardGroupOrdinalFromVisibleVar(GetLinkedVariableName(*ShowPin), TemplateGroupOrdinal))
	{
		return;
	}

	const TArray<int32> Slots = GetTargetSlotsForTemplateGroup(Context, TemplateGroupOrdinal);
	if (Slots.IsEmpty())
	{
		return;
	}

	int32& NodeOrdinal = Context.ShowMaterialSectionNodeOrdinalByGroup.FindOrAdd(TemplateGroupOrdinal);
	const int32 SlotOrdinal = NodeOrdinal % Slots.Num();
	++NodeOrdinal;
	Context.PatchedShowMaterialSectionNodeCountByGroup.FindOrAdd(TemplateGroupOrdinal)++;
	const int32 SlotIndex = Slots[SlotOrdinal];

	const FString SlotText = FString::FromInt(SlotIndex);
	if (UEdGraphPin* MaterialIdPin = FindPinByName(Node, TEXT("MaterialID")))
	{
		SetPinDefaultValue(Context, Node, *MaterialIdPin, SlotText);
	}
	if (UEdGraphPin* SectionIndexPin = FindPinByName(Node, TEXT("SectionIndex")))
	{
		SetPinDefaultValue(Context, Node, *SectionIndexPin, SlotText);
	}
}

void WarnAboutUnpatchedMaterialSlots(FRuntimeBuildContext& Context)
{
	if (!Context.Result)
	{
		return;
	}

	for (int32 GroupIndex = 0; GroupIndex < Context.Options.ToggleGroups.Num(); ++GroupIndex)
	{
		const int32 TemplateGroupOrdinal = GroupIndex + 1;
		const FNteMeshToggleGroup& Group = Context.Options.ToggleGroups[GroupIndex];
		const int32 PatchedNodeCount = Context.PatchedShowMaterialSectionNodeCountByGroup.FindRef(TemplateGroupOrdinal);
		if (PatchedNodeCount < Group.Slots.Num())
		{
			TArray<FString> UnpatchedSlots;
			for (int32 SlotOrdinal = PatchedNodeCount; SlotOrdinal < Group.Slots.Num(); ++SlotOrdinal)
			{
				UnpatchedSlots.Add(FString::FromInt(Group.Slots[SlotOrdinal]));
			}
			Context.Result->Warnings.Add(FString::Printf(
				TEXT("Template group %d has %d ShowMaterialSection node(s), but setup item '%s' binds %d slot(s). These slots will not be controlled: %s."),
				TemplateGroupOrdinal,
				PatchedNodeCount,
				*Group.Label,
				Group.Slots.Num(),
				*FString::Join(UnpatchedSlots, TEXT(","))));
		}
		else if (Group.Slots.Num() > 1 && PatchedNodeCount % Group.Slots.Num() != 0)
		{
			Context.Result->Warnings.Add(FString::Printf(
				TEXT("Template group %d has %d ShowMaterialSection node(s) for %d configured slots. Slots were assigned cyclically, but the final pass is partial; verify the template graph layout."),
				TemplateGroupOrdinal,
				PatchedNodeCount,
				Group.Slots.Num()));
		}
	}
}

bool IsModifierKey(const FString& KeyName);
FString GetModifierReplacementKey(const FInputChord& Chord, const FString& OldKeyName);
void PatchUiHotkeyNode(FRuntimeBuildContext& Context, UEdGraphNode& Node);

void PatchInputKeyNode(FRuntimeBuildContext& Context, UEdGraphNode& Node)
{
	UEdGraphPin* ReturnPin = FindPinByName(Node, TEXT("ReturnValue"));
	UEdGraphPin* KeyPin = FindPinByName(Node, TEXT("Key"));
	if (!ReturnPin || !KeyPin)
	{
		return;
	}

	int32 TemplateGroupOrdinal = INDEX_NONE;
	for (const UEdGraphPin* LinkedPin : ReturnPin->LinkedTo)
	{
		const UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
		if (!LinkedNode)
		{
			continue;
		}

		for (const UEdGraphPin* CandidatePin : LinkedNode->Pins)
		{
			const FString VariableName = CandidatePin ? GetLinkedVariableName(*CandidatePin) : FString();
			if (ParseStandardGroupOrdinalFromInputVar(VariableName, TemplateGroupOrdinal))
			{
				break;
			}
		}

		if (TemplateGroupOrdinal > 0)
		{
			break;
		}
	}

	if (TemplateGroupOrdinal <= 0)
	{
		if (KeyPin->DefaultValue == TEXT("Slash") || IsModifierKey(KeyPin->DefaultValue))
		{
			PatchUiHotkeyNode(Context, Node);
			return;
		}

		if (Context.Result)
		{
			Context.Result->Warnings.Add(FString::Printf(TEXT("Standard template input key node %s is not connected to an NTE_Toggle_Input_<ordinal> variable; leaving key %s unchanged."), *Node.GetName(), *KeyPin->DefaultValue));
		}
		return;
	}

	const FString NewKey = GetTargetKeyForTemplateGroup(Context, TemplateGroupOrdinal);
	if (!NewKey.IsEmpty())
	{
		if (IsModifierKey(KeyPin->DefaultValue))
		{
			SetPinDefaultValue(Context, Node, *KeyPin, GetModifierReplacementKey(Context.Options.ToggleGroups[TemplateGroupOrdinal - 1].Chord, KeyPin->DefaultValue));
		}
		else
		{
			SetPinDefaultValue(Context, Node, *KeyPin, NewKey);
		}
	}
}

bool IsModifierKey(const FString& KeyName)
{
	return KeyName == TEXT("LeftControl")
		|| KeyName == TEXT("RightControl")
		|| KeyName == TEXT("LeftAlt")
		|| KeyName == TEXT("RightAlt")
		|| KeyName == TEXT("LeftShift")
		|| KeyName == TEXT("RightShift")
		|| KeyName == TEXT("LeftCommand")
		|| KeyName == TEXT("RightCommand");
}

FString GetModifierReplacementKey(const FInputChord& Chord, const FString& OldKeyName)
{
	if (OldKeyName.Contains(TEXT("Control")) && Chord.bCtrl)
	{
		return OldKeyName.StartsWith(TEXT("Right")) ? TEXT("RightControl") : TEXT("LeftControl");
	}
	if (OldKeyName.Contains(TEXT("Alt")) && Chord.bAlt)
	{
		return OldKeyName.StartsWith(TEXT("Right")) ? TEXT("RightAlt") : TEXT("LeftAlt");
	}
	if (OldKeyName.Contains(TEXT("Shift")) && Chord.bShift)
	{
		return OldKeyName.StartsWith(TEXT("Right")) ? TEXT("RightShift") : TEXT("LeftShift");
	}
	if (OldKeyName.Contains(TEXT("Command")) && Chord.bCmd)
	{
		return OldKeyName.StartsWith(TEXT("Right")) ? TEXT("RightCommand") : TEXT("LeftCommand");
	}
	return Chord.Key.IsValid() ? Chord.Key.GetFName().ToString() : TEXT("None");
}

void PatchUiHotkeyNode(FRuntimeBuildContext& Context, UEdGraphNode& Node)
{
	UEdGraphPin* KeyPin = FindPinByName(Node, TEXT("Key"));
	if (!KeyPin)
	{
		return;
	}

	const FString UiKeyName = Context.Options.UiChord.Key.IsValid() ? Context.Options.UiChord.Key.GetFName().ToString() : FString();
	if (KeyPin->DefaultValue == TEXT("Slash") && !UiKeyName.IsEmpty())
	{
		SetPinDefaultValue(Context, Node, *KeyPin, UiKeyName);
	}
	else if (IsModifierKey(KeyPin->DefaultValue))
	{
		SetPinDefaultValue(Context, Node, *KeyPin, GetModifierReplacementKey(Context.Options.UiChord, KeyPin->DefaultValue));
	}
}

FEdGraphPinType MakeBlueprintObjectPinType(UClass* ObjectClass)
{
	FEdGraphPinType PinType;
	PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
	PinType.PinSubCategoryObject = ObjectClass;
	PinType.ContainerType = EPinContainerType::None;
	return PinType;
}

TArray<UK2Node*> FindVariableNodes(const FName VariableName, const UBlueprint& Blueprint)
{
	TArray<UK2Node*> Result;
	const auto AddFromGraphs = [&Result, VariableName](const TArray<TObjectPtr<UEdGraph>>& Graphs)
	{
		for (UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}

			for (UEdGraphNode* Node : Graph->Nodes)
			{
				UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node);
				if (VariableNode && VariableNode->GetVarName() == VariableName)
				{
					Result.Add(VariableNode);
				}
			}
		}
	};

	AddFromGraphs(Blueprint.UbergraphPages);
	AddFromGraphs(Blueprint.FunctionGraphs);
	AddFromGraphs(Blueprint.MacroGraphs);
	return Result;
}

void PatchMemberVariableType(FRuntimeBuildContext& Context, const FName VariableName, UClass* TargetClass)
{
	if (!TargetClass || !Context.PostProcessAnimBlueprint)
	{
		return;
	}

	const int32 VariableIndex = FBlueprintEditorUtils::FindNewVariableIndex(Context.PostProcessAnimBlueprint, VariableName);
	if (VariableIndex == INDEX_NONE)
	{
		return;
	}

	const FEdGraphPinType NewType = MakeBlueprintObjectPinType(TargetClass);
	if (Context.PostProcessAnimBlueprint->NewVariables[VariableIndex].VarType == NewType)
	{
		return;
	}

	const TArray<UK2Node*> VariableNodes = FindVariableNodes(VariableName, *Context.PostProcessAnimBlueprint);
	Context.PostProcessAnimBlueprint->Modify();
	Context.PostProcessAnimBlueprint->NewVariables[VariableIndex].VarType = NewType;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Context.PostProcessAnimBlueprint);

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	for (UK2Node* VariableNode : VariableNodes)
	{
		if (VariableNode)
		{
			K2Schema->ReconstructNode(*VariableNode, true);
		}
	}

	if (Context.Result)
	{
		Context.Result->Actions.Add(FString::Printf(TEXT("patched PostProcess member variable %s type to %s"), *VariableName.ToString(), *TargetClass->GetPathName()));
	}
}

void PatchCastNode(FRuntimeBuildContext& Context, UK2Node_DynamicCast& CastNode, UClass* NewTargetClass)
{
	if (!NewTargetClass || CastNode.TargetType == NewTargetClass)
	{
		return;
	}

	CastNode.Modify();
	CastNode.TargetType = NewTargetClass;
	CastNode.ReconstructNode();
	if (Context.Result)
	{
		Context.Result->Actions.Add(FString::Printf(TEXT("patched cast node %s to %s"), *CastNode.GetName(), *NewTargetClass->GetPathName()));
	}
}

void PatchObjectClassPins(FRuntimeBuildContext& Context, UEdGraphNode& Node)
{
	if (UEdGraphPin* SaveGameClassPin = FindPinByName(Node, TEXT("SaveGameClass")))
	{
		SetPinDefaultObject(Context, Node, *SaveGameClassPin, LoadGeneratedClassFromPackagePath(Context.TargetSaveGameBlueprintPath));
	}

	if (UEdGraphPin* WidgetTypePin = FindPinByName(Node, TEXT("WidgetType")))
	{
		SetPinDefaultObject(Context, Node, *WidgetTypePin, LoadGeneratedClassFromPackagePath(Context.TargetWidgetBlueprintPath));
	}
}

void PatchSaveSlotPins(FRuntimeBuildContext& Context, UEdGraphNode& Node)
{
	for (UEdGraphPin* Pin : Node.Pins)
	{
		if (Pin && Pin->PinName == TEXT("SlotName"))
		{
			SetPinDefaultValue(Context, Node, *Pin, Context.Options.SaveSlotName);
		}
	}
}

void PatchSaveGameBlueprintDefaults(FRuntimeBuildContext& Context)
{
	for (FBPVariableDescription& Variable : Context.SaveGameBlueprint->NewVariables)
	{
		int32 TemplateGroupOrdinal = INDEX_NONE;
		if (!ParseStandardGroupOrdinalFromVisibleVar(Variable.VarName.ToString(), TemplateGroupOrdinal))
		{
			continue;
		}

		const int32 GroupIndex = TemplateGroupOrdinal - 1;
		if (!Context.Options.ToggleGroups.IsValidIndex(GroupIndex))
		{
			continue;
		}

		Variable.DefaultValue = Context.Options.ToggleGroups[GroupIndex].bDefaultVisible ? TEXT("true") : TEXT("false");
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Context.SaveGameBlueprint);
	FKismetEditorUtilities::CompileBlueprint(Context.SaveGameBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
}

FString MakeStandardButtonLabelWidgetName(const int32 TemplateGroupOrdinal)
{
	return FString::Printf(
		TEXT("NTE_Toggle_Button_%02d_toggle_group_%d_Label"),
		TemplateGroupOrdinal,
		TemplateGroupOrdinal);
}

FString MakeStandardButtonWidgetName(const int32 TemplateGroupOrdinal)
{
	return FString::Printf(
		TEXT("NTE_Toggle_Button_%02d_toggle_group_%d"),
		TemplateGroupOrdinal,
		TemplateGroupOrdinal);
}

bool NeedsEmbeddedUnicodeFont(const FRuntimeBuildContext& Context)
{
	const auto ContainsNonAscii = [](const FString& Text)
	{
		for (const TCHAR Character : Text)
		{
			if (Character > 0x7f)
			{
				return true;
			}
		}
		return false;
	};

	for (const FNteMeshToggleGroup& Group : Context.Options.ToggleGroups)
	{
		if (ContainsNonAscii(Group.Label))
		{
			return true;
		}
	}
	return false;
}

bool LoadFontBytes(const FString& Filename, TArray<uint8>& OutBytes, FString& OutError)
{
	if (Filename.IsEmpty() || !FPaths::FileExists(Filename))
	{
		OutError = FString::Printf(TEXT("Unicode UI font file does not exist: %s"), *Filename);
		return false;
	}
	if (!FFileHelper::LoadFileToArray(OutBytes, *Filename) || OutBytes.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Could not read Unicode UI font file: %s"), *Filename);
		return false;
	}
	return true;
}

UFont* FindOrCreateEmbeddedUnicodeFont(UWidgetBlueprint& WidgetBlueprint, FString& OutError)
{
	const FName FontName(TEXT("NTE_Embedded_CJK_UI_Font"));
	const FName FontFaceName(TEXT("NTE_Embedded_CJK_UI_FontFace"));

	UPackage* Package = WidgetBlueprint.GetPackage();
	if (!Package)
	{
		OutError = FString::Printf(TEXT("Widget blueprint has no package: %s"), *WidgetBlueprint.GetName());
		return nullptr;
	}

	UFont* Font = FindObject<UFont>(Package, *FontName.ToString());
	UFontFace* FontFace = Font ? FindObject<UFontFace>(Font, *FontFaceName.ToString()) : nullptr;

	const FString FontFilename = FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf");
	TArray<uint8> FontBytes;
	if (!LoadFontBytes(FontFilename, FontBytes, OutError))
	{
		return nullptr;
	}

	WidgetBlueprint.Modify();
	if (!Font)
	{
		Font = NewObject<UFont>(Package, FontName, RF_Public | RF_Standalone | RF_Transactional);
	}
	if (!FontFace)
	{
		FontFace = NewObject<UFontFace>(Font, FontFaceName, RF_Public | RF_Transactional);
	}

	Font->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
	FontFace->SetFlags(RF_Public | RF_Transactional);
	if (!FontFace->FontFaceData->HasData())
	{
		FontFace->InitializeFromBulkData(FontFilename, EFontHinting::Default, FontBytes.GetData(), FontBytes.Num());
	}
	FontFace->LoadingPolicy = EFontLoadingPolicy::Inline;
	FontFace->Modify();

	Font->FontCacheType = EFontCacheType::Runtime;
	Font->LegacyFontSize = 16;
	Font->CompositeFont.DefaultTypeface.Fonts.Reset();
	FTypefaceEntry& Entry = Font->CompositeFont.DefaultTypeface.Fonts.AddDefaulted_GetRef();
	Entry.Name = FName(TEXT("Regular"));
	Entry.Font = FFontData(FontFace);
	Font->CompositeFont.FallbackTypeface.Typeface.Fonts.Reset();
	FTypefaceEntry& FallbackEntry = Font->CompositeFont.FallbackTypeface.Typeface.Fonts.AddDefaulted_GetRef();
	FallbackEntry.Name = FName(TEXT("Regular"));
	FallbackEntry.Font = FFontData(FontFace);
	Font->CompositeFont.MakeDirty();
	Font->Modify();
	Font->MarkPackageDirty();
	Package->MarkPackageDirty();

	return Font;
}

bool IsDescendantOfWidget(const UWidgetTree& WidgetTree, const UWidget& Candidate, const UWidget& ExpectedParent)
{
	bool bFound = false;
	UWidgetTree::ForWidgetAndChildren(
		const_cast<UWidget*>(&ExpectedParent),
		[&Candidate, &bFound](UWidget* Widget)
		{
			if (Widget == &Candidate)
			{
				bFound = true;
			}
		});
	return bFound;
}

bool ValidateWidgetTemplateChrome(const UWidgetBlueprint& WidgetBlueprint, FString& OutError)
{
	if (!WidgetBlueprint.WidgetTree)
	{
		OutError = TEXT("TemplateWidgetBlueprint must be a UWidgetBlueprint with a WidgetTree.");
		return false;
	}

	if (!Cast<UButton>(WidgetBlueprint.WidgetTree->FindWidget(FName(TEXT("NTE_Toggle_TitleBarButton")))))
	{
		OutError = TEXT("Widget template is missing Button 'NTE_Toggle_TitleBarButton' required for dragging the toggle UI.");
		return false;
	}

	if (!WidgetBlueprint.WidgetTree->FindWidget(FName(TEXT("NTE_Toggle_WindowPanel"))))
	{
		OutError = TEXT("Widget template is missing widget 'NTE_Toggle_WindowPanel' required for dragging the toggle UI.");
		return false;
	}

	return true;
}

bool ApplyFontToTextBlock(UTextBlock& TextBlock, const UFont* Font)
{
	if (!Font)
	{
		return false;
	}

	FSlateFontInfo FontInfo = TextBlock.GetFont();
	if (FontInfo.FontObject == Font)
	{
		return false;
	}

	TextBlock.Modify();
	FontInfo.FontObject = Font;
	FontInfo.TypefaceFontName = FName(TEXT("Regular"));
	TextBlock.SetFont(FontInfo);
	return true;
}

bool PatchWidgetBlueprintLabels(FRuntimeBuildContext& Context, FString& OutError)
{
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Context.WidgetBlueprint);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		OutError = TEXT("TemplateWidgetBlueprint must be a UWidgetBlueprint with a WidgetTree.");
		return false;
	}
	if (!ValidateWidgetTemplateChrome(*WidgetBlueprint, OutError))
	{
		return false;
	}

	bool bChanged = false;
	UFont* EmbeddedUnicodeFont = nullptr;
	if (NeedsEmbeddedUnicodeFont(Context))
	{
		EmbeddedUnicodeFont = FindOrCreateEmbeddedUnicodeFont(*WidgetBlueprint, OutError);
		if (!EmbeddedUnicodeFont)
		{
			return false;
		}
		bChanged = true;
	}

	if (EmbeddedUnicodeFont)
	{
		TArray<UWidget*> Widgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(Widgets);
		for (UWidget* Widget : Widgets)
		{
			if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
			{
				bChanged |= ApplyFontToTextBlock(*TextBlock, EmbeddedUnicodeFont);
			}
		}
	}

	for (int32 GroupIndex = 0; GroupIndex < Context.Options.ToggleGroups.Num(); ++GroupIndex)
	{
		const int32 TemplateGroupOrdinal = GroupIndex + 1;
		const FNteMeshToggleGroup& Group = Context.Options.ToggleGroups[GroupIndex];
		const FString ButtonWidgetName = MakeStandardButtonWidgetName(TemplateGroupOrdinal);
		UWidget* ButtonWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*ButtonWidgetName));
		UButton* Button = Cast<UButton>(ButtonWidget);
		if (!Button)
		{
			OutError = FString::Printf(
				TEXT("Widget template is missing Button '%s' required for toggle item '%s'."),
				*ButtonWidgetName,
				*Group.Label);
			return false;
		}

		const FString LabelWidgetName = MakeStandardButtonLabelWidgetName(TemplateGroupOrdinal);
		UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(FName(*LabelWidgetName));
		UTextBlock* LabelTextBlock = Cast<UTextBlock>(Widget);
		if (!LabelTextBlock)
		{
			OutError = FString::Printf(
				TEXT("Widget template is missing TextBlock '%s' required to label toggle item '%s'."),
				*LabelWidgetName,
				*Group.Label);
			return false;
		}
		if (!IsDescendantOfWidget(*WidgetBlueprint->WidgetTree, *LabelTextBlock, *Button))
		{
			OutError = FString::Printf(
				TEXT("Widget template TextBlock '%s' must be inside Button '%s' so the UI button has a visible label."),
				*LabelWidgetName,
				*ButtonWidgetName);
			return false;
		}

		const FText NewLabel = FText::FromString(Group.Label);
		if (!LabelTextBlock->GetText().EqualTo(NewLabel))
		{
			LabelTextBlock->Modify();
			LabelTextBlock->SetText(NewLabel);
			bChanged = true;
		}
		if (EmbeddedUnicodeFont)
		{
			bChanged |= ApplyFontToTextBlock(*LabelTextBlock, EmbeddedUnicodeFont);
		}

		if (Context.Result)
		{
			Context.Result->Actions.Add(FString::Printf(TEXT("patched widget label %s=%s"), *LabelWidgetName, *Group.Label));
		}
	}

	if (bChanged)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	}
	return true;
}

TSet<int32> CollectTemplateGroupOrdinals(const UBlueprint& Blueprint)
{
	TSet<int32> Ordinals;
	for (const FBPVariableDescription& Variable : Blueprint.NewVariables)
	{
		int32 Ordinal = INDEX_NONE;
		if (ParseStandardGroupOrdinalFromVisibleVar(Variable.VarName.ToString(), Ordinal))
		{
			Ordinals.Add(Ordinal);
		}
	}

	const auto AddFromGraphs = [&Ordinals](const TArray<TObjectPtr<UEdGraph>>& Graphs)
	{
		for (const UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}

			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				const UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node);
				if (!VariableNode)
				{
					continue;
				}

				int32 Ordinal = INDEX_NONE;
				if (ParseStandardGroupOrdinalFromVisibleVar(VariableNode->GetVarNameString(), Ordinal))
				{
					Ordinals.Add(Ordinal);
				}
			}
		}
	};

	AddFromGraphs(Blueprint.UbergraphPages);
	AddFromGraphs(Blueprint.FunctionGraphs);
	AddFromGraphs(Blueprint.MacroGraphs);
	return Ordinals;
}

bool ValidateTemplateCompatibility(const FRuntimeBuildContext& Context, FString& OutError)
{
	TSet<int32> Ordinals = CollectTemplateGroupOrdinals(*Context.SaveGameBlueprint);
	for (const int32 Ordinal : CollectTemplateGroupOrdinals(*Context.PostProcessAnimBlueprint))
	{
		Ordinals.Add(Ordinal);
	}

	int32 MaxOrdinal = 0;
	for (const int32 Ordinal : Ordinals)
	{
		MaxOrdinal = FMath::Max(MaxOrdinal, Ordinal);
	}

	if (MaxOrdinal != Context.Options.ToggleGroups.Num())
	{
		OutError = FString::Printf(
			TEXT("Standard runtime template group count (%d) does not match setup group count (%d). Choose a matching standard template or adjust the setup before generating runtime blueprints."),
			MaxOrdinal,
			Context.Options.ToggleGroups.Num());
		return false;
	}
	return true;
}

void PatchPostProcessBlueprintGraph(FRuntimeBuildContext& Context)
{
	UClass* TargetSaveGameClass = LoadGeneratedClassFromPackagePath(Context.TargetSaveGameBlueprintPath);
	UClass* TargetWidgetClass = LoadGeneratedClassFromPackagePath(Context.TargetWidgetBlueprintPath);

	PatchMemberVariableType(Context, TEXT("NTE_Toggle_SaveObject"), TargetSaveGameClass);
	PatchMemberVariableType(Context, TEXT("NTE_Toggle_Widget"), TargetWidgetClass);

	for (UEdGraph* Graph : Context.PostProcessAnimBlueprint->UbergraphPages)
	{
		if (!Graph)
		{
			continue;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			Node->Modify();
			PatchSaveSlotPins(Context, *Node);
			PatchObjectClassPins(Context, *Node);
			if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
			{
				const FString Title = CastNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
				if (Title.Contains(TEXT("BP_NTE_ModToggleSaveGame")))
				{
					PatchCastNode(Context, *CastNode, TargetSaveGameClass);
				}
				else if (Title.Contains(TEXT("WBP_NTE_ModToggleMenu")))
				{
					PatchCastNode(Context, *CastNode, TargetWidgetClass);
				}
			}
			if (IsCallFunctionNodeWithTitle(*Node, TEXT("ShowMaterialSection")) || IsCallFunctionNodeWithTitle(*Node, TEXT("Show Material Section")))
			{
				PatchShowMaterialSectionNode(Context, *Node);
			}
			if (IsCallFunctionNodeWithTitle(*Node, TEXT("IsInputKeyDown")) || IsCallFunctionNodeWithTitle(*Node, TEXT("Is Input Key Down")))
			{
				PatchInputKeyNode(Context, *Node);
			}
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Context.PostProcessAnimBlueprint);
	FBlueprintEditorUtils::RefreshAllNodes(Context.PostProcessAnimBlueprint);
	FKismetEditorUtilities::CompileBlueprint(Context.PostProcessAnimBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
}
}

bool BuildMeshToggleRuntimeBlueprints(
	const FNteMeshToggleSetupOptions& Options,
	FNteMeshToggleSetupResult& InOutResult,
	FNteMeshToggleBlueprintBuildResult& OutBuildResult,
	FString& OutError)
{
	OutBuildResult = FNteMeshToggleBlueprintBuildResult();
	if (Options.TemplatePostProcessAnimBlueprintPath.IsEmpty() || Options.TemplateWidgetBlueprintPath.IsEmpty() || Options.TemplateSaveGameBlueprintPath.IsEmpty())
	{
		OutError = TEXT("Runtime Blueprint generation needs TemplatePostProcessAnimBlueprint, TemplateWidgetBlueprint, and TemplateSaveGameBlueprint in the Toggle Setup.");
		return false;
	}

	FRuntimeBuildContext Context;
	Context.Options = Options;
	Context.TargetPostProcessAnimBlueprintPath = InOutResult.PostProcessAnimBlueprintPath;
	Context.TargetWidgetBlueprintPath = InOutResult.WidgetBlueprintPath;
	Context.TargetSaveGameBlueprintPath = InOutResult.SaveGameBlueprintPath;
	Context.Result = &OutBuildResult;

	Context.SaveGameBlueprint = Cast<UBlueprint>(DuplicateOrLoadAsset(
		Options.TemplateSaveGameBlueprintPath,
		Context.TargetSaveGameBlueprintPath,
		Options.bOverwriteExistingRuntimeAssets,
		OutBuildResult.Actions,
		OutError));
	if (!Context.SaveGameBlueprint)
	{
		return false;
	}

	Context.WidgetBlueprint = Cast<UBlueprint>(DuplicateOrLoadAsset(
		Options.TemplateWidgetBlueprintPath,
		Context.TargetWidgetBlueprintPath,
		Options.bOverwriteExistingRuntimeAssets,
		OutBuildResult.Actions,
		OutError));
	if (!Context.WidgetBlueprint)
	{
		return false;
	}

	Context.PostProcessAnimBlueprint = Cast<UAnimBlueprint>(DuplicateOrLoadAsset(
		Options.TemplatePostProcessAnimBlueprintPath,
		Context.TargetPostProcessAnimBlueprintPath,
		Options.bOverwriteExistingRuntimeAssets,
		OutBuildResult.Actions,
		OutError));
	if (!Context.PostProcessAnimBlueprint)
	{
		return false;
	}

	FKismetEditorUtilities::CompileBlueprint(Context.SaveGameBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	FKismetEditorUtilities::CompileBlueprint(Context.WidgetBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	FKismetEditorUtilities::CompileBlueprint(Context.PostProcessAnimBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);

	if (!ValidateTemplateCompatibility(Context, OutError))
	{
		return false;
	}

	PatchSaveGameBlueprintDefaults(Context);
	if (!PatchWidgetBlueprintLabels(Context, OutError))
	{
		return false;
	}
	PatchPostProcessBlueprintGraph(Context);
	WarnAboutUnpatchedMaterialSlots(Context);

	InOutResult.PostProcessAnimBlueprint = Context.PostProcessAnimBlueprint;
	InOutResult.WidgetBlueprint = Context.WidgetBlueprint;
	InOutResult.SaveGameBlueprint = Context.SaveGameBlueprint;

	if (!SaveLoadedAssetPackage(*Context.SaveGameBlueprint, OutError))
	{
		return false;
	}
	if (!SaveLoadedAssetPackage(*Context.WidgetBlueprint, OutError))
	{
		return false;
	}
	if (!SaveLoadedAssetPackage(*Context.PostProcessAnimBlueprint, OutError))
	{
		return false;
	}

	OutBuildResult.Actions.Add(TEXT("saved runtime blueprints"));
	return true;
}
}
