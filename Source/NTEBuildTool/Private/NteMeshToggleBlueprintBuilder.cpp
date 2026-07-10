// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteMeshToggleBlueprintBuilder.h"

#include "NTEBuildTool.h"
#include "NteEditorAssetUtils.h"
#include "NteMeshToggleStandardTemplateModel.h"

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
#include "Kismet/KismetMathLibrary.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Self.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "UObject/SavePackage.h"
#include "Blueprint/WidgetLayoutLibrary.h"
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
	UBlueprint* TemplateWidgetBlueprint = nullptr;
	UBlueprint* TemplateSaveGameBlueprint = nullptr;
	UClass* TargetWidgetClass = nullptr;
	UClass* TargetSaveGameClass = nullptr;
	FNteMeshToggleBlueprintBuildResult* Result = nullptr;
	FNteStandardToggleTemplateModel StandardTemplateModel;
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

UEdGraphPin* FindExecPin(UEdGraphNode& Node, const TCHAR* PinName = TEXT("execute"))
{
	return FindPinByName(Node, PinName);
}

UEdGraphPin* FindThenPin(UEdGraphNode& Node)
{
	return FindPinByName(Node, TEXT("then"));
}

bool TryLinkPins(UEdGraphPin* FromPin, UEdGraphPin* ToPin)
{
	if (!FromPin || !ToPin)
	{
		return false;
	}

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	return K2Schema && K2Schema->TryCreateConnection(FromPin, ToPin);
}

template <typename NodeType>
NodeType* AddK2Node(UEdGraph& Graph, const int32 NodePosX, const int32 NodePosY)
{
	NodeType* Node = NewObject<NodeType>(&Graph);
	Node->CreateNewGuid();
	Graph.AddNode(Node, false, false);
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Node->AllocateDefaultPins();
	return Node;
}

UK2Node_CallFunction* AddFunctionCallNode(UEdGraph& Graph, UFunction* Function, const int32 NodePosX, const int32 NodePosY)
{
	if (!Function)
	{
		return nullptr;
	}

	UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(&Graph);
	Node->CreateNewGuid();
	Graph.AddNode(Node, false, false);
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Node->SetFromFunction(Function);
	Node->AllocateDefaultPins();
	return Node;
}

UK2Node_VariableGet* AddVariableGetNode(UEdGraph& Graph, const FName VariableName, const int32 NodePosX, const int32 NodePosY)
{
	UK2Node_VariableGet* Node = NewObject<UK2Node_VariableGet>(&Graph);
	Node->CreateNewGuid();
	Graph.AddNode(Node, false, false);
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Node->VariableReference.SetSelfMember(VariableName);
	Node->AllocateDefaultPins();
	return Node;
}

UK2Node_VariableSet* AddVariableSetNode(UEdGraph& Graph, const FName VariableName, const int32 NodePosX, const int32 NodePosY)
{
	UK2Node_VariableSet* Node = NewObject<UK2Node_VariableSet>(&Graph);
	Node->CreateNewGuid();
	Graph.AddNode(Node, false, false);
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Node->VariableReference.SetSelfMember(VariableName);
	Node->AllocateDefaultPins();
	return Node;
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
		return TEXT("None");
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
	if (!ParseStandardToggleVisibleVariableName(GetLinkedVariableName(*ShowPin), TemplateGroupOrdinal))
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

bool ValidatePatchedMaterialSlots(FRuntimeBuildContext& Context, FString& OutError)
{
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
			OutError = FString::Printf(
				TEXT("Template group %d has %d ShowMaterialSection node(s), but setup item '%s' binds %d slot(s). These slots would not be controlled: %s. Add enough ShowMaterialSection nodes to the template or reduce this Toggle Item's slots."),
				TemplateGroupOrdinal,
				PatchedNodeCount,
				*Group.Label,
				Group.Slots.Num(),
				*FString::Join(UnpatchedSlots, TEXT(",")));
			return false;
		}
		else if (Group.Slots.Num() > 1 && PatchedNodeCount % Group.Slots.Num() != 0)
		{
			if (Context.Result)
			{
				Context.Result->Warnings.Add(FString::Printf(
					TEXT("Template group %d has %d ShowMaterialSection node(s) for %d configured slots. Slots were assigned cyclically, but the final pass is partial; verify the template graph layout."),
					TemplateGroupOrdinal,
					PatchedNodeCount,
					Group.Slots.Num()));
			}
		}
	}
	return true;
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
			if (ParseStandardToggleInputVariableName(VariableName, TemplateGroupOrdinal))
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
			if (Context.Options.ToggleGroups.IsValidIndex(TemplateGroupOrdinal - 1))
			{
				SetPinDefaultValue(Context, Node, *KeyPin, GetModifierReplacementKey(Context.Options.ToggleGroups[TemplateGroupOrdinal - 1].Chord, KeyPin->DefaultValue));
			}
			else
			{
				SetPinDefaultValue(Context, Node, *KeyPin, TEXT("None"));
			}
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
		SetPinDefaultObject(Context, Node, *SaveGameClassPin, Context.TargetSaveGameClass);
	}

	if (UEdGraphPin* WidgetTypePin = FindPinByName(Node, TEXT("WidgetType")))
	{
		SetPinDefaultObject(Context, Node, *WidgetTypePin, Context.TargetWidgetClass);
	}
}

bool WasGeneratedByBlueprint(const UClass* Class, const UBlueprint* Blueprint)
{
	if (!Class || !Blueprint)
	{
		return false;
	}
	if (Class->ClassGeneratedBy == Blueprint)
	{
		return true;
	}

	const UClass* AuthoritativeClass = Class->GetAuthoritativeClass();
	return AuthoritativeClass && AuthoritativeClass->ClassGeneratedBy == Blueprint;
}

