// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteAssetInspectionCommandlet.h"

#include "NTEBuildTool.h"
#include "HTPlayerAppearance.h"
#include "NteEditorAssetUtils.h"
#include "NteJsonFileUtils.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ContentWidget.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/SceneComponent.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBoxSlot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Variable.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "ReferenceSkeleton.h"
#include "StaticMeshResources.h"
#include "StaticParameterSet.h"
#include "Engine/Texture.h"
#include "WidgetBlueprint.h"

namespace
{
TSharedRef<FJsonObject> MakeVectorObject(const FVector& Value)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("X"), Value.X);
	Object->SetNumberField(TEXT("Y"), Value.Y);
	Object->SetNumberField(TEXT("Z"), Value.Z);
	return Object;
}

TSharedRef<FJsonObject> MakeRotatorObject(const FRotator& Value)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("Pitch"), Value.Pitch);
	Object->SetNumberField(TEXT("Yaw"), Value.Yaw);
	Object->SetNumberField(TEXT("Roll"), Value.Roll);
	return Object;
}

FString ObjectPackagePath(const UObject* Object)
{
	return Object ? Object->GetPackage()->GetName() : FString();
}

FString ObjectPath(const UObject* Object)
{
	return Object ? Object->GetPathName() : FString();
}

FString ClassPath(const UClass* Class)
{
	return Class ? Class->GetPathName() : FString();
}

TArray<FString> SplitAssetList(FString AssetsText)
{
	AssetsText.TrimStartAndEndInline();
	AssetsText.TrimQuotesInline();

	TArray<FString> Assets;
	AssetsText.ParseIntoArray(Assets, TEXT(","), true);
	for (FString& Asset : Assets)
	{
		Asset = NTEBuildTool::Editor::NormalizeAssetPathForText(Asset);
	}
	return Assets;
}

TArray<FString> ReadAssetListFile(const FString& AssetListFilename, FString& OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *AssetListFilename))
	{
		OutError = FString::Printf(TEXT("Could not read asset list file: %s"), *AssetListFilename);
		return {};
	}

	TArray<FString> Assets;
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (FJsonSerializer::Deserialize(Reader, JsonArray))
	{
		for (const TSharedPtr<FJsonValue>& Value : JsonArray)
		{
			if (Value.IsValid())
			{
				Assets.Add(NTEBuildTool::Editor::NormalizeAssetPathForText(Value->AsString()));
			}
		}
		return Assets;
	}

	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines, true);
	for (FString& Line : Lines)
	{
		Line.TrimStartAndEndInline();
		if (!Line.IsEmpty() && !Line.StartsWith(TEXT("#")))
		{
			Assets.Add(NTEBuildTool::Editor::NormalizeAssetPathForText(Line));
		}
	}
	return Assets;
}

TSharedRef<FJsonObject> MakeParameterInfoObject(const FMaterialParameterInfo& ParameterInfo)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("Name"), ParameterInfo.Name.ToString());
	Object->SetStringField(TEXT("Association"), TEXT("Global"));
	Object->SetNumberField(TEXT("Index"), ParameterInfo.Index);
	return Object;
}

