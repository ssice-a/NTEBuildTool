// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteCharacterRuntimeActionWriter.h"

#include "NteEditorAssetUtils.h"
#include "NteJsonFileUtils.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/ActorComponent.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/SceneComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/Font.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimBlueprintFactory.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "GameFramework/PlayerController.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Self.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace NTEBuildTool::Character
{
namespace
{
constexpr const TCHAR* GeneratedRuntimeNodeComment = TEXT("NTE Character RuntimeAction Generated");
constexpr const TCHAR* RuntimeUiWidgetVariableName = TEXT("NTE_RuntimeUi_Widget");
constexpr const TCHAR* RuntimeSaveObjectVariableName = TEXT("NTE_Runtime_SaveObject");
constexpr const TCHAR* RuntimeSaveSlotNameVariableName = TEXT("NTE_Runtime_SaveSlotName");

struct FRuntimeHotkey
{
	FKey Key;
	bool bCtrl = false;
	bool bAlt = false;
	bool bShift = false;
	bool bCmd = false;

	bool HasModifiers() const
	{
		return bCtrl || bAlt || bShift || bCmd;
	}
};

struct FRuntimeTargetComponentPins
{
	UEdGraphPin* ExecOut = nullptr;
	UEdGraphPin* ComponentPin = nullptr;
};

void AddError(FNteCharacterRuntimeActionWriteResult& Result, const FString& Error)
{
	Result.Errors.Add(Error);
}

void AddError(FNteCharacterRuntimeActionAssetWriteResult& Result, const FString& Error)
{
	Result.Errors.Add(Error);
}

void AddWarning(FNteCharacterRuntimeActionWriteResult& Result, const FString& Warning)
{
	Result.Warnings.Add(Warning);
}

void AddSeed(TArray<FString>& Seeds, FString Path)
{
	Path = NTEBuildTool::Editor::NormalizeAssetPathForText(Path);
	if (Path.StartsWith(TEXT("/Game/")) && !Path.Contains(TEXT(".")))
	{
		Seeds.AddUnique(Path);
	}
}

FString SanitizeVariableSuffix(const FString& RawValue)
{
	FString Result;
	for (const TCHAR Character : RawValue)
	{
		if (FChar::IsAlnum(Character))
		{
			Result.AppendChar(Character);
		}
		else
		{
			Result.AppendChar(TEXT('_'));
		}
	}
	while (Result.Contains(TEXT("__")))
	{
		Result.ReplaceInline(TEXT("__"), TEXT("_"));
	}
	Result.TrimStartAndEndInline();
	Result.RemoveFromStart(TEXT("_"));
	Result.RemoveFromEnd(TEXT("_"));
	return Result.IsEmpty() ? TEXT("Action") : Result;
}

FString MakeActionVariablePrefix(const FNteCharacterRuntimeActionPlanItem& Action)
{
	return TEXT("NTE_Action_") + SanitizeVariableSuffix(Action.Id);
}

FName MakeActionEnabledVariableName(const FNteCharacterRuntimeActionPlanItem& Action)
{
	return FName(*(MakeActionVariablePrefix(Action) + TEXT("_Enabled")));
}

FString JoinInts(const TArray<int32>& Values)
{
	TArray<FString> Parts;
	for (const int32 Value : Values)
	{
		Parts.Add(FString::FromInt(Value));
	}
	return FString::Join(Parts, TEXT(","));
}

bool ParseRuntimeHotkey(const FString& RawHotkey, FRuntimeHotkey& OutHotkey)
{
	OutHotkey = FRuntimeHotkey();
	FString NormalizedHotkey = RawHotkey;
	NormalizedHotkey.TrimStartAndEndInline();
	NormalizedHotkey.ReplaceInline(TEXT(" "), TEXT(""));
	if (NormalizedHotkey.IsEmpty())
	{
		return false;
	}

	TArray<FString> Tokens;
	NormalizedHotkey.ParseIntoArray(Tokens, TEXT("+"), true);
	FString KeyName;
	for (FString Token : Tokens)
	{
		Token.TrimStartAndEndInline();
		if (Token.IsEmpty())
		{
			continue;
		}

		if (Token.Equals(TEXT("Ctrl"), ESearchCase::IgnoreCase)
			|| Token.Equals(TEXT("Control"), ESearchCase::IgnoreCase))
		{
			OutHotkey.bCtrl = true;
			continue;
		}
		if (Token.Equals(TEXT("Alt"), ESearchCase::IgnoreCase))
		{
			OutHotkey.bAlt = true;
			continue;
		}
		if (Token.Equals(TEXT("Shift"), ESearchCase::IgnoreCase))
		{
			OutHotkey.bShift = true;
			continue;
		}
		if (Token.Equals(TEXT("Cmd"), ESearchCase::IgnoreCase)
			|| Token.Equals(TEXT("Command"), ESearchCase::IgnoreCase))
		{
			OutHotkey.bCmd = true;
			continue;
		}

		if (!KeyName.IsEmpty())
		{
			return false;
		}
		KeyName = Token;
	}

	if (KeyName.IsEmpty())
	{
		return false;
	}

	OutHotkey.Key = FKey(*KeyName);
	return OutHotkey.Key.IsValid();
}

FString PlanToCondensedJson(const FNteCharacterRuntimeActionPlan& Plan)
{
	FString JsonText;
	const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonText);
	FJsonSerializer::Serialize(CharacterRuntimeActionPlanToJson(Plan), Writer);
	return JsonText;
}

FEdGraphPinType MakePinType(const FName PinCategory)
{
	FEdGraphPinType PinType;
	PinType.PinCategory = PinCategory;
	return PinType;
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

UEdGraphPin* FindPinByName(UEdGraphNode& Node, const FName PinName)
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

void SetPinDefaultValue(UEdGraphNode& Node, UEdGraphPin& Pin, const FString& NewValue)
{
	if (Pin.DefaultValue == NewValue)
	{
		return;
	}

	Pin.Modify();
	Pin.DefaultValue = NewValue;
	Node.PinDefaultValueChanged(&Pin);
}

void SetPinDefaultObject(UEdGraphNode& Node, UEdGraphPin& Pin, UObject* NewObject)
{
	if (Pin.DefaultObject == NewObject)
	{
		return;
	}

	Pin.Modify();
	Pin.DefaultObject = NewObject;
	Node.PinDefaultValueChanged(&Pin);
}

template <typename NodeType>
NodeType* AddRuntimeK2Node(UEdGraph& Graph, const int32 NodePosX, const int32 NodePosY)
{
	NodeType* Node = NewObject<NodeType>(&Graph);
	Node->CreateNewGuid();
	Graph.AddNode(Node, false, false);
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Node->NodeComment = GeneratedRuntimeNodeComment;
	Node->AllocateDefaultPins();
	return Node;
}

UK2Node_CallFunction* AddRuntimeFunctionCallNode(UEdGraph& Graph, UFunction* Function, const int32 NodePosX, const int32 NodePosY)
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
	Node->NodeComment = GeneratedRuntimeNodeComment;
	Node->SetFromFunction(Function);
	Node->AllocateDefaultPins();
	return Node;
}

UK2Node_VariableGet* AddRuntimeVariableGetNode(UEdGraph& Graph, const FName VariableName, const int32 NodePosX, const int32 NodePosY)
{
	UK2Node_VariableGet* Node = NewObject<UK2Node_VariableGet>(&Graph);
	Node->CreateNewGuid();
	Graph.AddNode(Node, false, false);
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Node->NodeComment = GeneratedRuntimeNodeComment;
	Node->VariableReference.SetSelfMember(VariableName);
	Node->AllocateDefaultPins();
	return Node;
}

UK2Node_VariableSet* AddRuntimeVariableSetNode(UEdGraph& Graph, const FName VariableName, const int32 NodePosX, const int32 NodePosY)
{
	UK2Node_VariableSet* Node = NewObject<UK2Node_VariableSet>(&Graph);
	Node->CreateNewGuid();
	Graph.AddNode(Node, false, false);
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Node->NodeComment = GeneratedRuntimeNodeComment;
	Node->VariableReference.SetSelfMember(VariableName);
	Node->AllocateDefaultPins();
	return Node;
}

UK2Node_DynamicCast* AddRuntimeDynamicCastNode(UEdGraph& Graph, UClass* TargetClass, const int32 NodePosX, const int32 NodePosY)
{
	if (!TargetClass)
	{
		return nullptr;
	}

	UK2Node_DynamicCast* Node = NewObject<UK2Node_DynamicCast>(&Graph);
	Node->CreateNewGuid();
	Graph.AddNode(Node, false, false);
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Node->NodeComment = GeneratedRuntimeNodeComment;
	Node->TargetType = TargetClass;
	Node->AllocateDefaultPins();
	return Node;
}

void SetRuntimeExternalMemberReference(UK2Node_Variable& Node, const FName VariableName, UClass* OwnerClass)
{
	if (!OwnerClass)
	{
		return;
	}

	if (FProperty* TargetProperty = FindFProperty<FProperty>(OwnerClass, VariableName))
	{
		Node.VariableReference.SetFromField<FProperty>(TargetProperty, false, OwnerClass);
	}
	else
	{
		Node.VariableReference.SetExternalMember(VariableName, OwnerClass);
	}
}

UK2Node_VariableGet* AddRuntimeExternalVariableGetNode(
	UEdGraph& Graph,
	const FName VariableName,
	UClass* OwnerClass,
	const int32 NodePosX,
	const int32 NodePosY)
{
	if (!OwnerClass)
	{
		return nullptr;
	}

	UK2Node_VariableGet* Node = NewObject<UK2Node_VariableGet>(&Graph);
	Node->CreateNewGuid();
	Graph.AddNode(Node, false, false);
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Node->NodeComment = GeneratedRuntimeNodeComment;
	SetRuntimeExternalMemberReference(*Node, VariableName, OwnerClass);
	Node->AllocateDefaultPins();
	return Node;
}

UK2Node_VariableSet* AddRuntimeExternalVariableSetNode(
	UEdGraph& Graph,
	const FName VariableName,
	UClass* OwnerClass,
	const int32 NodePosX,
	const int32 NodePosY)
{
	if (!OwnerClass)
	{
		return nullptr;
	}

	UK2Node_VariableSet* Node = NewObject<UK2Node_VariableSet>(&Graph);
	Node->CreateNewGuid();
	Graph.AddNode(Node, false, false);
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Node->NodeComment = GeneratedRuntimeNodeComment;
	SetRuntimeExternalMemberReference(*Node, VariableName, OwnerClass);
	Node->AllocateDefaultPins();
	return Node;
}

FEdGraphPinType MakeStringPinType()
{
	return MakePinType(UEdGraphSchema_K2::PC_String);
}

FEdGraphPinType MakeBoolPinType()
{
	return MakePinType(UEdGraphSchema_K2::PC_Boolean);
}

FEdGraphPinType MakeIntPinType()
{
	return MakePinType(UEdGraphSchema_K2::PC_Int);
}

FEdGraphPinType MakeObjectPinType(UClass* ObjectClass)
{
	FEdGraphPinType PinType = MakePinType(UEdGraphSchema_K2::PC_Object);
	PinType.PinSubCategoryObject = ObjectClass;
	return PinType;
}

void RemoveGeneratedRuntimeNodes(UBlueprint& Blueprint)
{
	for (UEdGraph* Graph : Blueprint.UbergraphPages)
	{
		if (!Graph)
		{
			continue;
		}

		TArray<UEdGraphNode*> NodesToRemove;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeComment.StartsWith(GeneratedRuntimeNodeComment))
			{
				NodesToRemove.Add(Node);
			}
		}

		for (UEdGraphNode* Node : NodesToRemove)
		{
			Graph->RemoveNode(Node);
		}
	}
}

UEdGraph* EnsureEventGraph(UBlueprint& Blueprint)
{
	if (Blueprint.UbergraphPages.IsEmpty() || !Blueprint.UbergraphPages[0])
	{
		UEdGraph* EventGraph = FBlueprintEditorUtils::CreateNewGraph(
			&Blueprint,
			UEdGraphSchema_K2::GN_EventGraph,
			UEdGraph::StaticClass(),
			UEdGraphSchema_K2::StaticClass());
		FBlueprintEditorUtils::AddUbergraphPage(&Blueprint, EventGraph);
	}
	return Blueprint.UbergraphPages.IsEmpty() ? nullptr : Blueprint.UbergraphPages[0];
}

UK2Node_Event* FindOrAddAnimEvent(
	UBlueprint& Blueprint,
	UEdGraph& Graph,
	const TCHAR* FunctionName,
	const int32 NodePosY)
{
	if (UK2Node_Event* ExistingEvent = FBlueprintEditorUtils::FindOverrideForFunction(&Blueprint, UAnimInstance::StaticClass(), FunctionName))
	{
		return ExistingEvent;
	}
	int32 MutableNodePosY = NodePosY;
	return FKismetEditorUtilities::AddDefaultEventNode(&Blueprint, &Graph, FunctionName, UAnimInstance::StaticClass(), MutableNodePosY);
}

UK2Node_Event* FindOrAddAnimInitializeEvent(UBlueprint& Blueprint, UEdGraph& Graph)
{
	return FindOrAddAnimEvent(Blueprint, Graph, TEXT("BlueprintInitializeAnimation"), -1800);
}

UK2Node_Event* FindOrAddAnimUpdateEvent(UBlueprint& Blueprint, UEdGraph& Graph)
{
	return FindOrAddAnimEvent(Blueprint, Graph, TEXT("BlueprintUpdateAnimation"), 0);
}

UEdGraphPin* EnsureSequenceOutputPin(UK2Node_ExecutionSequence& SequenceNode, const int32 Index)
{
	while (!SequenceNode.GetThenPinGivenIndex(Index))
	{
		SequenceNode.AddInputPin();
	}
	return SequenceNode.GetThenPinGivenIndex(Index);
}

bool EnsureBlueprintVariable(UBlueprint& Blueprint, const FName VariableName, const FEdGraphPinType& PinType, const FString& DefaultValue, FNteCharacterRuntimeActionAssetWriteResult& Result)
{
	bool bChanged = false;
	bool bFound = false;
	for (FBPVariableDescription& Variable : Blueprint.NewVariables)
	{
		if (Variable.VarName != VariableName)
		{
			continue;
		}

		bFound = true;
		if (Variable.VarType != PinType)
		{
			Variable.VarType = PinType;
			bChanged = true;
			Result.Actions.Add(FString::Printf(TEXT("changed variable type %s"), *VariableName.ToString()));
		}
		if (Variable.DefaultValue != DefaultValue)
		{
			Variable.DefaultValue = DefaultValue;
			bChanged = true;
			Result.Actions.Add(FString::Printf(TEXT("updated variable default %s"), *VariableName.ToString()));
		}
		break;
	}

	if (!bFound)
	{
		FBlueprintEditorUtils::AddMemberVariable(&Blueprint, VariableName, PinType, DefaultValue);
		bChanged = true;
		Result.Actions.Add(FString::Printf(TEXT("added variable %s"), *VariableName.ToString()));
	}

	if (bChanged)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);
	}
	return bChanged;
}