bool PatchExternalRuntimeVariableNode(FRuntimeBuildContext& Context, UK2Node_Variable& VariableNode)
{
	if (!Context.PostProcessAnimBlueprint || VariableNode.VariableReference.IsSelfContext())
	{
		return false;
	}

	UClass* MemberParentClass = VariableNode.VariableReference.GetMemberParentClass(Context.PostProcessAnimBlueprint->GeneratedClass);
	UClass* NewParentClass = nullptr;
	if (WasGeneratedByBlueprint(MemberParentClass, Context.TemplateSaveGameBlueprint))
	{
		NewParentClass = Context.TargetSaveGameClass;
	}
	else if (WasGeneratedByBlueprint(MemberParentClass, Context.TemplateWidgetBlueprint))
	{
		NewParentClass = Context.TargetWidgetClass;
	}

	if (!NewParentClass || MemberParentClass == NewParentClass)
	{
		return false;
	}

	const FName MemberName = VariableNode.VariableReference.GetMemberName();
	VariableNode.Modify();
	if (FProperty* TargetProperty = FindFProperty<FProperty>(NewParentClass, MemberName))
	{
		VariableNode.VariableReference.SetFromField<FProperty>(TargetProperty, false, NewParentClass);
	}
	else
	{
		VariableNode.VariableReference.SetExternalMember(MemberName, NewParentClass);
	}
	VariableNode.ReconstructNode();

	if (Context.Result)
	{
		Context.Result->Actions.Add(FString::Printf(
			TEXT("patched external variable node %s.%s parent to %s"),
			*VariableNode.GetName(),
			*MemberName.ToString(),
			*NewParentClass->GetPathName()));
	}
	return true;
}

UEdGraphPin* FindReplacementForOrphanPin(const UEdGraphNode& Node, const UEdGraphPin& OrphanPin)
{
	for (UEdGraphPin* CandidatePin : Node.Pins)
	{
		if (CandidatePin
			&& CandidatePin != &OrphanPin
			&& !CandidatePin->bOrphanedPin
			&& CandidatePin->Direction == OrphanPin.Direction
			&& CandidatePin->PinName == OrphanPin.PinName)
		{
			return CandidatePin;
		}
	}
	return nullptr;
}

int32 RepairOrphanPinsAfterRuntimeClassPatch(FRuntimeBuildContext& Context)
{
	TMap<UEdGraphPin*, UEdGraphPin*> ReplacementByOrphanPin;
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

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Pin->bOrphanedPin)
				{
					if (UEdGraphPin* ReplacementPin = FindReplacementForOrphanPin(*Node, *Pin))
					{
						ReplacementByOrphanPin.Add(Pin, ReplacementPin);
					}
				}
			}
		}
	}

	if (ReplacementByOrphanPin.IsEmpty())
	{
		return 0;
	}

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	int32 RepairedPinCount = 0;
	for (const TPair<UEdGraphPin*, UEdGraphPin*>& Pair : ReplacementByOrphanPin)
	{
		UEdGraphPin* OrphanPin = Pair.Key;
		UEdGraphPin* ReplacementPin = Pair.Value;
		UEdGraphNode* OwningNode = OrphanPin ? OrphanPin->GetOwningNodeUnchecked() : nullptr;
		if (!OrphanPin || !ReplacementPin || !OwningNode || !K2Schema)
		{
			continue;
		}

		const TArray<UEdGraphPin*> LinkedPins = OrphanPin->LinkedTo;
		for (UEdGraphPin* LinkedPin : LinkedPins)
		{
			if (!LinkedPin)
			{
				continue;
			}

			UEdGraphPin* ResolvedLinkedPin = LinkedPin;
			if (UEdGraphPin* const* LinkedReplacementPin = ReplacementByOrphanPin.Find(LinkedPin))
			{
				ResolvedLinkedPin = *LinkedReplacementPin;
			}
			if (!ResolvedLinkedPin || ResolvedLinkedPin == ReplacementPin || ResolvedLinkedPin->bOrphanedPin)
			{
				continue;
			}
			if (ReplacementPin->LinkedTo.Contains(ResolvedLinkedPin))
			{
				continue;
			}

			if (ReplacementPin->Direction == EGPD_Output)
			{
				K2Schema->TryCreateConnection(ReplacementPin, ResolvedLinkedPin);
			}
			else if (ReplacementPin->Direction == EGPD_Input)
			{
				K2Schema->TryCreateConnection(ResolvedLinkedPin, ReplacementPin);
			}
			else
			{
				K2Schema->TryCreateConnection(ReplacementPin, ResolvedLinkedPin);
			}
		}

		OrphanPin->BreakAllPinLinks(false);
		if (OwningNode->RemovePin(OrphanPin))
		{
			++RepairedPinCount;
		}
	}

	if (RepairedPinCount > 0 && Context.Result)
	{
		Context.Result->Actions.Add(FString::Printf(TEXT("repaired %d orphan PostProcess pin(s) after runtime class patch"), RepairedPinCount));
	}
	return RepairedPinCount;
}

