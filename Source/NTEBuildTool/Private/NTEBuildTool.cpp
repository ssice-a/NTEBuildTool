// Copyright (c) 2026 NTEBuildTool contributors.

#include "NTEBuildTool.h"

#include "FModelPhysicsAssetImporter.h"
#include "NteEditorAssetUtils.h"
#include "NteMaterialInstanceTool.h"
#include "NteMeshToggleConfig.h"
#include "NteModPackageJob.h"
#include "NteNotificationUtils.h"

#include "ContentBrowserModule.h"
#include "Engine/SkeletalMesh.h"
#include "IContentBrowserSingleton.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "ScopedTransaction.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "NTEBuildTool"

DEFINE_LOG_CATEGORY(LogNTEBuildTool);

namespace
{
bool ChooseJsonFile(FString& OutFilename)
{
	return NTEBuildTool::Editor::ChooseJsonFileWithTitle(
		LOCTEXT("ChoosePhysicsAssetJson", "Choose FModel PhysicsAsset JSON"),
		TEXT("player_051_female_skin_PhysicsAsset.json"),
		OutFilename);
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
	Section.AddEntry(FToolMenuEntry::InitMenuEntry(
		TEXT("NTEBuildTool_CreateModMaterialInstanceFromRecipe"),
		LOCTEXT("CreateModMaterialInstanceFromRecipeLabel", "Create Material Instance From Recipe JSON"),
		LOCTEXT("CreateModMaterialInstanceFromRecipeTooltip", "Choose a material recipe JSON and create or update the mod MaterialInstanceConstant."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FNTEBuildToolModule::CreateModMaterialInstanceFromConfig))));
	Section.AddEntry(FToolMenuEntry::InitMenuEntry(
		TEXT("NTEBuildTool_CreateMeshToggleUiSetup"),
		LOCTEXT("CreateMeshToggleUiSetupLabel", "Manage Mesh Toggle Setup"),
		LOCTEXT("CreateMeshToggleUiSetupTooltip", "Create or update hotkey/UI material-slot visibility assets for the selected SkeletalMesh."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FNTEBuildToolModule::CreateMeshToggleUiSetup))));
	Section.AddEntry(FToolMenuEntry::InitMenuEntry(
		TEXT("NTEBuildTool_BuildSelectedAssetsModPackage"),
		LOCTEXT("BuildSelectedAssetsModPackageLabel", "Build Mod Package"),
		LOCTEXT("BuildSelectedAssetsModPackageTooltip", "Create a package job from selected assets or folders and launch the cook/package pipeline."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FNTEBuildToolModule::BuildSelectedAssetsModPackage))));
	Section.AddEntry(FToolMenuEntry::InitMenuEntry(
		TEXT("NTEBuildTool_BuildModPackageFromJobJson"),
		LOCTEXT("BuildModPackageFromJobJsonLabel", "Build Mod Package From Job JSON"),
		LOCTEXT("BuildModPackageFromJobJsonTooltip", "Choose an existing NTE mod package job JSON and launch the cook/package pipeline."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FNTEBuildToolModule::BuildModPackageFromJobJson))));
}

void FNTEBuildToolModule::ImportFModelPhysicsAssetJson()
{
	USkeletalMesh* SelectedSkeletalMesh = NTEBuildTool::Editor::GetSingleSelectedSkeletalMesh();
	if (!SelectedSkeletalMesh)
	{
		NTEBuildTool::Editor::ShowError(LOCTEXT("NoSkeletalMeshSelected", "Select exactly one SkeletalMesh in the Content Browser before importing."));
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
		NTEBuildTool::Editor::ShowError(FText::FromString(Error));
		return;
	}

	SelectedSkeletalMesh->SetPhysicsAsset(PhysicsAsset);
	SelectedSkeletalMesh->PostEditChange();
	SelectedSkeletalMesh->MarkPackageDirty();

	TArray<UObject*> ObjectsToSync;
	ObjectsToSync.Add(PhysicsAsset);
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync);

	NTEBuildTool::Editor::ShowInfo(FText::Format(
		LOCTEXT("ImportedPhysicsAsset", "Analyzed JSON and imported {0} bodies, {1} constraints, and {2} disabled collision pairs into {3}. It is assigned to {4}. Save the dirty assets after checking it."),
		FText::AsNumber(Summary.BodyCount),
		FText::AsNumber(Summary.ConstraintCount),
		FText::AsNumber(Summary.DisabledCollisionPairCount),
		FText::FromString(PhysicsAsset->GetName()),
		FText::FromString(SelectedSkeletalMesh->GetName())));
}