bool SaveLoadedAssetPackage(UObject& Asset, FNteCharacterRuntimeActionWriteResult& Result, FString& OutError)
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

	Result.SavedPackages.AddUnique(Package->GetName());
	return true;
}

UFont* LoadOrCreateSourceGameFontProxy(
	const FString& FontPath,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	if (!FontPath.StartsWith(TEXT("/Game/")) || FontPath.Contains(TEXT(".")))
	{
		AddError(AssetResult, FString::Printf(
			TEXT("Runtime UI FontPath must be a /Game package path: %s"),
			*FontPath));
		return nullptr;
	}

	if (UFont* ExistingFont = NTEBuildTool::Editor::LoadAssetByPath<UFont>(FontPath))
	{
		AssetResult.Actions.Add(FString::Printf(TEXT("uses source-game font %s"), *FontPath));
		return ExistingFont;
	}

	FString ExistingPackageFilename;
	if (FPackageName::DoesPackageExist(FontPath, &ExistingPackageFilename))
	{
		AddError(AssetResult, FString::Printf(
			TEXT("Runtime UI font package exists but its UFont could not be loaded: %s (%s)"),
			*FontPath,
			*ExistingPackageFilename));
		return nullptr;
	}

	UPackage* Package = FindPackage(nullptr, *FontPath);
	if (!Package)
	{
		Package = CreatePackage(*FontPath);
	}
	if (!Package)
	{
		AddError(AssetResult, FString::Printf(TEXT("Could not create source-game font proxy package: %s"), *FontPath));
		return nullptr;
	}

	const FName FontName(*FPackageName::GetShortName(FontPath));
	if (UObject* ExistingObject = FindObject<UObject>(Package, *FontName.ToString()))
	{
		if (UFont* ExistingFont = Cast<UFont>(ExistingObject))
		{
			return ExistingFont;
		}
		AddError(AssetResult, FString::Printf(
			TEXT("Source-game font proxy path is occupied by %s: %s"),
			*ExistingObject->GetClass()->GetPathName(),
			*FontPath));
		return nullptr;
	}

	UFont* FontProxy = NewObject<UFont>(Package, FontName, RF_Public | RF_Standalone | RF_Transactional);
	if (!FontProxy)
	{
		AddError(AssetResult, FString::Printf(TEXT("Could not create source-game font proxy: %s"), *FontPath));
		return nullptr;
	}

	FontProxy->FontCacheType = EFontCacheType::Runtime;
	FontProxy->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(FontProxy);
	Package->MarkPackageDirty();

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(FontPath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, FontProxy, *PackageFilename, SaveArgs))
	{
		AddError(AssetResult, FString::Printf(TEXT("Could not save source-game font proxy: %s"), *PackageFilename));
		return nullptr;
	}

	AssetResult.Actions.Add(FString::Printf(
		TEXT("created editor-only source-game font proxy %s"),
		*FontPath));
	return FontProxy;
}

UBlueprint* CreateOrLoadBlueprint(
	const FString& BlueprintPath,
	UClass* ParentClass,
	TSubclassOf<UBlueprint> BlueprintClass,
	TSubclassOf<UBlueprintGeneratedClass> GeneratedClass,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	if (BlueprintPath.IsEmpty())
	{
		AddError(AssetResult, TEXT("Blueprint path is empty."));
		return nullptr;
	}
	if (!BlueprintPath.StartsWith(TEXT("/Game/")) || BlueprintPath.Contains(TEXT(".")))
	{
		AddError(AssetResult, FString::Printf(TEXT("Blueprint path must be a /Game package path: %s"), *BlueprintPath));
		return nullptr;
	}

	const FString ObjectName = FPackageName::GetShortName(BlueprintPath);
	if (UPackage* ExistingPackage = FindPackage(nullptr, *BlueprintPath))
	{
		if (UObject* ExistingAsset = FindObject<UObject>(ExistingPackage, *ObjectName))
		{
			UBlueprint* ExistingBlueprint = Cast<UBlueprint>(ExistingAsset);
			if (!ExistingBlueprint)
			{
				AddError(AssetResult, FString::Printf(
					TEXT("Existing asset is %s, not a Blueprint: %s"),
					*ExistingAsset->GetClass()->GetPathName(),
					*BlueprintPath));
				return nullptr;
			}
			AssetResult.bUpdated = true;
			AssetResult.Actions.Add(FString::Printf(TEXT("loaded existing blueprint %s"), *BlueprintPath));
			return ExistingBlueprint;
		}
	}

	FString ExistingPackageFilename;
	if (FPackageName::DoesPackageExist(BlueprintPath, &ExistingPackageFilename))
	{
		if (UObject* ExistingAsset = NTEBuildTool::Editor::LoadAnyAssetByPath(BlueprintPath))
		{
			UBlueprint* ExistingBlueprint = Cast<UBlueprint>(ExistingAsset);
			if (!ExistingBlueprint)
			{
				AddError(AssetResult, FString::Printf(
					TEXT("Existing asset is %s, not a Blueprint: %s"),
					*ExistingAsset->GetClass()->GetPathName(),
					*BlueprintPath));
				return nullptr;
			}
			AssetResult.bUpdated = true;
			AssetResult.Actions.Add(FString::Printf(TEXT("loaded existing blueprint %s"), *BlueprintPath));
			return ExistingBlueprint;
		}

		AddError(AssetResult, FString::Printf(
			TEXT("Package exists but Blueprint asset could not be loaded: %s (%s)"),
			*BlueprintPath,
			*ExistingPackageFilename));
		return nullptr;
	}

	UPackage* Package = CreatePackage(*BlueprintPath);
	if (!Package)
	{
		AddError(AssetResult, FString::Printf(TEXT("Could not create package: %s"), *BlueprintPath));
		return nullptr;
	}

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		ParentClass,
		Package,
		FName(*FPackageName::GetShortName(BlueprintPath)),
		BPTYPE_Normal,
		BlueprintClass,
		GeneratedClass,
		NAME_None);
	if (!Blueprint)
	{
		AddError(AssetResult, FString::Printf(TEXT("Could not create blueprint: %s"), *BlueprintPath));
		return nullptr;
	}

	Blueprint->SetFlags(RF_Public | RF_Standalone);
	FAssetRegistryModule::AssetCreated(Blueprint);
	Package->MarkPackageDirty();
	AssetResult.bCreated = true;
	AssetResult.bUpdated = true;
	AssetResult.Actions.Add(FString::Printf(TEXT("created blueprint %s"), *BlueprintPath));
	return Blueprint;
}

UBlueprint* CreateOrLoadSaveGameBlueprint(const FString& BlueprintPath, FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	return CreateOrLoadBlueprint(
		BlueprintPath,
		USaveGame::StaticClass(),
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		AssetResult);
}

UBlueprint* CreateOrLoadWidgetBlueprint(const FString& BlueprintPath, FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	return CreateOrLoadBlueprint(
		BlueprintPath,
		UUserWidget::StaticClass(),
		UWidgetBlueprint::StaticClass(),
		UWidgetBlueprintGeneratedClass::StaticClass(),
		AssetResult);
}

FString MakeRuntimeWidgetNameSuffix(const FString& RawValue)
{
	return SanitizeVariableSuffix(RawValue);
}

FName MakeRuntimeActionButtonWidgetName(const FNteCharacterRuntimeActionPlanItem& Action)
{
	return FName(*(TEXT("NTE_ActionButton_") + MakeRuntimeWidgetNameSuffix(Action.Id)));
}

FName MakeRuntimeActionLabelWidgetName(const FNteCharacterRuntimeActionPlanItem& Action)
{
	return FName(*(TEXT("NTE_ActionLabel_") + MakeRuntimeWidgetNameSuffix(Action.Id)));
}

UWidgetTree* EnsureWidgetTree(UWidgetBlueprint& WidgetBlueprint)
{
	if (!WidgetBlueprint.WidgetTree)
	{
		WidgetBlueprint.Modify();
		WidgetBlueprint.WidgetTree = NewObject<UWidgetTree>(&WidgetBlueprint, TEXT("WidgetTree"), RF_Transactional);
	}
	return WidgetBlueprint.WidgetTree;
}

void DetachWidgetFromParent(UWidget& Widget)
{
	if (UPanelWidget* Parent = Widget.GetParent())
	{
		Parent->RemoveChild(&Widget);
	}
}

template <typename WidgetType>
WidgetType* FindOrCreateRuntimeWidget(
	UWidgetBlueprint& WidgetBlueprint,
	UWidgetTree& WidgetTree,
	const FName WidgetName,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	if (UWidget* ExistingWidget = WidgetTree.FindWidget(WidgetName))
	{
		WidgetType* ExistingTypedWidget = Cast<WidgetType>(ExistingWidget);
		if (!ExistingTypedWidget)
		{
			AddError(AssetResult, FString::Printf(
				TEXT("Widget '%s' exists but is %s, not %s."),
				*WidgetName.ToString(),
				*ExistingWidget->GetClass()->GetName(),
				*WidgetType::StaticClass()->GetName()));
			return nullptr;
		}
		ExistingTypedWidget->bIsVariable = true;
		return ExistingTypedWidget;
	}

	WidgetType* NewWidget = WidgetTree.ConstructWidget<WidgetType>(WidgetType::StaticClass(), WidgetName);
	if (!NewWidget)
	{
		AddError(AssetResult, FString::Printf(TEXT("Could not create widget '%s'."), *WidgetName.ToString()));
		return nullptr;
	}

	NewWidget->bIsVariable = true;
	WidgetBlueprint.OnVariableRemoved(WidgetName);
	WidgetBlueprint.OnVariableAdded(WidgetName);
	AssetResult.Actions.Add(FString::Printf(
		TEXT("created runtime widget %s:%s"),
		*WidgetName.ToString(),
		*NewWidget->GetClass()->GetName()));
	return NewWidget;
}