void AddMaterialInstanceInfo(const UMaterialInstanceConstant& MaterialInstance, FJsonObject& Object)
{
	Object.SetStringField(TEXT("Parent"), MaterialInstance.Parent ? MaterialInstance.Parent->GetPackage()->GetName() : FString());
	Object.SetStringField(TEXT("ParentObjectPath"), MaterialInstance.Parent ? MaterialInstance.Parent->GetPathName() : FString());
	Object.SetStringField(TEXT("ParentClass"), MaterialInstance.Parent ? MaterialInstance.Parent->GetClass()->GetName() : FString());
	Object.SetBoolField(TEXT("ParentLoads"), MaterialInstance.Parent != nullptr);

	TArray<TSharedPtr<FJsonValue>> Textures;
	for (const FTextureParameterValue& TextureParameter : MaterialInstance.TextureParameterValues)
	{
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetObjectField(TEXT("Parameter"), MakeParameterInfoObject(TextureParameter.ParameterInfo));
		Entry->SetStringField(TEXT("Value"), TextureParameter.ParameterValue ? TextureParameter.ParameterValue->GetPackage()->GetName() : FString());
		Entry->SetStringField(TEXT("ObjectPath"), TextureParameter.ParameterValue ? TextureParameter.ParameterValue->GetPathName() : FString());
		Textures.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Object.SetArrayField(TEXT("TextureOverrides"), Textures);
	Object.SetNumberField(TEXT("ScalarOverrideCount"), MaterialInstance.ScalarParameterValues.Num());
	Object.SetNumberField(TEXT("VectorOverrideCount"), MaterialInstance.VectorParameterValues.Num());

	TArray<TSharedPtr<FJsonValue>> Scalars;
	for (const FScalarParameterValue& ScalarParameter : MaterialInstance.ScalarParameterValues)
	{
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetObjectField(TEXT("Parameter"), MakeParameterInfoObject(ScalarParameter.ParameterInfo));
		Entry->SetNumberField(TEXT("Value"), ScalarParameter.ParameterValue);
		Scalars.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Object.SetArrayField(TEXT("ScalarOverrides"), Scalars);

	TArray<TSharedPtr<FJsonValue>> Vectors;
	for (const FVectorParameterValue& VectorParameter : MaterialInstance.VectorParameterValues)
	{
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetObjectField(TEXT("Parameter"), MakeParameterInfoObject(VectorParameter.ParameterInfo));
		const FLinearColor& Value = VectorParameter.ParameterValue;
		Entry->SetStringField(TEXT("Value"), Value.ToString());
		Entry->SetNumberField(TEXT("R"), Value.R);
		Entry->SetNumberField(TEXT("G"), Value.G);
		Entry->SetNumberField(TEXT("B"), Value.B);
		Entry->SetNumberField(TEXT("A"), Value.A);
		Vectors.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Object.SetArrayField(TEXT("VectorOverrides"), Vectors);

	const FStaticParameterSet StaticParameters = MaterialInstance.GetStaticParameters();
	TArray<TSharedPtr<FJsonValue>> StaticSwitches;
	for (const FStaticSwitchParameter& StaticSwitchParameter : StaticParameters.StaticSwitchParameters)
	{
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetObjectField(TEXT("Parameter"), MakeParameterInfoObject(StaticSwitchParameter.ParameterInfo));
		Entry->SetBoolField(TEXT("Value"), StaticSwitchParameter.Value);
		Entry->SetBoolField(TEXT("Override"), StaticSwitchParameter.bOverride);
		Entry->SetStringField(TEXT("ExpressionGuid"), StaticSwitchParameter.ExpressionGUID.ToString(EGuidFormats::DigitsWithHyphens));
		StaticSwitches.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Object.SetNumberField(TEXT("StaticSwitchOverrideCount"), StaticSwitches.Num());
	Object.SetArrayField(TEXT("StaticSwitchOverrides"), StaticSwitches);
}

void AddBlueprintBinaryPatternInfo(const UBlueprint& Blueprint, FJsonObject& Object)
{
	FString PackageFilename;
	if (!FPackageName::TryConvertLongPackageNameToFilename(Blueprint.GetPackage()->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		Object.SetBoolField(TEXT("BinaryPatternReadable"), false);
		return;
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *PackageFilename))
	{
		Object.SetBoolField(TEXT("BinaryPatternReadable"), false);
		return;
	}

	Object.SetBoolField(TEXT("BinaryPatternReadable"), true);
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

	const TArray<const ANSICHAR*> Patterns = {
		"IsInputKeyDown",
		"ShowMaterialSection",
		"AddToViewport",
		"RemoveFromParent",
		"SetInputMode_GameAndUIEx",
		"SetInputMode_GameOnly",
		"Slash",
		"LeftControl",
		"RightControl",
		"Up",
		"Down",
		"NumPadEight",
		"NumPadTwo"
	};

	TArray<TSharedPtr<FJsonValue>> FoundPatterns;
	for (const ANSICHAR* Pattern : Patterns)
	{
		if (ContainsAscii(Pattern))
		{
			FoundPatterns.Add(MakeShared<FJsonValueString>(ANSI_TO_TCHAR(Pattern)));
		}
	}
	Object.SetArrayField(TEXT("BinaryPatterns"), FoundPatterns);
}

void AddGraphPinInfo(const UEdGraphPin& Pin, FJsonObject& Object)
{
	Object.SetStringField(TEXT("Name"), Pin.PinName.ToString());
	Object.SetStringField(TEXT("Direction"), Pin.Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
	Object.SetStringField(TEXT("Category"), Pin.PinType.PinCategory.ToString());
	Object.SetStringField(TEXT("SubCategory"), Pin.PinType.PinSubCategory.ToString());
	Object.SetStringField(TEXT("SubCategoryObject"), Pin.PinType.PinSubCategoryObject.IsValid() ? Pin.PinType.PinSubCategoryObject->GetPathName() : FString());
	Object.SetStringField(TEXT("DefaultValue"), Pin.DefaultValue);
	Object.SetStringField(TEXT("DefaultTextValue"), Pin.DefaultTextValue.ToString());
	Object.SetStringField(TEXT("DefaultObject"), Pin.DefaultObject ? Pin.DefaultObject->GetPathName() : FString());
	Object.SetBoolField(TEXT("Orphaned"), Pin.bOrphanedPin);
	Object.SetBoolField(TEXT("NotConnectable"), Pin.bNotConnectable);
	Object.SetNumberField(TEXT("LinkedToCount"), Pin.LinkedTo.Num());

	TArray<TSharedPtr<FJsonValue>> LinkedPins;
	for (const UEdGraphPin* LinkedPin : Pin.LinkedTo)
	{
		if (!LinkedPin)
		{
			continue;
		}

		const UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
		const TSharedRef<FJsonObject> LinkObject = MakeShared<FJsonObject>();
		LinkObject->SetStringField(TEXT("NodeName"), LinkedNode ? LinkedNode->GetName() : FString());
		LinkObject->SetStringField(TEXT("NodeClass"), LinkedNode ? LinkedNode->GetClass()->GetName() : FString());
		LinkObject->SetStringField(TEXT("NodeTitle"), LinkedNode ? LinkedNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString() : FString());
		LinkObject->SetStringField(TEXT("PinName"), LinkedPin->PinName.ToString());
		LinkedPins.Add(MakeShared<FJsonValueObject>(LinkObject));
	}
	Object.SetArrayField(TEXT("LinkedTo"), LinkedPins);
}

void AddGraphNodeInfo(const UEdGraphNode& Node, FJsonObject& Object)
{
	Object.SetStringField(TEXT("Name"), Node.GetName());
	Object.SetStringField(TEXT("Class"), Node.GetClass()->GetName());
	Object.SetStringField(TEXT("Title"), Node.GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	Object.SetStringField(TEXT("NodeGuid"), Node.NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
	Object.SetBoolField(TEXT("HasValidNodeGuid"), Node.NodeGuid.IsValid());

	if (const UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(&Node))
	{
		const UBlueprint* Blueprint = VariableNode->GetBlueprint();
		UClass* BlueprintClass = Blueprint ? Blueprint->GeneratedClass : nullptr;
		UClass* MemberParentClass = VariableNode->VariableReference.GetMemberParentClass(BlueprintClass);
		Object.SetStringField(TEXT("VariableName"), VariableNode->VariableReference.GetMemberName().ToString());
		Object.SetBoolField(TEXT("VariableIsSelfContext"), VariableNode->VariableReference.IsSelfContext());
		Object.SetStringField(TEXT("VariableMemberParentClass"), MemberParentClass ? MemberParentClass->GetPathName() : FString());
		Object.SetStringField(TEXT("VariableMemberParentGeneratedBy"), MemberParentClass && MemberParentClass->ClassGeneratedBy ? MemberParentClass->ClassGeneratedBy->GetPathName() : FString());
		if (FProperty* Property = VariableNode->VariableReference.ResolveMember<FProperty>(BlueprintClass))
		{
			Object.SetStringField(TEXT("VariableResolvedOwnerClass"), Property->GetOwnerClass() ? Property->GetOwnerClass()->GetPathName() : FString());
			Object.SetStringField(TEXT("VariableResolvedPropertyClass"), Property->GetClass()->GetName());
		}
	}
	if (const UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(&Node))
	{
		Object.SetStringField(TEXT("CastTargetType"), CastNode->TargetType ? CastNode->TargetType->GetPathName() : FString());
		Object.SetStringField(TEXT("CastTargetGeneratedBy"), CastNode->TargetType && CastNode->TargetType->ClassGeneratedBy ? CastNode->TargetType->ClassGeneratedBy->GetPathName() : FString());
	}
	if (const UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(&Node))
	{
		UClass* FunctionParentClass = CallFunctionNode->FunctionReference.GetMemberParentClass(CallFunctionNode->GetBlueprintClassFromNode());
		Object.SetStringField(TEXT("FunctionName"), CallFunctionNode->FunctionReference.GetMemberName().ToString());
		Object.SetStringField(TEXT("FunctionParentClass"), FunctionParentClass ? FunctionParentClass->GetPathName() : FString());
		Object.SetStringField(TEXT("FunctionParentGeneratedBy"), FunctionParentClass && FunctionParentClass->ClassGeneratedBy ? FunctionParentClass->ClassGeneratedBy->GetPathName() : FString());
	}

	TArray<TSharedPtr<FJsonValue>> Pins;
	for (const UEdGraphPin* Pin : Node.Pins)
	{
		if (!Pin)
		{
			continue;
		}

		const TSharedRef<FJsonObject> PinObject = MakeShared<FJsonObject>();
		AddGraphPinInfo(*Pin, *PinObject);
		Pins.Add(MakeShared<FJsonValueObject>(PinObject));
	}
	Object.SetArrayField(TEXT("Pins"), Pins);
}

void AddGraphInfo(const UEdGraph& Graph, FJsonObject& Object)
{
	Object.SetStringField(TEXT("Name"), Graph.GetName());
	Object.SetStringField(TEXT("Class"), Graph.GetClass()->GetName());
	Object.SetNumberField(TEXT("NodeCount"), Graph.Nodes.Num());

	TArray<TSharedPtr<FJsonValue>> Nodes;
	for (const UEdGraphNode* Node : Graph.Nodes)
	{
		if (!Node)
		{
			continue;
		}

		const TSharedRef<FJsonObject> NodeObject = MakeShared<FJsonObject>();
		AddGraphNodeInfo(*Node, *NodeObject);
		Nodes.Add(MakeShared<FJsonValueObject>(NodeObject));
	}
	Object.SetArrayField(TEXT("Nodes"), Nodes);
}

void AddBlueprintGraphInfo(const UBlueprint& Blueprint, FJsonObject& Object)
{
	TArray<TSharedPtr<FJsonValue>> Graphs;
	const auto AddGraphArray = [&Graphs](const FString& GraphType, const TArray<TObjectPtr<UEdGraph>>& SourceGraphs)
	{
		for (const UEdGraph* Graph : SourceGraphs)
		{
			if (!Graph)
			{
				continue;
			}

			const TSharedRef<FJsonObject> GraphObject = MakeShared<FJsonObject>();
			GraphObject->SetStringField(TEXT("GraphType"), GraphType);
			AddGraphInfo(*Graph, *GraphObject);
			Graphs.Add(MakeShared<FJsonValueObject>(GraphObject));
		}
	};

	AddGraphArray(TEXT("Ubergraph"), Blueprint.UbergraphPages);
	AddGraphArray(TEXT("Function"), Blueprint.FunctionGraphs);
	AddGraphArray(TEXT("Macro"), Blueprint.MacroGraphs);
	AddGraphArray(TEXT("DelegateSignature"), Blueprint.DelegateSignatureGraphs);
	Object.SetArrayField(TEXT("Graphs"), Graphs);
}

const UEdGraph* FindAnimGraph(const UAnimBlueprint& AnimBlueprint)
{
	for (const UEdGraph* Graph : AnimBlueprint.FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == FName(TEXT("AnimGraph")))
		{
			return Graph;
		}
	}
	return nullptr;
}

bool NodeClassIs(const UEdGraphNode* Node, const TCHAR* ClassName)
{
	return Node && Node->GetClass() && Node->GetClass()->GetName() == ClassName;
}

int32 CountNodesByClass(const UEdGraph& Graph, const TCHAR* ClassName)
{
	int32 Count = 0;
	for (const UEdGraphNode* Node : Graph.Nodes)
	{
		if (NodeClassIs(Node, ClassName))
		{
			++Count;
		}
	}
	return Count;
}

bool NodeLinksToClass(const UEdGraphNode& Node, const TCHAR* LinkedNodeClassName)
{
	for (const UEdGraphPin* Pin : Node.Pins)
	{
		if (!Pin)
		{
			continue;
		}

		for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			const UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
			if (NodeClassIs(LinkedNode, LinkedNodeClassName))
			{
				return true;
			}
		}
	}
	return false;
}

bool AnyNodeLinksToClass(const UEdGraph& Graph, const TCHAR* SourceNodeClassName, const TCHAR* LinkedNodeClassName)
{
	for (const UEdGraphNode* Node : Graph.Nodes)
	{
		if (NodeClassIs(Node, SourceNodeClassName) && NodeLinksToClass(*Node, LinkedNodeClassName))
		{
			return true;
		}
	}
	return false;
}

const FStructProperty* FindAnimGraphNodeStructProperty(const UEdGraphNode& Node)
{
	return CastField<FStructProperty>(Node.GetClass()->FindPropertyByName(TEXT("Node")));
}

FString ReadAnimNodeBoneReferenceName(const UEdGraphNode& Node, const TCHAR* PropertyName)
{
	const FStructProperty* NodeProperty = FindAnimGraphNodeStructProperty(Node);
	if (!NodeProperty || !NodeProperty->Struct)
	{
		return FString();
	}

	const void* NodeContainer = NodeProperty->ContainerPtrToValuePtr<void>(&Node);
	const FStructProperty* BoneReferenceProperty = CastField<FStructProperty>(NodeProperty->Struct->FindPropertyByName(PropertyName));
	if (!BoneReferenceProperty || !BoneReferenceProperty->Struct)
	{
		return FString();
	}

	const void* BoneReferenceContainer = BoneReferenceProperty->ContainerPtrToValuePtr<void>(NodeContainer);
	const FNameProperty* BoneNameProperty = CastField<FNameProperty>(BoneReferenceProperty->Struct->FindPropertyByName(TEXT("BoneName")));
	if (!BoneNameProperty)
	{
		return FString();
	}

	return BoneNameProperty->GetPropertyValue_InContainer(BoneReferenceContainer).ToString();
}

bool ReadAnimNodeBoolProperty(const UEdGraphNode& Node, const TCHAR* PropertyName, bool& OutValue)
{
	const FStructProperty* NodeProperty = FindAnimGraphNodeStructProperty(Node);
	if (!NodeProperty || !NodeProperty->Struct)
	{
		return false;
	}

	const void* NodeContainer = NodeProperty->ContainerPtrToValuePtr<void>(&Node);
	const FBoolProperty* BoolProperty = CastField<FBoolProperty>(NodeProperty->Struct->FindPropertyByName(PropertyName));
	if (!BoolProperty)
	{
		return false;
	}

	OutValue = BoolProperty->GetPropertyValue_InContainer(NodeContainer);
	return true;
}

const void* AnimNodeStructContainer(const UEdGraphNode& Node, UScriptStruct*& OutStruct)
{
	const FStructProperty* NodeProperty = FindAnimGraphNodeStructProperty(Node);
	OutStruct = NodeProperty ? NodeProperty->Struct : nullptr;
	return NodeProperty && OutStruct
		? NodeProperty->ContainerPtrToValuePtr<void>(&Node)
		: nullptr;
}

FString ReadObjectReferenceField(const void* Container, const UStruct* Struct, const TCHAR* PropertyName)
{
	if (!Container || !Struct)
	{
		return FString();
	}

	const FProperty* Property = Struct->FindPropertyByName(PropertyName);
	if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
	{
		const UObject* Value = ObjectProperty->GetObjectPropertyValue_InContainer(Container);
		return Value ? Value->GetPathName() : FString();
	}
	if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
	{
		const FSoftObjectPtr Value = SoftObjectProperty->GetPropertyValue_InContainer(Container);
		return Value.ToSoftObjectPath().ToString();
	}
	return FString();
}

bool ReadFloatField(const void* Container, const UStruct* Struct, const TCHAR* PropertyName, double& OutValue)
{
	if (!Container || !Struct)
	{
		return false;
	}
	const FProperty* Property = Struct->FindPropertyByName(PropertyName);
	if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
	{
		const void* ValueContainer = NumericProperty->ContainerPtrToValuePtr<void>(Container);
		if (NumericProperty->IsFloatingPoint())
		{
			OutValue = NumericProperty->GetFloatingPointPropertyValue(ValueContainer);
			return true;
		}
		if (NumericProperty->IsInteger())
		{
			OutValue = NumericProperty->GetSignedIntPropertyValue(ValueContainer);
			return true;
		}
	}
	return false;
}

bool ReadBoolField(const void* Container, const UStruct* Struct, const TCHAR* PropertyName, bool& OutValue)
{
	if (!Container || !Struct)
	{
		return false;
	}
	if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Struct->FindPropertyByName(PropertyName)))
	{
		OutValue = BoolProperty->GetPropertyValue_InContainer(Container);
		return true;
	}
	return false;
}

FString ReadBoneReferenceContainer(const void* Container, const UStruct* Struct, const TCHAR* PropertyName)
{
	if (!Container || !Struct)
	{
		return FString();
	}
	const FStructProperty* BoneProperty = CastField<FStructProperty>(Struct->FindPropertyByName(PropertyName));
	if (!BoneProperty || !BoneProperty->Struct)
	{
		return FString();
	}
	const void* BoneContainer = BoneProperty->ContainerPtrToValuePtr<void>(Container);
	if (const FNameProperty* BoneNameProperty = CastField<FNameProperty>(BoneProperty->Struct->FindPropertyByName(TEXT("BoneName"))))
	{
		return BoneNameProperty->GetPropertyValue_InContainer(BoneContainer).ToString();
	}
	return FString();
}

TArray<TSharedPtr<FJsonValue>> ReadBoneReferenceArray(const void* Container, const UStruct* Struct, const TCHAR* PropertyName)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	if (!Container || !Struct)
	{
		return Values;
	}
	const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Struct->FindPropertyByName(PropertyName));
	const FStructProperty* InnerStruct = ArrayProperty ? CastField<FStructProperty>(ArrayProperty->Inner) : nullptr;
	if (!ArrayProperty || !InnerStruct || !InnerStruct->Struct)
	{
		return Values;
	}
	FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Container));
	for (int32 Index = 0; Index < Helper.Num(); ++Index)
	{
		const void* Element = Helper.GetRawPtr(Index);
		if (const FNameProperty* BoneNameProperty = CastField<FNameProperty>(InnerStruct->Struct->FindPropertyByName(TEXT("BoneName"))))
		{
			Values.Add(MakeShared<FJsonValueString>(BoneNameProperty->GetPropertyValue_InContainer(Element).ToString()));
		}
		else if (const FStructProperty* RootBoneProperty = CastField<FStructProperty>(InnerStruct->Struct->FindPropertyByName(TEXT("RootBone"))))
		{
			const void* RootBoneContainer = RootBoneProperty->ContainerPtrToValuePtr<void>(Element);
			if (const FNameProperty* RootBoneNameProperty = CastField<FNameProperty>(RootBoneProperty->Struct->FindPropertyByName(TEXT("BoneName"))))
			{
				Values.Add(MakeShared<FJsonValueString>(RootBoneNameProperty->GetPropertyValue_InContainer(RootBoneContainer).ToString()));
			}
		}
	}
	return Values;
}

