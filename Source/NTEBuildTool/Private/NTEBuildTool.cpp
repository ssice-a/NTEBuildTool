// Copyright (c) 2026 NTEBuildTool contributors.

#include "NTEBuildTool.h"

#include "FModelPhysicsAssetImporter.h"

#include "ContentBrowserModule.h"
#include "DesktopPlatformModule.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "IContentBrowserSingleton.h"
#include "IDesktopPlatform.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "ScopedTransaction.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "NTEBuildTool"

DEFINE_LOG_CATEGORY_STATIC(LogNTEBuildTool, Log, All);

namespace
{
void ShowError(const FText& Message)
{
	UE_LOG(LogNTEBuildTool, Error, TEXT("%s"), *Message.ToString());
	FMessageDialog::Open(EAppMsgType::Ok, Message, LOCTEXT("NTEBuildToolErrorTitle", "NTE Build Tool"));
}

void ShowInfo(const FText& Message)
{
	UE_LOG(LogNTEBuildTool, Display, TEXT("%s"), *Message.ToString());
	FMessageDialog::Open(EAppMsgType::Ok, Message, LOCTEXT("NTEBuildToolInfoTitle", "NTE Build Tool"));
}

USkeletalMesh* GetSingleSelectedSkeletalMesh()
{
	TArray<FAssetData> SelectedAssets;
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	USkeletalMesh* SelectedSkeletalMesh = nullptr;
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(AssetData.GetAsset()))
		{
			if (SelectedSkeletalMesh)
			{
				return nullptr;
			}

			SelectedSkeletalMesh = SkeletalMesh;
		}
	}

	return SelectedSkeletalMesh;
}

bool ChooseJsonFile(FString& OutFilename)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		ShowError(LOCTEXT("NoDesktopPlatform", "Desktop file dialog is unavailable."));
		return false;
	}

	TArray<FString> OpenFilenames;
	const void* ParentWindowHandle = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)
		: nullptr;

	const bool bOpened = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		LOCTEXT("ChoosePhysicsAssetJson", "Choose FModel PhysicsAsset JSON").ToString(),
		FPaths::ProjectDir(),
		TEXT("player_051_female_skin_PhysicsAsset.json"),
		TEXT("JSON files (*.json)|*.json"),
		EFileDialogFlags::None,
		OpenFilenames);

	if (!bOpened || OpenFilenames.IsEmpty())
	{
		return false;
	}

	OutFilename = OpenFilenames[0];
	return true;
}
}

void FNTEBuildToolModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FNTEBuildToolModule::RegisterMenus));
}

void FNTEBuildToolModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

IMPLEMENT_MODULE(FNTEBuildToolModule, NTEBuildTool)

void FNTEBuildToolModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
	FToolMenuSection& Section = Menu->AddSection(TEXT("NTEBuildTool"), LOCTEXT("NTEBuildToolSection", "NTE Build Tool"));
	Section.AddEntry(FToolMenuEntry::InitMenuEntry(
		TEXT("NTEBuildTool_ImportFModelPhysicsAsset"),
		LOCTEXT("ImportFModelPhysicsAssetLabel", "Import FModel PhysicsAsset JSON"),
		LOCTEXT("ImportFModelPhysicsAssetTooltip", "Analyze a FModel PhysicsAsset JSON, create a matching PhysicsAsset, and assign it to the selected SkeletalMesh."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FNTEBuildToolModule::ImportFModelPhysicsAssetJson))));
}

void FNTEBuildToolModule::ImportFModelPhysicsAssetJson()
{
	USkeletalMesh* SelectedSkeletalMesh = GetSingleSelectedSkeletalMesh();
	if (!SelectedSkeletalMesh)
	{
		ShowError(LOCTEXT("NoSkeletalMeshSelected", "Select exactly one SkeletalMesh in the Content Browser before importing."));
		return;
	}

	FString JsonFilename;
	if (!ChooseJsonFile(JsonFilename))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("ImportFModelPhysicsAssetTransaction", "Import FModel PhysicsAsset JSON"));
	SelectedSkeletalMesh->Modify();

	FString Error;
	NTEBuildTool::FFModelPhysicsAssetImportSummary Summary;
	UPhysicsAsset* PhysicsAsset = NTEBuildTool::FFModelPhysicsAssetImporter::ImportFromJsonFile(*SelectedSkeletalMesh, JsonFilename, Summary, Error);
	if (!PhysicsAsset)
	{
		ShowError(FText::FromString(Error));
		return;
	}

	SelectedSkeletalMesh->SetPhysicsAsset(PhysicsAsset);
	SelectedSkeletalMesh->PostEditChange();
	SelectedSkeletalMesh->MarkPackageDirty();

	TArray<UObject*> ObjectsToSync;
	ObjectsToSync.Add(PhysicsAsset);
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync);

	ShowInfo(FText::Format(
		LOCTEXT("ImportedPhysicsAsset", "Analyzed JSON and imported {0} bodies, {1} constraints, and {2} disabled collision pairs into {3}. It is assigned to {4}. Save the dirty assets after checking it."),
		FText::AsNumber(Summary.BodyCount),
		FText::AsNumber(Summary.ConstraintCount),
		FText::AsNumber(Summary.DisabledCollisionPairCount),
		FText::FromString(PhysicsAsset->GetName()),
		FText::FromString(SelectedSkeletalMesh->GetName())));
}

#undef LOCTEXT_NAMESPACE