bool RebuildRuntimeActionWidgetTree(
	UWidgetBlueprint& WidgetBlueprint,
	const FNteCharacterRuntimeActionPlan& Plan,
	const TArray<const FNteCharacterRuntimeActionPlanItem*>& Actions,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	if (!Plan.RuntimeUi.bEnableUi)
	{
		AssetResult.Actions.Add(TEXT("RuntimeUi disabled; widget layout generation skipped"));
		return false;
	}

	UWidgetTree* WidgetTree = EnsureWidgetTree(WidgetBlueprint);
	if (!WidgetTree)
	{
		AddError(AssetResult, TEXT("WidgetBlueprint has no WidgetTree and one could not be created."));
		return false;
	}

	UFont* RuntimeFont = LoadOrCreateSourceGameFontProxy(Plan.RuntimeUi.FontPath, AssetResult);
	if (!RuntimeFont)
	{
		return false;
	}

	WidgetBlueprint.Modify();
	WidgetTree->Modify();

	UCanvasPanel* Root = FindOrCreateRuntimeWidget<UCanvasPanel>(
		WidgetBlueprint,
		*WidgetTree,
		TEXT("NTE_CharacterActions_Root"),
		AssetResult);
	UVerticalBox* WindowPanel = FindOrCreateRuntimeWidget<UVerticalBox>(
		WidgetBlueprint,
		*WidgetTree,
		TEXT("NTE_CharacterActions_WindowPanel"),
		AssetResult);
	UTextBlock* TitleText = FindOrCreateRuntimeWidget<UTextBlock>(
		WidgetBlueprint,
		*WidgetTree,
		TEXT("NTE_CharacterActions_Title"),
		AssetResult);
	UVerticalBox* ButtonList = FindOrCreateRuntimeWidget<UVerticalBox>(
		WidgetBlueprint,
		*WidgetTree,
		TEXT("NTE_CharacterActions_ButtonList"),
		AssetResult);

	if (!Root || !WindowPanel || !TitleText || !ButtonList)
	{
		return false;
	}

	struct FRuntimeActionWidgetPair
	{
		const FNteCharacterRuntimeActionPlanItem* Action = nullptr;
		UButton* Button = nullptr;
		UTextBlock* Label = nullptr;
	};
	TArray<FRuntimeActionWidgetPair> ActionWidgets;
	for (const FNteCharacterRuntimeActionPlanItem* Action : Actions)
	{
		if (!Action)
		{
			continue;
		}

		FRuntimeActionWidgetPair& Pair = ActionWidgets.AddDefaulted_GetRef();
		Pair.Action = Action;
		Pair.Button = FindOrCreateRuntimeWidget<UButton>(
			WidgetBlueprint,
			*WidgetTree,
			MakeRuntimeActionButtonWidgetName(*Action),
			AssetResult);
		Pair.Label = FindOrCreateRuntimeWidget<UTextBlock>(
			WidgetBlueprint,
			*WidgetTree,
			MakeRuntimeActionLabelWidgetName(*Action),
			AssetResult);
		if (!Pair.Button || !Pair.Label)
		{
			return false;
		}
	}

	UTextBlock* EmptyText = nullptr;
	if (Actions.IsEmpty())
	{
		EmptyText = FindOrCreateRuntimeWidget<UTextBlock>(
			WidgetBlueprint,
			*WidgetTree,
			TEXT("NTE_CharacterActions_EmptyLabel"),
			AssetResult);
		if (!EmptyText)
		{
			return false;
		}
	}

	Root->ClearChildren();
	WindowPanel->ClearChildren();
	ButtonList->ClearChildren();
	WidgetTree->RootWidget = Root;

	DetachWidgetFromParent(*WindowPanel);
	UCanvasPanelSlot* WindowSlot = Root->AddChildToCanvas(WindowPanel);
	if (WindowSlot)
	{
		WindowSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		WindowSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		WindowSlot->SetPosition(FVector2D(80.0f, 80.0f));
		WindowSlot->SetSize(FVector2D(360.0f, 72.0f + FMath::Max(1, Actions.Num()) * 44.0f));
		WindowSlot->SetAutoSize(false);
	}

	const FString Title = Plan.RuntimeUi.Title.IsEmpty() ? TEXT("NTE Character Actions") : Plan.RuntimeUi.Title;
	TitleText->SetText(FText::FromString(Title));
	TitleText->SetFont(FSlateFontInfo(
		RuntimeFont,
		static_cast<float>(Plan.RuntimeUi.TitleFontSize),
		FName(*Plan.RuntimeUi.TitleFontTypeface)));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	DetachWidgetFromParent(*TitleText);
	if (UVerticalBoxSlot* TitleSlot = WindowPanel->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(12.0f, 8.0f, 12.0f, 6.0f));
		TitleSlot->SetHorizontalAlignment(HAlign_Fill);
		TitleSlot->SetVerticalAlignment(VAlign_Center);
	}

	DetachWidgetFromParent(*ButtonList);
	if (UVerticalBoxSlot* ButtonListSlot = WindowPanel->AddChildToVerticalBox(ButtonList))
	{
		ButtonListSlot->SetPadding(FMargin(8.0f, 0.0f, 8.0f, 8.0f));
		ButtonListSlot->SetHorizontalAlignment(HAlign_Fill);
		ButtonListSlot->SetVerticalAlignment(VAlign_Fill);
	}

	if (Actions.IsEmpty())
	{
		if (EmptyText)
		{
			EmptyText->SetText(FText::FromString(TEXT("No runtime actions configured")));
			EmptyText->SetFont(FSlateFontInfo(
				RuntimeFont,
				static_cast<float>(Plan.RuntimeUi.BodyFontSize),
				FName(*Plan.RuntimeUi.BodyFontTypeface)));
			EmptyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f)));
			DetachWidgetFromParent(*EmptyText);
			if (UVerticalBoxSlot* EmptySlot = ButtonList->AddChildToVerticalBox(EmptyText))
			{
				EmptySlot->SetPadding(FMargin(8.0f, 6.0f, 8.0f, 6.0f));
				EmptySlot->SetHorizontalAlignment(HAlign_Fill);
				EmptySlot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}

	for (const FRuntimeActionWidgetPair& Pair : ActionWidgets)
	{
		const FNteCharacterRuntimeActionPlanItem* Action = Pair.Action;
		UButton* Button = Pair.Button;
		UTextBlock* Label = Pair.Label;
		if (!Button || !Label)
		{
			return false;
		}

		const FString LabelText = Action->Label.IsEmpty() ? Action->Id : Action->Label;
		Label->SetText(FText::FromString(LabelText));
		Label->SetFont(FSlateFontInfo(
			RuntimeFont,
			static_cast<float>(Plan.RuntimeUi.BodyFontSize),
			FName(*Plan.RuntimeUi.BodyFontTypeface)));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Button->IsFocusable = false;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		Button->SetColorAndOpacity(FLinearColor::White);
		Button->SetBackgroundColor(FLinearColor(0.08f, 0.08f, 0.08f, 0.92f));

		DetachWidgetFromParent(*Label);
		Button->SetContent(Label);
		DetachWidgetFromParent(*Button);
		if (UVerticalBoxSlot* ButtonSlot = ButtonList->AddChildToVerticalBox(Button))
		{
			ButtonSlot->SetPadding(FMargin(4.0f, 4.0f, 4.0f, 4.0f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
			ButtonSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&WidgetBlueprint);
	if (UPackage* Package = WidgetBlueprint.GetPackage())
	{
		Package->MarkPackageDirty();
	}
	AssetResult.bUpdated = true;
	AssetResult.Actions.Add(FString::Printf(
		TEXT("rebuilt runtime widget layout with %d action button(s)"),
		Actions.Num()));
	return true;
}

bool ApplyAnimBlueprintSkeleton(
	UAnimBlueprint& AnimBlueprint,
	USkeletalMesh& HostMesh,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	USkeleton* TargetSkeleton = HostMesh.GetSkeleton();
	if (!TargetSkeleton)
	{
		AddError(AssetResult, FString::Printf(
			TEXT("Host SkeletalMesh has no Skeleton: %s"),
			*HostMesh.GetPathName()));
		return false;
	}

	bool bChanged = false;
	if (AnimBlueprint.TargetSkeleton != TargetSkeleton)
	{
		AnimBlueprint.Modify();
		AnimBlueprint.TargetSkeleton = TargetSkeleton;
		bChanged = true;
		AssetResult.Actions.Add(FString::Printf(
			TEXT("set AnimBlueprint TargetSkeleton to %s"),
			*TargetSkeleton->GetPathName()));
	}

	if (AnimBlueprint.GetPreviewMesh() != &HostMesh)
	{
		AnimBlueprint.Modify();
		AnimBlueprint.SetPreviewMesh(&HostMesh);
		bChanged = true;
		AssetResult.Actions.Add(FString::Printf(
			TEXT("set AnimBlueprint PreviewSkeletalMesh to %s"),
			*HostMesh.GetPathName()));
	}

	if (UAnimBlueprintGeneratedClass* GeneratedAnimClass = Cast<UAnimBlueprintGeneratedClass>(AnimBlueprint.GeneratedClass))
	{
		if (GeneratedAnimClass->TargetSkeleton != TargetSkeleton)
		{
			GeneratedAnimClass->Modify();
			GeneratedAnimClass->TargetSkeleton = TargetSkeleton;
			bChanged = true;
		}
	}
	if (UAnimBlueprintGeneratedClass* SkeletonGeneratedAnimClass = Cast<UAnimBlueprintGeneratedClass>(AnimBlueprint.SkeletonGeneratedClass))
	{
		if (SkeletonGeneratedAnimClass->TargetSkeleton != TargetSkeleton)
		{
			SkeletonGeneratedAnimClass->Modify();
			SkeletonGeneratedAnimClass->TargetSkeleton = TargetSkeleton;
			bChanged = true;
		}
	}

	if (bChanged)
	{
		AssetResult.bUpdated = true;
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&AnimBlueprint);
	}
	return true;
}

USkeletalMesh* LoadHostSkeletalMesh(const FNteCharacterRuntimeActionHostPlan& Host, FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	if (Host.MeshPath.IsEmpty())
	{
		AddError(AssetResult, FString::Printf(
			TEXT("Runtime action host '%s' has no MeshPath; cannot assign AnimBlueprint skeleton."),
			*Host.MeshId));
		return nullptr;
	}

	USkeletalMesh* HostMesh = NTEBuildTool::Editor::LoadAssetByPath<USkeletalMesh>(Host.MeshPath);
	if (!HostMesh)
	{
		AddError(AssetResult, FString::Printf(
			TEXT("Runtime action host '%s' mesh could not be loaded: %s"),
			*Host.MeshId,
			*Host.MeshPath));
		return nullptr;
	}

	if (!HostMesh->GetSkeleton())
	{
		AddError(AssetResult, FString::Printf(
			TEXT("Runtime action host '%s' mesh has no Skeleton: %s"),
			*Host.MeshId,
			*Host.MeshPath));
		return nullptr;
	}

	return HostMesh;
}

UBlueprint* CreateOrLoadAnimBlueprintForHost(
	const FNteCharacterRuntimeActionHostPlan& Host,
	USkeletalMesh& HostMesh,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	const FString& BlueprintPath = Host.AnimBlueprintPath;
	if (BlueprintPath.IsEmpty())
	{
		AddError(AssetResult, TEXT("Blueprint path is empty."));
		return nullptr;
	}
	if (!BlueprintPath.StartsWith(TEXT("/Game/")) || BlueprintPath.Contains(TEXT(".")))
	{
		AddError(AssetResult, FString::Printf(TEXT("Blueprint path must be a /Game package path: %s"), *BlueprintPath));
		return nullptr;
	}

	const FString ObjectName = FPackageName::GetShortName(BlueprintPath);
	auto LoadExistingAnimBlueprint = [&AssetResult, &BlueprintPath, &HostMesh](UObject* ExistingAsset) -> UBlueprint*
	{
		UAnimBlueprint* ExistingBlueprint = Cast<UAnimBlueprint>(ExistingAsset);
		if (!ExistingBlueprint)
		{
			AddError(AssetResult, FString::Printf(
				TEXT("Existing asset is %s, not an AnimBlueprint: %s"),
				ExistingAsset ? *ExistingAsset->GetClass()->GetPathName() : TEXT("<null>"),
				*BlueprintPath));
			return nullptr;
		}
		AssetResult.bUpdated = true;
		AssetResult.Actions.Add(FString::Printf(TEXT("loaded existing AnimBlueprint %s"), *BlueprintPath));
		ApplyAnimBlueprintSkeleton(*ExistingBlueprint, HostMesh, AssetResult);
		return ExistingBlueprint;
	};

	if (UPackage* ExistingPackage = FindPackage(nullptr, *BlueprintPath))
	{
		if (UObject* ExistingAsset = FindObject<UObject>(ExistingPackage, *ObjectName))
		{
			return LoadExistingAnimBlueprint(ExistingAsset);
		}
	}

	FString ExistingPackageFilename;
	if (FPackageName::DoesPackageExist(BlueprintPath, &ExistingPackageFilename))
	{
		if (UObject* ExistingAsset = NTEBuildTool::Editor::LoadAnyAssetByPath(BlueprintPath))
		{
			return LoadExistingAnimBlueprint(ExistingAsset);
		}

		AddError(AssetResult, FString::Printf(
			TEXT("Package exists but AnimBlueprint asset could not be loaded: %s (%s)"),
			*BlueprintPath,
			*ExistingPackageFilename));
		return nullptr;
	}

	UPackage* Package = CreatePackage(*BlueprintPath);
	if (!Package)
	{
		AddError(AssetResult, FString::Printf(TEXT("Could not create package: %s"), *BlueprintPath));
		return nullptr;
	}

	UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
	Factory->BlueprintType = BPTYPE_Normal;
	Factory->ParentClass = UAnimInstance::StaticClass();
	Factory->TargetSkeleton = HostMesh.GetSkeleton();
	Factory->PreviewSkeletalMesh = &HostMesh;
	Factory->bTemplate = false;

	UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(Factory->FactoryCreateNew(
		UAnimBlueprint::StaticClass(),
		Package,
		FName(*ObjectName),
		RF_Public | RF_Standalone,
		nullptr,
		GWarn));
	if (!Blueprint)
	{
		AddError(AssetResult, FString::Printf(TEXT("Could not create AnimBlueprint: %s"), *BlueprintPath));
		return nullptr;
	}

	Blueprint->SetFlags(RF_Public | RF_Standalone);
	ApplyAnimBlueprintSkeleton(*Blueprint, HostMesh, AssetResult);
	FAssetRegistryModule::AssetCreated(Blueprint);
	Package->MarkPackageDirty();
	AssetResult.bCreated = true;
	AssetResult.bUpdated = true;
	AssetResult.Actions.Add(FString::Printf(
		TEXT("created AnimBlueprint %s for mesh %s"),
		*BlueprintPath,
		*HostMesh.GetPathName()));
	return Blueprint;
}

UClass* LoadRuntimeSaveGameGeneratedClass(
	const FNteCharacterRuntimeActionPlan& Plan,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	if (Plan.SaveGameBlueprintPath.IsEmpty())
	{
		AddError(AssetResult, TEXT("Runtime action SaveGameBlueprintPath is empty."));
		return nullptr;
	}

	UBlueprint* SaveGameBlueprint = Cast<UBlueprint>(NTEBuildTool::Editor::LoadAnyAssetByPath(Plan.SaveGameBlueprintPath));
	if (!SaveGameBlueprint)
	{
		AddError(AssetResult, FString::Printf(
			TEXT("Runtime action SaveGame Blueprint could not be loaded: %s"),
			*Plan.SaveGameBlueprintPath));
		return nullptr;
	}

	if (!SaveGameBlueprint->GeneratedClass)
	{
		FKismetEditorUtilities::CompileBlueprint(SaveGameBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	}
	if (!SaveGameBlueprint->GeneratedClass || !SaveGameBlueprint->GeneratedClass->IsChildOf(USaveGame::StaticClass()))
	{
		AddError(AssetResult, FString::Printf(
			TEXT("Runtime action SaveGame Blueprint has no generated USaveGame class: %s"),
			*Plan.SaveGameBlueprintPath));
		return nullptr;
	}

	return SaveGameBlueprint->GeneratedClass;
}

void AddPlanVariablesToBlueprint(
	UBlueprint& Blueprint,
	const FNteCharacterRuntimeActionPlan& Plan,
	const TArray<const FNteCharacterRuntimeActionPlanItem*>& Actions,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	EnsureBlueprintVariable(Blueprint, TEXT("NTE_CharacterActionPlanJson"), MakeStringPinType(), PlanToCondensedJson(Plan), AssetResult);
	EnsureBlueprintVariable(Blueprint, TEXT("NTE_CharacterActionCount"), MakeIntPinType(), FString::FromInt(Actions.Num()), AssetResult);
	EnsureBlueprintVariable(Blueprint, RuntimeSaveSlotNameVariableName, MakeStringPinType(), Plan.SaveSlotName, AssetResult);
	EnsureBlueprintVariable(Blueprint, TEXT("NTE_RuntimeUi_EnableUi"), MakeBoolPinType(), Plan.RuntimeUi.bEnableUi ? TEXT("true") : TEXT("false"), AssetResult);
	EnsureBlueprintVariable(Blueprint, TEXT("NTE_RuntimeUi_ToggleHotkey"), MakeStringPinType(), Plan.RuntimeUi.ToggleUiHotkey, AssetResult);
	EnsureBlueprintVariable(Blueprint, TEXT("NTE_RuntimeUi_Title"), MakeStringPinType(), Plan.RuntimeUi.Title, AssetResult);
	EnsureBlueprintVariable(Blueprint, TEXT("NTE_RuntimeUi_DefaultVisible"), MakeBoolPinType(), Plan.RuntimeUi.bDefaultVisible ? TEXT("true") : TEXT("false"), AssetResult);
	EnsureBlueprintVariable(Blueprint, TEXT("NTE_RuntimeUi_CurrentVisible"), MakeBoolPinType(), Plan.RuntimeUi.bDefaultVisible ? TEXT("true") : TEXT("false"), AssetResult);
	for (const FNteCharacterRuntimeActionPlanItem* Action : Actions)
	{
		if (!Action)
		{
			continue;
		}

		const FString Prefix = MakeActionVariablePrefix(*Action);
		EnsureBlueprintVariable(Blueprint, FName(*(Prefix + TEXT("_Id"))), MakeStringPinType(), Action->Id, AssetResult);
		EnsureBlueprintVariable(Blueprint, FName(*(Prefix + TEXT("_Label"))), MakeStringPinType(), Action->Label, AssetResult);
		EnsureBlueprintVariable(Blueprint, FName(*(Prefix + TEXT("_Type"))), MakeStringPinType(), Action->ActionType, AssetResult);
		EnsureBlueprintVariable(Blueprint, FName(*(Prefix + TEXT("_TargetMeshId"))), MakeStringPinType(), Action->TargetMeshId, AssetResult);
		EnsureBlueprintVariable(Blueprint, FName(*(Prefix + TEXT("_MaterialSlots"))), MakeStringPinType(), JoinInts(Action->MaterialSlots), AssetResult);
		EnsureBlueprintVariable(Blueprint, FName(*(Prefix + TEXT("_Enabled"))), MakeBoolPinType(), Action->bDefaultEnabled ? TEXT("true") : TEXT("false"), AssetResult);
	}
}

void AddRuntimeStateObjectVariableToBlueprint(
	UBlueprint& Blueprint,
	const FNteCharacterRuntimeActionPlan& Plan,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	if (Plan.Actions.IsEmpty() && !Plan.RuntimeUi.bEnableUi)
	{
		return;
	}

	UClass* SaveGameClass = LoadRuntimeSaveGameGeneratedClass(Plan, AssetResult);
	if (!SaveGameClass)
	{
		return;
	}

	EnsureBlueprintVariable(Blueprint, RuntimeSaveObjectVariableName, MakeObjectPinType(SaveGameClass), FString(), AssetResult);
}

void AddRuntimeHostVariablesToBlueprint(
	UBlueprint& Blueprint,
	const FNteCharacterRuntimeActionPlan& Plan,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	AddRuntimeStateObjectVariableToBlueprint(Blueprint, Plan, AssetResult);
	if (!Plan.RuntimeUi.bEnableUi)
	{
		return;
	}

	EnsureBlueprintVariable(Blueprint, RuntimeUiWidgetVariableName, MakeObjectPinType(UUserWidget::StaticClass()), FString(), AssetResult);
}

UEdGraphPin* AddPlayerControllerKeyQuery(
	UEdGraph& Graph,
	UFunction* PlayerControllerFunction,
	const FKey& Key,
	const int32 NodePosX,
	const int32 NodePosY)
{
	UFunction* GetPlayerControllerFunction = UGameplayStatics::StaticClass()->FindFunctionByName(TEXT("GetPlayerController"));
	if (!GetPlayerControllerFunction || !PlayerControllerFunction || !Key.IsValid())
	{
		return nullptr;
	}

	UK2Node_Self* SelfNode = AddRuntimeK2Node<UK2Node_Self>(Graph, NodePosX, NodePosY + 120);
	UK2Node_CallFunction* GetPlayerControllerNode = AddRuntimeFunctionCallNode(Graph, GetPlayerControllerFunction, NodePosX + 220, NodePosY + 80);
	UK2Node_CallFunction* KeyQueryNode = AddRuntimeFunctionCallNode(Graph, PlayerControllerFunction, NodePosX + 520, NodePosY);
	if (!SelfNode || !GetPlayerControllerNode || !KeyQueryNode)
	{
		return nullptr;
	}

	if (UEdGraphPin* PlayerIndexPin = FindPinByName(*GetPlayerControllerNode, TEXT("PlayerIndex")))
	{
		SetPinDefaultValue(*GetPlayerControllerNode, *PlayerIndexPin, TEXT("0"));
	}
	TryLinkPins(FindPinByName(*SelfNode, TEXT("self")), FindPinByName(*GetPlayerControllerNode, TEXT("WorldContextObject")));
	TryLinkPins(FindPinByName(*GetPlayerControllerNode, TEXT("ReturnValue")), FindPinByName(*KeyQueryNode, TEXT("self")));
	if (UEdGraphPin* KeyPin = FindPinByName(*KeyQueryNode, TEXT("Key")))
	{
		SetPinDefaultValue(*KeyQueryNode, *KeyPin, Key.GetFName().ToString());
	}
	return FindPinByName(*KeyQueryNode, TEXT("ReturnValue"));
}

UEdGraphPin* AddBoolBinaryNode(
	UEdGraph& Graph,
	UFunction* BoolFunction,
	UEdGraphPin* A,
	UEdGraphPin* B,
	const int32 NodePosX,
	const int32 NodePosY)
{
	if (!BoolFunction || !A || !B)
	{
		return nullptr;
	}

	UK2Node_CallFunction* Node = AddRuntimeFunctionCallNode(Graph, BoolFunction, NodePosX, NodePosY);
	if (!Node)
	{
		return nullptr;
	}
	TryLinkPins(A, FindPinByName(*Node, TEXT("A")));
	TryLinkPins(B, FindPinByName(*Node, TEXT("B")));
	return FindPinByName(*Node, TEXT("ReturnValue"));
}

UEdGraphPin* AddAnyModifierDownCondition(
	UEdGraph& Graph,
	UFunction* IsInputKeyDownFunction,
	UFunction* BoolOrFunction,
	const FKey& LeftKey,
	const FKey& RightKey,
	const int32 NodePosX,
	const int32 NodePosY)
{
	UEdGraphPin* LeftDown = AddPlayerControllerKeyQuery(Graph, IsInputKeyDownFunction, LeftKey, NodePosX, NodePosY);
	UEdGraphPin* RightDown = AddPlayerControllerKeyQuery(Graph, IsInputKeyDownFunction, RightKey, NodePosX, NodePosY + 260);
	return AddBoolBinaryNode(Graph, BoolOrFunction, LeftDown, RightDown, NodePosX + 1060, NodePosY + 120);
}

UEdGraphPin* AddHotkeyCondition(UEdGraph& Graph, const FRuntimeHotkey& Hotkey, const int32 NodePosX, const int32 NodePosY)
{
	UFunction* WasInputKeyJustPressedFunction = APlayerController::StaticClass()->FindFunctionByName(TEXT("WasInputKeyJustPressed"));
	UFunction* IsInputKeyDownFunction = APlayerController::StaticClass()->FindFunctionByName(TEXT("IsInputKeyDown"));
	UFunction* BoolAndFunction = UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("BooleanAND"));
	UFunction* BoolOrFunction = UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("BooleanOR"));
	if (!WasInputKeyJustPressedFunction || !IsInputKeyDownFunction || !BoolAndFunction || !BoolOrFunction)
	{
		return nullptr;
	}

	UEdGraphPin* Condition = AddPlayerControllerKeyQuery(Graph, WasInputKeyJustPressedFunction, Hotkey.Key, NodePosX, NodePosY);
	int32 ModifierRow = 0;
	auto AppendModifierCondition = [&](const bool bEnabled, const FKey& LeftKey, const FKey& RightKey)
	{
		if (!bEnabled || !Condition)
		{
			return;
		}

		UEdGraphPin* ModifierCondition = AddAnyModifierDownCondition(
			Graph,
			IsInputKeyDownFunction,
			BoolOrFunction,
			LeftKey,
			RightKey,
			NodePosX,
			NodePosY + 420 + ModifierRow * 620);
		Condition = AddBoolBinaryNode(Graph, BoolAndFunction, Condition, ModifierCondition, NodePosX + 1380, NodePosY + 260 + ModifierRow * 260);
		++ModifierRow;
	};

	AppendModifierCondition(Hotkey.bCtrl, FKey(TEXT("LeftControl")), FKey(TEXT("RightControl")));
	AppendModifierCondition(Hotkey.bAlt, FKey(TEXT("LeftAlt")), FKey(TEXT("RightAlt")));
	AppendModifierCondition(Hotkey.bShift, FKey(TEXT("LeftShift")), FKey(TEXT("RightShift")));
	AppendModifierCondition(Hotkey.bCmd, FKey(TEXT("LeftCommand")), FKey(TEXT("RightCommand")));
	return Condition;
}