void AddKawaiiLimitArrayInfo(const void* Container, const UStruct* Struct, const TCHAR* PropertyName, FJsonObject& Object)
{
	if (!Container || !Struct)
	{
		return;
	}
	const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Struct->FindPropertyByName(PropertyName));
	if (!ArrayProperty)
	{
		return;
	}
	FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Container));
	Object.SetNumberField(FString::Printf(TEXT("%sCount"), PropertyName), Helper.Num());
	TArray<TSharedPtr<FJsonValue>> Entries;
	const FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProperty->Inner);
	if (InnerStruct && InnerStruct->Struct)
	{
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			const void* Element = Helper.GetRawPtr(Index);
			const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("DrivingBone"), ReadBoneReferenceContainer(Element, InnerStruct->Struct, TEXT("DrivingBone")));
			bool bEnable = false;
			if (ReadBoolField(Element, InnerStruct->Struct, TEXT("bEnable"), bEnable))
			{
				Entry->SetBoolField(TEXT("Enable"), bEnable);
			}
			double Radius = 0.0;
			if (ReadFloatField(Element, InnerStruct->Struct, TEXT("Radius"), Radius))
			{
				Entry->SetNumberField(TEXT("Radius"), Radius);
			}
			double Length = 0.0;
			if (ReadFloatField(Element, InnerStruct->Struct, TEXT("Length"), Length))
			{
				Entry->SetNumberField(TEXT("Length"), Length);
			}
			Entries.Add(MakeShared<FJsonValueObject>(Entry));
		}
	}
	Object.SetArrayField(FString::Printf(TEXT("%sEntries"), PropertyName), Entries);
}

