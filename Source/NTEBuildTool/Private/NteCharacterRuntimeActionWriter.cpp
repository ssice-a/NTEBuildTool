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
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/SceneComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimBlueprintFactory.h"
#include "GameFramework/SaveGame.h"
#include "GameFramework/PlayerController.h"
#include "K2Node_CallFunction.h"
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
#include "WidgetBlueprint.h"

#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace NTEBuildTool::Character
{
namespace
{
constexpr const TCHAR* GeneratedRuntimeNodeComment = TEXT("NTE Character RuntimeAction Generated");

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

UK2Node_Event* FindOrAddAnimUpdateEvent(UBlueprint& Blueprint, UEdGraph& Graph)
{
	if (UK2Node_Event* ExistingUpdate = FBlueprintEditorUtils::FindOverrideForFunction(&Blueprint, UAnimInstance::StaticClass(), TEXT("BlueprintUpdateAnimation")))
	{
		return ExistingUpdate;
	}
	int32 NodePosY = 0;
	return FKismetEditorUtilities::AddDefaultEventNode(&Blueprint, &Graph, TEXT("BlueprintUpdateAnimation"), UAnimInstance::StaticClass(), NodePosY);
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

void AddPlanVariablesToBlueprint(
	UBlueprint& Blueprint,
	const FNteCharacterRuntimeActionPlan& Plan,
	const TArray<const FNteCharacterRuntimeActionPlanItem*>& Actions,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	EnsureBlueprintVariable(Blueprint, TEXT("NTE_CharacterActionPlanJson"), MakeStringPinType(), PlanToCondensedJson(Plan), AssetResult);
	EnsureBlueprintVariable(Blueprint, TEXT("NTE_CharacterActionCount"), MakeIntPinType(), FString::FromInt(Actions.Num()), AssetResult);
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

UEdGraphPin* AddToggledEnabledValue(
	UEdGraph& Graph,
	const FNteCharacterRuntimeActionPlanItem& Action,
	const int32 NodePosX,
	const int32 NodePosY)
{
	UFunction* BoolNotFunction = UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Not_PreBool"));
	if (!BoolNotFunction)
	{
		return nullptr;
	}

	const FName EnabledVariableName = MakeActionEnabledVariableName(Action);
	UK2Node_VariableGet* GetEnabledNode = AddRuntimeVariableGetNode(Graph, EnabledVariableName, NodePosX, NodePosY);
	UK2Node_CallFunction* NotNode = AddRuntimeFunctionCallNode(Graph, BoolNotFunction, NodePosX + 260, NodePosY);
	if (!GetEnabledNode || !NotNode)
	{
		return nullptr;
	}

	TryLinkPins(FindPinByName(*GetEnabledNode, EnabledVariableName), FindPinByName(*NotNode, TEXT("A")));
	return FindPinByName(*NotNode, TEXT("ReturnValue"));
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

UK2Node_CallFunction* AddOwningComponentCall(UEdGraph& Graph, const int32 NodePosX, const int32 NodePosY)
{
	UFunction* GetOwningComponentFunction = UAnimInstance::StaticClass()->FindFunctionByName(TEXT("GetOwningComponent"));
	return AddRuntimeFunctionCallNode(Graph, GetOwningComponentFunction, NodePosX, NodePosY);
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
	UK2Node_CallFunction* GetOwningComponentNode = AddOwningComponentCall(Graph, NodePosX, NodePosY + 180);
	if (!ShowMaterialSectionFunction || !GetOwningComponentNode)
	{
		return ExecIn;
	}

	UEdGraphPin* CurrentExec = ExecIn;
	for (int32 SlotOrdinal = 0; SlotOrdinal < Action.MaterialSlots.Num(); ++SlotOrdinal)
	{
		const int32 SlotIndex = Action.MaterialSlots[SlotOrdinal];
		UK2Node_CallFunction* ShowNode = AddRuntimeFunctionCallNode(Graph, ShowMaterialSectionFunction, NodePosX + 320 + SlotOrdinal * 320, NodePosY);
		if (!ShowNode)
		{
			continue;
		}

		TryLinkPins(CurrentExec, FindExecPin(*ShowNode));
		TryLinkPins(FindPinByName(*GetOwningComponentNode, TEXT("ReturnValue")), FindPinByName(*ShowNode, TEXT("self")));
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
	UK2Node_CallFunction* GetOwningComponentNode = AddOwningComponentCall(Graph, NodePosX, NodePosY + 180);
	UK2Node_CallFunction* SetVisibilityNode = AddRuntimeFunctionCallNode(Graph, SetVisibilityFunction, NodePosX + 320, NodePosY);
	if (!SetVisibilityFunction || !GetOwningComponentNode || !SetVisibilityNode)
	{
		return ExecIn;
	}

	TryLinkPins(ExecIn, FindExecPin(*SetVisibilityNode));
	TryLinkPins(FindPinByName(*GetOwningComponentNode, TEXT("ReturnValue")), FindPinByName(*SetVisibilityNode, TEXT("self")));
	TryLinkPins(NewEnabledValuePin, FindPinByName(*SetVisibilityNode, TEXT("bNewVisibility")));
	if (UEdGraphPin* PropagatePin = FindPinByName(*SetVisibilityNode, TEXT("bPropagateToChildren")))
	{
		SetPinDefaultValue(*SetVisibilityNode, *PropagatePin, TEXT("true"));
	}
	return FindThenPin(*SetVisibilityNode);
}

bool IsGraphSupportedAction(const FNteCharacterRuntimeActionPlanItem& Action, FString& OutReason)
{
	if (Action.Hotkey.IsEmpty())
	{
		OutReason = TEXT("action has no hotkey; UI click binding is not generated yet");
		return false;
	}
	if (!Action.TargetLookupMode.Equals(TEXT("OwningComponent"), ESearchCase::IgnoreCase))
	{
		OutReason = FString::Printf(TEXT("target lookup mode '%s' is not generated yet"), *Action.TargetLookupMode);
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

void AddRuntimeExecutionGraphToAnimBlueprint(
	UBlueprint& Blueprint,
	const TArray<const FNteCharacterRuntimeActionPlanItem*>& Actions,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	TArray<const FNteCharacterRuntimeActionPlanItem*> GraphActions;
	for (const FNteCharacterRuntimeActionPlanItem* Action : Actions)
	{
		if (!Action)
		{
			continue;
		}

		FString UnsupportedReason;
		if (IsGraphSupportedAction(*Action, UnsupportedReason))
		{
			GraphActions.Add(Action);
		}
		else if (Action->bFirstSliceBlueprintSupported)
		{
			AssetResult.Warnings.Add(FString::Printf(TEXT("Skipped execution graph for action '%s': %s."), *Action->Id, *UnsupportedReason));
		}
	}

	if (GraphActions.IsEmpty())
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
	UK2Node_Event* UpdateEvent = FindOrAddAnimUpdateEvent(Blueprint, *EventGraph);
	if (!UpdateEvent)
	{
		AddError(AssetResult, TEXT("Could not create BlueprintUpdateAnimation event for runtime actions."));
		return;
	}

	UK2Node_ExecutionSequence* SequenceNode = AddRuntimeK2Node<UK2Node_ExecutionSequence>(*EventGraph, 260, 0);
	TryLinkPins(FindThenPin(*UpdateEvent), FindExecPin(*SequenceNode));

	for (int32 ActionIndex = 0; ActionIndex < GraphActions.Num(); ++ActionIndex)
	{
		const FNteCharacterRuntimeActionPlanItem& Action = *GraphActions[ActionIndex];
		FRuntimeHotkey Hotkey;
		if (!ParseRuntimeHotkey(Action.Hotkey, Hotkey))
		{
			AssetResult.Warnings.Add(FString::Printf(TEXT("Skipped execution graph for action '%s': invalid hotkey '%s'."), *Action.Id, *Action.Hotkey));
			continue;
		}

		const int32 BaseY = ActionIndex * 1400;
		UEdGraphPin* SequenceThenPin = EnsureSequenceOutputPin(*SequenceNode, ActionIndex);
		UEdGraphPin* HotkeyConditionPin = AddHotkeyCondition(*EventGraph, Hotkey, 520, BaseY);
		UK2Node_IfThenElse* BranchNode = AddRuntimeK2Node<UK2Node_IfThenElse>(*EventGraph, 2240, BaseY);
		UEdGraphPin* NewEnabledValuePin = AddToggledEnabledValue(*EventGraph, Action, 2500, BaseY + 160);
		UK2Node_VariableSet* SetEnabledNode = AddSetEnabledNode(*EventGraph, Action, NewEnabledValuePin, 2820, BaseY);

		TryLinkPins(SequenceThenPin, FindExecPin(*BranchNode));
		TryLinkPins(HotkeyConditionPin, FindPinByName(*BranchNode, TEXT("Condition")));
		if (SetEnabledNode)
		{
			TryLinkPins(FindThenPin(*BranchNode), FindExecPin(*SetEnabledNode));
		}

		UEdGraphPin* ApplyExec = SetEnabledNode ? FindThenPin(*SetEnabledNode) : FindThenPin(*BranchNode);
		if (Action.ActionType.Equals(TEXT("MaterialSlotVisibility"), ESearchCase::IgnoreCase))
		{
			AddMaterialSlotVisibilityApplyNodes(*EventGraph, Action, ApplyExec, NewEnabledValuePin, 3140, BaseY);
		}
		else if (Action.ActionType.Equals(TEXT("AttachedMeshVisibility"), ESearchCase::IgnoreCase))
		{
			AddAttachedMeshVisibilityApplyNodes(*EventGraph, ApplyExec, NewEnabledValuePin, 3140, BaseY);
		}

		AssetResult.Actions.Add(FString::Printf(TEXT("generated hotkey execution graph for action %s"), *Action.Id));
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
	AddSeed(Seeds, Plan.WidgetBlueprintPath);
	AddSeed(Seeds, Plan.SaveGameBlueprintPath);
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

	if (Plan.Actions.IsEmpty())
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

	FNteCharacterRuntimeActionAssetWriteResult WidgetResult;
	WidgetResult.AssetPath = Plan.WidgetBlueprintPath;
	WidgetResult.AssetKind = TEXT("WidgetBlueprint");
	if (UBlueprint* WidgetBlueprint = CreateOrLoadWidgetBlueprint(Plan.WidgetBlueprintPath, WidgetResult))
	{
		AddPlanVariablesToBlueprint(*WidgetBlueprint, Plan, AllActions, WidgetResult);
		CompileAndSaveBlueprint(*WidgetBlueprint, WidgetResult, Result);
	}
	AppendAssetResult(Result, MoveTemp(WidgetResult));

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
			if (HostPathUseCount.FindRef(Host.AnimBlueprintPath) == 1)
			{
				AddRuntimeExecutionGraphToAnimBlueprint(*HostBlueprint, HostActions, HostResult);
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