UEdGraphPin* AddCurrentEnabledValue(
	UEdGraph& Graph,
	const FNteCharacterRuntimeActionPlanItem& Action,
	const int32 NodePosX,
	const int32 NodePosY)
{
	const FName EnabledVariableName = MakeActionEnabledVariableName(Action);
	UK2Node_VariableGet* GetEnabledNode = AddRuntimeVariableGetNode(Graph, EnabledVariableName, NodePosX, NodePosY);
	return GetEnabledNode ? FindPinByName(*GetEnabledNode, EnabledVariableName) : nullptr;
}

UK2Node_VariableSet* AddSetEnabledNode(
	UEdGraph& Graph,
	const FNteCharacterRuntimeActionPlanItem& Action,
	UEdGraphPin* NewEnabledValuePin,
	const int32 NodePosX,
	const int32 NodePosY)
{
	const FName EnabledVariableName = MakeActionEnabledVariableName(Action);
	UK2Node_VariableSet* SetEnabledNode = AddRuntimeVariableSetNode(Graph, EnabledVariableName, NodePosX, NodePosY);
	if (!SetEnabledNode)
	{
		return nullptr;
	}

	TryLinkPins(NewEnabledValuePin, FindPinByName(*SetEnabledNode, EnabledVariableName));
	return SetEnabledNode;
}

UEdGraphPin* AddRuntimeSaveObjectValuePin(UEdGraph& Graph, const int32 NodePosX, const int32 NodePosY)
{
	UK2Node_VariableGet* GetSaveObjectNode = AddRuntimeVariableGetNode(Graph, RuntimeSaveObjectVariableName, NodePosX, NodePosY);
	return GetSaveObjectNode ? FindPinByName(*GetSaveObjectNode, RuntimeSaveObjectVariableName) : nullptr;
}

UEdGraphPin* AddRuntimeSaveGameVariableValuePin(
	UEdGraph& Graph,
	UClass* SaveGameClass,
	const FName VariableName,
	const int32 NodePosX,
	const int32 NodePosY)
{
	UEdGraphPin* SaveObjectPin = AddRuntimeSaveObjectValuePin(Graph, NodePosX, NodePosY + 160);
	UK2Node_VariableGet* GetValueNode = AddRuntimeExternalVariableGetNode(Graph, VariableName, SaveGameClass, NodePosX + 260, NodePosY);
	if (!SaveObjectPin || !GetValueNode)
	{
		return nullptr;
	}

	TryLinkPins(SaveObjectPin, FindPinByName(*GetValueNode, TEXT("self")));
	return FindPinByName(*GetValueNode, VariableName);
}

UK2Node_VariableSet* AddRuntimeSaveGameVariableSetNode(
	UEdGraph& Graph,
	UClass* SaveGameClass,
	const FName VariableName,
	UEdGraphPin* NewValuePin,
	const int32 NodePosX,
	const int32 NodePosY)
{
	UEdGraphPin* SaveObjectPin = AddRuntimeSaveObjectValuePin(Graph, NodePosX, NodePosY + 180);
	UK2Node_VariableSet* SetValueNode = AddRuntimeExternalVariableSetNode(Graph, VariableName, SaveGameClass, NodePosX + 260, NodePosY);
	if (!SaveObjectPin || !SetValueNode)
	{
		return nullptr;
	}

	TryLinkPins(SaveObjectPin, FindPinByName(*SetValueNode, TEXT("self")));
	TryLinkPins(NewValuePin, FindPinByName(*SetValueNode, VariableName));
	return SetValueNode;
}

UEdGraphPin* AddToggledSaveGameBoolValue(
	UEdGraph& Graph,
	UClass* SaveGameClass,
	const FName VariableName,
	const int32 NodePosX,
	const int32 NodePosY)
{
	UFunction* BoolNotFunction = UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Not_PreBool"));
	if (!BoolNotFunction)
	{
		return nullptr;
	}

	UEdGraphPin* CurrentValuePin = AddRuntimeSaveGameVariableValuePin(Graph, SaveGameClass, VariableName, NodePosX, NodePosY);
	UK2Node_CallFunction* NotNode = AddRuntimeFunctionCallNode(Graph, BoolNotFunction, NodePosX + 620, NodePosY);
	if (!CurrentValuePin || !NotNode)
	{
		return nullptr;
	}

	TryLinkPins(CurrentValuePin, FindPinByName(*NotNode, TEXT("A")));
	return FindPinByName(*NotNode, TEXT("ReturnValue"));
}

UEdGraphPin* AddBoolNotEqualValue(
	UEdGraph& Graph,
	UEdGraphPin* A,
	UEdGraphPin* B,
	const int32 NodePosX,
	const int32 NodePosY)
{
	UFunction* BoolNotEqualFunction = UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("NotEqual_BoolBool"));
	return AddBoolBinaryNode(Graph, BoolNotEqualFunction, A, B, NodePosX, NodePosY);
}

UEdGraphPin* AddRuntimeSaveGameToSlotNodes(
	UEdGraph& Graph,
	const FNteCharacterRuntimeActionPlan& Plan,
	UEdGraphPin* ExecIn,
	UEdGraphPin* SaveObjectPin,
	const int32 NodePosX,
	const int32 NodePosY)
{
	if (!ExecIn || !SaveObjectPin)
	{
		return ExecIn;
	}

	UFunction* SaveGameToSlotFunction = UGameplayStatics::StaticClass()->FindFunctionByName(TEXT("SaveGameToSlot"));
	UK2Node_CallFunction* SaveNode = AddRuntimeFunctionCallNode(Graph, SaveGameToSlotFunction, NodePosX, NodePosY);
	if (!SaveGameToSlotFunction || !SaveNode)
	{
		return ExecIn;
	}

	TryLinkPins(ExecIn, FindExecPin(*SaveNode));
	TryLinkPins(SaveObjectPin, FindPinByName(*SaveNode, TEXT("SaveGameObject")));
	if (UEdGraphPin* SlotNamePin = FindPinByName(*SaveNode, TEXT("SlotName")))
	{
		SetPinDefaultValue(*SaveNode, *SlotNamePin, Plan.SaveSlotName);
	}
	if (UEdGraphPin* UserIndexPin = FindPinByName(*SaveNode, TEXT("UserIndex")))
	{
		SetPinDefaultValue(*SaveNode, *UserIndexPin, TEXT("0"));
	}
	return FindThenPin(*SaveNode);
}

UEdGraphPin* AddRuntimeSaveGameToSlotNodes(
	UEdGraph& Graph,
	const FNteCharacterRuntimeActionPlan& Plan,
	UEdGraphPin* ExecIn,
	const int32 NodePosX,
	const int32 NodePosY)
{
	UEdGraphPin* SaveObjectPin = AddRuntimeSaveObjectValuePin(Graph, NodePosX, NodePosY + 180);
	return AddRuntimeSaveGameToSlotNodes(Graph, Plan, ExecIn, SaveObjectPin, NodePosX + 260, NodePosY);
}

FObjectProperty* FindWidgetObjectProperty(UWidgetBlueprint& WidgetBlueprint, const FName WidgetName)
{
	if (WidgetBlueprint.SkeletonGeneratedClass)
	{
		if (FObjectProperty* Property = FindFProperty<FObjectProperty>(WidgetBlueprint.SkeletonGeneratedClass, WidgetName))
		{
			return Property;
		}
	}
	if (WidgetBlueprint.GeneratedClass)
	{
		if (FObjectProperty* Property = FindFProperty<FObjectProperty>(WidgetBlueprint.GeneratedClass, WidgetName))
		{
			return Property;
		}
	}
	return nullptr;
}

UK2Node_ComponentBoundEvent* AddRuntimeWidgetButtonClickedEvent(
	UWidgetBlueprint& WidgetBlueprint,
	UEdGraph& Graph,
	const FName ButtonWidgetName,
	const int32 NodePosX,
	const int32 NodePosY,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	FObjectProperty* ButtonProperty = FindWidgetObjectProperty(WidgetBlueprint, ButtonWidgetName);
	if (!ButtonProperty)
	{
		AddError(AssetResult, FString::Printf(
			TEXT("Could not find generated Widget variable property for Button '%s'."),
			*ButtonWidgetName.ToString()));
		return nullptr;
	}

	const FMulticastDelegateProperty* OnClickedProperty = FindFProperty<FMulticastDelegateProperty>(UButton::StaticClass(), TEXT("OnClicked"));
	if (!OnClickedProperty)
	{
		AddError(AssetResult, TEXT("Could not find UButton.OnClicked delegate property."));
		return nullptr;
	}

	if (FKismetEditorUtilities::FindBoundEventForComponent(&WidgetBlueprint, TEXT("OnClicked"), ButtonWidgetName))
	{
		AddError(AssetResult, FString::Printf(
			TEXT("Button '%s' already has an OnClicked bound event that was not generated by NTE; refusing to overwrite it."),
			*ButtonWidgetName.ToString()));
		return nullptr;
	}

	UK2Node_ComponentBoundEvent* EventNode = NewObject<UK2Node_ComponentBoundEvent>(&Graph);
	if (!EventNode)
	{
		AddError(AssetResult, FString::Printf(
			TEXT("Could not create OnClicked bound event for Button '%s'."),
			*ButtonWidgetName.ToString()));
		return nullptr;
	}

	EventNode->CreateNewGuid();
	Graph.AddNode(EventNode, false, false);
	EventNode->NodePosX = NodePosX;
	EventNode->NodePosY = NodePosY;
	EventNode->NodeComment = GeneratedRuntimeNodeComment;
	EventNode->InitializeComponentBoundEventParams(ButtonProperty, OnClickedProperty);
	EventNode->AllocateDefaultPins();
	return EventNode;
}

