// Copyright (c) 2026 NTEBuildTool contributors.

#include "NTEBuildTool.h"

#include "FModelPhysicsAssetImporter.h"
#include "NteBuildToolSettings.h"
#include "NteCharacterModSpec.h"
#include "NteCharacterMaterialPlan.h"
#include "NteCharacterMaterialWriter.h"
#include "NteEditorAssetUtils.h"
#include "NteMaterialInstanceDialog.h"
#include "NteMaterialInstanceTool.h"
#include "NteMeshModWorkspaceDialog.h"
#include "NteMeshToggleConfig.h"
#include "NteMeshToggleDialog.h"
#include "NteModPackageDialog.h"
#include "NteModPackageJob.h"
#include "NteModPackagePlan.h"
#include "NteNotificationUtils.h"

#include "ContentBrowserModule.h"
#include "Dom/JsonValue.h"
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

FString MakeDefaultWorkspaceMaterialInstancePathFromMeshPath(const FString& MeshPath, const int32 SlotIndex)
{
	if (!NTEBuildTool::Editor::IsGamePackageName(MeshPath))
	{
		return FString();
	}

	FString MaterialName = TEXT("MI_mod_") + FPackageName::GetShortName(MeshPath);
	if (SlotIndex != INDEX_NONE)
	{
		MaterialName += FString::Printf(TEXT("_slot%d"), SlotIndex);
	}
	return NTEBuildTool::Editor::JoinAssetPath(FPackageName::GetLongPackagePath(MeshPath) / TEXT("mod/Materials"), MaterialName);
}

NTEBuildTool::Material::FNteMaterialInstanceOptions MakeWorkspaceMaterialDefaults(
	USkeletalMesh* SelectedMesh,
	const NTEBuildTool::Workspace::FNteMeshModWorkspaceResult& WorkspaceResult)
{
	NTEBuildTool::Material::FNteMaterialInstanceOptions Options;
	Options.MeshPath = WorkspaceResult.MeshPath.IsEmpty() && SelectedMesh
		? NTEBuildTool::Editor::GetAssetPackagePath(SelectedMesh)
		: WorkspaceResult.MeshPath;
	Options.SlotIndex = WorkspaceResult.SlotIndex;
	Options.bAssignToMeshSlot = !Options.MeshPath.IsEmpty() && Options.SlotIndex != INDEX_NONE;
	Options.ParentMaterialPath = WorkspaceResult.MaterialPath;
	Options.OutputMaterialPath = MakeDefaultWorkspaceMaterialInstancePathFromMeshPath(Options.MeshPath, WorkspaceResult.SlotIndex);
	return Options;
}

FString MakeMaterialOperationId(const int32 SlotIndex)
{
	return SlotIndex != INDEX_NONE
		? FString::Printf(TEXT("main_slot_%d_material"), SlotIndex)
		: TEXT("main_material");
}

FString SanitizeSpecFilenamePart(FString Value)
{
	Value.TrimStartAndEndInline();
	if (Value.IsEmpty())
	{
		Value = TEXT("CharacterModSpec");
	}

	const TCHAR* InvalidChars = TEXT("/\\:*?\"<>|");
	for (const TCHAR* Cursor = InvalidChars; Cursor && *Cursor; ++Cursor)
	{
		Value.ReplaceCharInline(*Cursor, TCHAR('_'));
	}
	return Value;
}

FString MakeDefaultCharacterSpecFilename(const NTEBuildTool::Character::FNteCharacterModSpec& Spec)
{
	FString BaseName = Spec.WorkspaceName;
	if (BaseName.IsEmpty())
	{
		BaseName = Spec.Package.ModName;
	}
	if (BaseName.IsEmpty() && !Spec.MainMeshPath.IsEmpty())
	{
		BaseName = FPackageName::GetShortName(Spec.MainMeshPath);
	}
	return SanitizeSpecFilenamePart(BaseName) + TEXT(".spec.json");
}

