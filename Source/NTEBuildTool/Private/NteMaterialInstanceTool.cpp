// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteMaterialInstanceTool.h"

#include "NteEditorAssetUtils.h"
#include "NteJsonFileUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "StaticParameterSet.h"
#include "UObject/SavePackage.h"

namespace NTEBuildTool::Material
{
using namespace NTEBuildTool::Editor;
using namespace NTEBuildTool::Json;

namespace
{
FMaterialParameterInfo MakeGlobalParameterInfo(const FString& ParameterName)
{
	return FMaterialParameterInfo(FName(*ParameterName), EMaterialParameterAssociation::GlobalParameter, INDEX_NONE);
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

bool EnsureAssetFolder(const FString& FolderPath, FString& OutError)
{
	if (!IsGameContentPath(FolderPath))
	{
		OutError = FString::Printf(TEXT("Invalid /Game folder path: %s"), *FolderPath);
		return false;
	}

	FString FolderFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(FolderPath, FolderFilename))
	{
		IFileManager::Get().MakeDirectory(*FolderFilename, true);
	}
	return true;
}

UMaterial* GetDefaultParentMaterial()
{
	return LoadObject<UMaterial>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
}

UMaterial* CreatePlaceholderParentMaterial(const FString& ParentMaterialPath, FString& OutError)
{
	if (!IsGamePackageName(ParentMaterialPath))
	{
		OutError = FString::Printf(TEXT("Cannot create placeholder parent outside /Game: %s"), *ParentMaterialPath);
		return nullptr;
	}

	const FString ParentFolder = FPackageName::GetLongPackagePath(ParentMaterialPath);
	if (!EnsureAssetFolder(ParentFolder, OutError))
	{
		return nullptr;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
	UObject* CreatedAsset = AssetToolsModule.Get().CreateAsset(
		FPackageName::GetShortName(ParentMaterialPath),
		ParentFolder,
		UMaterial::StaticClass(),
		Factory);

	UMaterial* Material = Cast<UMaterial>(CreatedAsset);
	if (!Material)
	{
		OutError = FString::Printf(TEXT("Could not create placeholder material: %s"), *ParentMaterialPath);
		return nullptr;
	}

	Material->MarkPackageDirty();
	if (!SaveAssetPackage(*Material, OutError))
	{
		return nullptr;
	}
	return Material;
}

UMaterialInstanceConstant* CreatePlaceholderParentMaterialInstance(const FString& ParentMaterialPath, FString& OutError)
{
	if (!IsGamePackageName(ParentMaterialPath))
	{
		OutError = FString::Printf(TEXT("Cannot create placeholder parent outside /Game: %s"), *ParentMaterialPath);
		return nullptr;
	}

	UMaterial* DefaultParent = GetDefaultParentMaterial();
	if (!DefaultParent)
	{
		OutError = TEXT("/Engine/EngineMaterials/DefaultMaterial could not be loaded for a placeholder material instance parent.");
		return nullptr;
	}

	const FString ParentFolder = FPackageName::GetLongPackagePath(ParentMaterialPath);
	if (!EnsureAssetFolder(ParentFolder, OutError))
	{
		return nullptr;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	Factory->InitialParent = DefaultParent;

	UObject* CreatedAsset = AssetToolsModule.Get().CreateAsset(
		FPackageName::GetShortName(ParentMaterialPath),
		ParentFolder,
		UMaterialInstanceConstant::StaticClass(),
		Factory);

	UMaterialInstanceConstant* MaterialInstance = Cast<UMaterialInstanceConstant>(CreatedAsset);
	if (!MaterialInstance)
	{
		OutError = FString::Printf(TEXT("Could not create placeholder material instance parent: %s"), *ParentMaterialPath);
		return nullptr;
	}

	FAssetRegistryModule::AssetCreated(MaterialInstance);
	MaterialInstance->MarkPackageDirty();
	if (!SaveAssetPackage(*MaterialInstance, OutError))
	{
		return nullptr;
	}
	return MaterialInstance;
}

bool ShouldCreatePlaceholderParentAsMaterialInstance(const FString& ParentMaterialPath, const FNteMaterialInstanceOptions& Options)
{
	const FString ParentAssetName = FPackageName::GetShortName(ParentMaterialPath);
	if (ParentAssetName.StartsWith(TEXT("MI_"), ESearchCase::IgnoreCase))
	{
		return true;
	}

	const FString SourceAssetName = FPaths::GetBaseFilename(Options.SourceMaterialJson);
	return SourceAssetName.StartsWith(TEXT("MI_"), ESearchCase::IgnoreCase);
}

UMaterialInterface* LoadOrCreateParentMaterial(const FString& ParentMaterialPath, const FNteMaterialInstanceOptions& Options, FNteMaterialInstanceCreateResult& OutResult, FString& OutError)
{
	if (!ParentMaterialPath.IsEmpty())
	{
		if (UMaterialInterface* ExistingParent = LoadAssetByPath<UMaterialInterface>(ParentMaterialPath))
		{
			return ExistingParent;
		}

		if (Options.bEnsureParentPlaceholder)
		{
			if (ShouldCreatePlaceholderParentAsMaterialInstance(ParentMaterialPath, Options))
			{
				if (UMaterialInstanceConstant* Placeholder = CreatePlaceholderParentMaterialInstance(ParentMaterialPath, OutError))
				{
					OutResult.bCreatedParentPlaceholder = true;
					OutResult.bCreatedMaterialProxy = true;
					return Placeholder;
				}
			}
			else if (UMaterial* Placeholder = CreatePlaceholderParentMaterial(ParentMaterialPath, OutError))
			{
				OutResult.bCreatedParentPlaceholder = true;
				OutResult.bCreatedMaterialProxy = true;
				return Placeholder;
			}
		}
	}

	UMaterial* DefaultParent = GetDefaultParentMaterial();
	if (!DefaultParent)
	{
		OutError = ParentMaterialPath.IsEmpty()
			? TEXT("No parent material was specified and /Engine/EngineMaterials/DefaultMaterial could not be loaded.")
			: FString::Printf(TEXT("Could not load or create parent material: %s"), *ParentMaterialPath);
		return nullptr;
	}

	return DefaultParent;
}

UMaterialInstanceConstant* LoadOrCreateMaterialInstance(const FString& OutputMaterialPath, UMaterialInterface& ParentMaterial, FString& OutError)
{
	if (!IsGamePackageName(OutputMaterialPath))
	{
		OutError = FString::Printf(TEXT("Invalid output material path: %s"), *OutputMaterialPath);
		return nullptr;
	}

	if (UMaterialInstanceConstant* Existing = LoadAssetByPath<UMaterialInstanceConstant>(OutputMaterialPath))
	{
		return Existing;
	}

	const FString OutputFolder = FPackageName::GetLongPackagePath(OutputMaterialPath);
	if (!EnsureAssetFolder(OutputFolder, OutError))
	{
		return nullptr;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	Factory->InitialParent = &ParentMaterial;

	UObject* CreatedAsset = AssetToolsModule.Get().CreateAsset(
		FPackageName::GetShortName(OutputMaterialPath),
		OutputFolder,
		UMaterialInstanceConstant::StaticClass(),
		Factory);

	UMaterialInstanceConstant* MaterialInstance = Cast<UMaterialInstanceConstant>(CreatedAsset);
	if (!MaterialInstance)
	{
		OutError = FString::Printf(TEXT("Could not create material instance: %s"), *OutputMaterialPath);
		return nullptr;
	}

	FAssetRegistryModule::AssetCreated(MaterialInstance);
	return MaterialInstance;
}

bool AssignMaterialToMeshSlot(const FString& MeshPath, const int32 SlotIndex, UMaterialInterface& Material, UObject*& OutMesh, FString& OutError)
{
	UObject* MeshAsset = LoadAnyAssetByPath(MeshPath);
	if (!MeshAsset)
	{
		OutError = FString::Printf(TEXT("Could not load target mesh for slot assignment: %s"), *MeshPath);
		return false;
	}

	if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(MeshAsset))
	{
		if (!SkeletalMesh->GetMaterials().IsValidIndex(SlotIndex))
		{
			OutError = FString::Printf(TEXT("Slot index %d is out of range for skeletal mesh: %s"), SlotIndex, *MeshPath);
			return false;
		}

		SkeletalMesh->Modify();
		TArray<FSkeletalMaterial> Materials = SkeletalMesh->GetMaterials();
		Materials[SlotIndex].MaterialInterface = &Material;
		SkeletalMesh->SetMaterials(Materials);
		SkeletalMesh->PostEditChange();
		SkeletalMesh->MarkPackageDirty();
		OutMesh = SkeletalMesh;
		return true;
	}

	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(MeshAsset))
	{
		TArray<FStaticMaterial>& Materials = StaticMesh->GetStaticMaterials();
		if (!Materials.IsValidIndex(SlotIndex))
		{
			OutError = FString::Printf(TEXT("Slot index %d is out of range for static mesh: %s"), SlotIndex, *MeshPath);
			return false;
		}

		StaticMesh->Modify();
		Materials[SlotIndex].MaterialInterface = &Material;
		StaticMesh->PostEditChange();
		StaticMesh->MarkPackageDirty();
		OutMesh = StaticMesh;
		return true;
	}

	OutError = FString::Printf(TEXT("Target asset is neither a SkeletalMesh nor a StaticMesh: %s (%s)"), *MeshPath, *MeshAsset->GetClass()->GetName());
	return false;
}

void ApplyTextureParameters(UMaterialInstanceConstant& MaterialInstance, const FJsonObject& Textures, FNteMaterialApplySummary& Summary)
{
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Textures.Values)
	{
		FString TexturePath = Entry.Value.IsValid() ? Entry.Value->AsString() : FString();
		TexturePath = NormalizeAssetPathForText(TexturePath);
		if (TexturePath.IsEmpty())
		{
			continue;
		}

		UTexture* Texture = LoadAssetByPath<UTexture>(TexturePath);
		if (!Texture)
		{
			Summary.MissingTextures.Add(TexturePath);
			continue;
		}

		MaterialInstance.SetTextureParameterValueEditorOnly(MakeGlobalParameterInfo(Entry.Key), Texture);
		++Summary.TextureOverrides;
	}
}

void AppendTextureParameters(const FJsonObject& Source, FJsonObject& Target)
{
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Source.Values)
	{
		if (!Entry.Value.IsValid())
		{
			continue;
		}

		Target.SetStringField(Entry.Key, Entry.Value->AsString());
	}
}

void ApplyScalarParameters(UMaterialInstanceConstant& MaterialInstance, const FJsonObject& Scalars, FNteMaterialApplySummary& Summary)
{
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Scalars.Values)
	{
		if (!Entry.Value.IsValid())
		{
			continue;
		}

		MaterialInstance.SetScalarParameterValueEditorOnly(MakeGlobalParameterInfo(Entry.Key), static_cast<float>(Entry.Value->AsNumber()));
		++Summary.ScalarOverrides;
	}
}