bool CastNodeTargetsRuntimeClass(const UK2Node_DynamicCast& CastNode, const TCHAR* RuntimeClassName)
{
	const FString Title = CastNode.GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	if (Title.Contains(RuntimeClassName))
	{
		return true;
	}

	return CastNode.TargetType && CastNode.TargetType->GetName().Contains(RuntimeClassName);
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
		if (!ParseStandardToggleVisibleVariableName(Variable.VarName.ToString(), TemplateGroupOrdinal))
		{
			continue;
		}

		const int32 GroupIndex = TemplateGroupOrdinal - 1;
		Variable.DefaultValue = Context.Options.ToggleGroups.IsValidIndex(GroupIndex) && Context.Options.ToggleGroups[GroupIndex].bDefaultVisible ? TEXT("true") : TEXT("false");
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Context.SaveGameBlueprint);
	FKismetEditorUtilities::CompileBlueprint(Context.SaveGameBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
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

bool SetWidgetVisibilityIfDifferent(UWidget& Widget, const ESlateVisibility Visibility)
{
	if (Widget.GetVisibility() == Visibility)
	{
		return false;
	}

	Widget.Modify();
	Widget.SetVisibility(Visibility);
	return true;
}

bool SetWidgetEnabledIfDifferent(UWidget& Widget, const bool bEnabled)
{
	if (Widget.GetIsEnabled() == bEnabled)
	{
		return false;
	}

	Widget.Modify();
	Widget.SetIsEnabled(bEnabled);
	return true;
}

bool PatchButtonFocusableIfDifferent(UButton& Button, const bool bFocusable)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (Button.IsFocusable == bFocusable)
	{
		return false;
	}

	Button.Modify();
	Button.IsFocusable = bFocusable;
	return true;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FEdGraphPinType MakeBoolPinType()
{
	FEdGraphPinType PinType;
	PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	return PinType;
}

FEdGraphPinType MakeVector2DPinType()
{
	FEdGraphPinType PinType;
	PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	PinType.PinSubCategoryObject = TBaseStructure<FVector2D>::Get();
	return PinType;
}

bool EnsureWidgetMemberVariable(UWidgetBlueprint& WidgetBlueprint, const FName VariableName, const FEdGraphPinType& PinType, const FString& DefaultValue = FString())
{
	if (FBlueprintEditorUtils::FindNewVariableIndex(&WidgetBlueprint, VariableName) != INDEX_NONE)
	{
		return false;
	}

	FBlueprintEditorUtils::AddMemberVariable(&WidgetBlueprint, VariableName, PinType, DefaultValue);
	FBlueprintEditorUtils::SetBlueprintVariableCategory(&WidgetBlueprint, VariableName, nullptr, FText::FromString(TEXT("NTE Toggle Runtime")), true);
	return true;
}

void RemoveGeneratedWidgetDragNodes(UWidgetBlueprint& WidgetBlueprint)
{
	static const FString GeneratedPrefix = TEXT("NTE_Toggle_WBP_Drag");

	for (UEdGraph* Graph : WidgetBlueprint.UbergraphPages)
	{
		if (!Graph)
		{
			continue;
		}

		for (int32 NodeIndex = Graph->Nodes.Num() - 1; NodeIndex >= 0; --NodeIndex)
		{
			UEdGraphNode* Node = Graph->Nodes[NodeIndex];
			if (!Node)
			{
				continue;
			}

			if (Node->NodeComment.StartsWith(GeneratedPrefix))
			{
				Graph->RemoveNode(Node);
			}
		}
	}
}

void MarkGeneratedWidgetDragNode(UEdGraphNode& Node)
{
	if (!Node.NodeGuid.IsValid())
	{
		Node.CreateNewGuid();
	}
	Node.NodeComment = TEXT("NTE_Toggle_WBP_Drag");
	Node.bCommentBubblePinned = false;
	Node.bCommentBubbleVisible = false;
}

template <typename NodeType>
NodeType* AddGeneratedWidgetDragNode(UEdGraph& Graph, const int32 NodePosX, const int32 NodePosY)
{
	NodeType* Node = AddK2Node<NodeType>(Graph, NodePosX, NodePosY);
	MarkGeneratedWidgetDragNode(*Node);
	return Node;
}

UK2Node_CallFunction* AddGeneratedWidgetDragFunctionCall(UEdGraph& Graph, UFunction* Function, const int32 NodePosX, const int32 NodePosY)
{
	UK2Node_CallFunction* Node = AddFunctionCallNode(Graph, Function, NodePosX, NodePosY);
	if (Node)
	{
		MarkGeneratedWidgetDragNode(*Node);
	}
	return Node;
}

UK2Node_VariableGet* AddGeneratedWidgetDragVariableGet(UEdGraph& Graph, const FName VariableName, const int32 NodePosX, const int32 NodePosY)
{
	UK2Node_VariableGet* Node = AddVariableGetNode(Graph, VariableName, NodePosX, NodePosY);
	MarkGeneratedWidgetDragNode(*Node);
	return Node;
}

UK2Node_VariableSet* AddGeneratedWidgetDragVariableSet(UEdGraph& Graph, const FName VariableName, const int32 NodePosX, const int32 NodePosY)
{
	UK2Node_VariableSet* Node = AddVariableSetNode(Graph, VariableName, NodePosX, NodePosY);
	MarkGeneratedWidgetDragNode(*Node);
	return Node;
}

UK2Node_Event* FindOrAddWidgetTickEvent(UWidgetBlueprint& WidgetBlueprint, UEdGraph& Graph)
{
	if (UK2Node_Event* ExistingTick = FBlueprintEditorUtils::FindOverrideForFunction(&WidgetBlueprint, UUserWidget::StaticClass(), TEXT("Tick")))
	{
		return ExistingTick;
	}

	int32 NodePosY = 0;
	UK2Node_Event* TickEvent = FKismetEditorUtilities::AddDefaultEventNode(&WidgetBlueprint, &Graph, TEXT("Tick"), UUserWidget::StaticClass(), NodePosY);
	if (TickEvent)
	{
		MarkGeneratedWidgetDragNode(*TickEvent);
	}
	return TickEvent;
}

bool PatchWidgetBlueprintSelfDragGraph(FRuntimeBuildContext& Context, UWidgetBlueprint& WidgetBlueprint, bool& bOutChanged, FString& OutError)
{
	bOutChanged = false;
	if (!WidgetBlueprint.WidgetTree)
	{
		OutError = TEXT("TemplateWidgetBlueprint must be a UWidgetBlueprint with a WidgetTree.");
		return false;
	}

	if (!Cast<UButton>(WidgetBlueprint.WidgetTree->FindWidget(FName(TEXT("NTE_Toggle_TitleBarButton")))))
	{
		OutError = TEXT("Widget template is missing Button 'NTE_Toggle_TitleBarButton' required for generated widget-owned dragging.");
		return false;
	}

	if (!WidgetBlueprint.WidgetTree->FindWidget(FName(TEXT("NTE_Toggle_WindowPanel"))))
	{
		OutError = TEXT("Widget template is missing widget 'NTE_Toggle_WindowPanel' required for generated widget-owned dragging.");
		return false;
	}

	EnsureWidgetMemberVariable(WidgetBlueprint, TEXT("NTE_Toggle_WBP_UIDragging"), MakeBoolPinType(), TEXT("false"));
	EnsureWidgetMemberVariable(WidgetBlueprint, TEXT("NTE_Toggle_WBP_PreviousLeftMouseDown"), MakeBoolPinType(), TEXT("false"));
	EnsureWidgetMemberVariable(WidgetBlueprint, TEXT("NTE_Toggle_WBP_DragLastMousePosition"), MakeVector2DPinType());
	EnsureWidgetMemberVariable(WidgetBlueprint, TEXT("NTE_Toggle_WBP_DragOffset"), MakeVector2DPinType());

	if (WidgetBlueprint.UbergraphPages.IsEmpty() || !WidgetBlueprint.UbergraphPages[0])
	{
		UEdGraph* EventGraph = FBlueprintEditorUtils::CreateNewGraph(&WidgetBlueprint, UEdGraphSchema_K2::GN_EventGraph, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		FBlueprintEditorUtils::AddUbergraphPage(&WidgetBlueprint, EventGraph);
	}

	UEdGraph* EventGraph = WidgetBlueprint.UbergraphPages[0];
	if (!EventGraph)
	{
		OutError = TEXT("WidgetBlueprint has no event graph and one could not be created.");
		return false;
	}

	RemoveGeneratedWidgetDragNodes(WidgetBlueprint);

	UFunction* IsTitlePressedFunction = UButton::StaticClass()->FindFunctionByName(TEXT("IsPressed"));
	UFunction* GetMousePositionFunction = UWidgetLayoutLibrary::StaticClass()->FindFunctionByName(TEXT("GetMousePositionOnViewport"));
	UFunction* AddVector2DFunction = UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Add_Vector2DVector2D"));
	UFunction* SubtractVector2DFunction = UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Subtract_Vector2DVector2D"));
	UFunction* BoolAndFunction = UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("BooleanAND"));
	UFunction* BoolNotFunction = UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Not_PreBool"));
	UFunction* SetRenderTranslationFunction = UWidget::StaticClass()->FindFunctionByName(TEXT("SetRenderTranslation"));

	if (!IsTitlePressedFunction || !GetMousePositionFunction || !AddVector2DFunction || !SubtractVector2DFunction || !BoolAndFunction || !BoolNotFunction || !SetRenderTranslationFunction)
	{
		OutError = TEXT("Could not resolve one or more UMG/Kismet functions needed for generated widget-owned dragging.");
		return false;
	}

	UK2Node_Event* TickEvent = FindOrAddWidgetTickEvent(WidgetBlueprint, *EventGraph);
	if (!TickEvent)
	{
		OutError = TEXT("Could not create Widget Tick event for generated widget-owned dragging.");
		return false;
	}

	UK2Node_ExecutionSequence* SequenceNode = AddGeneratedWidgetDragNode<UK2Node_ExecutionSequence>(*EventGraph, 260, 0);
	SequenceNode->AddInputPin();

	UK2Node_Self* GetSelfNode = AddGeneratedWidgetDragNode<UK2Node_Self>(*EventGraph, 0, 180);
	UK2Node_CallFunction* GetMousePositionNode = AddGeneratedWidgetDragFunctionCall(*EventGraph, GetMousePositionFunction, 260, 180);

	UK2Node_VariableGet* GetTitleButtonForPressedNode = AddGeneratedWidgetDragVariableGet(*EventGraph, TEXT("NTE_Toggle_TitleBarButton"), 520, 360);
	UK2Node_CallFunction* IsTitlePressedNode = AddGeneratedWidgetDragFunctionCall(*EventGraph, IsTitlePressedFunction, 780, 360);
	UK2Node_CallFunction* NotPreviousMouseDownNode = AddGeneratedWidgetDragFunctionCall(*EventGraph, BoolNotFunction, 780, 520);
	UK2Node_VariableGet* GetPreviousMouseDownForNotNode = AddGeneratedWidgetDragVariableGet(*EventGraph, TEXT("NTE_Toggle_WBP_PreviousLeftMouseDown"), 520, 520);
	UK2Node_CallFunction* StartDragConditionNode = AddGeneratedWidgetDragFunctionCall(*EventGraph, BoolAndFunction, 1040, 440);
	UK2Node_IfThenElse* StartDragBranchNode = AddGeneratedWidgetDragNode<UK2Node_IfThenElse>(*EventGraph, 1300, 440);
	UK2Node_VariableSet* SetDraggingTrueNode = AddGeneratedWidgetDragVariableSet(*EventGraph, TEXT("NTE_Toggle_WBP_UIDragging"), 1560, 440);
	UK2Node_VariableSet* SetDragLastMouseOnStartNode = AddGeneratedWidgetDragVariableSet(*EventGraph, TEXT("NTE_Toggle_WBP_DragLastMousePosition"), 1820, 440);

	UK2Node_VariableGet* GetDraggingNode = AddGeneratedWidgetDragVariableGet(*EventGraph, TEXT("NTE_Toggle_WBP_UIDragging"), 520, 780);
	UK2Node_CallFunction* NotTitlePressedNode = AddGeneratedWidgetDragFunctionCall(*EventGraph, BoolNotFunction, 780, 780);
	UK2Node_IfThenElse* StopDragBranchNode = AddGeneratedWidgetDragNode<UK2Node_IfThenElse>(*EventGraph, 1040, 780);
	UK2Node_VariableSet* SetDraggingFalseNode = AddGeneratedWidgetDragVariableSet(*EventGraph, TEXT("NTE_Toggle_WBP_UIDragging"), 1300, 780);
	UK2Node_IfThenElse* DragMoveBranchNode = AddGeneratedWidgetDragNode<UK2Node_IfThenElse>(*EventGraph, 1040, 960);
	UK2Node_VariableGet* GetLastMouseForDeltaNode = AddGeneratedWidgetDragVariableGet(*EventGraph, TEXT("NTE_Toggle_WBP_DragLastMousePosition"), 1040, 1140);
	UK2Node_CallFunction* MouseDeltaNode = AddGeneratedWidgetDragFunctionCall(*EventGraph, SubtractVector2DFunction, 1300, 1060);
	UK2Node_VariableGet* GetDragOffsetNode = AddGeneratedWidgetDragVariableGet(*EventGraph, TEXT("NTE_Toggle_WBP_DragOffset"), 1300, 1240);
	UK2Node_CallFunction* NewDragOffsetNode = AddGeneratedWidgetDragFunctionCall(*EventGraph, AddVector2DFunction, 1560, 1100);
	UK2Node_VariableSet* SetDragOffsetNode = AddGeneratedWidgetDragVariableSet(*EventGraph, TEXT("NTE_Toggle_WBP_DragOffset"), 1820, 1100);
	UK2Node_VariableGet* GetWindowPanelForMoveNode = AddGeneratedWidgetDragVariableGet(*EventGraph, TEXT("NTE_Toggle_WindowPanel"), 1820, 1300);
	UK2Node_CallFunction* SetRenderTranslationNode = AddGeneratedWidgetDragFunctionCall(*EventGraph, SetRenderTranslationFunction, 2080, 1100);
	UK2Node_VariableSet* SetDragLastMouseAfterMoveNode = AddGeneratedWidgetDragVariableSet(*EventGraph, TEXT("NTE_Toggle_WBP_DragLastMousePosition"), 2340, 1100);

	UK2Node_VariableSet* SetPreviousMouseDownNode = AddGeneratedWidgetDragVariableSet(*EventGraph, TEXT("NTE_Toggle_WBP_PreviousLeftMouseDown"), 520, 1600);

	TryLinkPins(FindThenPin(*TickEvent), FindExecPin(*GetMousePositionNode));
	TryLinkPins(FindThenPin(*GetMousePositionNode), FindExecPin(*SequenceNode));
	TryLinkPins(FindPinByName(*SequenceNode, TEXT("then_0")), FindExecPin(*StartDragBranchNode));
	TryLinkPins(FindPinByName(*SequenceNode, TEXT("then_1")), FindExecPin(*StopDragBranchNode));
	TryLinkPins(FindPinByName(*SequenceNode, TEXT("then_2")), FindExecPin(*SetPreviousMouseDownNode));

	TryLinkPins(FindPinByName(*GetSelfNode, TEXT("self")), FindPinByName(*GetMousePositionNode, TEXT("WorldContextObject")));

	TryLinkPins(FindPinByName(*GetTitleButtonForPressedNode, TEXT("NTE_Toggle_TitleBarButton")), FindPinByName(*IsTitlePressedNode, TEXT("self")));
	TryLinkPins(FindPinByName(*GetPreviousMouseDownForNotNode, TEXT("NTE_Toggle_WBP_PreviousLeftMouseDown")), FindPinByName(*NotPreviousMouseDownNode, TEXT("A")));
	TryLinkPins(FindPinByName(*IsTitlePressedNode, TEXT("ReturnValue")), FindPinByName(*StartDragConditionNode, TEXT("A")));
	TryLinkPins(FindPinByName(*NotPreviousMouseDownNode, TEXT("ReturnValue")), FindPinByName(*StartDragConditionNode, TEXT("B")));
	TryLinkPins(FindPinByName(*StartDragConditionNode, TEXT("ReturnValue")), FindPinByName(*StartDragBranchNode, TEXT("Condition")));
	TryLinkPins(FindThenPin(*StartDragBranchNode), FindExecPin(*SetDraggingTrueNode));
	if (UEdGraphPin* DraggingTruePin = FindPinByName(*SetDraggingTrueNode, TEXT("NTE_Toggle_WBP_UIDragging")))
	{
		DraggingTruePin->DefaultValue = TEXT("true");
	}
	TryLinkPins(FindThenPin(*SetDraggingTrueNode), FindExecPin(*SetDragLastMouseOnStartNode));
	TryLinkPins(FindPinByName(*GetMousePositionNode, TEXT("ReturnValue")), FindPinByName(*SetDragLastMouseOnStartNode, TEXT("NTE_Toggle_WBP_DragLastMousePosition")));

	TryLinkPins(FindPinByName(*IsTitlePressedNode, TEXT("ReturnValue")), FindPinByName(*NotTitlePressedNode, TEXT("A")));
	TryLinkPins(FindPinByName(*NotTitlePressedNode, TEXT("ReturnValue")), FindPinByName(*StopDragBranchNode, TEXT("Condition")));
	TryLinkPins(FindThenPin(*StopDragBranchNode), FindExecPin(*SetDraggingFalseNode));
	if (UEdGraphPin* DraggingFalsePin = FindPinByName(*SetDraggingFalseNode, TEXT("NTE_Toggle_WBP_UIDragging")))
	{
		DraggingFalsePin->DefaultValue = TEXT("false");
	}
	TryLinkPins(FindPinByName(*StopDragBranchNode, TEXT("else")), FindExecPin(*DragMoveBranchNode));
	TryLinkPins(FindPinByName(*GetDraggingNode, TEXT("NTE_Toggle_WBP_UIDragging")), FindPinByName(*DragMoveBranchNode, TEXT("Condition")));
	TryLinkPins(FindThenPin(*DragMoveBranchNode), FindExecPin(*SetDragOffsetNode));

	TryLinkPins(FindPinByName(*GetMousePositionNode, TEXT("ReturnValue")), FindPinByName(*MouseDeltaNode, TEXT("A")));
	TryLinkPins(FindPinByName(*GetLastMouseForDeltaNode, TEXT("NTE_Toggle_WBP_DragLastMousePosition")), FindPinByName(*MouseDeltaNode, TEXT("B")));
	TryLinkPins(FindPinByName(*GetDragOffsetNode, TEXT("NTE_Toggle_WBP_DragOffset")), FindPinByName(*NewDragOffsetNode, TEXT("A")));
	TryLinkPins(FindPinByName(*MouseDeltaNode, TEXT("ReturnValue")), FindPinByName(*NewDragOffsetNode, TEXT("B")));
	TryLinkPins(FindPinByName(*NewDragOffsetNode, TEXT("ReturnValue")), FindPinByName(*SetDragOffsetNode, TEXT("NTE_Toggle_WBP_DragOffset")));
	TryLinkPins(FindThenPin(*SetDragOffsetNode), FindExecPin(*SetRenderTranslationNode));
	TryLinkPins(FindPinByName(*GetWindowPanelForMoveNode, TEXT("NTE_Toggle_WindowPanel")), FindPinByName(*SetRenderTranslationNode, TEXT("self")));
	TryLinkPins(FindPinByName(*NewDragOffsetNode, TEXT("ReturnValue")), FindPinByName(*SetRenderTranslationNode, TEXT("Translation")));
	TryLinkPins(FindThenPin(*SetRenderTranslationNode), FindExecPin(*SetDragLastMouseAfterMoveNode));
	TryLinkPins(FindPinByName(*GetMousePositionNode, TEXT("ReturnValue")), FindPinByName(*SetDragLastMouseAfterMoveNode, TEXT("NTE_Toggle_WBP_DragLastMousePosition")));

	TryLinkPins(FindPinByName(*IsTitlePressedNode, TEXT("ReturnValue")), FindPinByName(*SetPreviousMouseDownNode, TEXT("NTE_Toggle_WBP_PreviousLeftMouseDown")));

	EventGraph->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&WidgetBlueprint);
	if (Context.Result)
	{
		Context.Result->Actions.Add(TEXT("patched widget-owned drag graph"));
	}
	bOutChanged = true;
	return true;
}