void UpsertMaterialOperation(
	TArray<NTEBuildTool::Character::FNteCharacterMaterialOperationSpec>& Operations,
	NTEBuildTool::Character::FNteCharacterMaterialOperationSpec&& Operation)
{
	for (NTEBuildTool::Character::FNteCharacterMaterialOperationSpec& ExistingOperation : Operations)
	{
		if (ExistingOperation.Id == Operation.Id)
		{
			ExistingOperation = MoveTemp(Operation);
			return;
		}
	}

	Operations.Add(MoveTemp(Operation));
}

FString SaveCharacterSpecAfterWorkspaceAction(
	const NTEBuildTool::Character::FNteCharacterModSpec& CharacterSpec,
	const FString& ExistingSpecFilename)
{
	FString SpecFilename = ExistingSpecFilename;
	if (SpecFilename.IsEmpty())
	{
		if (!NTEBuildTool::Editor::ChooseSaveJsonFileWithTitle(
			LOCTEXT("SaveCharacterModSpecAfterAction", "Save Updated CharacterModSpec JSON"),
			MakeDefaultCharacterSpecFilename(CharacterSpec),
			SpecFilename))
		{
			return FString();
		}
	}

	FString Error;
	if (!NTEBuildTool::Character::SaveCharacterModSpecToJsonFile(CharacterSpec, SpecFilename, Error))
	{
		NTEBuildTool::Editor::ShowError(FText::Format(
			LOCTEXT("WorkspaceSpecSaveAfterActionFailed", "The workspace action finished, but CharacterModSpec could not be saved:\n{0}"),
			FText::FromString(Error)));
		return FString();
	}

	return SpecFilename;
}

TMap<FString, FString> JsonStringObjectToMap(const TSharedPtr<FJsonObject>& Object)
{
	TMap<FString, FString> Result;
	if (!Object.IsValid())
	{
		return Result;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
	{
		if (Pair.Value.IsValid())
		{
			const FString SourceTexturePath = NTEBuildTool::Editor::NormalizeAssetPathForText(Pair.Key);
			const FString ReplacementTexturePath = NTEBuildTool::Editor::NormalizeAssetPathForText(Pair.Value->AsString());
			if (!SourceTexturePath.IsEmpty() && !ReplacementTexturePath.IsEmpty())
			{
				Result.Add(SourceTexturePath, ReplacementTexturePath);
			}
		}
	}
	return Result;
}

void RunCreateModMaterialInstanceFromSourceJson(const NTEBuildTool::Material::FNteMaterialInstanceOptions& InitialOptions)
{
	FString SourceMaterialJson;
	if (!NTEBuildTool::Editor::ChooseJsonFileWithTitle(
		LOCTEXT("ChooseFModelMaterialJson", "Choose FModel Material JSON"),
		TEXT("MI_source_material.json"),
		SourceMaterialJson))
	{
		return;
	}

	NTEBuildTool::Material::FNteMaterialInstanceOptions Options;
	TSharedPtr<FJsonObject> SourceTextureOverrides;
	if (!NTEBuildTool::Material::ShowMaterialInstanceRecipeDialog(SourceMaterialJson, InitialOptions, Options, SourceTextureOverrides))
	{
		return;
	}

	FString Error;
	NTEBuildTool::Material::FNteMaterialConfigApplyResult Result;
	const FScopedTransaction Transaction(LOCTEXT("CreateModMaterialInstanceFromSourceTransaction", "Create Mod Material Instance From FModel Source"));
	if (!NTEBuildTool::Material::ApplyModMaterialConfig(
		Options,
		SourceTextureOverrides.IsValid() ? SourceTextureOverrides.Get() : nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		Result,
		Error))
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(Error));
		return;
	}

	NTEBuildTool::Editor::ShowSuccessNotification(FText::Format(
		LOCTEXT("CreatedModMaterialInstanceFromSource", "Created/updated {0}. Source texture groups: {1}. Report: {2}"),
		FText::FromString(Result.OutputMaterialPath),
		FText::AsNumber(Result.CreateResult.ApplySummary.SourceTextureOverrideGroups),
		FText::FromString(Result.ReportFilename)));
}

