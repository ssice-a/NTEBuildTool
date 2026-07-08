// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteAssetInspectionCommandlet.h"

#include "NTEBuildTool.h"
#include "NteEditorAssetUtils.h"
#include "NteJsonFileUtils.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Blueprint/UserWidget.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "StaticMeshResources.h"
#include "StaticParameterSet.h"
#include "Engine/Texture.h"

namespace
{
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
		"BP_NTE_ModToggleController",
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
	Object.SetBoolField(TEXT("LooksLikeStandardPostProcessTemplateRuntime"), ContainsAscii("IsInputKeyDown") && ContainsAscii("ShowMaterialSection"));
}

void AddGraphPinInfo(const UEdGraphPin& Pin, FJsonObject& Object)
{
	Object.SetStringField(TEXT("Name"), Pin.PinName.ToString());
	Object.SetStringField(TEXT("Direction"), Pin.Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
	Object.SetStringField(TEXT("Category"), Pin.PinType.PinCategory.ToString());
	Object.SetStringField(TEXT("SubCategory"), Pin.PinType.PinSubCategory.ToString());
	Object.SetStringField(TEXT("DefaultValue"), Pin.DefaultValue);
	Object.SetStringField(TEXT("DefaultTextValue"), Pin.DefaultTextValue.ToString());
	Object.SetStringField(TEXT("DefaultObject"), Pin.DefaultObject ? Pin.DefaultObject->GetPathName() : FString());
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

void AddSkeletalMeshInfo(const USkeletalMesh& SkeletalMesh, FJsonObject& Object)
{
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
			for (int32 SectionIndex = 0; SectionIndex < LodData.RenderSections.Num(); ++SectionIndex)
			{
				const FSkelMeshRenderSection& Section = LodData.RenderSections[SectionIndex];
				const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
				Entry->SetNumberField(TEXT("LodIndex"), LodIndex);
				Entry->SetNumberField(TEXT("SectionIndex"), SectionIndex);
				Entry->SetNumberField(TEXT("MaterialIndex"), Section.MaterialIndex);
				Entry->SetNumberField(TEXT("NumTriangles"), Section.NumTriangles);
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
	if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Asset))
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
	}
	else if (const UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
	{
		Object->SetStringField(TEXT("GeneratedClass"), Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetPathName() : FString());
		Object->SetStringField(TEXT("ParentClass"), Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : FString());
		AddBlueprintBinaryPatternInfo(*Blueprint, *Object);
		AddBlueprintGraphInfo(*Blueprint, *Object);
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