bool PatchWidgetChromeBehavior(FRuntimeBuildContext& Context, UWidgetBlueprint& WidgetBlueprint, bool& bOutChanged, FString& OutError)
{
	bOutChanged = false;
	if (!WidgetBlueprint.WidgetTree)
	{
		OutError = TEXT("TemplateWidgetBlueprint must be a UWidgetBlueprint with a WidgetTree.");
		return false;
	}

	UButton* TitleButton = Cast<UButton>(WidgetBlueprint.WidgetTree->FindWidget(FName(TEXT("NTE_Toggle_TitleBarButton"))));
	if (!TitleButton)
	{
		OutError = TEXT("Widget template is missing Button 'NTE_Toggle_TitleBarButton' required for dragging the toggle UI.");
		return false;
	}

	bool bChanged = false;
	bChanged |= SetWidgetVisibilityIfDifferent(*TitleButton, ESlateVisibility::Visible);
	bChanged |= SetWidgetEnabledIfDifferent(*TitleButton, true);
	bChanged |= PatchButtonFocusableIfDifferent(*TitleButton, false);

	TArray<UWidget*> Widgets;
	WidgetBlueprint.WidgetTree->GetAllWidgets(Widgets);
	for (UWidget* Widget : Widgets)
	{
		if (!Widget || Widget == TitleButton)
		{
			continue;
		}

		if (IsDescendantOfWidget(*WidgetBlueprint.WidgetTree, *Widget, *TitleButton))
		{
			bChanged |= SetWidgetVisibilityIfDifferent(*Widget, ESlateVisibility::HitTestInvisible);
		}
	}

	for (int32 GroupIndex = 0; GroupIndex < Context.Options.ToggleGroups.Num(); ++GroupIndex)
	{
		const int32 TemplateGroupOrdinal = GroupIndex + 1;
		UButton* Button = Cast<UButton>(WidgetBlueprint.WidgetTree->FindWidget(FName(*MakeStandardToggleButtonWidgetName(TemplateGroupOrdinal))));
		if (Button)
		{
			bChanged |= PatchButtonFocusableIfDifferent(*Button, false);
		}

		UTextBlock* LabelTextBlock = Cast<UTextBlock>(WidgetBlueprint.WidgetTree->FindWidget(FName(*MakeStandardToggleButtonLabelWidgetName(TemplateGroupOrdinal))));
		if (LabelTextBlock)
		{
			bChanged |= SetWidgetVisibilityIfDifferent(*LabelTextBlock, ESlateVisibility::HitTestInvisible);
		}
	}

	if (bChanged && Context.Result)
	{
		Context.Result->Actions.Add(TEXT("patched widget chrome hit-test behavior for drag/buttons"));
	}
	bOutChanged = bChanged;
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
	bool bChromeChanged = false;
	if (!PatchWidgetChromeBehavior(Context, *WidgetBlueprint, bChromeChanged, OutError))
	{
		return false;
	}
	bChanged |= bChromeChanged;
	bool bDragGraphChanged = false;
	if (!PatchWidgetBlueprintSelfDragGraph(Context, *WidgetBlueprint, bDragGraphChanged, OutError))
	{
		return false;
	}
	bChanged |= bDragGraphChanged;

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
		const FString ButtonWidgetName = MakeStandardToggleButtonWidgetName(TemplateGroupOrdinal);
		UWidget* ButtonWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*ButtonWidgetName));
		UButton* Button = Cast<UButton>(ButtonWidget);
		if (!Button)
		{
			OutError = FString::Printf(
				TEXT("Widget template is missing Button '%s' required for standard toggle group %d."),
				*ButtonWidgetName,
				TemplateGroupOrdinal);
			return false;
		}

		const FString LabelWidgetName = MakeStandardToggleButtonLabelWidgetName(TemplateGroupOrdinal);
		UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(FName(*LabelWidgetName));
		UTextBlock* LabelTextBlock = Cast<UTextBlock>(Widget);
		if (!LabelTextBlock)
		{
			OutError = FString::Printf(
				TEXT("Widget template is missing TextBlock '%s' required for standard toggle group %d."),
				*LabelWidgetName,
				TemplateGroupOrdinal);
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

		const FNteMeshToggleGroup& Group = Context.Options.ToggleGroups[GroupIndex];
		bChanged |= SetWidgetVisibilityIfDifferent(*Button, ESlateVisibility::Visible);
		bChanged |= SetWidgetEnabledIfDifferent(*Button, true);
		bChanged |= SetWidgetVisibilityIfDifferent(*LabelTextBlock, ESlateVisibility::HitTestInvisible);
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

	TArray<int32> WidgetGroupOrdinals = Context.StandardTemplateModel.GetWidgetGroups().Array();
	WidgetGroupOrdinals.Sort();
	for (const int32 WidgetGroupOrdinal : WidgetGroupOrdinals)
	{
		if (WidgetGroupOrdinal <= Context.Options.ToggleGroups.Num())
		{
			continue;
		}

		UButton* Button = Cast<UButton>(WidgetBlueprint->WidgetTree->FindWidget(FName(*MakeStandardToggleButtonWidgetName(WidgetGroupOrdinal))));
		if (Button)
		{
			bChanged |= SetWidgetVisibilityIfDifferent(*Button, ESlateVisibility::Collapsed);
			bChanged |= SetWidgetEnabledIfDifferent(*Button, false);
		}

		UTextBlock* LabelTextBlock = Cast<UTextBlock>(WidgetBlueprint->WidgetTree->FindWidget(FName(*MakeStandardToggleButtonLabelWidgetName(WidgetGroupOrdinal))));
		if (LabelTextBlock)
		{
			bChanged |= SetWidgetVisibilityIfDifferent(*LabelTextBlock, ESlateVisibility::Collapsed);
		}
		if (Context.Result && (Button || LabelTextBlock))
		{
			Context.Result->Actions.Add(FString::Printf(TEXT("hid unused widget group %d"), WidgetGroupOrdinal));
		}
	}

	if (bChanged)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	}
	return true;
}