bool JsonObjectToLinearColor(const FJsonObject& Object, FLinearColor& OutColor)
{
	double R = 0.0;
	double G = 0.0;
	double B = 0.0;
	double A = 1.0;
	if (!Object.TryGetNumberField(TEXT("R"), R) || !Object.TryGetNumberField(TEXT("G"), G) || !Object.TryGetNumberField(TEXT("B"), B))
	{
		return false;
	}
	Object.TryGetNumberField(TEXT("A"), A);
	OutColor = FLinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B), static_cast<float>(A));
	return true;
}

void ApplyVectorParameters(UMaterialInstanceConstant& MaterialInstance, const FJsonObject& Vectors, FNteMaterialApplySummary& Summary)
{
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Vectors.Values)
	{
		if (!Entry.Value.IsValid() || Entry.Value->Type != EJson::Object)
		{
			Summary.InvalidVectors.Add(Entry.Key);
			continue;
		}

		FLinearColor Color;
		if (!JsonObjectToLinearColor(*Entry.Value->AsObject(), Color))
		{
			Summary.InvalidVectors.Add(Entry.Key);
			continue;
		}

		MaterialInstance.SetVectorParameterValueEditorOnly(MakeGlobalParameterInfo(Entry.Key), Color);
		++Summary.VectorOverrides;
	}
}