void AddRuntimeWidgetClickGraphToWidgetBlueprint(
	UWidgetBlueprint& WidgetBlueprint,
	const FNteCharacterRuntimeActionPlan& Plan,
	const TArray<const FNteCharacterRuntimeActionPlanItem*>& Actions,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	if (!Plan.RuntimeUi.bEnableUi)
	{
		return;
	}
	if (Actions.IsEmpty())
	{
		AssetResult.Actions.Add(TEXT("RuntimeUi has no action buttons; widget click graph generation skipped"));
		return;
	}

	RemoveGeneratedRuntimeNodes(WidgetBlueprint);
	FKismetEditorUtilities::CompileBlueprint(&WidgetBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);

	UEdGraph* EventGraph = EnsureEventGraph(WidgetBlueprint);
	if (!EventGraph)
	{
		AddError(AssetResult, TEXT("WidgetBlueprint has no EventGraph and one could not be created."));
		return;
	}

	UClass* SaveGameClass = LoadRuntimeSaveGameGeneratedClass(Plan, AssetResult);
	if (!SaveGameClass)
	{
		return;
	}

	int32 GeneratedBindingCount = 0;
	for (int32 ActionIndex = 0; ActionIndex < Actions.Num(); ++ActionIndex)
	{
		const FNteCharacterRuntimeActionPlanItem* Action = Actions[ActionIndex];
		if (!Action)
		{
			continue;
		}

		const int32 BaseY = ActionIndex * 420;
		const FName EnabledVariableName = MakeActionEnabledVariableName(*Action);
		const FName ButtonWidgetName = MakeRuntimeActionButtonWidgetName(*Action);
		UK2Node_ComponentBoundEvent* ClickedEvent = AddRuntimeWidgetButtonClickedEvent(
			WidgetBlueprint,
			*EventGraph,
			ButtonWidgetName,
			0,
			BaseY,
			AssetResult);
		if (!ClickedEvent)
		{
			continue;
		}

		UEdGraphPin* NewEnabledValuePin = AddToggledSaveGameBoolValue(*EventGraph, SaveGameClass, EnabledVariableName, 360, BaseY + 140);
		UK2Node_VariableSet* SetEnabledNode = AddRuntimeSaveGameVariableSetNode(*EventGraph, SaveGameClass, EnabledVariableName, NewEnabledValuePin, 1180, BaseY);
		if (SetEnabledNode)
		{
			TryLinkPins(FindThenPin(*ClickedEvent), FindExecPin(*SetEnabledNode));
			AddRuntimeSaveGameToSlotNodes(*EventGraph, Plan, FindThenPin(*SetEnabledNode), 1700, BaseY);
		}

		++GeneratedBindingCount;
		AssetResult.Actions.Add(FString::Printf(
			TEXT("generated widget SaveGame click state graph for action %s"),
			*Action->Id));
	}

	if (GeneratedBindingCount > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&WidgetBlueprint);
		if (UPackage* Package = WidgetBlueprint.GetPackage())
		{
			Package->MarkPackageDirty();
		}
		AssetResult.bUpdated = true;
		AssetResult.Actions.Add(FString::Printf(
			TEXT("generated widget click bindings for %d action button(s)"),
			GeneratedBindingCount));
	}
}

FString SlateVisibilityPinDefault(const ESlateVisibility Visibility)
{
	if (const UEnum* VisibilityEnum = StaticEnum<ESlateVisibility>())
	{
		return VisibilityEnum->GetNameStringByValue(static_cast<int64>(Visibility));
	}
	return Visibility == ESlateVisibility::Visible ? TEXT("Visible") : TEXT("Collapsed");
}

UClass* LoadRuntimeWidgetGeneratedClass(
	const FNteCharacterRuntimeActionPlan& Plan,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	if (!Plan.RuntimeUi.bEnableUi)
	{
		return nullptr;
	}
	if (Plan.WidgetBlueprintPath.IsEmpty())
	{
		AddError(AssetResult, TEXT("Runtime UI is enabled but WidgetBlueprintPath is empty."));
		return nullptr;
	}

	UBlueprint* WidgetBlueprint = Cast<UBlueprint>(NTEBuildTool::Editor::LoadAnyAssetByPath(Plan.WidgetBlueprintPath));
	if (!WidgetBlueprint)
	{
		AddError(AssetResult, FString::Printf(
			TEXT("Runtime UI WidgetBlueprint could not be loaded: %s"),
			*Plan.WidgetBlueprintPath));
		return nullptr;
	}

	if (!WidgetBlueprint->GeneratedClass)
	{
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	}
	if (!WidgetBlueprint->GeneratedClass || !WidgetBlueprint->GeneratedClass->IsChildOf(UUserWidget::StaticClass()))
	{
		AddError(AssetResult, FString::Printf(
			TEXT("Runtime UI WidgetBlueprint has no generated UUserWidget class: %s"),
			*Plan.WidgetBlueprintPath));
		return nullptr;
	}

	return WidgetBlueprint->GeneratedClass;
}

void AddRuntimeSaveGameLoadOrCreateNodes(
	UEdGraph& Graph,
	const FNteCharacterRuntimeActionPlan& Plan,
	UEdGraphPin* ExecIn,
	const int32 NodePosX,
	const int32 NodePosY,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	if (!ExecIn)
	{
		return;
	}

	UClass* SaveGameClass = LoadRuntimeSaveGameGeneratedClass(Plan, AssetResult);
	UFunction* DoesSaveGameExistFunction = UGameplayStatics::StaticClass()->FindFunctionByName(TEXT("DoesSaveGameExist"));
	UFunction* LoadGameFromSlotFunction = UGameplayStatics::StaticClass()->FindFunctionByName(TEXT("LoadGameFromSlot"));
	UFunction* CreateSaveGameObjectFunction = UGameplayStatics::StaticClass()->FindFunctionByName(TEXT("CreateSaveGameObject"));
	if (!SaveGameClass || !DoesSaveGameExistFunction || !LoadGameFromSlotFunction || !CreateSaveGameObjectFunction)
	{
		AddError(AssetResult, TEXT("Runtime SaveGame load/create graph could not resolve required SaveGame functions."));
		return;
	}

	UK2Node_CallFunction* ExistsNode = AddRuntimeFunctionCallNode(Graph, DoesSaveGameExistFunction, NodePosX, NodePosY);
	UK2Node_IfThenElse* BranchNode = AddRuntimeK2Node<UK2Node_IfThenElse>(Graph, NodePosX + 340, NodePosY);
	UK2Node_CallFunction* LoadNode = AddRuntimeFunctionCallNode(Graph, LoadGameFromSlotFunction, NodePosX + 680, NodePosY - 220);
	UK2Node_DynamicCast* LoadCastNode = AddRuntimeDynamicCastNode(Graph, SaveGameClass, NodePosX + 1020, NodePosY - 220);
	UK2Node_VariableSet* SetLoadedSaveNode = AddRuntimeVariableSetNode(Graph, RuntimeSaveObjectVariableName, NodePosX + 1360, NodePosY - 220);
	UK2Node_CallFunction* CreateNode = AddRuntimeFunctionCallNode(Graph, CreateSaveGameObjectFunction, NodePosX + 680, NodePosY + 260);
	UK2Node_DynamicCast* CreateCastNode = AddRuntimeDynamicCastNode(Graph, SaveGameClass, NodePosX + 1020, NodePosY + 260);
	UK2Node_VariableSet* SetCreatedSaveNode = AddRuntimeVariableSetNode(Graph, RuntimeSaveObjectVariableName, NodePosX + 1360, NodePosY + 260);
	if (!ExistsNode || !BranchNode || !LoadNode || !LoadCastNode || !SetLoadedSaveNode || !CreateNode || !CreateCastNode || !SetCreatedSaveNode)
	{
		AddError(AssetResult, TEXT("Runtime SaveGame load/create graph could not create required nodes."));
		return;
	}

	TryLinkPins(ExecIn, FindExecPin(*ExistsNode));
	TryLinkPins(FindThenPin(*ExistsNode), FindExecPin(*BranchNode));
	TryLinkPins(FindPinByName(*ExistsNode, TEXT("ReturnValue")), FindPinByName(*BranchNode, TEXT("Condition")));

	auto SetSlotPins = [&Plan](UEdGraphNode& Node)
	{
		if (UEdGraphPin* SlotNamePin = FindPinByName(Node, TEXT("SlotName")))
		{
			SetPinDefaultValue(Node, *SlotNamePin, Plan.SaveSlotName);
		}
		if (UEdGraphPin* UserIndexPin = FindPinByName(Node, TEXT("UserIndex")))
		{
			SetPinDefaultValue(Node, *UserIndexPin, TEXT("0"));
		}
	};
	SetSlotPins(*ExistsNode);
	SetSlotPins(*LoadNode);

	TryLinkPins(FindThenPin(*BranchNode), FindExecPin(*LoadNode));
	TryLinkPins(FindThenPin(*LoadNode), FindExecPin(*LoadCastNode));
	TryLinkPins(FindPinByName(*LoadNode, TEXT("ReturnValue")), FindPinByName(*LoadCastNode, UEdGraphSchema_K2::PN_ObjectToCast));
	TryLinkPins(LoadCastNode->GetValidCastPin(), FindExecPin(*SetLoadedSaveNode));
	TryLinkPins(LoadCastNode->GetCastResultPin(), FindPinByName(*SetLoadedSaveNode, RuntimeSaveObjectVariableName));

	TryLinkPins(FindPinByName(*BranchNode, TEXT("Else")), FindExecPin(*CreateNode));
	if (UEdGraphPin* SaveGameClassPin = FindPinByName(*CreateNode, TEXT("SaveGameClass")))
	{
		SetPinDefaultObject(*CreateNode, *SaveGameClassPin, SaveGameClass);
	}
	TryLinkPins(FindThenPin(*CreateNode), FindExecPin(*CreateCastNode));
	TryLinkPins(FindPinByName(*CreateNode, TEXT("ReturnValue")), FindPinByName(*CreateCastNode, UEdGraphSchema_K2::PN_ObjectToCast));
	TryLinkPins(CreateCastNode->GetValidCastPin(), FindExecPin(*SetCreatedSaveNode));
	TryLinkPins(CreateCastNode->GetCastResultPin(), FindPinByName(*SetCreatedSaveNode, RuntimeSaveObjectVariableName));
	AddRuntimeSaveGameToSlotNodes(Graph, Plan, FindThenPin(*SetCreatedSaveNode), CreateCastNode->GetCastResultPin(), NodePosX + 1700, NodePosY + 260);

	AssetResult.Actions.Add(FString::Printf(
		TEXT("generated runtime SaveGame load/create graph for slot %s"),
		*Plan.SaveSlotName));
}

UK2Node_VariableSet* AddSetRuntimeUiCurrentVisibleNode(
	UEdGraph& Graph,
	UEdGraphPin* NewVisibleValuePin,
	const int32 NodePosX,
	const int32 NodePosY)
{
	UK2Node_VariableSet* SetVisibleNode = AddRuntimeVariableSetNode(Graph, TEXT("NTE_RuntimeUi_CurrentVisible"), NodePosX, NodePosY);
	if (!SetVisibleNode)
	{
		return nullptr;
	}

	TryLinkPins(NewVisibleValuePin, FindPinByName(*SetVisibleNode, TEXT("NTE_RuntimeUi_CurrentVisible")));
	return SetVisibleNode;
}

UEdGraphPin* AddRuntimeUiSetVisibilityLiteralNode(
	UEdGraph& Graph,
	UEdGraphPin* ExecIn,
	UEdGraphPin* WidgetPin,
	const ESlateVisibility Visibility,
	const int32 NodePosX,
	const int32 NodePosY)
{
	if (!ExecIn || !WidgetPin)
	{
		return ExecIn;
	}

	UFunction* SetVisibilityFunction = UWidget::StaticClass()->FindFunctionByName(TEXT("SetVisibility"));
	UK2Node_CallFunction* SetVisibilityNode = AddRuntimeFunctionCallNode(Graph, SetVisibilityFunction, NodePosX, NodePosY);
	if (!SetVisibilityFunction || !SetVisibilityNode)
	{
		return ExecIn;
	}

	TryLinkPins(ExecIn, FindExecPin(*SetVisibilityNode));
	TryLinkPins(WidgetPin, FindPinByName(*SetVisibilityNode, TEXT("self")));
	if (UEdGraphPin* VisibilityPin = FindPinByName(*SetVisibilityNode, TEXT("InVisibility")))
	{
		SetPinDefaultValue(*SetVisibilityNode, *VisibilityPin, SlateVisibilityPinDefault(Visibility));
	}
	return FindThenPin(*SetVisibilityNode);
}

void AddRuntimeUiSetVisibilityFromBoolNodes(
	UEdGraph& Graph,
	UEdGraphPin* ExecIn,
	UEdGraphPin* WidgetPin,
	UEdGraphPin* VisibleValuePin,
	const int32 NodePosX,
	const int32 NodePosY)
{
	if (!ExecIn || !WidgetPin || !VisibleValuePin)
	{
		return;
	}

	UK2Node_IfThenElse* BranchNode = AddRuntimeK2Node<UK2Node_IfThenElse>(Graph, NodePosX, NodePosY);
	if (!BranchNode)
	{
		return;
	}

	TryLinkPins(ExecIn, FindExecPin(*BranchNode));
	TryLinkPins(VisibleValuePin, FindPinByName(*BranchNode, TEXT("Condition")));
	AddRuntimeUiSetVisibilityLiteralNode(Graph, FindThenPin(*BranchNode), WidgetPin, ESlateVisibility::Visible, NodePosX + 320, NodePosY - 120);
	AddRuntimeUiSetVisibilityLiteralNode(Graph, FindPinByName(*BranchNode, TEXT("Else")), WidgetPin, ESlateVisibility::Collapsed, NodePosX + 320, NodePosY + 140);
}