bool ValidateTemplateCompatibility(const FRuntimeBuildContext& Context, FString& OutError)
{
	const FNteStandardToggleTemplateModel& Model = Context.StandardTemplateModel;
	const int32 RequiredGroupCount = Context.Options.ToggleGroups.Num();
	const int32 SaveGameCapacity = Model.GetSaveGameVisibleCapacity();
	const int32 PostProcessCapacity = Model.GetPostProcessVisibleCapacity();
	if (SaveGameCapacity <= 0)
	{
		OutError = TEXT("Standard SaveGame template does not expose any contiguous NTE_Toggle_*_Visible groups.");
		return false;
	}
	if (PostProcessCapacity <= 0)
	{
		OutError = TEXT("Standard Post Process template does not expose any contiguous NTE_Toggle_*_Visible groups.");
		return false;
	}

	if (SaveGameCapacity < RequiredGroupCount)
	{
		OutError = FString::Printf(
			TEXT("Standard SaveGame template capacity (%d) is smaller than setup group count (%d). Add contiguous NTE_Toggle_*_Visible variables or reduce the setup."),
			SaveGameCapacity,
			RequiredGroupCount);
		return false;
	}
	if (PostProcessCapacity < RequiredGroupCount)
	{
		OutError = FString::Printf(
			TEXT("Standard Post Process template capacity (%d) is smaller than setup group count (%d). Add contiguous NTE_Toggle_*_Visible groups to the template graph or reduce the setup."),
			PostProcessCapacity,
			RequiredGroupCount);
		return false;
	}

	for (int32 GroupIndex = 0; GroupIndex < RequiredGroupCount; ++GroupIndex)
	{
		const int32 TemplateGroupOrdinal = GroupIndex + 1;
		if (!Model.WidgetButtonGroups.Contains(TemplateGroupOrdinal))
		{
			OutError = FString::Printf(
				TEXT("Standard Widget template is missing Button '%s' for setup group %d."),
				*MakeStandardToggleButtonWidgetName(TemplateGroupOrdinal),
				TemplateGroupOrdinal);
			return false;
		}
		if (!Model.WidgetLabelGroups.Contains(TemplateGroupOrdinal))
		{
			OutError = FString::Printf(
				TEXT("Standard Widget template is missing TextBlock '%s' for setup group %d."),
				*MakeStandardToggleButtonLabelWidgetName(TemplateGroupOrdinal),
				TemplateGroupOrdinal);
			return false;
		}

		const FNteMeshToggleGroup& Group = Context.Options.ToggleGroups[GroupIndex];
		if (Group.Chord.Key.IsValid() && !Model.PostProcessInputGroups.Contains(TemplateGroupOrdinal))
		{
			OutError = FString::Printf(
				TEXT("Standard Post Process template is missing input marker 'NTE_Toggle_Input_%d_*' for setup group %d with hotkey %s. Add the marker or clear the hotkey for a UI-only item."),
				TemplateGroupOrdinal,
				TemplateGroupOrdinal,
				*Group.Chord.Key.GetFName().ToString());
			return false;
		}
	}

	if (SaveGameCapacity != PostProcessCapacity && Context.Result)
	{
		Context.Result->Warnings.Add(FString::Printf(
			TEXT("Standard template state capacity differs: SaveGame exposes %d group(s), Post Process exposes %d group(s). The usable runtime state capacity is %d."),
			SaveGameCapacity,
			PostProcessCapacity,
			Model.GetRuntimeStateCapacity()));
	}
	if (Model.GetRuntimeStateCapacity() > RequiredGroupCount && Context.Result)
	{
		Context.Result->Warnings.Add(FString::Printf(
			TEXT("Standard runtime template has %d state group(s) and setup uses %d. Extra input groups will be patched to None and any extra widget groups will be hidden in the generated runtime."),
			Model.GetRuntimeStateCapacity(),
			RequiredGroupCount));
	}
	return true;
}