void FNTEBuildToolModule::CreateModMaterialInstanceFromConfig()
{
	FString ConfigFilename;
	if (!NTEBuildTool::Editor::ChooseJsonFileWithTitle(
		LOCTEXT("ChooseModMaterialConfig", "Choose NTE Mod Material Config JSON"),
		TEXT("NTE_ModMaterialConfig.json"),
		ConfigFilename))
	{
		return;
	}

	FString Error;
	NTEBuildTool::Material::FNteMaterialConfigApplyResult Result;
	const FScopedTransaction Transaction(LOCTEXT("CreateModMaterialInstanceTransaction", "Create Mod Material Instance From Config"));
	if (!NTEBuildTool::Material::ApplyModMaterialConfigFromFile(ConfigFilename, Result, Error))
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(Error));
		return;
	}

	NTEBuildTool::Editor::ShowSuccessNotification(FText::Format(
		LOCTEXT("CreatedModMaterialInstance", "Created/updated {0}. Report: {1}"),
		FText::FromString(Result.OutputMaterialPath),
		FText::FromString(Result.ReportFilename)));
}

void FNTEBuildToolModule::CreateMeshToggleUiSetup()
{
	NTEBuildTool::Toggle::FNteMeshToggleSetupOptions Options;
	if (USkeletalMesh* SelectedSkeletalMesh = NTEBuildTool::Editor::GetSingleSelectedSkeletalMesh())
	{
		Options.MeshPath = SelectedSkeletalMesh->GetPackage()->GetName();
		Options.OutputFolder = FPackageName::GetLongPackagePath(Options.MeshPath) / TEXT("mod/Runtime");
	}

	NTEBuildTool::Toggle::FNteMeshToggleSetupResult Result;
	FString Error;
	const FScopedTransaction Transaction(LOCTEXT("CreateMeshToggleUiSetupTransaction", "Create Mesh Toggle UI Setup"));
	if (!NTEBuildTool::Toggle::RunMeshToggleUiSetup(Options, Result, Error))
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(Error));
		return;
	}

	NTEBuildTool::Editor::ShowSuccessNotification(FText::Format(
		LOCTEXT("CreatedMeshToggleUiSetup", "Updated mesh toggle setup for {0}."),
		FText::FromString(Options.MeshPath)));
}

void FNTEBuildToolModule::BuildSelectedAssetsModPackage()
{
	NTEBuildTool::Editor::ShowError(LOCTEXT("BuildSelectedAssetsNotMigrated", "Build Mod Package UI has not been migrated into the modular package pipeline yet."));
}

void FNTEBuildToolModule::BuildModPackageFromJobJson()
{
	FString JobFilename;
	if (!NTEBuildTool::Editor::ChooseJsonFileWithTitle(
		LOCTEXT("ChooseModPackageJobJson", "Choose NTE Mod Package Job JSON"),
		TEXT("nte_mod_P.job.json"),
		JobFilename))
	{
		return;
	}

	FString Error;
	NTEBuildTool::Package::FNteModPackageLaunchResult Result;
	if (!NTEBuildTool::Package::LaunchModPackageBuildJob(JobFilename, Result, Error))
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(Error));
		return;
	}

	NTEBuildTool::Editor::ShowInfo(FText::Format(
		LOCTEXT("ModPackageBuildFromJobStarted", "Started mod package build. Job file: {0}"),
		FText::FromString(Result.JobFile)));
}

#undef LOCTEXT_NAMESPACE