void AddRuntimeUiInitializeNodes(
	UEdGraph& Graph,
	const FNteCharacterRuntimeActionPlan& Plan,
	UEdGraphPin* ExecIn,
	const int32 NodePosX,
	const int32 NodePosY,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	if (!Plan.RuntimeUi.bEnableUi || !ExecIn)
	{
		return;
	}

	UClass* WidgetClass = LoadRuntimeWidgetGeneratedClass(Plan, AssetResult);
	UClass* SaveGameClass = LoadRuntimeSaveGameGeneratedClass(Plan, AssetResult);
	UFunction* GetPlayerControllerFunction = UGameplayStatics::StaticClass()->FindFunctionByName(TEXT("GetPlayerController"));
	UFunction* CreateWidgetFunction = UWidgetBlueprintLibrary::StaticClass()->FindFunctionByName(TEXT("Create"));
	UFunction* AddToViewportFunction = UUserWidget::StaticClass()->FindFunctionByName(TEXT("AddToViewport"));
	if (!WidgetClass || !SaveGameClass || !GetPlayerControllerFunction || !CreateWidgetFunction || !AddToViewportFunction)
	{
		AddError(AssetResult, TEXT("Runtime UI initialize graph could not resolve required UMG functions."));
		return;
	}

	UEdGraphPin* SaveVisibleValuePin = AddRuntimeSaveGameVariableValuePin(Graph, SaveGameClass, TEXT("NTE_RuntimeUi_CurrentVisible"), NodePosX, NodePosY + 180);
	UK2Node_VariableSet* SetCurrentVisibleNode = AddRuntimeVariableSetNode(Graph, TEXT("NTE_RuntimeUi_CurrentVisible"), NodePosX + 620, NodePosY);
	UK2Node_Self* SelfNode = AddRuntimeK2Node<UK2Node_Self>(Graph, NodePosX + 620, NodePosY + 260);
	UK2Node_CallFunction* GetPlayerControllerNode = AddRuntimeFunctionCallNode(Graph, GetPlayerControllerFunction, NodePosX + 900, NodePosY + 180);
	UK2Node_CallFunction* CreateWidgetNode = AddRuntimeFunctionCallNode(Graph, CreateWidgetFunction, NodePosX + 1240, NodePosY);
	UK2Node_DynamicCast* CastWidgetNode = AddRuntimeDynamicCastNode(Graph, WidgetClass, NodePosX + 1580, NodePosY);
	UK2Node_VariableSet* SetWidgetNode = AddRuntimeVariableSetNode(Graph, RuntimeUiWidgetVariableName, NodePosX + 1920, NodePosY);
	UK2Node_VariableSet* SetWidgetSaveObjectNode = AddRuntimeExternalVariableSetNode(Graph, RuntimeSaveObjectVariableName, WidgetClass, NodePosX + 2260, NodePosY);
	UK2Node_CallFunction* AddToViewportNode = AddRuntimeFunctionCallNode(Graph, AddToViewportFunction, NodePosX + 2600, NodePosY);
	UEdGraphPin* RuntimeSaveObjectPin = AddRuntimeSaveObjectValuePin(Graph, NodePosX + 2260, NodePosY + 220);
	if (!SaveVisibleValuePin || !SetCurrentVisibleNode || !SelfNode || !GetPlayerControllerNode || !CreateWidgetNode || !CastWidgetNode || !SetWidgetNode || !SetWidgetSaveObjectNode || !AddToViewportNode || !RuntimeSaveObjectPin)
	{
		AddError(AssetResult, TEXT("Runtime UI initialize graph could not create required nodes."));
		return;
	}

	TryLinkPins(ExecIn, FindExecPin(*SetCurrentVisibleNode));
	TryLinkPins(SaveVisibleValuePin, FindPinByName(*SetCurrentVisibleNode, TEXT("NTE_RuntimeUi_CurrentVisible")));
	TryLinkPins(FindThenPin(*SetCurrentVisibleNode), FindExecPin(*CreateWidgetNode));

	if (UEdGraphPin* PlayerIndexPin = FindPinByName(*GetPlayerControllerNode, TEXT("PlayerIndex")))
	{
		SetPinDefaultValue(*GetPlayerControllerNode, *PlayerIndexPin, TEXT("0"));
	}
	TryLinkPins(FindPinByName(*SelfNode, TEXT("self")), FindPinByName(*GetPlayerControllerNode, TEXT("WorldContextObject")));
	TryLinkPins(FindPinByName(*SelfNode, TEXT("self")), FindPinByName(*CreateWidgetNode, TEXT("WorldContextObject")));
	TryLinkPins(FindPinByName(*GetPlayerControllerNode, TEXT("ReturnValue")), FindPinByName(*CreateWidgetNode, TEXT("OwningPlayer")));
	if (UEdGraphPin* WidgetTypePin = FindPinByName(*CreateWidgetNode, TEXT("WidgetType")))
	{
		SetPinDefaultObject(*CreateWidgetNode, *WidgetTypePin, WidgetClass);
	}

	TryLinkPins(FindThenPin(*CreateWidgetNode), FindExecPin(*CastWidgetNode));
	TryLinkPins(FindPinByName(*CreateWidgetNode, TEXT("ReturnValue")), FindPinByName(*CastWidgetNode, UEdGraphSchema_K2::PN_ObjectToCast));
	TryLinkPins(CastWidgetNode->GetValidCastPin(), FindExecPin(*SetWidgetNode));
	TryLinkPins(CastWidgetNode->GetCastResultPin(), FindPinByName(*SetWidgetNode, RuntimeUiWidgetVariableName));
	TryLinkPins(FindThenPin(*SetWidgetNode), FindExecPin(*SetWidgetSaveObjectNode));
	TryLinkPins(CastWidgetNode->GetCastResultPin(), FindPinByName(*SetWidgetSaveObjectNode, TEXT("self")));
	TryLinkPins(RuntimeSaveObjectPin, FindPinByName(*SetWidgetSaveObjectNode, RuntimeSaveObjectVariableName));
	TryLinkPins(FindThenPin(*SetWidgetSaveObjectNode), FindExecPin(*AddToViewportNode));
	TryLinkPins(CastWidgetNode->GetCastResultPin(), FindPinByName(*AddToViewportNode, TEXT("self")));
	if (UEdGraphPin* ZOrderPin = FindPinByName(*AddToViewportNode, TEXT("ZOrder")))
	{
		SetPinDefaultValue(*AddToViewportNode, *ZOrderPin, TEXT("1000"));
	}

	AddRuntimeUiSetVisibilityFromBoolNodes(
		Graph,
		FindThenPin(*AddToViewportNode),
		CastWidgetNode->GetCastResultPin(),
		SaveVisibleValuePin,
		NodePosX + 2940,
		NodePosY);
	AssetResult.Actions.Add(TEXT("generated runtime UI initialize graph"));
}

void AddRuntimeUiHotkeyNodes(
	UEdGraph& Graph,
	const FNteCharacterRuntimeActionPlan& Plan,
	UEdGraphPin* ExecIn,
	const int32 NodePosX,
	const int32 NodePosY,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	if (!Plan.RuntimeUi.bEnableUi || Plan.RuntimeUi.ToggleUiHotkey.IsEmpty() || !ExecIn)
	{
		return;
	}

	UClass* SaveGameClass = LoadRuntimeSaveGameGeneratedClass(Plan, AssetResult);
	if (!SaveGameClass)
	{
		return;
	}

	FRuntimeHotkey Hotkey;
	if (!ParseRuntimeHotkey(Plan.RuntimeUi.ToggleUiHotkey, Hotkey))
	{
		AssetResult.Warnings.Add(FString::Printf(
			TEXT("Skipped runtime UI hotkey graph: invalid hotkey '%s'."),
			*Plan.RuntimeUi.ToggleUiHotkey));
		return;
	}

	UEdGraphPin* HotkeyConditionPin = AddHotkeyCondition(Graph, Hotkey, NodePosX, NodePosY);
	UK2Node_IfThenElse* BranchNode = AddRuntimeK2Node<UK2Node_IfThenElse>(Graph, NodePosX + 1720, NodePosY);
	UEdGraphPin* NewVisibleValuePin = AddToggledSaveGameBoolValue(Graph, SaveGameClass, TEXT("NTE_RuntimeUi_CurrentVisible"), NodePosX + 1980, NodePosY + 160);
	UK2Node_VariableSet* SetSaveVisibleNode = AddRuntimeSaveGameVariableSetNode(Graph, SaveGameClass, TEXT("NTE_RuntimeUi_CurrentVisible"), NewVisibleValuePin, NodePosX + 2660, NodePosY);
	UK2Node_VariableSet* SetVisibleNode = AddSetRuntimeUiCurrentVisibleNode(Graph, NewVisibleValuePin, NodePosX + 3440, NodePosY);
	UK2Node_VariableGet* GetWidgetNode = AddRuntimeVariableGetNode(Graph, RuntimeUiWidgetVariableName, NodePosX + 3760, NodePosY + 180);
	if (!HotkeyConditionPin || !BranchNode || !NewVisibleValuePin || !SetSaveVisibleNode || !SetVisibleNode || !GetWidgetNode)
	{
		AddError(AssetResult, TEXT("Runtime UI hotkey graph could not create required nodes."));
		return;
	}

	TryLinkPins(ExecIn, FindExecPin(*BranchNode));
	TryLinkPins(HotkeyConditionPin, FindPinByName(*BranchNode, TEXT("Condition")));
	TryLinkPins(FindThenPin(*BranchNode), FindExecPin(*SetSaveVisibleNode));
	UEdGraphPin* AfterSaveExec = AddRuntimeSaveGameToSlotNodes(Graph, Plan, FindThenPin(*SetSaveVisibleNode), NodePosX + 3180, NodePosY);
	TryLinkPins(AfterSaveExec, FindExecPin(*SetVisibleNode));
	AddRuntimeUiSetVisibilityFromBoolNodes(
		Graph,
		FindThenPin(*SetVisibleNode),
		FindPinByName(*GetWidgetNode, RuntimeUiWidgetVariableName),
		NewVisibleValuePin,
		NodePosX + 2960,
		NodePosY);
	AssetResult.Actions.Add(FString::Printf(
		TEXT("generated runtime UI toggle hotkey graph for %s"),
		*Plan.RuntimeUi.ToggleUiHotkey));
}

UK2Node_CallFunction* AddOwningComponentCall(UEdGraph& Graph, const int32 NodePosX, const int32 NodePosY)
{
	UFunction* GetOwningComponentFunction = UAnimInstance::StaticClass()->FindFunctionByName(TEXT("GetOwningComponent"));
	return AddRuntimeFunctionCallNode(Graph, GetOwningComponentFunction, NodePosX, NodePosY);
}

FString GetFirstTargetComponentTag(const FNteCharacterRuntimeActionPlanItem& Action)
{
	for (const FString& Tag : Action.TargetComponentTags)
	{
		if (!Tag.IsEmpty())
		{
			return Tag;
		}
	}
	return FString();
}

FRuntimeTargetComponentPins AddRuntimeTargetComponentNodes(
	UEdGraph& Graph,
	const FNteCharacterRuntimeActionPlanItem& Action,
	UClass* RequiredComponentClass,
	UEdGraphPin* ExecIn,
	const int32 NodePosX,
	const int32 NodePosY)
{
	FRuntimeTargetComponentPins Result;
	Result.ExecOut = ExecIn;
	if (!RequiredComponentClass)
	{
		return Result;
	}

	if (Action.TargetLookupMode.Equals(TEXT("OwningComponent"), ESearchCase::IgnoreCase))
	{
		UK2Node_CallFunction* GetOwningComponentNode = AddOwningComponentCall(Graph, NodePosX, NodePosY);
		Result.ComponentPin = GetOwningComponentNode ? FindPinByName(*GetOwningComponentNode, TEXT("ReturnValue")) : nullptr;
		return Result;
	}

	if (!Action.TargetLookupMode.Equals(TEXT("OwnerComponentByTags"), ESearchCase::IgnoreCase))
	{
		return Result;
	}

	const FString TargetTag = GetFirstTargetComponentTag(Action);
	if (TargetTag.IsEmpty())
	{
		return Result;
	}

	UFunction* GetOwnerFunction = UActorComponent::StaticClass()->FindFunctionByName(TEXT("GetOwner"));
	UFunction* FindComponentByTagFunction = AActor::StaticClass()->FindFunctionByName(TEXT("FindComponentByTag"));
	UK2Node_CallFunction* GetOwningComponentNode = AddOwningComponentCall(Graph, NodePosX, NodePosY + 220);
	UK2Node_CallFunction* GetOwnerNode = AddRuntimeFunctionCallNode(Graph, GetOwnerFunction, NodePosX + 320, NodePosY + 120);
	UK2Node_CallFunction* FindComponentNode = AddRuntimeFunctionCallNode(Graph, FindComponentByTagFunction, NodePosX + 660, NodePosY);
	UK2Node_DynamicCast* CastComponentNode = AddRuntimeDynamicCastNode(Graph, RequiredComponentClass, NodePosX + 1000, NodePosY);
	if (!GetOwningComponentNode || !GetOwnerNode || !FindComponentNode || !CastComponentNode)
	{
		return Result;
	}

	UEdGraphPin* CurrentExec = ExecIn;
	if (UEdGraphPin* GetOwnerExecPin = FindExecPin(*GetOwnerNode))
	{
		TryLinkPins(CurrentExec, GetOwnerExecPin);
		CurrentExec = FindThenPin(*GetOwnerNode);
	}

	TryLinkPins(FindPinByName(*GetOwningComponentNode, TEXT("ReturnValue")), FindPinByName(*GetOwnerNode, TEXT("self")));
	TryLinkPins(CurrentExec, FindExecPin(*FindComponentNode));
	TryLinkPins(FindPinByName(*GetOwnerNode, TEXT("ReturnValue")), FindPinByName(*FindComponentNode, TEXT("self")));
	if (UEdGraphPin* ComponentClassPin = FindPinByName(*FindComponentNode, TEXT("ComponentClass")))
	{
		SetPinDefaultObject(*FindComponentNode, *ComponentClassPin, RequiredComponentClass);
	}
	if (UEdGraphPin* TagPin = FindPinByName(*FindComponentNode, TEXT("Tag")))
	{
		SetPinDefaultValue(*FindComponentNode, *TagPin, TargetTag);
	}

	TryLinkPins(FindThenPin(*FindComponentNode), FindExecPin(*CastComponentNode));
	TryLinkPins(FindPinByName(*FindComponentNode, TEXT("ReturnValue")), FindPinByName(*CastComponentNode, UEdGraphSchema_K2::PN_ObjectToCast));
	Result.ExecOut = CastComponentNode->GetValidCastPin();
	Result.ComponentPin = CastComponentNode->GetCastResultPin();
	return Result;
}