void PatchPostProcessBlueprintGraph(FRuntimeBuildContext& Context)
{
	PatchMemberVariableType(Context, TEXT("NTE_Toggle_SaveObject"), Context.TargetSaveGameClass);
	PatchMemberVariableType(Context, TEXT("NTE_Toggle_Widget"), Context.TargetWidgetClass);

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
				if (CastNodeTargetsRuntimeClass(*CastNode, TEXT("BP_NTE_ModToggleSaveGame")))
				{
					PatchCastNode(Context, *CastNode, Context.TargetSaveGameClass);
				}
				else if (CastNodeTargetsRuntimeClass(*CastNode, TEXT("WBP_NTE_ModToggleMenu")))
				{
					PatchCastNode(Context, *CastNode, Context.TargetWidgetClass);
				}
			}
			if (UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node))
			{
				PatchExternalRuntimeVariableNode(Context, *VariableNode);
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
	RepairOrphanPinsAfterRuntimeClassPatch(Context);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Context.PostProcessAnimBlueprint);
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

	Context.TemplateSaveGameBlueprint = Cast<UBlueprint>(LoadAnyAssetByPath(Options.TemplateSaveGameBlueprintPath));
	Context.TemplateWidgetBlueprint = Cast<UBlueprint>(LoadAnyAssetByPath(Options.TemplateWidgetBlueprintPath));
	if (!Context.TemplateSaveGameBlueprint)
	{
		OutError = FString::Printf(TEXT("Could not load template SaveGame Blueprint for reference patching: %s"), *Options.TemplateSaveGameBlueprintPath);
		return false;
	}
	if (!Context.TemplateWidgetBlueprint)
	{
		OutError = FString::Printf(TEXT("Could not load template Widget Blueprint for reference patching: %s"), *Options.TemplateWidgetBlueprintPath);
		return false;
	}

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
	Context.TargetSaveGameClass = Context.SaveGameBlueprint->GeneratedClass;
	Context.TargetWidgetClass = Context.WidgetBlueprint->GeneratedClass;
	if (!Context.TargetSaveGameClass)
	{
		OutError = FString::Printf(TEXT("Generated SaveGame class is not available after compile: %s"), *Context.TargetSaveGameBlueprintPath);
		return false;
	}
	if (!Context.TargetWidgetClass)
	{
		OutError = FString::Printf(TEXT("Generated Widget class is not available after compile: %s"), *Context.TargetWidgetBlueprintPath);
		return false;
	}

	FKismetEditorUtilities::CompileBlueprint(Context.PostProcessAnimBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	Context.StandardTemplateModel = BuildStandardToggleTemplateModel(
		Context.SaveGameBlueprint,
		Context.PostProcessAnimBlueprint,
		Cast<UWidgetBlueprint>(Context.WidgetBlueprint));

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
	if (!ValidatePatchedMaterialSlots(Context, OutError))
	{
		return false;
	}

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