void AddKawaiiNodeRuntimeInfo(const UEdGraphNode& Node, FJsonObject& Object)
{
	UScriptStruct* NodeStruct = nullptr;
	const void* NodeContainer = AnimNodeStructContainer(Node, NodeStruct);
	if (!NodeContainer || !NodeStruct)
	{
		Object.SetBoolField(TEXT("RuntimeNodePropertiesReadable"), false);
		return;
	}
	Object.SetBoolField(TEXT("RuntimeNodePropertiesReadable"), true);
	Object.SetStringField(TEXT("ExcludeBones"), FString());
	Object.SetArrayField(TEXT("ExcludeBoneNames"), ReadBoneReferenceArray(NodeContainer, NodeStruct, TEXT("ExcludeBones")));
	Object.SetArrayField(TEXT("AdditionalRootBoneNames"), ReadBoneReferenceArray(NodeContainer, NodeStruct, TEXT("AdditionalRootBones")));
	for (const TCHAR* PropertyName : {
		TEXT("LimitsDataAsset"), TEXT("PhysicsAssetForLimits"), TEXT("BoneConstraintsDataAsset")})
	{
		Object.SetStringField(PropertyName, ReadObjectReferenceField(NodeContainer, NodeStruct, PropertyName));
	}
	for (const TCHAR* PropertyName : {
		TEXT("bUseRelativeMove"), TEXT("bAllowWorldCollision"), TEXT("bOverrideCollisionParams"),
		TEXT("bIgnoreSelfComponent"), TEXT("bEnableWind"), TEXT("bUpdatePhysicsSettingsInGame")})
	{
		bool Value = false;
		if (ReadBoolField(NodeContainer, NodeStruct, PropertyName, Value))
		{
			Object.SetBoolField(PropertyName, Value);
		}
	}
	for (const TCHAR* PropertyName : {
		TEXT("SphericalLimits"), TEXT("CapsuleLimits"), TEXT("BoxLimits"), TEXT("PlanarLimits"),
		TEXT("BoneConstraints"), TEXT("MergedBoneConstraints")})
	{
		AddKawaiiLimitArrayInfo(NodeContainer, NodeStruct, PropertyName, Object);
	}
}

void AddKawaiiDataAssetInfo(const UObject& Asset, FJsonObject& Object)
{
	const UStruct* Struct = Asset.GetClass();
	const void* Container = &Asset;
	Object.SetStringField(TEXT("Skeleton"), ReadObjectReferenceField(Container, Struct, TEXT("Skeleton")));
	Object.SetStringField(TEXT("PreviewSkeleton"), ReadObjectReferenceField(Container, Struct, TEXT("PreviewSkeleton")));
	for (const TCHAR* PropertyName : {
		TEXT("SphericalLimits"), TEXT("CapsuleLimits"), TEXT("BoxLimits"), TEXT("PlanarLimits"),
		TEXT("BoneConstraintsData"), TEXT("BoneConstraints")})
	{
		AddKawaiiLimitArrayInfo(Container, Struct, PropertyName, Object);
	}
}