void ApplyStaticSwitchParameters(UMaterialInstanceConstant& MaterialInstance, const FJsonObject& Switches, FNteMaterialApplySummary& Summary)
{
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Switches.Values)
	{
		if (!Entry.Value.IsValid())
		{
			continue;
		}

		MaterialInstance.SetStaticSwitchParameterValueEditorOnly(MakeGlobalParameterInfo(Entry.Key), Entry.Value->AsBool());
		++Summary.StaticSwitchOverrides;
	}
}

void AddOverrideReportEntry(TArray<TSharedPtr<FJsonValue>>& Overrides, const FString& Type, const FString& ParameterName, const FString& Value, const FString& AssetPath = FString())
{
	const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
	Entry->SetStringField(TEXT("Type"), Type);

	const TSharedRef<FJsonObject> Parameter = MakeShared<FJsonObject>();
	Parameter->SetStringField(TEXT("Name"), ParameterName);
	Parameter->SetStringField(TEXT("Association"), TEXT("Global"));
	Parameter->SetNumberField(TEXT("Index"), INDEX_NONE);
	Entry->SetObjectField(TEXT("Parameter"), Parameter);

	Entry->SetStringField(TEXT("DisplayName"), ParameterName);
	Entry->SetStringField(TEXT("Value"), Value);
	if (!AssetPath.IsEmpty())
	{
		Entry->SetStringField(TEXT("Asset"), AssetPath);
		Entry->SetBoolField(TEXT("AssetLoads"), LoadAnyAssetByPath(AssetPath) != nullptr);
	}

	Overrides.Add(MakeShared<FJsonValueObject>(Entry));
}
}