void RunCharacterMaterialOperationFromSourceJson(
	USkeletalMesh* SelectedMesh,
	const NTEBuildTool::Workspace::FNteMeshModWorkspaceResult& WorkspaceResult)
{
	FString SourceMaterialJson;
	if (!NTEBuildTool::Editor::ChooseJsonFileWithTitle(
		LOCTEXT("ChooseCharacterMaterialSourceJson", "Choose FModel Material JSON"),
		TEXT("MI_source_material.json"),
		SourceMaterialJson))
	{
		return;
	}

	NTEBuildTool::Material::FNteMaterialInstanceOptions Options;
	TSharedPtr<FJsonObject> SourceTextureOverrides;
	if (!NTEBuildTool::Material::ShowMaterialInstanceRecipeDialog(
		SourceMaterialJson,
		MakeWorkspaceMaterialDefaults(SelectedMesh, WorkspaceResult),
		Options,
		SourceTextureOverrides))
	{
		return;
	}

	NTEBuildTool::Character::FNteCharacterModSpec CharacterSpec = WorkspaceResult.CharacterSpec;
	if (!Options.MeshPath.IsEmpty())
	{
		CharacterSpec.MainMeshPath = Options.MeshPath;
	}
	else if (SelectedMesh)
	{
		CharacterSpec.MainMeshPath = NTEBuildTool::Editor::GetAssetPackagePath(SelectedMesh);
	}

	NTEBuildTool::Character::FNteCharacterMaterialOperationSpec Operation;
	Operation.Id = MakeMaterialOperationId(Options.SlotIndex);
	Operation.TargetMeshId = TEXT("main");
	Operation.SlotIndex = Options.SlotIndex;
	Operation.SlotName = WorkspaceResult.SlotName;
	Operation.SourceMaterialJson = Options.SourceMaterialJson.IsEmpty() ? SourceMaterialJson : Options.SourceMaterialJson;
	Operation.ParentMaterialPath = Options.ParentMaterialPath;
	Operation.OutputMaterialPath = Options.OutputMaterialPath;
	Operation.SourceTextureOverrides = JsonStringObjectToMap(SourceTextureOverrides);
	Operation.bAssignToSlot = Options.bAssignToMeshSlot;
	UpsertMaterialOperation(CharacterSpec.MaterialOperations, MoveTemp(Operation));

	const NTEBuildTool::Character::FNteCharacterMaterialPlan MaterialPlan =
		NTEBuildTool::Character::BuildCharacterMaterialPlanFromSpec(CharacterSpec);

	const FScopedTransaction Transaction(LOCTEXT("ApplyCharacterMaterialOperationTransaction", "Apply Character Material Operation"));
	const NTEBuildTool::Character::FNteCharacterMaterialWriteResult WriteResult =
		NTEBuildTool::Character::WriteCharacterMaterials(MaterialPlan);
	if (WriteResult.HasErrors())
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Join(WriteResult.Errors, TEXT("\n"))));
		return;
	}

	const FString SavedSpecFilename = SaveCharacterSpecAfterWorkspaceAction(CharacterSpec, WorkspaceResult.SpecFilename);
	FString OutputMaterialPath;
	int32 SourceTextureGroups = 0;
	if (!WriteResult.Operations.IsEmpty())
	{
		OutputMaterialPath = WriteResult.Operations[0].OutputMaterialPath;
		SourceTextureGroups = WriteResult.Operations[0].SourceTextureOverrideGroups;
	}
	NTEBuildTool::Editor::ShowSuccessNotification(FText::Format(
		LOCTEXT("AppliedCharacterMaterialOperation", "Applied CharacterModSpec material operation. Output: {0}. Source texture groups: {1}. Spec: {2}"),
		FText::FromString(OutputMaterialPath),
		FText::AsNumber(SourceTextureGroups),
		FText::FromString(SavedSpecFilename.IsEmpty() ? TEXT("<not saved>") : SavedSpecFilename)));
}