void AddAnimBlueprintGraphSummary(const UAnimBlueprint& AnimBlueprint, FJsonObject& Object)
{
	const UEdGraph* AnimGraph = FindAnimGraph(AnimBlueprint);
	const TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetBoolField(TEXT("HasAnimGraph"), AnimGraph != nullptr);

	if (!AnimGraph)
	{
		Object.SetObjectField(TEXT("AnimGraphSummary"), Summary);
		return;
	}

	constexpr const TCHAR* CopyPoseClass = TEXT("AnimGraphNode_CopyPoseFromMesh");
	constexpr const TCHAR* LocalToComponentClass = TEXT("AnimGraphNode_LocalToComponentSpace");
	constexpr const TCHAR* LocalRefPoseClass = TEXT("AnimGraphNode_LocalRefPose");
	constexpr const TCHAR* LayeredBoneBlendClass = TEXT("AnimGraphNode_LayeredBoneBlend");
	constexpr const TCHAR* KawaiiClass = TEXT("AnimGraphNode_KawaiiPhysics");
	constexpr const TCHAR* ComponentToLocalClass = TEXT("AnimGraphNode_ComponentToLocalSpace");
	constexpr const TCHAR* RootClass = TEXT("AnimGraphNode_Root");

	const int32 CopyPoseCount = CountNodesByClass(*AnimGraph, CopyPoseClass);
	const int32 LocalToComponentCount = CountNodesByClass(*AnimGraph, LocalToComponentClass);
	const int32 LocalRefPoseCount = CountNodesByClass(*AnimGraph, LocalRefPoseClass);
	const int32 LayeredBoneBlendCount = CountNodesByClass(*AnimGraph, LayeredBoneBlendClass);
	const int32 KawaiiCount = CountNodesByClass(*AnimGraph, KawaiiClass);
	const int32 ComponentToLocalCount = CountNodesByClass(*AnimGraph, ComponentToLocalClass);
	const int32 RootCount = CountNodesByClass(*AnimGraph, RootClass);
	const bool bCopyPoseLinkedToLocal = AnyNodeLinksToClass(*AnimGraph, CopyPoseClass, LocalToComponentClass);
	const bool bLinkedInputPoseLinkedToLayered = AnyNodeLinksToClass(*AnimGraph, TEXT("AnimGraphNode_LinkedInputPose"), LayeredBoneBlendClass);
	const bool bLocalRefPoseLinkedToLayered = AnyNodeLinksToClass(*AnimGraph, LocalRefPoseClass, LayeredBoneBlendClass);
	const bool bLayeredLinkedToLocalToComponent = AnyNodeLinksToClass(*AnimGraph, LayeredBoneBlendClass, LocalToComponentClass);
	const bool bLocalLinkedToKawaii = AnyNodeLinksToClass(*AnimGraph, LocalToComponentClass, KawaiiClass);
	const bool bKawaiiLinkedToComponent = AnyNodeLinksToClass(*AnimGraph, KawaiiClass, ComponentToLocalClass);
	const bool bComponentLinkedToRoot = AnyNodeLinksToClass(*AnimGraph, ComponentToLocalClass, RootClass);

	Summary->SetStringField(TEXT("AnimGraphName"), AnimGraph->GetName());
	Summary->SetNumberField(TEXT("NodeCount"), AnimGraph->Nodes.Num());
	Summary->SetNumberField(TEXT("CopyPoseFromMeshCount"), CopyPoseCount);
	Summary->SetNumberField(TEXT("LocalToComponentSpaceCount"), LocalToComponentCount);
	Summary->SetNumberField(TEXT("LocalRefPoseCount"), LocalRefPoseCount);
	Summary->SetNumberField(TEXT("LayeredBoneBlendCount"), LayeredBoneBlendCount);
	Summary->SetNumberField(TEXT("KawaiiPhysicsCount"), KawaiiCount);
	Summary->SetNumberField(TEXT("ComponentToLocalSpaceCount"), ComponentToLocalCount);
	Summary->SetNumberField(TEXT("RootCount"), RootCount);
	Summary->SetBoolField(TEXT("CopyPoseLinkedToLocalToComponent"), bCopyPoseLinkedToLocal);
	Summary->SetBoolField(TEXT("LinkedInputPoseLinkedToLayeredBoneBlend"), bLinkedInputPoseLinkedToLayered);
	Summary->SetBoolField(TEXT("LocalRefPoseLinkedToLayeredBoneBlend"), bLocalRefPoseLinkedToLayered);
	Summary->SetBoolField(TEXT("LayeredBoneBlendLinkedToLocalToComponent"), bLayeredLinkedToLocalToComponent);
	Summary->SetBoolField(TEXT("LocalToComponentLinkedToKawaii"), bLocalLinkedToKawaii);
	Summary->SetBoolField(TEXT("KawaiiLinkedToComponentToLocal"), bKawaiiLinkedToComponent);
	Summary->SetBoolField(TEXT("ComponentToLocalLinkedToRoot"), bComponentLinkedToRoot);
	Summary->SetBoolField(
		TEXT("HasExpectedAttachedKawaiiChain"),
		CopyPoseCount > 0
			&& LocalToComponentCount > 0
			&& KawaiiCount > 0
			&& ComponentToLocalCount > 0
			&& RootCount > 0
			&& bCopyPoseLinkedToLocal
			&& bLocalLinkedToKawaii
			&& bKawaiiLinkedToComponent
			&& bComponentLinkedToRoot);
	Summary->SetBoolField(
		TEXT("HasExpectedPostProcessKawaiiChain"),
		CopyPoseCount == 0
			&& LocalRefPoseCount == 1
			&& LayeredBoneBlendCount == 1
			&& LocalToComponentCount > 0
			&& KawaiiCount > 0
			&& ComponentToLocalCount > 0
			&& RootCount > 0
			&& bLinkedInputPoseLinkedToLayered
			&& bLocalRefPoseLinkedToLayered
			&& bLayeredLinkedToLocalToComponent
			&& bLocalLinkedToKawaii
			&& bKawaiiLinkedToComponent
			&& bComponentLinkedToRoot);

	TArray<TSharedPtr<FJsonValue>> CopyPoseNodes;
	TArray<TSharedPtr<FJsonValue>> KawaiiNodes;
	TArray<TSharedPtr<FJsonValue>> ReferencePoseBranchFilters;
	bool bReferencePoseBranchDepthsAllZero = true;
	for (const UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (NodeClassIs(Node, CopyPoseClass))
		{
			const TSharedRef<FJsonObject> CopyPoseObject = MakeShared<FJsonObject>();
			CopyPoseObject->SetStringField(TEXT("Name"), Node->GetName());
			CopyPoseObject->SetStringField(TEXT("Title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
			bool bUseAttachedParent = false;
			if (ReadAnimNodeBoolProperty(*Node, TEXT("bUseAttachedParent"), bUseAttachedParent))
			{
				CopyPoseObject->SetBoolField(TEXT("UseAttachedParent"), bUseAttachedParent);
			}
			CopyPoseNodes.Add(MakeShared<FJsonValueObject>(CopyPoseObject));
		}
		else if (NodeClassIs(Node, LayeredBoneBlendClass))
		{
			if (const UAnimGraphNode_LayeredBoneBlend* LayeredBlendNode = Cast<UAnimGraphNode_LayeredBoneBlend>(Node))
			{
				for (const FInputBlendPose& Layer : LayeredBlendNode->Node.LayerSetup)
				{
					for (const FBranchFilter& Filter : Layer.BranchFilters)
					{
						const TSharedRef<FJsonObject> FilterObject = MakeShared<FJsonObject>();
						FilterObject->SetStringField(TEXT("BoneName"), Filter.BoneName.ToString());
						FilterObject->SetNumberField(TEXT("BlendDepth"), Filter.BlendDepth);
						ReferencePoseBranchFilters.Add(MakeShared<FJsonValueObject>(FilterObject));
						bReferencePoseBranchDepthsAllZero &= Filter.BlendDepth == 0;
					}
				}
			}
		}
		else if (NodeClassIs(Node, KawaiiClass))
		{
			const TSharedRef<FJsonObject> KawaiiObject = MakeShared<FJsonObject>();
			KawaiiObject->SetStringField(TEXT("Name"), Node->GetName());
			KawaiiObject->SetStringField(TEXT("Title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
			KawaiiObject->SetStringField(TEXT("RootBone"), ReadAnimNodeBoneReferenceName(*Node, TEXT("RootBone")));
			KawaiiObject->SetBoolField(TEXT("LinkedToComponentToLocal"), NodeLinksToClass(*Node, ComponentToLocalClass));
			AddKawaiiNodeRuntimeInfo(*Node, *KawaiiObject);
			KawaiiNodes.Add(MakeShared<FJsonValueObject>(KawaiiObject));
		}
	}
	Summary->SetArrayField(TEXT("CopyPoseNodes"), CopyPoseNodes);
	Summary->SetArrayField(TEXT("KawaiiNodes"), KawaiiNodes);
	Summary->SetArrayField(TEXT("ReferencePoseBranchFilters"), ReferencePoseBranchFilters);
	Summary->SetBoolField(TEXT("ReferencePoseBranchDepthsAllZero"), bReferencePoseBranchDepthsAllZero);

	Object.SetObjectField(TEXT("AnimGraphSummary"), Summary);
}

TArray<TSharedPtr<FJsonValue>> NamesToJsonValues(const TArray<FName>& Names)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	for (const FName& Name : Names)
	{
		Result.Add(MakeShared<FJsonValueString>(Name.ToString()));
	}
	return Result;
}

TArray<TSharedPtr<FJsonValue>> SCSChildNamesToJsonValues(const USCS_Node& Node)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	for (const USCS_Node* ChildNode : Node.GetChildNodes())
	{
		if (ChildNode)
		{
			Result.Add(MakeShared<FJsonValueString>(ChildNode->GetVariableName().ToString()));
		}
	}
	return Result;
}

void AddSCSNodeInfo(const USCS_Node& Node, const TMap<const USCS_Node*, FString>& ParentVariableNames, FJsonObject& Object)
{
	const UActorComponent* ComponentTemplate = Node.ComponentTemplate;
	Object.SetStringField(TEXT("VariableName"), Node.GetVariableName().ToString());
	Object.SetStringField(TEXT("ParentVariableName"), ParentVariableNames.FindRef(&Node));
	Object.SetStringField(TEXT("ParentComponentOrVariableName"), Node.ParentComponentOrVariableName.ToString());
	Object.SetBoolField(TEXT("IsParentComponentNative"), Node.bIsParentComponentNative);
	Object.SetStringField(TEXT("AttachToName"), Node.AttachToName.ToString());
	Object.SetStringField(TEXT("ComponentTemplateName"), ComponentTemplate ? ComponentTemplate->GetName() : FString());
	Object.SetStringField(TEXT("ComponentClass"), ComponentTemplate ? ComponentTemplate->GetClass()->GetPathName() : FString());
	Object.SetArrayField(TEXT("Children"), SCSChildNamesToJsonValues(Node));

	if (!ComponentTemplate)
	{
		return;
	}

	Object.SetArrayField(TEXT("ComponentTags"), NamesToJsonValues(ComponentTemplate->ComponentTags));

	if (const USceneComponent* SceneComponent = Cast<USceneComponent>(ComponentTemplate))
	{
		Object.SetObjectField(TEXT("RelativeLocation"), MakeVectorObject(SceneComponent->GetRelativeLocation()));
		Object.SetObjectField(TEXT("RelativeRotation"), MakeRotatorObject(SceneComponent->GetRelativeRotation()));
		Object.SetObjectField(TEXT("RelativeScale3D"), MakeVectorObject(SceneComponent->GetRelativeScale3D()));
	}

	if (const USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ComponentTemplate))
	{
		Object.SetStringField(TEXT("SkeletalMesh"), ObjectPackagePath(SkeletalMeshComponent->GetSkeletalMeshAsset()));
		Object.SetStringField(TEXT("SkeletalMeshObjectPath"), ObjectPath(SkeletalMeshComponent->GetSkeletalMeshAsset()));
		Object.SetStringField(TEXT("AnimClass"), ClassPath(SkeletalMeshComponent->AnimClass.Get()));
		Object.SetStringField(TEXT("AnimationMode"), StaticEnum<EAnimationMode::Type>()->GetNameStringByValue(static_cast<int64>(SkeletalMeshComponent->GetAnimationMode())));
	}
}

void AddBlueprintSCSInfo(const UBlueprint& Blueprint, FJsonObject& Object)
{
	const USimpleConstructionScript* SimpleConstructionScript = Blueprint.SimpleConstructionScript;
	Object.SetBoolField(TEXT("HasSimpleConstructionScript"), SimpleConstructionScript != nullptr);
	if (!SimpleConstructionScript)
	{
		return;
	}

	TArray<USCS_Node*> AllNodes = SimpleConstructionScript->GetAllNodes();
	TMap<const USCS_Node*, FString> ParentVariableNames;
	for (const USCS_Node* Node : AllNodes)
	{
		if (!Node)
		{
			continue;
		}

		for (const USCS_Node* ChildNode : Node->GetChildNodes())
		{
			if (ChildNode)
			{
				ParentVariableNames.Add(ChildNode, Node->GetVariableName().ToString());
			}
		}
	}

	TArray<TSharedPtr<FJsonValue>> Nodes;
	for (const USCS_Node* Node : AllNodes)
	{
		if (!Node)
		{
			continue;
		}

		const TSharedRef<FJsonObject> NodeObject = MakeShared<FJsonObject>();
		AddSCSNodeInfo(*Node, ParentVariableNames, *NodeObject);
		Nodes.Add(MakeShared<FJsonValueObject>(NodeObject));
	}

	Object.SetNumberField(TEXT("SCSNodeCount"), Nodes.Num());
	Object.SetArrayField(TEXT("SimpleConstructionScriptNodes"), Nodes);
}

TSharedRef<FJsonObject> MakeWidgetInfoObject(const UWidget& Widget)
{
	const auto MarginToString = [](const FMargin& Margin)
	{
		return FString::Printf(TEXT("(Left=%f,Top=%f,Right=%f,Bottom=%f)"), Margin.Left, Margin.Top, Margin.Right, Margin.Bottom);
	};

	const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
	Entry->SetStringField(TEXT("Name"), Widget.GetName());
	Entry->SetStringField(TEXT("Class"), Widget.GetClass()->GetName());
	Entry->SetStringField(TEXT("Visibility"), StaticEnum<ESlateVisibility>()->GetNameStringByValue(static_cast<int64>(Widget.GetVisibility())));
	Entry->SetBoolField(TEXT("IsEnabled"), Widget.GetIsEnabled());
	Entry->SetBoolField(TEXT("IsVariable"), Widget.bIsVariable);
	Entry->SetStringField(TEXT("RenderTransformTranslation"), Widget.GetRenderTransform().Translation.ToString());
	Entry->SetStringField(TEXT("RenderTransformScale"), Widget.GetRenderTransform().Scale.ToString());
	Entry->SetStringField(TEXT("RenderTransformPivot"), Widget.GetRenderTransformPivot().ToString());

	const UPanelWidget* ParentWidget = Widget.GetParent();
	Entry->SetStringField(TEXT("Parent"), ParentWidget ? ParentWidget->GetName() : FString());
	Entry->SetStringField(TEXT("ParentClass"), ParentWidget ? ParentWidget->GetClass()->GetName() : FString());
	if (ParentWidget)
	{
		Entry->SetNumberField(TEXT("ParentChildIndex"), ParentWidget->GetChildIndex(&Widget));
	}

	if (const UPanelWidget* PanelWidget = Cast<UPanelWidget>(&Widget))
	{
		TArray<TSharedPtr<FJsonValue>> Children;
		for (int32 ChildIndex = 0; ChildIndex < PanelWidget->GetChildrenCount(); ++ChildIndex)
		{
			if (const UWidget* ChildWidget = PanelWidget->GetChildAt(ChildIndex))
			{
				Children.Add(MakeShared<FJsonValueString>(ChildWidget->GetName()));
			}
		}
		Entry->SetArrayField(TEXT("Children"), Children);
	}

	if (const UPanelSlot* Slot = Widget.Slot)
	{
		Entry->SetStringField(TEXT("SlotClass"), Slot->GetClass()->GetName());
		if (const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			Entry->SetStringField(TEXT("SlotPosition"), CanvasSlot->GetPosition().ToString());
			Entry->SetStringField(TEXT("SlotSize"), CanvasSlot->GetSize().ToString());
			Entry->SetStringField(TEXT("SlotOffsets"), MarginToString(CanvasSlot->GetOffsets()));
			Entry->SetStringField(TEXT("SlotAnchorsMinimum"), CanvasSlot->GetAnchors().Minimum.ToString());
			Entry->SetStringField(TEXT("SlotAnchorsMaximum"), CanvasSlot->GetAnchors().Maximum.ToString());
			Entry->SetStringField(TEXT("SlotAlignment"), CanvasSlot->GetAlignment().ToString());
			Entry->SetBoolField(TEXT("SlotAutoSize"), CanvasSlot->GetAutoSize());
			Entry->SetNumberField(TEXT("SlotZOrder"), CanvasSlot->GetZOrder());
		}
		else if (const UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(Slot))
		{
			Entry->SetStringField(TEXT("SlotPadding"), MarginToString(VerticalSlot->GetPadding()));
			Entry->SetStringField(TEXT("SlotSizeRule"), StaticEnum<ESlateSizeRule::Type>()->GetNameStringByValue(static_cast<int64>(VerticalSlot->GetSize().SizeRule)));
			Entry->SetNumberField(TEXT("SlotSizeValue"), VerticalSlot->GetSize().Value);
			Entry->SetStringField(TEXT("SlotHorizontalAlignment"), StaticEnum<EHorizontalAlignment>()->GetNameStringByValue(static_cast<int64>(VerticalSlot->GetHorizontalAlignment())));
			Entry->SetStringField(TEXT("SlotVerticalAlignment"), StaticEnum<EVerticalAlignment>()->GetNameStringByValue(static_cast<int64>(VerticalSlot->GetVerticalAlignment())));
		}
		else if (const UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(Slot))
		{
			Entry->SetStringField(TEXT("SlotPadding"), MarginToString(HorizontalSlot->GetPadding()));
			Entry->SetStringField(TEXT("SlotSizeRule"), StaticEnum<ESlateSizeRule::Type>()->GetNameStringByValue(static_cast<int64>(HorizontalSlot->GetSize().SizeRule)));
			Entry->SetNumberField(TEXT("SlotSizeValue"), HorizontalSlot->GetSize().Value);
			Entry->SetStringField(TEXT("SlotHorizontalAlignment"), StaticEnum<EHorizontalAlignment>()->GetNameStringByValue(static_cast<int64>(HorizontalSlot->GetHorizontalAlignment())));
			Entry->SetStringField(TEXT("SlotVerticalAlignment"), StaticEnum<EVerticalAlignment>()->GetNameStringByValue(static_cast<int64>(HorizontalSlot->GetVerticalAlignment())));
		}
		else if (const UScrollBoxSlot* ScrollBoxSlot = Cast<UScrollBoxSlot>(Slot))
		{
			Entry->SetStringField(TEXT("SlotPadding"), MarginToString(ScrollBoxSlot->GetPadding()));
			Entry->SetStringField(TEXT("SlotHorizontalAlignment"), StaticEnum<EHorizontalAlignment>()->GetNameStringByValue(static_cast<int64>(ScrollBoxSlot->GetHorizontalAlignment())));
			Entry->SetStringField(TEXT("SlotVerticalAlignment"), StaticEnum<EVerticalAlignment>()->GetNameStringByValue(static_cast<int64>(ScrollBoxSlot->GetVerticalAlignment())));
		}
		else if (const USizeBoxSlot* SizeBoxSlot = Cast<USizeBoxSlot>(Slot))
		{
			Entry->SetStringField(TEXT("SlotPadding"), MarginToString(SizeBoxSlot->GetPadding()));
			Entry->SetStringField(TEXT("SlotHorizontalAlignment"), StaticEnum<EHorizontalAlignment>()->GetNameStringByValue(static_cast<int64>(SizeBoxSlot->GetHorizontalAlignment())));
			Entry->SetStringField(TEXT("SlotVerticalAlignment"), StaticEnum<EVerticalAlignment>()->GetNameStringByValue(static_cast<int64>(SizeBoxSlot->GetVerticalAlignment())));
		}
	}

	if (const UTextBlock* TextBlock = Cast<UTextBlock>(&Widget))
	{
		Entry->SetStringField(TEXT("Text"), TextBlock->GetText().ToString());
		Entry->SetStringField(TEXT("ColorAndOpacity"), TextBlock->GetColorAndOpacity().GetSpecifiedColor().ToString());
	}

	if (const UContentWidget* ContentWidget = Cast<UContentWidget>(&Widget))
	{
		const UWidget* Content = ContentWidget->GetContent();
		Entry->SetStringField(TEXT("Content"), Content ? Content->GetName() : FString());
		Entry->SetStringField(TEXT("ContentClass"), Content ? Content->GetClass()->GetName() : FString());
	}

	if (const UButton* Button = Cast<UButton>(&Widget))
	{
		Entry->SetStringField(TEXT("BackgroundColor"), Button->GetBackgroundColor().ToString());
		Entry->SetStringField(TEXT("ColorAndOpacity"), Button->GetColorAndOpacity().ToString());
		Entry->SetBoolField(TEXT("IsFocusable"), Button->GetIsFocusable());
	}

	return Entry;
}

void AddWidgetBlueprintInfo(const UWidgetBlueprint& WidgetBlueprint, FJsonObject& Object)
{
	const UWidgetTree* WidgetTree = WidgetBlueprint.WidgetTree;
	Object.SetBoolField(TEXT("HasWidgetTree"), WidgetTree != nullptr);
	if (!WidgetTree)
	{
		return;
	}

	Object.SetStringField(TEXT("WidgetTreePath"), WidgetTree->GetPathName());
	Object.SetStringField(TEXT("RootWidget"), WidgetTree->RootWidget ? WidgetTree->RootWidget->GetName() : FString());
	Object.SetStringField(TEXT("RootWidgetClass"), WidgetTree->RootWidget ? WidgetTree->RootWidget->GetClass()->GetName() : FString());

	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);
	TArray<TSharedPtr<FJsonValue>> WidgetValues;
	for (const UWidget* Widget : Widgets)
	{
		if (Widget)
		{
			WidgetValues.Add(MakeShared<FJsonValueObject>(MakeWidgetInfoObject(*Widget)));
		}
	}
	Object.SetArrayField(TEXT("Widgets"), WidgetValues);
}

