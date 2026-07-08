// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteTextureImportCommandlet.h"

#include "NTEBuildTool.h"
#include "NteEditorAssetUtils.h"

#include "AssetToolsModule.h"
#include "AutomatedAssetImportData.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "TextureResource.h"
#include "UObject/SavePackage.h"

namespace
{
TextureCompressionSettings ParseCompression(const FString& Value)
{
	if (Value.Equals(TEXT("BC7"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("TC_BC7"), ESearchCase::IgnoreCase))
	{
		return TC_BC7;
	}
	if (Value.Equals(TEXT("Normal"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("Normalmap"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("TC_Normalmap"), ESearchCase::IgnoreCase))
	{
		return TC_Normalmap;
	}
	return TC_Default;
}

TextureGroup ParseTextureGroup(const FString& Value)
{
	if (Value.Equals(TEXT("Character"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("TEXTUREGROUP_Character"), ESearchCase::IgnoreCase))
	{
		return TEXTUREGROUP_Character;
	}
	return TEXTUREGROUP_World;
}

bool SaveImportedAsset(UObject& Asset, FString& OutError)
{
	UPackage* Package = Asset.GetPackage();
	if (!Package)
	{
		OutError = FString::Printf(TEXT("Imported asset has no package: %s"), *Asset.GetName());
		return false;
	}

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, &Asset, *PackageFilename, SaveArgs))
	{
		OutError = FString::Printf(TEXT("Could not save imported asset: %s"), *PackageFilename);
		return false;
	}

	return true;
}
}

UNteTextureImportCommandlet::UNteTextureImportCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
	UseCommandletResultAsExitCode = true;
	HelpDescription = TEXT("Imports a texture from disk into a /Game package path and applies common texture settings.");
	HelpUsage = TEXT("UnrealEditor-Cmd.exe <Project>.uproject -run=NteTextureImport -Source=<png> -Dest=/Game/Folder [-Name=AssetName] [-SRGB=true] [-Compression=BC7|Normal] [-LODGroup=Character]");
}

int32 UNteTextureImportCommandlet::Main(const FString& Params)
{
	FString SourceFilename;
	FString DestinationPath;
	FString AssetName;
	FString CompressionName = TEXT("BC7");
	FString LODGroupName = TEXT("Character");
	bool bSRGB = true;

	FParse::Value(*Params, TEXT("Source="), SourceFilename);
	FParse::Value(*Params, TEXT("Dest="), DestinationPath);
	FParse::Value(*Params, TEXT("Name="), AssetName);
	FParse::Value(*Params, TEXT("Compression="), CompressionName);
	FParse::Value(*Params, TEXT("LODGroup="), LODGroupName);
	FParse::Bool(*Params, TEXT("SRGB="), bSRGB);

	FPaths::NormalizeFilename(SourceFilename);
	DestinationPath = NTEBuildTool::Editor::NormalizeAssetPathForText(DestinationPath);
	if (DestinationPath.Contains(TEXT(".")))
	{
		DestinationPath = FPackageName::GetLongPackagePath(DestinationPath);
	}

	if (SourceFilename.IsEmpty() || !FPaths::FileExists(SourceFilename))
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Source texture does not exist: %s"), *SourceFilename);
		return 1;
	}
	if (!NTEBuildTool::Editor::IsGameContentPath(DestinationPath))
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Destination must be a /Game path: %s"), *DestinationPath);
		return 2;
	}

	if (AssetName.IsEmpty())
	{
		AssetName = FPaths::GetBaseFilename(SourceFilename);
	}
	const FString OutputAssetPath = NTEBuildTool::Editor::JoinAssetPath(DestinationPath, AssetName);

	UAutomatedAssetImportData* ImportData = NewObject<UAutomatedAssetImportData>();
	ImportData->Filenames.Add(SourceFilename);
	ImportData->DestinationPath = DestinationPath;
	ImportData->bReplaceExisting = true;
	ImportData->bSkipReadOnly = false;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	AssetToolsModule.Get().ImportAssetsAutomated(ImportData);

	UTexture2D* Texture = NTEBuildTool::Editor::LoadAssetByPath<UTexture2D>(OutputAssetPath);
	if (!Texture)
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Import did not produce expected texture: %s"), *OutputAssetPath);
		return 3;
	}

	Texture->Modify();
	Texture->SRGB = bSRGB;
	Texture->CompressionSettings = ParseCompression(CompressionName);
	Texture->LODGroup = ParseTextureGroup(LODGroupName);
	Texture->MipGenSettings = TMGS_FromTextureGroup;
	Texture->NeverStream = false;
	Texture->PostEditChange();
	Texture->MarkPackageDirty();

	FString Error;
	if (!SaveImportedAsset(*Texture, Error))
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("%s"), *Error);
		return 4;
	}

	UE_LOG(LogNTEBuildTool, Display, TEXT("Imported texture: %s"), *OutputAssetPath);
	return 0;
}