FString DeriveParentMaterialPathFromFModelJson(const FString& SourceMaterialJson)
{
	FString Normalized = SourceMaterialJson;
	FPaths::NormalizeFilename(Normalized);

	const FString ContentMarker = TEXT("/Content/");
	const int32 ContentIndex = Normalized.Find(ContentMarker, ESearchCase::IgnoreCase, ESearchDir::FromStart);
	if (ContentIndex == INDEX_NONE)
	{
		return FString();
	}

	FString RelativePath = Normalized.Mid(ContentIndex + ContentMarker.Len());
	RelativePath.RemoveFromEnd(TEXT(".json"), ESearchCase::IgnoreCase);
	return TEXT("/Game/") + RelativePath;
}

FString MakeModMaterialNameFromFModelJson(const FString& SourceMaterialJson)
{
	return TEXT("MI_mod_") + FPaths::GetBaseFilename(SourceMaterialJson);
}

FString DeriveModMaterialFolderFromParentPath(const FString& ParentMaterialPath, const FString& FallbackPath)
{
	const FString ParentFolder = FPackageName::GetLongPackagePath(ParentMaterialPath);
	const TArray<FString> SourceMarkers = {
		TEXT("/ter/"),
		TEXT("/ter_new/"),
		TEXT("/materials/"),
		TEXT("/Materials/")
	};

	for (const FString& Marker : SourceMarkers)
	{
		const int32 MarkerIndex = ParentFolder.Find(Marker, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if (MarkerIndex != INDEX_NONE)
		{
			return ParentFolder.Left(MarkerIndex) / TEXT("mod/Materials");
		}
	}

	return IsGameContentPath(FallbackPath) ? FallbackPath : TEXT("/Game");
}

const FJsonObject* FindSourceMaterialParameterObject(const FJsonObject* SourceObject, const TCHAR* SectionName)
{
	if (!SourceObject)
	{
		return nullptr;
	}

	const TSharedPtr<FJsonObject>* Parameters = nullptr;
	if (SourceObject->TryGetObjectField(TEXT("Parameters"), Parameters) && Parameters && Parameters->IsValid())
	{
		const TSharedPtr<FJsonObject>* Section = nullptr;
		if ((*Parameters)->TryGetObjectField(SectionName, Section) && Section && Section->IsValid())
		{
			return Section->Get();
		}
	}

	const TSharedPtr<FJsonObject>* Section = nullptr;
	if (SourceObject->TryGetObjectField(SectionName, Section) && Section && Section->IsValid())
	{
		return Section->Get();
	}

	return nullptr;
}

TArray<FNteMaterialSourceTextureUsage> BuildSourceTextureUsage(const FJsonObject* SourceTextures)
{
	TArray<FNteMaterialSourceTextureUsage> Usage;
	TMap<FString, int32> UsageBySourceTexture;
	if (!SourceTextures)
	{
		return Usage;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : SourceTextures->Values)
	{
		if (!Entry.Value.IsValid())
		{
			continue;
		}

		const FString SourceTexturePath = NormalizeAssetPathForText(Entry.Value->AsString());
		if (SourceTexturePath.IsEmpty())
		{
			continue;
		}

		int32* ExistingIndex = UsageBySourceTexture.Find(SourceTexturePath);
		if (!ExistingIndex)
		{
			ExistingIndex = &UsageBySourceTexture.Add(SourceTexturePath, Usage.Num());
			FNteMaterialSourceTextureUsage& NewUsage = Usage.AddDefaulted_GetRef();
			NewUsage.SourceTexturePath = SourceTexturePath;
		}

		Usage[*ExistingIndex].ParameterNames.AddUnique(Entry.Key);
	}

	Usage.Sort([](const FNteMaterialSourceTextureUsage& A, const FNteMaterialSourceTextureUsage& B)
	{
		return A.SourceTexturePath < B.SourceTexturePath;
	});
	for (FNteMaterialSourceTextureUsage& Entry : Usage)
	{
		Entry.ParameterNames.Sort();
	}
	return Usage;
}

TSharedRef<FJsonObject> ExpandSourceTextureOverridesToParameters(
	const TArray<FNteMaterialSourceTextureUsage>& SourceTextureUsage,
	const FJsonObject* SourceTextureOverrides,
	int32& OutMatchedGroups,
	TArray<FString>* OutUnmatchedSourceTextures)
{
	OutMatchedGroups = 0;
	if (OutUnmatchedSourceTextures)
	{
		OutUnmatchedSourceTextures->Reset();
	}

	const TSharedRef<FJsonObject> ParameterOverrides = MakeShared<FJsonObject>();
	if (!SourceTextureOverrides)
	{
		return ParameterOverrides;
	}

	TMap<FString, FString> NormalizedOverrides;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& OverrideEntry : SourceTextureOverrides->Values)
	{
		if (!OverrideEntry.Value.IsValid())
		{
			continue;
		}

		const FString SourceTexturePath = NormalizeAssetPathForText(OverrideEntry.Key);
		const FString ReplacementTexturePath = NormalizeAssetPathForText(OverrideEntry.Value->AsString());
		if (!SourceTexturePath.IsEmpty() && !ReplacementTexturePath.IsEmpty())
		{
			NormalizedOverrides.Add(SourceTexturePath, ReplacementTexturePath);
		}
	}

	for (const FNteMaterialSourceTextureUsage& Usage : SourceTextureUsage)
	{
		const FString* ReplacementTexturePath = NormalizedOverrides.Find(Usage.SourceTexturePath);
		if (!ReplacementTexturePath)
		{
			continue;
		}

		++OutMatchedGroups;
		for (const FString& ParameterName : Usage.ParameterNames)
		{
			ParameterOverrides->SetStringField(ParameterName, *ReplacementTexturePath);
		}
	}

	if (OutUnmatchedSourceTextures)
	{
		for (const TPair<FString, FString>& OverrideEntry : NormalizedOverrides)
		{
			bool bMatched = false;
			for (const FNteMaterialSourceTextureUsage& Usage : SourceTextureUsage)
			{
				if (Usage.SourceTexturePath == OverrideEntry.Key)
				{
					bMatched = true;
					break;
				}
			}

			if (!bMatched)
			{
				OutUnmatchedSourceTextures->Add(OverrideEntry.Key);
			}
		}
		OutUnmatchedSourceTextures->Sort();
	}

	return ParameterOverrides;
}