void AddSkeletalMeshInfo(const USkeletalMesh& SkeletalMesh, FJsonObject& Object)
{
	const FReferenceSkeleton& ReferenceSkeleton = SkeletalMesh.GetRefSkeleton();
	TArray<TSharedPtr<FJsonValue>> ReferenceBones;
	for (int32 BoneIndex = 0; BoneIndex < ReferenceSkeleton.GetNum(); ++BoneIndex)
	{
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("Index"), BoneIndex);
		Entry->SetStringField(TEXT("Name"), ReferenceSkeleton.GetBoneName(BoneIndex).ToString());
		Entry->SetNumberField(TEXT("ParentIndex"), ReferenceSkeleton.GetParentIndex(BoneIndex));
		ReferenceBones.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Object.SetArrayField(TEXT("ReferenceSkeleton"), ReferenceBones);

	TArray<TSharedPtr<FJsonValue>> Materials;
	const TArray<FSkeletalMaterial>& SkeletalMaterials = SkeletalMesh.GetMaterials();
	for (int32 Index = 0; Index < SkeletalMaterials.Num(); ++Index)
	{
		const FSkeletalMaterial& Material = SkeletalMaterials[Index];
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("Index"), Index);
		Entry->SetStringField(TEXT("SlotName"), Material.MaterialSlotName.ToString());
#if WITH_EDITORONLY_DATA
		Entry->SetStringField(TEXT("ImportedSlotName"), Material.ImportedMaterialSlotName.ToString());
#endif
		Entry->SetStringField(TEXT("Material"), Material.MaterialInterface ? Material.MaterialInterface->GetPackage()->GetName() : FString());
		Entry->SetStringField(TEXT("MaterialObjectPath"), Material.MaterialInterface ? Material.MaterialInterface->GetPathName() : FString());
		Entry->SetStringField(TEXT("MaterialClass"), Material.MaterialInterface ? Material.MaterialInterface->GetClass()->GetName() : FString());
		Materials.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Object.SetArrayField(TEXT("Materials"), Materials);

	const UClass* PostProcessClass = SkeletalMesh.GetPostProcessAnimBlueprint();
	Object.SetStringField(TEXT("PostProcessAnimBlueprintClass"), PostProcessClass ? PostProcessClass->GetPathName() : FString());

	TArray<TSharedPtr<FJsonValue>> LodSections;
	if (const FSkeletalMeshRenderData* RenderData = SkeletalMesh.GetResourceForRendering())
	{
		for (int32 LodIndex = 0; LodIndex < RenderData->LODRenderData.Num(); ++LodIndex)
		{
			const FSkeletalMeshLODRenderData& LodData = RenderData->LODRenderData[LodIndex];
			const FSkinWeightVertexBuffer* SkinWeights = LodData.GetSkinWeightVertexBuffer();
			for (int32 SectionIndex = 0; SectionIndex < LodData.RenderSections.Num(); ++SectionIndex)
			{
				const FSkelMeshRenderSection& Section = LodData.RenderSections[SectionIndex];
				const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
				Entry->SetNumberField(TEXT("LodIndex"), LodIndex);
				Entry->SetNumberField(TEXT("SectionIndex"), SectionIndex);
				Entry->SetNumberField(TEXT("MaterialIndex"), Section.MaterialIndex);
				Entry->SetNumberField(TEXT("NumTriangles"), Section.NumTriangles);
				Entry->SetNumberField(TEXT("BaseVertexIndex"), Section.BaseVertexIndex);
				Entry->SetNumberField(TEXT("NumVertices"), Section.NumVertices);
				Entry->SetNumberField(TEXT("MaxBoneInfluences"), Section.MaxBoneInfluences);

				TArray<TSharedPtr<FJsonValue>> BoneMap;
				for (int32 LocalBoneIndex = 0; LocalBoneIndex < Section.BoneMap.Num(); ++LocalBoneIndex)
				{
					const int32 SkeletonBoneIndex = Section.BoneMap[LocalBoneIndex];
					const TSharedRef<FJsonObject> BoneEntry = MakeShared<FJsonObject>();
					BoneEntry->SetNumberField(TEXT("LocalIndex"), LocalBoneIndex);
					BoneEntry->SetNumberField(TEXT("SkeletonIndex"), SkeletonBoneIndex);
					BoneEntry->SetStringField(
						TEXT("Name"),
						ReferenceSkeleton.IsValidIndex(SkeletonBoneIndex)
							? ReferenceSkeleton.GetBoneName(SkeletonBoneIndex).ToString()
							: FString());
					BoneMap.Add(MakeShared<FJsonValueObject>(BoneEntry));
				}
				Entry->SetArrayField(TEXT("BoneMap"), BoneMap);

				TMap<int32, int32> WeightedBoneInfluenceCounts;
				int32 InvalidSkinWeightReferenceCount = 0;
				if (SkinWeights)
				{
					const uint32 VertexEnd = Section.BaseVertexIndex + Section.NumVertices;
					for (uint32 VertexIndex = Section.BaseVertexIndex; VertexIndex < VertexEnd; ++VertexIndex)
					{
						uint32 VertexWeightOffset = 0;
						uint32 VertexInfluenceCount = 0;
						SkinWeights->GetVertexInfluenceOffsetCount(VertexIndex, VertexWeightOffset, VertexInfluenceCount);
						for (uint32 InfluenceIndex = 0; InfluenceIndex < VertexInfluenceCount; ++InfluenceIndex)
						{
							if (SkinWeights->GetBoneWeight(VertexIndex, InfluenceIndex) == 0)
							{
								continue;
							}

							const int32 LocalBoneIndex = static_cast<int32>(SkinWeights->GetBoneIndex(VertexIndex, InfluenceIndex));
							if (!Section.BoneMap.IsValidIndex(LocalBoneIndex))
							{
								++InvalidSkinWeightReferenceCount;
								continue;
							}

							const int32 SkeletonBoneIndex = Section.BoneMap[LocalBoneIndex];
							if (!ReferenceSkeleton.IsValidIndex(SkeletonBoneIndex))
							{
								++InvalidSkinWeightReferenceCount;
								continue;
							}
							++WeightedBoneInfluenceCounts.FindOrAdd(SkeletonBoneIndex);
						}
					}
				}

				TArray<int32> WeightedBoneIndices;
				WeightedBoneInfluenceCounts.GetKeys(WeightedBoneIndices);
				WeightedBoneIndices.Sort();
				TArray<TSharedPtr<FJsonValue>> WeightedBones;
				for (const int32 SkeletonBoneIndex : WeightedBoneIndices)
				{
					const TSharedRef<FJsonObject> BoneEntry = MakeShared<FJsonObject>();
					BoneEntry->SetNumberField(TEXT("SkeletonIndex"), SkeletonBoneIndex);
					BoneEntry->SetStringField(TEXT("Name"), ReferenceSkeleton.GetBoneName(SkeletonBoneIndex).ToString());
					BoneEntry->SetNumberField(TEXT("InfluenceCount"), WeightedBoneInfluenceCounts[SkeletonBoneIndex]);
					WeightedBones.Add(MakeShared<FJsonValueObject>(BoneEntry));
				}
				Entry->SetArrayField(TEXT("WeightedBones"), WeightedBones);
				Entry->SetNumberField(TEXT("InvalidSkinWeightReferenceCount"), InvalidSkinWeightReferenceCount);
				LodSections.Add(MakeShared<FJsonValueObject>(Entry));
			}
		}
	}
	Object.SetArrayField(TEXT("LodSections"), LodSections);
}