void RunCharacterModPackageFromSpec(NTEBuildTool::Character::FNteCharacterModSpec CharacterSpec)
{
	FString Error;
	NTEBuildTool::Package::FNtePackagePlan Plan;
	if (!NTEBuildTool::Package::BuildPackagePlanFromCharacterModSpec(CharacterSpec, Plan, Error))
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(Error));
		return;
	}

	NTEBuildTool::Package::FNtePackagePlanDialogResult DialogResult;
	if (!NTEBuildTool::Package::ShowPackagePlanDialog(Plan, DialogResult))
	{
		return;
	}

	CharacterSpec.Package.ModsDir = DialogResult.ModsDir;
	CharacterSpec.Package.ModName = DialogResult.ModName;
	CharacterSpec.Package.JobFilename = DialogResult.JobFilename;
	CharacterSpec.Package.bBuildAfterCreate = DialogResult.bLaunchBuild;

	NTEBuildTool::Package::FNteModPackageJobCreateResult CreateResult;
	if (!NTEBuildTool::Package::CreateModPackageJobFromCharacterModSpec(CharacterSpec, Plan, CreateResult, Error))
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(Error));
		return;
	}

	if (!DialogResult.bLaunchBuild)
	{
		NTEBuildTool::Editor::ShowInfo(FText::Format(
			LOCTEXT("CreatedCharacterModPackageJob", "Created CharacterModSpec package job with {0} packages. Job file: {1}"),
			FText::AsNumber(CreateResult.PackageCount),
			FText::FromString(CreateResult.JobFile)));
		return;
	}

	NTEBuildTool::Package::FNteModPackageLaunchResult LaunchResult;
	if (!NTEBuildTool::Package::LaunchModPackageBuildJob(CreateResult.JobFile, LaunchResult, Error))
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(Error));
		return;
	}

	NTEBuildTool::Editor::ShowInfo(FText::Format(
		LOCTEXT("BuildCharacterModPackageFinished", "Created CharacterModSpec package job with {0} packages and finished build. Job file: {1}"),
		FText::AsNumber(CreateResult.PackageCount),
		FText::FromString(LaunchResult.JobFile)));
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
		TEXT("NTEBuildTool_OpenCharacterModWorkspace"),
		LOCTEXT("OpenCharacterModWorkspaceLabel", "Open Character Mod Workspace"),
		LOCTEXT("OpenCharacterModWorkspaceTooltip", "Start from one character appearance and coordinate meshes, materials, runtime actions, physics, and packaging."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FNTEBuildToolModule::OpenCharacterModWorkspace))));
	Section.AddEntry(FToolMenuEntry::InitMenuEntry(
		TEXT("NTEBuildTool_ImportFModelPhysicsAsset"),
		LOCTEXT("ImportFModelPhysicsAssetLabel", "Import FModel PhysicsAsset JSON"),
		LOCTEXT("ImportFModelPhysicsAssetTooltip", "Analyze a FModel PhysicsAsset JSON, create a matching PhysicsAsset, and assign it to the selected SkeletalMesh."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FNTEBuildToolModule::ImportFModelPhysicsAssetJson))));
	Section.AddEntry(FToolMenuEntry::InitMenuEntry(
		TEXT("NTEBuildTool_CreateModMaterialInstanceFromSourceJson"),
		LOCTEXT("CreateModMaterialInstanceFromSourceJsonLabel", "Create Material Instance From FModel Material JSON"),
		LOCTEXT("CreateModMaterialInstanceFromSourceJsonTooltip", "Inspect source texture usage from a FModel material JSON and create or update a mod MaterialInstanceConstant."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FNTEBuildToolModule::CreateModMaterialInstanceFromSourceJson))));
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