bool CreateOrUpdateModMaterialInstance(
	const FNteMaterialInstanceOptions& Options,
	const FJsonObject* SourceTextures,
	const FJsonObject* SourceScalars,
	const FJsonObject* SourceColors,
	const FJsonObject* SourceSwitches,
	const FJsonObject* TextureOverrides,
	const FJsonObject* ScalarOverrides,
	const FJsonObject* VectorOverrides,
	const FJsonObject* StaticSwitchOverrides,
	FNteMaterialInstanceCreateResult& OutResult,
	FString& OutError)
{
	OutResult = FNteMaterialInstanceCreateResult();

	if (!IsGamePackageName(Options.OutputMaterialPath))
	{
		OutError = FString::Printf(TEXT("OutputMaterial must be a /Game package path: %s"), *Options.OutputMaterialPath);
		return false;
	}

	UMaterialInterface* ParentMaterial = LoadOrCreateParentMaterial(Options.ParentMaterialPath, Options, OutResult, OutError);
	if (!ParentMaterial)
	{
		return false;
	}

	UMaterialInstanceConstant* MaterialInstance = LoadOrCreateMaterialInstance(Options.OutputMaterialPath, *ParentMaterial, OutError);
	if (!MaterialInstance)
	{
		return false;
	}

	MaterialInstance->Modify();
	MaterialInstance->SetParentEditorOnly(ParentMaterial);
	if (Options.bResetForPakTextureOnly)
	{
		MaterialInstance->ClearParameterValuesEditorOnly();
	}
	else if (Options.bCopySourceParameters)
	{
		MaterialInstance->CopyMaterialUniformParametersEditorOnly(ParentMaterial, false);
	}

	if (Options.bCopySourceTextures && SourceTextures)
	{
		ApplyTextureParameters(*MaterialInstance, *SourceTextures, OutResult.ApplySummary);
	}
	if (TextureOverrides)
	{
		ApplyTextureParameters(*MaterialInstance, *TextureOverrides, OutResult.ApplySummary);
	}
	if (Options.bCopySourceParameters && SourceScalars)
	{
		ApplyScalarParameters(*MaterialInstance, *SourceScalars, OutResult.ApplySummary);
	}
	if (ScalarOverrides)
	{
		ApplyScalarParameters(*MaterialInstance, *ScalarOverrides, OutResult.ApplySummary);
	}
	if (Options.bCopySourceParameters && SourceColors)
	{
		ApplyVectorParameters(*MaterialInstance, *SourceColors, OutResult.ApplySummary);
	}
	if (VectorOverrides)
	{
		ApplyVectorParameters(*MaterialInstance, *VectorOverrides, OutResult.ApplySummary);
	}
	if (Options.bAllowStaticSwitchOverrides)
	{
		if (SourceSwitches && Options.bCopySourceParameters)
		{
			ApplyStaticSwitchParameters(*MaterialInstance, *SourceSwitches, OutResult.ApplySummary);
		}
		if (StaticSwitchOverrides)
		{
			ApplyStaticSwitchParameters(*MaterialInstance, *StaticSwitchOverrides, OutResult.ApplySummary);
		}
	}

	MaterialInstance->PostEditChange();
	MaterialInstance->MarkPackageDirty();
	OutResult.MaterialInstance = MaterialInstance;

	if (Options.bAssignToMeshSlot)
	{
		UObject* TargetMesh = nullptr;
		if (!AssignMaterialToMeshSlot(Options.MeshPath, Options.SlotIndex, *MaterialInstance, TargetMesh, OutError))
		{
			return false;
		}

		OutResult.TargetMesh = TargetMesh;
		OutResult.AssignedMaterialPath = Options.OutputMaterialPath;
	}

	if (!SaveAssetPackage(*MaterialInstance, OutError))
	{
		return false;
	}
	if (OutResult.TargetMesh && !SaveAssetPackage(*OutResult.TargetMesh, OutError))
	{
		return false;
	}

	return true;
}

