// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteAssetInspectionCommandlet.h"

#include "NTEBuildTool.h"
#include "NteEditorAssetUtils.h"
#include "NteJsonFileUtils.h"

#include "Animation/AnimBlueprint.h"
#include "Blueprint/UserWidget.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

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
	else if (const UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
	{
		Object->SetStringField(TEXT("GeneratedClass"), Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetPathName() : FString());
		Object->SetStringField(TEXT("ParentClass"), Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : FString());
	}
	else if (const UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Asset))
	{
		Object->SetStringField(TEXT("GeneratedClass"), AnimBlueprint->GeneratedClass ? AnimBlueprint->GeneratedClass->GetPathName() : FString());
		Object->SetStringField(TEXT("ParentClass"), AnimBlueprint->ParentClass ? AnimBlueprint->ParentClass->GetPathName() : FString());
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