void AddStaticMeshInfo(const UStaticMesh& StaticMesh, FJsonObject& Object)
{
	TArray<TSharedPtr<FJsonValue>> Materials;
	const TArray<FStaticMaterial>& StaticMaterials = StaticMesh.GetStaticMaterials();
	for (int32 Index = 0; Index < StaticMaterials.Num(); ++Index)
	{
		const FStaticMaterial& Material = StaticMaterials[Index];
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("Index"), Index);
		Entry->SetStringField(TEXT("SlotName"), Material.MaterialSlotName.ToString());
#if WITH_EDITORONLY_DATA
		Entry->SetStringField(TEXT("ImportedSlotName"), Material.ImportedMaterialSlotName.ToString());
#endif
		Entry->SetStringField(TEXT("Material"), Material.MaterialInterface ? Material.MaterialInterface->GetPackage()->GetName() : FString());
		Entry->SetStringField(TEXT("MaterialObjectPath"), Material.MaterialInterface ? Material.MaterialInterface->GetPathName() : FString());
		Entry->SetStringField(TEXT("MaterialClass"), Material.MaterialInterface ? Material.MaterialInterface->GetClass()->GetName() : FString());
		Materials.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Object.SetArrayField(TEXT("Materials"), Materials);

	TArray<TSharedPtr<FJsonValue>> LodSections;
	if (const FStaticMeshRenderData* RenderData = StaticMesh.GetRenderData())
	{
		for (int32 LodIndex = 0; LodIndex < RenderData->LODResources.Num(); ++LodIndex)
		{
			const FStaticMeshLODResources& LodResources = RenderData->LODResources[LodIndex];
			for (int32 SectionIndex = 0; SectionIndex < LodResources.Sections.Num(); ++SectionIndex)
			{
				const FStaticMeshSection& Section = LodResources.Sections[SectionIndex];
				const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
				Entry->SetNumberField(TEXT("LodIndex"), LodIndex);
				Entry->SetNumberField(TEXT("SectionIndex"), SectionIndex);
				Entry->SetNumberField(TEXT("MaterialIndex"), Section.MaterialIndex);
				Entry->SetNumberField(TEXT("FirstIndex"), Section.FirstIndex);
				Entry->SetNumberField(TEXT("NumTriangles"), Section.NumTriangles);
				Entry->SetNumberField(TEXT("MinVertexIndex"), Section.MinVertexIndex);
				Entry->SetNumberField(TEXT("MaxVertexIndex"), Section.MaxVertexIndex);
				LodSections.Add(MakeShared<FJsonValueObject>(Entry));
			}
		}
	}
	Object.SetArrayField(TEXT("LodSections"), LodSections);
}