bool ApplyModMaterialConfigFromFile(const FString& ConfigFilename, FNteMaterialConfigApplyResult& OutResult, FString& OutError)
{
	OutResult = FNteMaterialConfigApplyResult();

	TSharedPtr<FJsonObject> Config;
	if (!LoadJsonObjectFromFile(ConfigFilename, Config, OutError))
	{
		return false;
	}

	OutResult.SourceMaterialJson = GetStringAny(*Config, TEXT("SourceMaterialJson"), TEXT("sourceMaterialJson"), TEXT("FModelMaterialJson"));
	OutResult.OutputMaterialPath = NormalizeAssetPathForText(GetStringAny(*Config, TEXT("OutputMaterial"), TEXT("outputMaterial")));
	OutResult.ParentMaterialPath = NormalizeAssetPathForText(GetStringAny(*Config, TEXT("ParentMaterial"), TEXT("parentMaterial")));
	if (OutResult.ParentMaterialPath.IsEmpty() && !OutResult.SourceMaterialJson.IsEmpty())
	{
		OutResult.ParentMaterialPath = DeriveParentMaterialPathFromFModelJson(OutResult.SourceMaterialJson);
	}

	if (OutResult.OutputMaterialPath.IsEmpty())
	{
		FString OutputFolder = DeriveModMaterialFolderFromParentPath(OutResult.ParentMaterialPath, GetSelectedContentBrowserPath());
		OutResult.OutputMaterialPath = JoinAssetPath(OutputFolder, MakeModMaterialNameFromFModelJson(OutResult.SourceMaterialJson));
	}

	TSharedPtr<FJsonObject> SourceMaterial;
	if (!OutResult.SourceMaterialJson.IsEmpty() && !LoadJsonObjectFromFile(OutResult.SourceMaterialJson, SourceMaterial, OutError))
	{
		return false;
	}

	FNteMaterialInstanceOptions Options;
	Options.SourceMaterialJson = OutResult.SourceMaterialJson;
	Options.ParentMaterialPath = OutResult.ParentMaterialPath;
	Options.OutputMaterialPath = OutResult.OutputMaterialPath;
	Options.MeshPath = NormalizeAssetPathForText(GetStringAny(*Config, TEXT("Mesh"), TEXT("mesh")));
	GetIntAny(*Config, Options.SlotIndex, TEXT("Slot"), TEXT("slot"));
	GetBoolAny(*Config, Options.bAssignToMeshSlot, TEXT("AssignToMeshSlot"), TEXT("assignToMeshSlot"));
	Options.bAssignToMeshSlot = Options.bAssignToMeshSlot || (!Options.MeshPath.IsEmpty() && Options.SlotIndex != INDEX_NONE);
	GetBoolAny(*Config, Options.bCopySourceParameters, TEXT("CopySourceParameters"), TEXT("copySourceParameters"));
	GetBoolAny(*Config, Options.bCopySourceTextures, TEXT("CopySourceTextures"), TEXT("copySourceTextures"));
	GetBoolAny(*Config, Options.bResetForPakTextureOnly, TEXT("ResetForPakTextureOnly"), TEXT("resetForPakTextureOnly"));
	GetBoolAny(*Config, Options.bAllowStaticSwitchOverrides, TEXT("AllowStaticSwitchOverrides"), TEXT("allowStaticSwitchOverrides"));
	GetBoolAny(*Config, Options.bEnsureParentPlaceholder, TEXT("EnsureParentPlaceholder"), TEXT("ensureParentPlaceholder"));

	const TSharedPtr<FJsonObject>* TextureOverrides = nullptr;
	const TSharedPtr<FJsonObject>* SourceTextureOverrides = nullptr;
	const TSharedPtr<FJsonObject>* ScalarOverrides = nullptr;
	const TSharedPtr<FJsonObject>* VectorOverrides = nullptr;
	const TSharedPtr<FJsonObject>* StaticSwitchOverrides = nullptr;
	TryGetObjectAny(*Config, TextureOverrides, TEXT("TextureOverrides"), TEXT("textureOverrides"));
	TryGetObjectAny(*Config, SourceTextureOverrides, TEXT("SourceTextureOverrides"), TEXT("sourceTextureOverrides"));
	TryGetObjectAny(*Config, ScalarOverrides, TEXT("ScalarOverrides"), TEXT("scalarOverrides"));
	TryGetObjectAny(*Config, VectorOverrides, TEXT("VectorOverrides"), TEXT("vectorOverrides"), TEXT("ColorOverrides"));
	TryGetObjectAny(*Config, StaticSwitchOverrides, TEXT("StaticSwitchOverrides"), TEXT("staticSwitchOverrides"));

	const FJsonObject* SourceTextures = FindSourceMaterialParameterObject(SourceMaterial.Get(), TEXT("Textures"));
	const FJsonObject* SourceScalars = FindSourceMaterialParameterObject(SourceMaterial.Get(), TEXT("Scalars"));
	const FJsonObject* SourceColors = FindSourceMaterialParameterObject(SourceMaterial.Get(), TEXT("Colors"));
	const FJsonObject* SourceSwitches = FindSourceMaterialParameterObject(SourceMaterial.Get(), TEXT("Switches"));

	TSharedPtr<FJsonObject> ExpandedTextureOverrides;
	int32 MatchedSourceTextureGroups = 0;
	TArray<FString> UnmatchedSourceTextureOverrides;
	if (SourceTextureOverrides && SourceTextureOverrides->IsValid())
	{
		OutResult.SourceTextureUsage = BuildSourceTextureUsage(SourceTextures);
		ExpandedTextureOverrides = ExpandSourceTextureOverridesToParameters(
			OutResult.SourceTextureUsage,
			SourceTextureOverrides->Get(),
			MatchedSourceTextureGroups,
			&UnmatchedSourceTextureOverrides);
	}
	else
	{
		OutResult.SourceTextureUsage = BuildSourceTextureUsage(SourceTextures);
	}
	if (TextureOverrides && TextureOverrides->IsValid())
	{
		if (!ExpandedTextureOverrides.IsValid())
		{
			ExpandedTextureOverrides = MakeShared<FJsonObject>();
		}
		AppendTextureParameters(*TextureOverrides->Get(), *ExpandedTextureOverrides);
	}

	if (!CreateOrUpdateModMaterialInstance(
		Options,
		SourceTextures,
		SourceScalars,
		SourceColors,
		SourceSwitches,
		ExpandedTextureOverrides.IsValid() ? ExpandedTextureOverrides.Get() : nullptr,
		ScalarOverrides && ScalarOverrides->IsValid() ? ScalarOverrides->Get() : nullptr,
		VectorOverrides && VectorOverrides->IsValid() ? VectorOverrides->Get() : nullptr,
		StaticSwitchOverrides && StaticSwitchOverrides->IsValid() ? StaticSwitchOverrides->Get() : nullptr,
		OutResult.CreateResult,
		OutError))
	{
		return false;
	}
	OutResult.CreateResult.ApplySummary.SourceTextureOverrideGroups = MatchedSourceTextureGroups;
	OutResult.CreateResult.ApplySummary.UnmatchedSourceTextureOverrides = UnmatchedSourceTextureOverrides;

	return SaveMaterialInstanceOverrideReport(*OutResult.CreateResult.MaterialInstance, OutResult.ReportFilename, OutError);
}