UEdGraphPin* AddMaterialSlotVisibilityApplyNodes(
	UEdGraph& Graph,
	const FNteCharacterRuntimeActionPlanItem& Action,
	UEdGraphPin* ExecIn,
	UEdGraphPin* NewEnabledValuePin,
	const int32 NodePosX,
	const int32 NodePosY)
{
	if (!ExecIn || !NewEnabledValuePin || Action.MaterialSlots.IsEmpty())
	{
		return ExecIn;
	}

	UFunction* ShowMaterialSectionFunction = USkinnedMeshComponent::StaticClass()->FindFunctionByName(TEXT("ShowMaterialSection"));
	const FRuntimeTargetComponentPins TargetComponent = AddRuntimeTargetComponentNodes(
		Graph,
		Action,
		USkinnedMeshComponent::StaticClass(),
		ExecIn,
		NodePosX,
		NodePosY + 180);
	if (!ShowMaterialSectionFunction || !TargetComponent.ExecOut || !TargetComponent.ComponentPin)
	{
		return ExecIn;
	}

	UEdGraphPin* CurrentExec = TargetComponent.ExecOut;
	for (int32 SlotOrdinal = 0; SlotOrdinal < Action.MaterialSlots.Num(); ++SlotOrdinal)
	{
		const int32 SlotIndex = Action.MaterialSlots[SlotOrdinal];
		UK2Node_CallFunction* ShowNode = AddRuntimeFunctionCallNode(Graph, ShowMaterialSectionFunction, NodePosX + 1320 + SlotOrdinal * 320, NodePosY);
		if (!ShowNode)
		{
			continue;
		}

		TryLinkPins(CurrentExec, FindExecPin(*ShowNode));
		TryLinkPins(TargetComponent.ComponentPin, FindPinByName(*ShowNode, TEXT("self")));
		TryLinkPins(NewEnabledValuePin, FindPinByName(*ShowNode, TEXT("bShow")));
		if (UEdGraphPin* MaterialIdPin = FindPinByName(*ShowNode, TEXT("MaterialID")))
		{
			SetPinDefaultValue(*ShowNode, *MaterialIdPin, FString::FromInt(SlotIndex));
		}
		if (UEdGraphPin* SectionIndexPin = FindPinByName(*ShowNode, TEXT("SectionIndex")))
		{
			SetPinDefaultValue(*ShowNode, *SectionIndexPin, FString::FromInt(SlotIndex));
		}
		if (UEdGraphPin* LodIndexPin = FindPinByName(*ShowNode, TEXT("LODIndex")))
		{
			SetPinDefaultValue(*ShowNode, *LodIndexPin, TEXT("0"));
		}
		CurrentExec = FindThenPin(*ShowNode);
	}
	return CurrentExec;
}

UEdGraphPin* AddAttachedMeshVisibilityApplyNodes(
	UEdGraph& Graph,
	const FNteCharacterRuntimeActionPlanItem& Action,
	UEdGraphPin* ExecIn,
	UEdGraphPin* NewEnabledValuePin,
	const int32 NodePosX,
	const int32 NodePosY)
{
	if (!ExecIn || !NewEnabledValuePin)
	{
		return ExecIn;
	}

	UFunction* SetVisibilityFunction = USceneComponent::StaticClass()->FindFunctionByName(TEXT("SetVisibility"));
	const FRuntimeTargetComponentPins TargetComponent = AddRuntimeTargetComponentNodes(
		Graph,
		Action,
		USceneComponent::StaticClass(),
		ExecIn,
		NodePosX,
		NodePosY + 180);
	UK2Node_CallFunction* SetVisibilityNode = AddRuntimeFunctionCallNode(Graph, SetVisibilityFunction, NodePosX + 1320, NodePosY);
	if (!SetVisibilityFunction || !TargetComponent.ExecOut || !TargetComponent.ComponentPin || !SetVisibilityNode)
	{
		return ExecIn;
	}

	TryLinkPins(TargetComponent.ExecOut, FindExecPin(*SetVisibilityNode));
	TryLinkPins(TargetComponent.ComponentPin, FindPinByName(*SetVisibilityNode, TEXT("self")));
	TryLinkPins(NewEnabledValuePin, FindPinByName(*SetVisibilityNode, TEXT("bNewVisibility")));
	if (UEdGraphPin* PropagatePin = FindPinByName(*SetVisibilityNode, TEXT("bPropagateToChildren")))
	{
		SetPinDefaultValue(*SetVisibilityNode, *PropagatePin, TEXT("true"));
	}
	return FindThenPin(*SetVisibilityNode);
}

UEdGraphPin* AddRuntimeActionApplyNodes(
	UEdGraph& Graph,
	const FNteCharacterRuntimeActionPlanItem& Action,
	UEdGraphPin* ExecIn,
	UEdGraphPin* EnabledValuePin,
	const int32 NodePosX,
	const int32 NodePosY)
{
	if (Action.ActionType.Equals(TEXT("MaterialSlotVisibility"), ESearchCase::IgnoreCase))
	{
		return AddMaterialSlotVisibilityApplyNodes(Graph, Action, ExecIn, EnabledValuePin, NodePosX, NodePosY);
	}
	if (Action.ActionType.Equals(TEXT("AttachedMeshVisibility"), ESearchCase::IgnoreCase))
	{
		return AddAttachedMeshVisibilityApplyNodes(Graph, Action, ExecIn, EnabledValuePin, NodePosX, NodePosY);
	}
	return ExecIn;
}

void AddRuntimeActionInitialApplyFromSaveGameNodes(
	UEdGraph& Graph,
	UClass* SaveGameClass,
	const FNteCharacterRuntimeActionPlanItem& Action,
	UEdGraphPin* ExecIn,
	const int32 NodePosX,
	const int32 NodePosY)
{
	const FName EnabledVariableName = MakeActionEnabledVariableName(Action);
	UEdGraphPin* SaveEnabledValuePin = AddRuntimeSaveGameVariableValuePin(Graph, SaveGameClass, EnabledVariableName, NodePosX, NodePosY + 160);
	UK2Node_VariableSet* SetLocalEnabledNode = AddSetEnabledNode(Graph, Action, SaveEnabledValuePin, NodePosX + 620, NodePosY);
	if (!SaveEnabledValuePin || !SetLocalEnabledNode)
	{
		return;
	}

	TryLinkPins(ExecIn, FindExecPin(*SetLocalEnabledNode));
	AddRuntimeActionApplyNodes(Graph, Action, FindThenPin(*SetLocalEnabledNode), SaveEnabledValuePin, NodePosX + 940, NodePosY);
}

void AddRuntimeActionHotkeySaveGameNodes(
	UEdGraph& Graph,
	const FNteCharacterRuntimeActionPlan& Plan,
	UClass* SaveGameClass,
	const FNteCharacterRuntimeActionPlanItem& Action,
	UEdGraphPin* ExecIn,
	const int32 NodePosX,
	const int32 NodePosY,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	FRuntimeHotkey Hotkey;
	if (!ParseRuntimeHotkey(Action.Hotkey, Hotkey))
	{
		AssetResult.Warnings.Add(FString::Printf(TEXT("Skipped execution graph for action '%s': invalid hotkey '%s'."), *Action.Id, *Action.Hotkey));
		return;
	}

	const FName EnabledVariableName = MakeActionEnabledVariableName(Action);
	UEdGraphPin* HotkeyConditionPin = AddHotkeyCondition(Graph, Hotkey, NodePosX, NodePosY);
	UK2Node_IfThenElse* BranchNode = AddRuntimeK2Node<UK2Node_IfThenElse>(Graph, NodePosX + 1720, NodePosY);
	UEdGraphPin* NewEnabledValuePin = AddToggledSaveGameBoolValue(Graph, SaveGameClass, EnabledVariableName, NodePosX + 1980, NodePosY + 160);
	UK2Node_VariableSet* SetSaveEnabledNode = AddRuntimeSaveGameVariableSetNode(Graph, SaveGameClass, EnabledVariableName, NewEnabledValuePin, NodePosX + 2660, NodePosY);
	if (!HotkeyConditionPin || !BranchNode || !NewEnabledValuePin || !SetSaveEnabledNode)
	{
		AddError(AssetResult, FString::Printf(TEXT("Runtime hotkey graph for action '%s' could not create required SaveGame nodes."), *Action.Id));
		return;
	}

	TryLinkPins(ExecIn, FindExecPin(*BranchNode));
	TryLinkPins(HotkeyConditionPin, FindPinByName(*BranchNode, TEXT("Condition")));
	TryLinkPins(FindThenPin(*BranchNode), FindExecPin(*SetSaveEnabledNode));
	AddRuntimeSaveGameToSlotNodes(Graph, Plan, FindThenPin(*SetSaveEnabledNode), NodePosX + 3180, NodePosY);
}

void AddRuntimeActionSyncFromSaveGameNodes(
	UEdGraph& Graph,
	UClass* SaveGameClass,
	const FNteCharacterRuntimeActionPlanItem& Action,
	UEdGraphPin* ExecIn,
	const int32 NodePosX,
	const int32 NodePosY)
{
	const FName EnabledVariableName = MakeActionEnabledVariableName(Action);
	UEdGraphPin* SaveEnabledValuePin = AddRuntimeSaveGameVariableValuePin(Graph, SaveGameClass, EnabledVariableName, NodePosX, NodePosY + 160);
	UEdGraphPin* LocalEnabledValuePin = AddCurrentEnabledValue(Graph, Action, NodePosX, NodePosY + 420);
	UEdGraphPin* ChangedConditionPin = AddBoolNotEqualValue(Graph, SaveEnabledValuePin, LocalEnabledValuePin, NodePosX + 620, NodePosY + 260);
	UK2Node_IfThenElse* BranchNode = AddRuntimeK2Node<UK2Node_IfThenElse>(Graph, NodePosX + 940, NodePosY);
	UK2Node_VariableSet* SetLocalEnabledNode = AddSetEnabledNode(Graph, Action, SaveEnabledValuePin, NodePosX + 1260, NodePosY);
	if (!SaveEnabledValuePin || !LocalEnabledValuePin || !ChangedConditionPin || !BranchNode || !SetLocalEnabledNode)
	{
		return;
	}

	TryLinkPins(ExecIn, FindExecPin(*BranchNode));
	TryLinkPins(ChangedConditionPin, FindPinByName(*BranchNode, TEXT("Condition")));
	TryLinkPins(FindThenPin(*BranchNode), FindExecPin(*SetLocalEnabledNode));
	AddRuntimeActionApplyNodes(Graph, Action, FindThenPin(*SetLocalEnabledNode), SaveEnabledValuePin, NodePosX + 1580, NodePosY);
}

bool IsApplySupportedAction(const FNteCharacterRuntimeActionPlanItem& Action, FString& OutReason)
{
	const bool bUsesOwningComponent = Action.TargetLookupMode.Equals(TEXT("OwningComponent"), ESearchCase::IgnoreCase);
	const bool bUsesOwnerComponentByTags = Action.TargetLookupMode.Equals(TEXT("OwnerComponentByTags"), ESearchCase::IgnoreCase);
	if (!bUsesOwningComponent && !bUsesOwnerComponentByTags)
	{
		OutReason = FString::Printf(TEXT("target lookup mode '%s' is not generated yet"), *Action.TargetLookupMode);
		return false;
	}
	if (bUsesOwnerComponentByTags && Action.TargetComponentTags.IsEmpty())
	{
		OutReason = TEXT("OwnerComponentByTags needs TargetComponentTags or target attached mesh MeshComponentOwnedTags");
		return false;
	}
	if (Action.ActionType.Equals(TEXT("MaterialSlotVisibility"), ESearchCase::IgnoreCase))
	{
		if (Action.MaterialSlots.IsEmpty())
		{
			OutReason = TEXT("MaterialSlotVisibility has no material slots");
			return false;
		}
		return true;
	}
	if (Action.ActionType.Equals(TEXT("AttachedMeshVisibility"), ESearchCase::IgnoreCase))
	{
		return true;
	}

	OutReason = FString::Printf(TEXT("action type '%s' is not in the generated execution graph slice"), *Action.ActionType);
	return false;
}

bool IsHotkeySupportedAction(const FNteCharacterRuntimeActionPlanItem& Action, FString& OutReason)
{
	if (Action.Hotkey.IsEmpty())
	{
		OutReason = TEXT("action has no hotkey; only initial state application is generated");
		return false;
	}
	return IsApplySupportedAction(Action, OutReason);
}