void FNTEBuildToolModule::OpenCharacterModWorkspace()
{
	USkeletalMesh* SelectedSkeletalMesh = NTEBuildTool::Editor::GetSingleSelectedSkeletalMesh();
	NTEBuildTool::Workspace::FNteMeshModWorkspaceResult WorkspaceResult;
	if (!NTEBuildTool::Workspace::ShowMeshModWorkspaceDialog(SelectedSkeletalMesh, WorkspaceResult))
	{
		return;
	}

	switch (WorkspaceResult.Action)
	{
	case NTEBuildTool::Workspace::ENteMeshModWorkspaceAction::ApplyMaterialOperation:
		RunCharacterMaterialOperationFromSourceJson(SelectedSkeletalMesh, WorkspaceResult);
		break;
	case NTEBuildTool::Workspace::ENteMeshModWorkspaceAction::ConfigureToggleRuntime:
		CreateMeshToggleUiSetup();
		break;
	case NTEBuildTool::Workspace::ENteMeshModWorkspaceAction::BuildPackage:
		RunCharacterModPackageFromSpec(WorkspaceResult.CharacterSpec);
		break;
	default:
		break;
	}
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

void FNTEBuildToolModule::CreateModMaterialInstanceFromSourceJson()
{
	RunCreateModMaterialInstanceFromSourceJson(NTEBuildTool::Material::FNteMaterialInstanceOptions());
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
	USkeletalMesh* SelectedSkeletalMesh = NTEBuildTool::Editor::GetSingleSelectedSkeletalMesh();
	if (!SelectedSkeletalMesh)
	{
		NTEBuildTool::Editor::ShowError(LOCTEXT("NoToggleSkeletalMeshSelected", "Select exactly one SkeletalMesh in the Content Browser before generating toggle runtime assets."));
		return;
	}

	NTEBuildTool::Toggle::FNteMeshToggleSetupOptions Options;
	if (!NTEBuildTool::Toggle::ShowMeshToggleSetupDialog(*SelectedSkeletalMesh, Options))
	{
		return;
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
		LOCTEXT("CreatedMeshToggleUiSetup", "Generated toggle runtime for {0}."),
		FText::FromString(Options.TargetMeshPath)));
}

void FNTEBuildToolModule::BuildSelectedAssetsModPackage()
{
	FString Error;
	NTEBuildTool::Package::FNtePackagePlan Plan;
	if (!NTEBuildTool::Package::BuildPackagePlanFromSelection(Plan, Error))
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(Error));
		return;
	}
	NTEBuildTool::Package::FNtePackagePlanDialogResult DialogResult;
	if (!NTEBuildTool::Package::ShowPackagePlanDialog(Plan, DialogResult))
	{
		return;
	}

	const TArray<FString> Packages = NTEBuildTool::Package::GetIncludedPackageNames(Plan);
	if (Packages.IsEmpty())
	{
		NTEBuildTool::Editor::ShowError(LOCTEXT("NoPackagePlanIncludes", "The package plan has no checked packages."));
		return;
	}

	NTEBuildTool::Package::FNteModPackageJobCreateOptions CreateOptions;
	CreateOptions.ModsDir = DialogResult.ModsDir;
	CreateOptions.ModName = DialogResult.ModName;
	CreateOptions.JobFilename = DialogResult.JobFilename;
	CreateOptions.Packages = Packages;
	CreateOptions.bCollectContentBrowserSelection = false;

	NTEBuildTool::Package::FNteModPackageJobCreateResult CreateResult;
	if (!NTEBuildTool::Package::CreateModPackageJobFromSelection(CreateOptions, CreateResult, Error))
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(Error));
		return;
	}

	if (!DialogResult.bLaunchBuild)
	{
		NTEBuildTool::Editor::ShowInfo(FText::Format(
			LOCTEXT("CreatedSelectedAssetsPackageJob", "Created package job with {0} packages. Job file: {1}"),
			FText::AsNumber(CreateResult.PackageCount),
			FText::FromString(CreateResult.JobFile)));
		return;
	}

	NTEBuildTool::Package::FNteModPackageLaunchResult LaunchResult;
	if (!NTEBuildTool::Package::LaunchModPackageBuildJob(CreateResult.JobFile, LaunchResult, Error))
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(Error));
		return;
	}

	NTEBuildTool::Editor::ShowInfo(FText::Format(
		LOCTEXT("BuildSelectedAssetsPackageStarted", "Created package job with {0} packages and finished build. Job file: {1}"),
		FText::AsNumber(CreateResult.PackageCount),
		FText::FromString(LaunchResult.JobFile)));
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