bool SaveMaterialInstanceOverrideReport(UMaterialInstanceConstant& MaterialInstance, FString& OutFilename, FString& OutError)
{
	OutFilename = FPaths::ProjectSavedDir() / TEXT("NTEBuildTool/MaterialReports") / (MaterialInstance.GetName() + TEXT(".json"));
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("Format"), TEXT("NTE.MaterialInstanceOverrideReport"));
	Root->SetNumberField(TEXT("Version"), 1.0);
	Root->SetStringField(TEXT("MaterialInstance"), MaterialInstance.GetPackage()->GetName());
	Root->SetStringField(TEXT("MaterialInstanceObjectPath"), ToObjectPath(MaterialInstance.GetPackage()->GetName()));
	if (MaterialInstance.Parent)
	{
		Root->SetStringField(TEXT("Parent"), MaterialInstance.Parent->GetPackage()->GetName());
		Root->SetStringField(TEXT("ParentObjectPath"), MaterialInstance.Parent->GetPathName());
	}

	TArray<TSharedPtr<FJsonValue>> Overrides;
	for (const FTextureParameterValue& TextureParameter : MaterialInstance.TextureParameterValues)
	{
		const FString AssetPath = TextureParameter.ParameterValue ? TextureParameter.ParameterValue->GetPackage()->GetName() : FString();
		AddOverrideReportEntry(
			Overrides,
			TEXT("Texture"),
			TextureParameter.ParameterInfo.Name.ToString(),
			TextureParameter.ParameterValue ? TextureParameter.ParameterValue->GetPathName() : FString(),
			AssetPath);
	}
	for (const FScalarParameterValue& ScalarParameter : MaterialInstance.ScalarParameterValues)
	{
		AddOverrideReportEntry(
			Overrides,
			TEXT("Scalar"),
			ScalarParameter.ParameterInfo.Name.ToString(),
			FString::SanitizeFloat(ScalarParameter.ParameterValue));
	}
	for (const FVectorParameterValue& VectorParameter : MaterialInstance.VectorParameterValues)
	{
		AddOverrideReportEntry(
			Overrides,
			TEXT("Vector"),
			VectorParameter.ParameterInfo.Name.ToString(),
			VectorParameter.ParameterValue.ToString());
	}
	const FStaticParameterSet StaticParameters = MaterialInstance.GetStaticParameters();
	for (const FStaticSwitchParameter& StaticSwitchParameter : StaticParameters.StaticSwitchParameters)
	{
		AddOverrideReportEntry(
			Overrides,
			TEXT("StaticSwitch"),
			StaticSwitchParameter.ParameterInfo.Name.ToString(),
			StaticSwitchParameter.Value ? TEXT("true") : TEXT("false"));
	}
	Root->SetArrayField(TEXT("Overrides"), Overrides);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutFilename), true);
	return SaveJsonObjectToFile(Root, OutFilename, OutError);
}
}