void AddRuntimeExecutionGraphToAnimBlueprint(
	UBlueprint& Blueprint,
	const FNteCharacterRuntimeActionPlan& Plan,
	const TArray<const FNteCharacterRuntimeActionPlanItem*>& Actions,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	TArray<const FNteCharacterRuntimeActionPlanItem*> ApplyActions;
	TArray<const FNteCharacterRuntimeActionPlanItem*> HotkeyActions;
	for (const FNteCharacterRuntimeActionPlanItem* Action : Actions)
	{
		if (!Action)
		{
			continue;
		}

		FString UnsupportedReason;
		if (IsApplySupportedAction(*Action, UnsupportedReason))
		{
			ApplyActions.Add(Action);
			FString HotkeyUnsupportedReason;
			if (IsHotkeySupportedAction(*Action, HotkeyUnsupportedReason))
			{
				HotkeyActions.Add(Action);
			}
		}
		else if (Action->bFirstSliceBlueprintSupported)
		{
			AssetResult.Warnings.Add(FString::Printf(TEXT("Skipped execution graph for action '%s': %s."), *Action->Id, *UnsupportedReason));
		}
	}

	if (ApplyActions.IsEmpty() && !Plan.RuntimeUi.bEnableUi)
	{
		return;
	}

	UEdGraph* EventGraph = EnsureEventGraph(Blueprint);
	if (!EventGraph)
	{
		AddError(AssetResult, TEXT("AnimBlueprint has no EventGraph and one could not be created."));
		return;
	}

	RemoveGeneratedRuntimeNodes(Blueprint);
	UK2Node_Event* InitializeEvent = FindOrAddAnimInitializeEvent(Blueprint, *EventGraph);
	if (!InitializeEvent)
	{
		AddError(AssetResult, TEXT("Could not create BlueprintInitializeAnimation event for runtime actions."));
		return;
	}
	UClass* SaveGameClass = LoadRuntimeSaveGameGeneratedClass(Plan, AssetResult);
	if (!SaveGameClass)
	{
		return;
	}

	UK2Node_ExecutionSequence* InitializeSequenceNode = AddRuntimeK2Node<UK2Node_ExecutionSequence>(*EventGraph, 260, -1800);
	TryLinkPins(FindThenPin(*InitializeEvent), FindExecPin(*InitializeSequenceNode));

	UEdGraphPin* SaveGameInitializeThenPin = EnsureSequenceOutputPin(*InitializeSequenceNode, 0);
	AddRuntimeSaveGameLoadOrCreateNodes(*EventGraph, Plan, SaveGameInitializeThenPin, 520, -2400, AssetResult);

	for (int32 ActionIndex = 0; ActionIndex < ApplyActions.Num(); ++ActionIndex)
	{
		const FNteCharacterRuntimeActionPlanItem& Action = *ApplyActions[ActionIndex];
		const int32 BaseY = -1800 - ActionIndex * 520;
		UEdGraphPin* SequenceThenPin = EnsureSequenceOutputPin(*InitializeSequenceNode, ActionIndex + 1);
		AddRuntimeActionInitialApplyFromSaveGameNodes(*EventGraph, SaveGameClass, Action, SequenceThenPin, 520, BaseY);
		AssetResult.Actions.Add(FString::Printf(TEXT("generated SaveGame initial apply graph for action %s"), *Action.Id));
	}

	if (Plan.RuntimeUi.bEnableUi)
	{
		const int32 UiInitializeIndex = ApplyActions.Num() + 1;
		UEdGraphPin* SequenceThenPin = EnsureSequenceOutputPin(*InitializeSequenceNode, UiInitializeIndex);
		AddRuntimeUiInitializeNodes(*EventGraph, Plan, SequenceThenPin, 520, -1800 - UiInitializeIndex * 520, AssetResult);
	}

	const bool bHasRuntimeUiHotkey = Plan.RuntimeUi.bEnableUi && !Plan.RuntimeUi.ToggleUiHotkey.IsEmpty();
	const bool bNeedsSaveGamePolling = !ApplyActions.IsEmpty() && (Plan.RuntimeUi.bEnableUi || !HotkeyActions.IsEmpty());
	if (!HotkeyActions.IsEmpty() || bHasRuntimeUiHotkey || bNeedsSaveGamePolling)
	{
		UK2Node_Event* UpdateEvent = FindOrAddAnimUpdateEvent(Blueprint, *EventGraph);
		if (!UpdateEvent)
		{
			AddError(AssetResult, TEXT("Could not create BlueprintUpdateAnimation event for runtime actions."));
			return;
		}

		UK2Node_ExecutionSequence* SequenceNode = AddRuntimeK2Node<UK2Node_ExecutionSequence>(*EventGraph, 260, 0);
		TryLinkPins(FindThenPin(*UpdateEvent), FindExecPin(*SequenceNode));

		for (int32 ActionIndex = 0; ActionIndex < HotkeyActions.Num(); ++ActionIndex)
		{
			const FNteCharacterRuntimeActionPlanItem& Action = *HotkeyActions[ActionIndex];
			const int32 BaseY = ActionIndex * 1400;
			UEdGraphPin* SequenceThenPin = EnsureSequenceOutputPin(*SequenceNode, ActionIndex);
			AddRuntimeActionHotkeySaveGameNodes(*EventGraph, Plan, SaveGameClass, Action, SequenceThenPin, 520, BaseY, AssetResult);
			AssetResult.Actions.Add(FString::Printf(TEXT("generated SaveGame hotkey graph for action %s"), *Action.Id));
		}

		int32 NextUpdateSequenceIndex = HotkeyActions.Num();
		if (bHasRuntimeUiHotkey)
		{
			const int32 UiHotkeyIndex = NextUpdateSequenceIndex++;
			const int32 BaseY = UiHotkeyIndex * 1400;
			UEdGraphPin* SequenceThenPin = EnsureSequenceOutputPin(*SequenceNode, UiHotkeyIndex);
			AddRuntimeUiHotkeyNodes(*EventGraph, Plan, SequenceThenPin, 520, BaseY, AssetResult);
		}

		if (bNeedsSaveGamePolling)
		{
			const int32 PollIndex = NextUpdateSequenceIndex++;
			const int32 PollBaseY = PollIndex * 1400;
			UEdGraphPin* PollThenPin = EnsureSequenceOutputPin(*SequenceNode, PollIndex);
			if (ApplyActions.Num() == 1)
			{
				AddRuntimeActionSyncFromSaveGameNodes(*EventGraph, SaveGameClass, *ApplyActions[0], PollThenPin, 520, PollBaseY);
			}
			else
			{
				UK2Node_ExecutionSequence* PollSequenceNode = AddRuntimeK2Node<UK2Node_ExecutionSequence>(*EventGraph, 520, PollBaseY);
				TryLinkPins(PollThenPin, FindExecPin(*PollSequenceNode));
				for (int32 ActionIndex = 0; ActionIndex < ApplyActions.Num(); ++ActionIndex)
				{
					UEdGraphPin* ActionPollThenPin = EnsureSequenceOutputPin(*PollSequenceNode, ActionIndex);
					AddRuntimeActionSyncFromSaveGameNodes(
						*EventGraph,
						SaveGameClass,
						*ApplyActions[ActionIndex],
						ActionPollThenPin,
						860,
						PollBaseY + ActionIndex * 620);
				}
			}
			AssetResult.Actions.Add(FString::Printf(
				TEXT("generated SaveGame polling apply graph for %d action(s)"),
				ApplyActions.Num()));
		}
	}

	for (int32 ActionIndex = 0; ActionIndex < ApplyActions.Num(); ++ActionIndex)
	{
		const FNteCharacterRuntimeActionPlanItem& Action = *ApplyActions[ActionIndex];
		if (Action.Hotkey.IsEmpty())
		{
			AssetResult.Actions.Add(FString::Printf(TEXT("action %s has no hotkey; generated initial apply graph only"), *Action.Id));
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);
}

void CompileAndSaveBlueprint(
	UBlueprint& Blueprint,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult,
	FNteCharacterRuntimeActionWriteResult& Result)
{
	FKismetEditorUtilities::CompileBlueprint(&Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	FString Error;
	if (!SaveLoadedAssetPackage(Blueprint, Result, Error))
	{
		AddError(AssetResult, Error);
		AddError(Result, Error);
		return;
	}
	AssetResult.Actions.Add(FString::Printf(TEXT("saved %s"), *Blueprint.GetPackage()->GetName()));
}

TArray<const FNteCharacterRuntimeActionPlanItem*> CollectAllActionPointers(const FNteCharacterRuntimeActionPlan& Plan)
{
	TArray<const FNteCharacterRuntimeActionPlanItem*> Actions;
	for (const FNteCharacterRuntimeActionPlanItem& Action : Plan.Actions)
	{
		Actions.Add(&Action);
	}
	return Actions;
}

TArray<const FNteCharacterRuntimeActionPlanItem*> CollectHostActionPointers(
	const FNteCharacterRuntimeActionPlan& Plan,
	const FNteCharacterRuntimeActionHostPlan& Host)
{
	TArray<const FNteCharacterRuntimeActionPlanItem*> Actions;
	for (const FString& ActionId : Host.ActionIds)
	{
		for (const FNteCharacterRuntimeActionPlanItem& Action : Plan.Actions)
		{
			if (Action.Id == ActionId)
			{
				Actions.Add(&Action);
				break;
			}
		}
	}
	return Actions;
}

void AppendAssetResult(FNteCharacterRuntimeActionWriteResult& Result, FNteCharacterRuntimeActionAssetWriteResult&& AssetResult)
{
	if (!AssetResult.Errors.IsEmpty())
	{
		Result.Errors.Append(AssetResult.Errors);
	}
	if (!AssetResult.Warnings.IsEmpty())
	{
		Result.Warnings.Append(AssetResult.Warnings);
	}
	Result.Assets.Add(MoveTemp(AssetResult));
}

TSharedRef<FJsonObject> AssetWriteResultToJson(const FNteCharacterRuntimeActionAssetWriteResult& Asset)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("AssetPath"), Asset.AssetPath);
	Object->SetStringField(TEXT("AssetKind"), Asset.AssetKind);
	Object->SetBoolField(TEXT("Created"), Asset.bCreated);
	Object->SetBoolField(TEXT("Updated"), Asset.bUpdated);
	Object->SetArrayField(TEXT("Actions"), Json::StringArrayToJsonValues(Asset.Actions));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Asset.Warnings));
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Asset.Errors));
	return Object;
}
}

TArray<FString> CollectCharacterRuntimeActionPlanPackageSeeds(const FNteCharacterRuntimeActionPlan& Plan)
{
	TArray<FString> Seeds;
	if (!Plan.Actions.IsEmpty() || Plan.RuntimeUi.bEnableUi)
	{
		AddSeed(Seeds, Plan.SaveGameBlueprintPath);
	}
	if (Plan.RuntimeUi.bEnableUi)
	{
		AddSeed(Seeds, Plan.WidgetBlueprintPath);
	}
	for (const FNteCharacterRuntimeActionHostPlan& Host : Plan.Hosts)
	{
		AddSeed(Seeds, Host.AnimBlueprintPath);
	}
	Seeds.Sort();
	return Seeds;
}

FNteCharacterRuntimeActionWriteResult WriteCharacterRuntimeActions(const FNteCharacterRuntimeActionPlan& Plan)
{
	FNteCharacterRuntimeActionWriteResult Result;
	Result.RuntimeAssetRootPath = Plan.RuntimeAssetRootPath;
	Result.WidgetBlueprintPath = Plan.WidgetBlueprintPath;
	Result.SaveGameBlueprintPath = Plan.SaveGameBlueprintPath;
	Result.SaveSlotName = Plan.SaveSlotName;

	if (Plan.Actions.IsEmpty() && !Plan.RuntimeUi.bEnableUi)
	{
		AddWarning(Result, TEXT("CharacterModSpec has no RuntimeActions; nothing to write."));
		return Result;
	}
	if (!Plan.Errors.IsEmpty())
	{
		Result.Errors.Append(Plan.Errors);
		return Result;
	}

	const TArray<const FNteCharacterRuntimeActionPlanItem*> AllActions = CollectAllActionPointers(Plan);
	TMap<FString, int32> HostPathUseCount;
	for (const FNteCharacterRuntimeActionHostPlan& Host : Plan.Hosts)
	{
		if (!Host.AnimBlueprintPath.IsEmpty())
		{
			HostPathUseCount.FindOrAdd(Host.AnimBlueprintPath)++;
		}
	}
	TSet<FString> SharedHostPathWarningsEmitted;

	FNteCharacterRuntimeActionAssetWriteResult SaveGameResult;
	SaveGameResult.AssetPath = Plan.SaveGameBlueprintPath;
	SaveGameResult.AssetKind = TEXT("SaveGameBlueprint");
	if (UBlueprint* SaveGameBlueprint = CreateOrLoadSaveGameBlueprint(Plan.SaveGameBlueprintPath, SaveGameResult))
	{
		AddPlanVariablesToBlueprint(*SaveGameBlueprint, Plan, AllActions, SaveGameResult);
		CompileAndSaveBlueprint(*SaveGameBlueprint, SaveGameResult, Result);
	}
	AppendAssetResult(Result, MoveTemp(SaveGameResult));

	if (Plan.RuntimeUi.bEnableUi)
	{
		FNteCharacterRuntimeActionAssetWriteResult WidgetResult;
		WidgetResult.AssetPath = Plan.WidgetBlueprintPath;
		WidgetResult.AssetKind = TEXT("WidgetBlueprint");
		if (UBlueprint* WidgetBlueprint = CreateOrLoadWidgetBlueprint(Plan.WidgetBlueprintPath, WidgetResult))
		{
			AddPlanVariablesToBlueprint(*WidgetBlueprint, Plan, AllActions, WidgetResult);
			AddRuntimeStateObjectVariableToBlueprint(*WidgetBlueprint, Plan, WidgetResult);
			if (UWidgetBlueprint* RuntimeWidgetBlueprint = Cast<UWidgetBlueprint>(WidgetBlueprint))
			{
				RebuildRuntimeActionWidgetTree(*RuntimeWidgetBlueprint, Plan, AllActions, WidgetResult);
				AddRuntimeWidgetClickGraphToWidgetBlueprint(*RuntimeWidgetBlueprint, Plan, AllActions, WidgetResult);
			}
			else
			{
				AddError(WidgetResult, FString::Printf(
					TEXT("Runtime UI is enabled but generated Widget asset is %s, not UWidgetBlueprint."),
					*WidgetBlueprint->GetClass()->GetPathName()));
			}
			CompileAndSaveBlueprint(*WidgetBlueprint, WidgetResult, Result);
		}
		AppendAssetResult(Result, MoveTemp(WidgetResult));
	}

	for (const FNteCharacterRuntimeActionHostPlan& Host : Plan.Hosts)
	{
		FNteCharacterRuntimeActionAssetWriteResult HostResult;
		HostResult.AssetPath = Host.AnimBlueprintPath;
		HostResult.AssetKind = FString::Printf(TEXT("%s:%s"), *Host.HostKind, *Host.MeshId);
		if (Host.AnimBlueprintPath.IsEmpty())
		{
			AddError(HostResult, FString::Printf(TEXT("Runtime action host '%s' has no AnimBlueprintPath."), *Host.MeshId));
			AppendAssetResult(Result, MoveTemp(HostResult));
			continue;
		}

		USkeletalMesh* HostMesh = LoadHostSkeletalMesh(Host, HostResult);
		UBlueprint* HostBlueprint = HostMesh ? CreateOrLoadAnimBlueprintForHost(Host, *HostMesh, HostResult) : nullptr;
		if (HostBlueprint)
		{
			const TArray<const FNteCharacterRuntimeActionPlanItem*> HostActions = CollectHostActionPointers(Plan, Host);
			AddPlanVariablesToBlueprint(*HostBlueprint, Plan, HostActions, HostResult);
			AddRuntimeHostVariablesToBlueprint(*HostBlueprint, Plan, HostResult);
			if (HostPathUseCount.FindRef(Host.AnimBlueprintPath) == 1)
			{
				AddRuntimeExecutionGraphToAnimBlueprint(*HostBlueprint, Plan, HostActions, HostResult);
			}
			else
			{
				if (!SharedHostPathWarningsEmitted.Contains(Host.AnimBlueprintPath))
				{
					HostResult.Warnings.Add(FString::Printf(
						TEXT("Skipped execution graph for shared AnimBlueprint '%s'. Give each runtime host mesh a distinct RuntimeAnimBlueprintPath before generating executable hotkey logic."),
						*Host.AnimBlueprintPath));
					SharedHostPathWarningsEmitted.Add(Host.AnimBlueprintPath);
				}
				else
				{
					HostResult.Actions.Add(FString::Printf(
						TEXT("shared AnimBlueprint execution graph skip already reported for %s"),
						*Host.AnimBlueprintPath));
				}
			}
			CompileAndSaveBlueprint(*HostBlueprint, HostResult, Result);
		}
		AppendAssetResult(Result, MoveTemp(HostResult));
	}

	return Result;
}

TSharedRef<FJsonObject> CharacterRuntimeActionWriteResultToJson(const FNteCharacterRuntimeActionWriteResult& Result)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("RuntimeAssetRootPath"), Result.RuntimeAssetRootPath);
	Object->SetStringField(TEXT("WidgetBlueprintPath"), Result.WidgetBlueprintPath);
	Object->SetStringField(TEXT("SaveGameBlueprintPath"), Result.SaveGameBlueprintPath);
	Object->SetStringField(TEXT("SaveSlotName"), Result.SaveSlotName);

	TArray<TSharedPtr<FJsonValue>> Assets;
	for (const FNteCharacterRuntimeActionAssetWriteResult& Asset : Result.Assets)
	{
		Assets.Add(MakeShared<FJsonValueObject>(AssetWriteResultToJson(Asset)));
	}
	Object->SetArrayField(TEXT("Assets"), Assets);
	Object->SetNumberField(TEXT("AssetCount"), Result.Assets.Num());
	Object->SetArrayField(TEXT("SavedPackages"), Json::StringArrayToJsonValues(Result.SavedPackages));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Result.Warnings));
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Result.Errors));
	return Object;
}
}