TSharedRef<FJsonObject> MakeFashionMeshDataObject(const FCharacterMeshData& MeshData)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("CharacterMesh"), ObjectPackagePath(MeshData.CharacterMesh.Get()));
	Object->SetStringField(TEXT("CharacterMeshObjectPath"), ObjectPath(MeshData.CharacterMesh.Get()));
	Object->SetStringField(TEXT("AnimInstanceClass"), ClassPath(Cast<UClass>(MeshData.AnimInstance.Get())));
	return Object;
}

TSharedRef<FJsonObject> MakeFashionAttachedMeshDataObject(const FAttachedMeshData& MeshData)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("CharacterMesh"), ObjectPackagePath(MeshData.CharacterMesh.Get()));
	Object->SetStringField(TEXT("CharacterMeshObjectPath"), ObjectPath(MeshData.CharacterMesh.Get()));
	Object->SetStringField(TEXT("AnimInstanceClass"), ClassPath(Cast<UClass>(MeshData.AnimInstance.Get())));
	Object->SetStringField(TEXT("MobileAnimInstanceClass"), ClassPath(Cast<UClass>(MeshData.MobileAnimInstance.Get())));
	Object->SetStringField(TEXT("SocketName"), MeshData.SocketName.ToString());
	Object->SetArrayField(TEXT("MeshComponentOwnedTags"), NTEBuildTool::Json::StringArrayToJsonValues([&MeshData]()
	{
		TArray<FString> Tags;
		for (const FName& Tag : MeshData.MeshComponentOwnedTags)
		{
			Tags.Add(Tag.ToString());
		}
		return Tags;
	}()));
	Object->SetObjectField(TEXT("RelativeLocation"), MakeVectorObject(MeshData.RelativeLocation));
	Object->SetObjectField(TEXT("RelativeRotation"), MakeRotatorObject(MeshData.RelativeRotation));
	Object->SetObjectField(TEXT("RelativeScale3D"), MakeVectorObject(MeshData.RelativeScale3D));
	return Object;
}

void AddHTPlayerAppearanceInfo(const UHTPlayerAppearance& Appearance, FJsonObject& Object)
{
	Object.SetObjectField(TEXT("FashionMeshData"), MakeFashionMeshDataObject(Appearance.FashionMeshData));
	Object.SetNumberField(TEXT("CapsuleHalfHeight"), Appearance.CapsuleHalfHeight);
	Object.SetNumberField(TEXT("CapsuleRadius"), Appearance.CapsuleRadius);
	Object.SetObjectField(TEXT("RelativeLocation"), MakeVectorObject(Appearance.RelativeLocation));
	Object.SetNumberField(TEXT("FPSCameraCapsuleTopOffset"), Appearance.FPSCameraCapsuleTopOffset);
	Object.SetStringField(TEXT("UltraSkillSequence"), ObjectPath(Appearance.UltraSkillSequence.Get()));

	TArray<TSharedPtr<FJsonValue>> AttachedMeshes;
	for (const FAttachedMeshData& AttachedMesh : Appearance.ArrayFashionAttachedMeshData)
	{
		AttachedMeshes.Add(MakeShared<FJsonValueObject>(MakeFashionAttachedMeshDataObject(AttachedMesh)));
	}
	Object.SetArrayField(TEXT("ArrayFashionAttachedMeshData"), AttachedMeshes);
	Object.SetNumberField(TEXT("AttachedMeshCount"), Appearance.ArrayFashionAttachedMeshData.Num());
}

TSharedRef<FJsonObject> InspectAsset(const FString& AssetPath)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("Path"), AssetPath);

	UObject* Asset = NTEBuildTool::Editor::LoadAnyAssetByPath(AssetPath);
	Object->SetBoolField(TEXT("Loads"), Asset != nullptr);
	if (!Asset)
	{
		return Object;
	}

	Object->SetStringField(TEXT("Class"), Asset->GetClass()->GetName());
	Object->SetStringField(TEXT("ObjectPath"), Asset->GetPathName());
	if (const UHTPlayerAppearance* Appearance = Cast<UHTPlayerAppearance>(Asset))
	{
		AddHTPlayerAppearanceInfo(*Appearance, *Object);
	}
	else if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Asset))
	{
		AddSkeletalMeshInfo(*SkeletalMesh, *Object);
	}
	else if (const UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))
	{
		AddStaticMeshInfo(*StaticMesh, *Object);
	}
	else if (const UMaterialInstanceConstant* MaterialInstance = Cast<UMaterialInstanceConstant>(Asset))
	{
		AddMaterialInstanceInfo(*MaterialInstance, *Object);
	}
	else if (const UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Asset))
	{
		Object->SetStringField(TEXT("GeneratedClass"), AnimBlueprint->GeneratedClass ? AnimBlueprint->GeneratedClass->GetPathName() : FString());
		Object->SetStringField(TEXT("ParentClass"), AnimBlueprint->ParentClass ? AnimBlueprint->ParentClass->GetPathName() : FString());
		AddBlueprintBinaryPatternInfo(*AnimBlueprint, *Object);
		AddBlueprintGraphInfo(*AnimBlueprint, *Object);
		AddAnimBlueprintGraphSummary(*AnimBlueprint, *Object);
	}
	else if (Asset->GetClass()->GetName().Contains(TEXT("KawaiiPhysicsLimitsDataAsset"))
		|| Asset->GetClass()->GetName().Contains(TEXT("KawaiiPhysicsBoneConstraintsDataAsset")))
	{
		AddKawaiiDataAssetInfo(*Asset, *Object);
	}
	else if (const UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Asset))
	{
		Object->SetStringField(TEXT("GeneratedClass"), WidgetBlueprint->GeneratedClass ? WidgetBlueprint->GeneratedClass->GetPathName() : FString());
		Object->SetStringField(TEXT("ParentClass"), WidgetBlueprint->ParentClass ? WidgetBlueprint->ParentClass->GetPathName() : FString());
		AddBlueprintBinaryPatternInfo(*WidgetBlueprint, *Object);
		AddBlueprintGraphInfo(*WidgetBlueprint, *Object);
		AddWidgetBlueprintInfo(*WidgetBlueprint, *Object);
	}
	else if (const UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
	{
		Object->SetStringField(TEXT("GeneratedClass"), Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetPathName() : FString());
		Object->SetStringField(TEXT("ParentClass"), Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : FString());
		AddBlueprintBinaryPatternInfo(*Blueprint, *Object);
		AddBlueprintGraphInfo(*Blueprint, *Object);
		AddBlueprintSCSInfo(*Blueprint, *Object);
	}

	return Object;
}
}

UNteAssetInspectionCommandlet::UNteAssetInspectionCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
	UseCommandletResultAsExitCode = true;
	HelpDescription = TEXT("Inspects selected NTE assets and writes a JSON report.");
	HelpUsage = TEXT("UnrealEditor-Cmd.exe <Project>.uproject -run=NteAssetInspection -Assets=/Game/A,/Game/B -AssetList=<txt-or-json> -Output=<json>");
}

int32 UNteAssetInspectionCommandlet::Main(const FString& Params)
{
	FString Error;
	FString AssetsText;
	FString AssetListFilename;
	FParse::Value(*Params, TEXT("Assets="), AssetsText);
	FParse::Value(*Params, TEXT("AssetList="), AssetListFilename);
	FPaths::NormalizeFilename(AssetListFilename);

	TArray<FString> Assets = !AssetListFilename.IsEmpty()
		? ReadAssetListFile(AssetListFilename, Error)
		: SplitAssetList(AssetsText);
	if (!Error.IsEmpty())
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("%s"), *Error);
		return 1;
	}
	if (Assets.IsEmpty())
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Missing -Assets=/Game/A,/Game/B or -AssetList=<txt-or-json>."));
		return 1;
	}

	FString OutputFilename;
	if (!FParse::Value(*Params, TEXT("Output="), OutputFilename) || OutputFilename.IsEmpty())
	{
		OutputFilename = FPaths::ProjectSavedDir() / TEXT("NTEBuildTool/AssetInspection.json");
	}
	FPaths::NormalizeFilename(OutputFilename);

	TArray<TSharedPtr<FJsonValue>> AssetObjects;
	for (const FString& AssetPath : Assets)
	{
		AssetObjects.Add(MakeShared<FJsonValueObject>(InspectAsset(AssetPath)));
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("Format"), TEXT("NTE.AssetInspection"));
	Root->SetNumberField(TEXT("Version"), 1.0);
	Root->SetArrayField(TEXT("Assets"), AssetObjects);

	if (!NTEBuildTool::Json::SaveJsonObjectToFile(Root, OutputFilename, Error))
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Could not write report: %s"), *Error);
		return 2;
	}

	UE_LOG(LogNTEBuildTool, Display, TEXT("Asset inspection report: %s"), *OutputFilename);
	return 0;
}
