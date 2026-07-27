// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteCharacterModSpecCommandlet.h"

#include "NTEBuildTool.h"
#include "NteAppearanceAssemblyPlan.h"
#include "NteAppearanceAssemblyWriter.h"
#include "NteCharacterKawaiiAssetSync.h"
#include "NteCharacterKawaiiPlan.h"
#include "NteCharacterKawaiiPresetImporter.h"
#include "NteCharacterKawaiiWriter.h"
#include "NteCharacterMaterialPlan.h"
#include "NteCharacterMaterialWriter.h"
#include "NteCharacterModSpec.h"
#include "NteCharacterRuntimeActionPlan.h"
#include "NteCharacterRuntimeActionWriter.h"
#include "NteJsonFileUtils.h"
#include "NteModPackageJob.h"
#include "NteModPackagePlan.h"
#include "NteModPackageTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
TArray<TSharedPtr<FJsonValue>> StringArrayToJsonValues(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	for (const FString& Value : Values)
	{
		Result.Add(MakeShared<FJsonValueString>(Value));
	}
	return Result;
}

TSharedRef<FJsonObject> MakeSpecSummaryObject(const NTEBuildTool::Character::FNteCharacterModSpec& Spec)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("WorkspaceName"), Spec.WorkspaceName);
	Object->SetStringField(TEXT("MainMeshPath"), Spec.MainMeshPath);
	Object->SetStringField(TEXT("PlayerAppearanceAssetPath"), Spec.Appearance.PlayerAppearanceAssetPath);
	Object->SetStringField(TEXT("UIActorClassPath"), Spec.Appearance.UIActorClassPath);
	Object->SetNumberField(TEXT("AttachedMeshCount"), Spec.AttachedMeshes.Num());
	Object->SetNumberField(TEXT("MaterialOperationCount"), Spec.MaterialOperations.Num());
	Object->SetNumberField(TEXT("RuntimeActionCount"), Spec.RuntimeActions.Num());
	Object->SetNumberField(TEXT("KawaiiPresetCount"), Spec.KawaiiPresets.Num());
	Object->SetStringField(TEXT("ModName"), Spec.Package.ModName);
	Object->SetStringField(TEXT("ModsDir"), Spec.Package.ModsDir);
	return Object;
}

TSharedRef<FJsonObject> PackagePlanToJson(const NTEBuildTool::Package::FNtePackagePlan& Plan)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Candidates;
	for (const NTEBuildTool::Package::FNtePackagePlanCandidate& Candidate : Plan.Candidates)
	{
		const TSharedRef<FJsonObject> CandidateObject = MakeShared<FJsonObject>();
		CandidateObject->SetStringField(TEXT("PackageName"), Candidate.PackageName);
		CandidateObject->SetStringField(TEXT("Kind"), NTEBuildTool::Package::PackagePlanCandidateKindToString(Candidate.Kind));
		CandidateObject->SetStringField(TEXT("Reason"), Candidate.Reason);
		CandidateObject->SetBoolField(TEXT("DefaultIncluded"), Candidate.bDefaultIncluded);
		Candidates.Add(MakeShared<FJsonValueObject>(CandidateObject));
	}
	Object->SetNumberField(TEXT("CandidateCount"), Plan.Candidates.Num());
	Object->SetArrayField(TEXT("Candidates"), Candidates);
	Object->SetArrayField(TEXT("IncludedPackages"), StringArrayToJsonValues(NTEBuildTool::Package::GetIncludedPackageNames(Plan)));
	return Object;
}

TSharedRef<FJsonObject> PackageJobCreateResultToJson(const NTEBuildTool::Package::FNteModPackageJobCreateResult& Result)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("JobFile"), Result.JobFile);
	Object->SetStringField(TEXT("ModName"), Result.ModName);
	Object->SetNumberField(TEXT("PackageCount"), Result.PackageCount);
	Object->SetArrayField(TEXT("Packages"), StringArrayToJsonValues(Result.Job.Packages));
	Object->SetStringField(TEXT("ModsDir"), Result.Job.ModsDir);
	Object->SetStringField(TEXT("GameMountName"), Result.Job.GameMountName);
	Object->SetBoolField(TEXT("Unversioned"), Result.Job.bUnversioned);
	Object->SetBoolField(TEXT("SkipCook"), Result.Job.bSkipCook);
	Object->SetBoolField(TEXT("RequiresHTGameStub"), Result.Job.bRequiresHTGameStub);
	return Object;
}

TSharedRef<FJsonObject> PackageBuildLaunchResultToJson(const NTEBuildTool::Package::FNteModPackageLaunchResult& Result)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("JobFile"), Result.JobFile);
	Object->SetStringField(TEXT("WorkRoot"), Result.WorkRoot);
	Object->SetStringField(TEXT("CommandLine"), Result.CommandLine);
	return Object;
}

TSharedRef<FJsonObject> KawaiiAssetSyncResultToJson(const NTEBuildTool::Character::FNteCharacterKawaiiAssetSyncResult& Result)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("PresetId"), Result.PresetId);
	Object->SetStringField(TEXT("RuntimeAnimBlueprintPath"), Result.RuntimeAnimBlueprintPath);
	Object->SetArrayField(TEXT("UpdatedFields"), StringArrayToJsonValues(Result.UpdatedFields));
	Object->SetArrayField(TEXT("Warnings"), StringArrayToJsonValues(Result.Warnings));
	Object->SetArrayField(TEXT("Errors"), StringArrayToJsonValues(Result.Errors));
	return Object;
}

TArray<TSharedPtr<FJsonValue>> KawaiiAssetSyncResultsToJsonValues(const TArray<NTEBuildTool::Character::FNteCharacterKawaiiAssetSyncResult>& Results)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const NTEBuildTool::Character::FNteCharacterKawaiiAssetSyncResult& Result : Results)
	{
		Values.Add(MakeShared<FJsonValueObject>(KawaiiAssetSyncResultToJson(Result)));
	}
	return Values;
}
}

UNteCharacterModSpecCommandlet::UNteCharacterModSpecCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
	UseCommandletResultAsExitCode = true;
	HelpDescription = TEXT("Validates an NTE CharacterModSpec JSON and writes a report.");
	HelpUsage = TEXT("UnrealEditor-Cmd.exe <Project>.uproject -run=NteCharacterModSpec -Spec=<json> [-Output=<json>] [-ImportKawaiiJson=<fmodel anim layer json>] [-KawaiiTargetMeshId=<mesh id>] [-KawaiiPresetPrefix=<prefix>] [-NoReplaceKawaiiPresets] [-SyncKawaiiFromAssets] [-KawaiiPresetId=<id>] [-WriteUpdatedSpec=<json>|-SaveSpec] [-ApplyAppearance] [-ApplyMaterials] [-ApplyRuntimeActions] [-ApplyKawaii] [-WritePackageJob] [-BuildPackage] [-FailOnWarnings]");
}

int32 UNteCharacterModSpecCommandlet::Main(const FString& Params)
{
	FString SpecFilename;
	if (!FParse::Value(*Params, TEXT("Spec="), SpecFilename) && !FParse::Value(*Params, TEXT("Config="), SpecFilename))
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Missing -Spec=<json>."));
		return 1;
	}
	FPaths::NormalizeFilename(SpecFilename);

	FString Error;
	NTEBuildTool::Character::FNteCharacterModSpec Spec;
	if (!NTEBuildTool::Character::LoadCharacterModSpecFromJsonFile(SpecFilename, Spec, Error))
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("CharacterModSpec could not be read: %s"), *Error);
		return 2;
	}

	FString ImportKawaiiJson;
	const bool bImportKawaiiJson = FParse::Value(*Params, TEXT("ImportKawaiiJson="), ImportKawaiiJson);
	FString WriteUpdatedSpecFilename;
	const bool bSaveSpec = FParse::Param(*Params, TEXT("SaveSpec"));
	const bool bWriteUpdatedSpec = FParse::Value(*Params, TEXT("WriteUpdatedSpec="), WriteUpdatedSpecFilename);
	TArray<FString> SpecUpdateErrors;
	NTEBuildTool::Character::FNteCharacterKawaiiImportResult KawaiiImportResult;
	if (bImportKawaiiJson)
	{
		FString KawaiiTargetMeshId;
		FString KawaiiPresetPrefix;
		FParse::Value(*Params, TEXT("KawaiiTargetMeshId="), KawaiiTargetMeshId);
		FParse::Value(*Params, TEXT("KawaiiPresetPrefix="), KawaiiPresetPrefix);

		NTEBuildTool::Character::FNteCharacterKawaiiImportOptions ImportOptions;
		ImportOptions.SourceJsonPath = ImportKawaiiJson;
		ImportOptions.TargetMeshId = KawaiiTargetMeshId.IsEmpty() ? TEXT("main") : KawaiiTargetMeshId;
		ImportOptions.PresetIdPrefix = KawaiiPresetPrefix;
		ImportOptions.bReplaceExistingById = !FParse::Param(*Params, TEXT("NoReplaceKawaiiPresets"));
		KawaiiImportResult = NTEBuildTool::Character::ImportKawaiiPresetsFromFModelJson(ImportOptions);
		if (!KawaiiImportResult.HasErrors())
		{
			NTEBuildTool::Character::UpsertKawaiiPresets(Spec, KawaiiImportResult.ImportedPresets, ImportOptions.bReplaceExistingById, KawaiiImportResult);
		}
	}

	const bool bSyncKawaiiFromAssets = FParse::Param(*Params, TEXT("SyncKawaiiFromAssets"));
	FString SyncKawaiiPresetId;
	FParse::Value(*Params, TEXT("KawaiiPresetId="), SyncKawaiiPresetId);
	TArray<NTEBuildTool::Character::FNteCharacterKawaiiAssetSyncResult> KawaiiAssetSyncResults;
	if (bSyncKawaiiFromAssets && !KawaiiImportResult.HasErrors())
	{
		const NTEBuildTool::Character::FNteCharacterKawaiiPlan SyncPlan =
			NTEBuildTool::Character::BuildCharacterKawaiiPlanFromSpec(Spec);
		for (const NTEBuildTool::Character::FNteCharacterKawaiiPresetPlanItem& PlanItem : SyncPlan.Presets)
		{
			if (!SyncKawaiiPresetId.IsEmpty() && PlanItem.Id != SyncKawaiiPresetId)
			{
				continue;
			}

			int32 PresetIndex = INDEX_NONE;
			for (int32 Index = 0; Index < Spec.KawaiiPresets.Num(); ++Index)
			{
				if (Spec.KawaiiPresets[Index].Id == PlanItem.Id)
				{
					PresetIndex = Index;
					break;
				}
			}

			if (PresetIndex == INDEX_NONE)
			{
				NTEBuildTool::Character::FNteCharacterKawaiiAssetSyncResult MissingResult;
				MissingResult.PresetId = PlanItem.Id;
				MissingResult.RuntimeAnimBlueprintPath = PlanItem.RuntimeAnimBlueprintPath;
				MissingResult.Errors.Add(FString::Printf(TEXT("Kawaii preset '%s' is present in KawaiiPlan but not CharacterModSpec."), *PlanItem.Id));
				KawaiiAssetSyncResults.Add(MoveTemp(MissingResult));
				continue;
			}

			KawaiiAssetSyncResults.Add(NTEBuildTool::Character::SyncKawaiiPresetSpecFromGeneratedAssets(
				PlanItem,
				Spec.KawaiiPresets[PresetIndex]));
		}
		if (!SyncKawaiiPresetId.IsEmpty() && KawaiiAssetSyncResults.IsEmpty())
		{
			NTEBuildTool::Character::FNteCharacterKawaiiAssetSyncResult MissingResult;
			MissingResult.PresetId = SyncKawaiiPresetId;
			MissingResult.Errors.Add(FString::Printf(TEXT("No Kawaii preset matched -KawaiiPresetId=%s."), *SyncKawaiiPresetId));
			KawaiiAssetSyncResults.Add(MoveTemp(MissingResult));
		}
	}

	bool bHasKawaiiAssetSyncErrors = false;
	for (const NTEBuildTool::Character::FNteCharacterKawaiiAssetSyncResult& SyncResult : KawaiiAssetSyncResults)
	{
		bHasKawaiiAssetSyncErrors |= SyncResult.HasErrors();
	}
	if ((bSaveSpec || bWriteUpdatedSpec) && !KawaiiImportResult.HasErrors() && !bHasKawaiiAssetSyncErrors && (bImportKawaiiJson || bSyncKawaiiFromAssets))
	{
		if (bSaveSpec && WriteUpdatedSpecFilename.IsEmpty())
		{
			WriteUpdatedSpecFilename = SpecFilename;
		}
		FPaths::NormalizeFilename(WriteUpdatedSpecFilename);
		FString SaveError;
		if (!NTEBuildTool::Character::SaveCharacterModSpecToJsonFile(Spec, WriteUpdatedSpecFilename, SaveError))
		{
			SpecUpdateErrors.Add(FString::Printf(TEXT("Could not write updated CharacterModSpec '%s': %s"), *WriteUpdatedSpecFilename, *SaveError));
		}
	}

	NTEBuildTool::Character::FNteCharacterModSpecValidationResult Validation =
		NTEBuildTool::Character::ValidateCharacterModSpec(Spec);
	if (bImportKawaiiJson)
	{
		Validation.Errors.Append(KawaiiImportResult.Errors);
		Validation.Warnings.Append(KawaiiImportResult.Warnings);
	}
	if (bSyncKawaiiFromAssets)
	{
		for (const NTEBuildTool::Character::FNteCharacterKawaiiAssetSyncResult& SyncResult : KawaiiAssetSyncResults)
		{
			Validation.Errors.Append(SyncResult.Errors);
			Validation.Warnings.Append(SyncResult.Warnings);
		}
		Validation.Errors.Append(SpecUpdateErrors);
	}
	const NTEBuildTool::Character::FNteAppearanceAssemblyPlan AppearancePlan =
		NTEBuildTool::Character::BuildAppearanceAssemblyPlanFromSpec(Spec);
	const NTEBuildTool::Character::FNteCharacterMaterialPlan MaterialPlan =
		NTEBuildTool::Character::BuildCharacterMaterialPlanFromSpec(Spec);
	const NTEBuildTool::Character::FNteCharacterRuntimeActionPlan RuntimeActionPlan =
		NTEBuildTool::Character::BuildCharacterRuntimeActionPlanFromSpec(Spec);
	const NTEBuildTool::Character::FNteCharacterKawaiiPlan KawaiiPlan =
		NTEBuildTool::Character::BuildCharacterKawaiiPlanFromSpec(Spec);
	TArray<FString> PackageSeeds = NTEBuildTool::Character::CollectCharacterModSpecPackageSeeds(Spec);
	for (const FString& MaterialSeed : NTEBuildTool::Character::CollectCharacterMaterialPlanPackageSeeds(MaterialPlan))
	{
		PackageSeeds.AddUnique(MaterialSeed);
	}
	for (const FString& RuntimeSeed : NTEBuildTool::Character::CollectCharacterRuntimeActionPlanPackageSeeds(RuntimeActionPlan))
	{
		PackageSeeds.AddUnique(RuntimeSeed);
	}
	for (const FString& KawaiiSeed : NTEBuildTool::Character::CollectCharacterKawaiiPlanPackageSeeds(KawaiiPlan))
	{
		PackageSeeds.AddUnique(KawaiiSeed);
	}
	PackageSeeds.Sort();
	const bool bApplyAppearance = FParse::Param(*Params, TEXT("ApplyAppearance"));
	NTEBuildTool::Character::FNteAppearanceAssemblyWriteResult AppearanceWriteResult;
	if (bApplyAppearance && !Validation.HasErrors())
	{
		AppearanceWriteResult = NTEBuildTool::Character::WriteAppearanceAssembly(AppearancePlan);
	}
	const bool bApplyMaterials = FParse::Param(*Params, TEXT("ApplyMaterials"));
	NTEBuildTool::Character::FNteCharacterMaterialWriteResult MaterialWriteResult;
	if (bApplyMaterials && !Validation.HasErrors())
	{
		MaterialWriteResult = NTEBuildTool::Character::WriteCharacterMaterials(MaterialPlan);
	}
	const bool bApplyRuntimeActions = FParse::Param(*Params, TEXT("ApplyRuntimeActions"));
	NTEBuildTool::Character::FNteCharacterRuntimeActionWriteResult RuntimeActionWriteResult;
	if (bApplyRuntimeActions && !Validation.HasErrors())
	{
		RuntimeActionWriteResult = NTEBuildTool::Character::WriteCharacterRuntimeActions(RuntimeActionPlan);
	}
	const bool bApplyKawaii = FParse::Param(*Params, TEXT("ApplyKawaii"));
	NTEBuildTool::Character::FNteCharacterKawaiiWriteResult KawaiiWriteResult;
	if (bApplyKawaii && !Validation.HasErrors() && KawaiiPlan.Errors.IsEmpty())
	{
		KawaiiWriteResult = NTEBuildTool::Character::WriteCharacterKawaiiAssets(KawaiiPlan);
	}

	FString PackagePlanError;
	NTEBuildTool::Package::FNtePackagePlan PackagePlan;
	const bool bHasPackagePlan = NTEBuildTool::Package::BuildPackagePlanFromCharacterModSpec(Spec, PackagePlan, PackagePlanError);
	const bool bBuildPackage = FParse::Param(*Params, TEXT("BuildPackage")) || Spec.Package.bBuildAfterCreate;
	const bool bWritePackageJob = FParse::Param(*Params, TEXT("WritePackageJob")) || bBuildPackage;
	const bool bHasKawaiiPresets = !KawaiiPlan.Presets.IsEmpty();
	const bool bKawaiiReadyForPackage = !bHasKawaiiPresets || (bApplyKawaii && !KawaiiWriteResult.HasErrors());
	FString PackageJobError;
	NTEBuildTool::Package::FNteModPackageJobCreateResult PackageJobResult;
	bool bHasPackageJob = false;
	if (bWritePackageJob && bHasPackagePlan && !Validation.HasErrors() && KawaiiPlan.Errors.IsEmpty() && bKawaiiReadyForPackage)
	{
		bHasPackageJob = NTEBuildTool::Package::CreateModPackageJobFromCharacterModSpec(Spec, PackagePlan, PackageJobResult, PackageJobError);
	}
	else if (bWritePackageJob && !bKawaiiReadyForPackage)
	{
		PackageJobError = bApplyKawaii
			? TEXT("Kawaii writer failed; package job was not written.")
			: TEXT("CharacterModSpec has Kawaii presets; run -ApplyKawaii successfully before writing/building a package job.");
	}
	else if (bWritePackageJob && !KawaiiPlan.Errors.IsEmpty())
	{
		PackageJobError = TEXT("KawaiiPlan has errors; package job was not written.");
	}
	FString PackageBuildError;
	NTEBuildTool::Package::FNteModPackageLaunchResult PackageBuildResult;
	bool bHasPackageBuild = false;
	if (bBuildPackage && bHasPackageJob)
	{
		bHasPackageBuild = NTEBuildTool::Package::LaunchModPackageBuildJob(PackageJobResult.JobFile, PackageBuildResult, PackageBuildError);
	}

	TArray<FString> ReportErrors = Validation.Errors;
	ReportErrors.Append(KawaiiPlan.Errors);
	if (bApplyKawaii)
	{
		ReportErrors.Append(KawaiiWriteResult.Errors);
	}
	if (bWritePackageJob && !bHasPackageJob && !PackageJobError.IsEmpty())
	{
		ReportErrors.Add(PackageJobError);
	}
	if (bBuildPackage && !bHasPackageBuild && !PackageBuildError.IsEmpty())
	{
		ReportErrors.Add(PackageBuildError);
	}
	TArray<FString> ReportWarnings = Validation.Warnings;
	ReportWarnings.Append(KawaiiPlan.Warnings);
	if (bApplyKawaii)
	{
		ReportWarnings.Append(KawaiiWriteResult.Warnings);
	}

	FString OutputFilename;
	if (!FParse::Value(*Params, TEXT("Output="), OutputFilename) || OutputFilename.IsEmpty())
	{
		OutputFilename = FPaths::ProjectSavedDir() / TEXT("NTEBuildTool/CharacterModSpecReport.json");
	}
	FPaths::NormalizeFilename(OutputFilename);

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("Format"), TEXT("NTE.CharacterModSpecReport"));
	Root->SetNumberField(TEXT("Version"), 1);
	Root->SetStringField(TEXT("SpecFile"), SpecFilename);
	Root->SetObjectField(TEXT("Summary"), MakeSpecSummaryObject(Spec));
	Root->SetNumberField(TEXT("ErrorCount"), ReportErrors.Num());
	Root->SetNumberField(TEXT("WarningCount"), ReportWarnings.Num());
	Root->SetArrayField(TEXT("Errors"), StringArrayToJsonValues(ReportErrors));
	Root->SetArrayField(TEXT("Warnings"), StringArrayToJsonValues(ReportWarnings));
	Root->SetArrayField(TEXT("PackageSeeds"), StringArrayToJsonValues(PackageSeeds));
	Root->SetObjectField(TEXT("AppearanceAssemblyPlan"), NTEBuildTool::Character::AppearanceAssemblyPlanToJson(AppearancePlan));
	Root->SetObjectField(TEXT("MaterialPlan"), NTEBuildTool::Character::CharacterMaterialPlanToJson(MaterialPlan));
	Root->SetObjectField(TEXT("RuntimeActionPlan"), NTEBuildTool::Character::CharacterRuntimeActionPlanToJson(RuntimeActionPlan));
	Root->SetObjectField(TEXT("KawaiiPlan"), NTEBuildTool::Character::CharacterKawaiiPlanToJson(KawaiiPlan));
	if (bImportKawaiiJson)
	{
		Root->SetObjectField(TEXT("KawaiiImportResult"), NTEBuildTool::Character::CharacterKawaiiImportResultToJson(KawaiiImportResult));
	}
	Root->SetBoolField(TEXT("SyncKawaiiFromAssets"), bSyncKawaiiFromAssets);
	if (bSyncKawaiiFromAssets)
	{
		Root->SetArrayField(TEXT("KawaiiAssetSyncResults"), KawaiiAssetSyncResultsToJsonValues(KawaiiAssetSyncResults));
	}
	Root->SetObjectField(TEXT("KawaiiPresetPlan"), NTEBuildTool::Character::CharacterKawaiiPresetPlanToJson(Spec));
	Root->SetBoolField(TEXT("ApplyAppearance"), bApplyAppearance);
	if (bApplyAppearance)
	{
		Root->SetObjectField(TEXT("AppearanceAssemblyWriteResult"), NTEBuildTool::Character::AppearanceAssemblyWriteResultToJson(AppearanceWriteResult));
	}
	Root->SetBoolField(TEXT("ApplyMaterials"), bApplyMaterials);
	if (bApplyMaterials)
	{
		Root->SetObjectField(TEXT("MaterialWriteResult"), NTEBuildTool::Character::CharacterMaterialWriteResultToJson(MaterialWriteResult));
	}
	Root->SetBoolField(TEXT("ApplyRuntimeActions"), bApplyRuntimeActions);
	if (bApplyRuntimeActions)
	{
		Root->SetObjectField(TEXT("RuntimeActionWriteResult"), NTEBuildTool::Character::CharacterRuntimeActionWriteResultToJson(RuntimeActionWriteResult));
	}
	Root->SetBoolField(TEXT("ApplyKawaii"), bApplyKawaii);
	if (bApplyKawaii)
	{
		Root->SetObjectField(TEXT("KawaiiWriteResult"), NTEBuildTool::Character::CharacterKawaiiWriteResultToJson(KawaiiWriteResult));
	}
	if (bHasPackagePlan)
	{
		Root->SetObjectField(TEXT("PackagePlan"), PackagePlanToJson(PackagePlan));
	}
	else
	{
		Root->SetStringField(TEXT("PackagePlanError"), PackagePlanError);
	}
	Root->SetBoolField(TEXT("WritePackageJob"), bWritePackageJob);
	if (bWritePackageJob)
	{
		if (bHasPackageJob)
		{
			Root->SetObjectField(TEXT("PackageJob"), PackageJobCreateResultToJson(PackageJobResult));
		}
		else
		{
			Root->SetStringField(TEXT("PackageJobError"), PackageJobError);
		}
	}
	Root->SetBoolField(TEXT("BuildPackage"), bBuildPackage);
	if (bBuildPackage)
	{
		if (bHasPackageBuild)
		{
			Root->SetObjectField(TEXT("PackageBuild"), PackageBuildLaunchResultToJson(PackageBuildResult));
		}
		else
		{
			Root->SetStringField(TEXT("PackageBuildError"), PackageBuildError);
		}
	}
	Root->SetObjectField(TEXT("NormalizedSpec"), NTEBuildTool::Character::CharacterModSpecToJson(Spec));

	if (!NTEBuildTool::Json::SaveJsonObjectToFile(Root, OutputFilename, Error))
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Could not write CharacterModSpec report: %s"), *Error);
		return 3;
	}

	for (const FString& Warning : Validation.Warnings)
	{
		UE_LOG(LogNTEBuildTool, Warning, TEXT("%s"), *Warning);
	}
	for (const FString& ValidationError : Validation.Errors)
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("%s"), *ValidationError);
	}
	if (bSyncKawaiiFromAssets)
	{
		for (const NTEBuildTool::Character::FNteCharacterKawaiiAssetSyncResult& SyncResult : KawaiiAssetSyncResults)
		{
			for (const FString& UpdatedField : SyncResult.UpdatedFields)
			{
				UE_LOG(LogNTEBuildTool, Display, TEXT("Synced Kawaii preset '%s' field: %s"), *SyncResult.PresetId, *UpdatedField);
			}
		}
	}
	if (bApplyAppearance)
	{
		for (const FString& Warning : AppearanceWriteResult.Warnings)
		{
			UE_LOG(LogNTEBuildTool, Warning, TEXT("%s"), *Warning);
		}
		for (const FString& WriteError : AppearanceWriteResult.Errors)
		{
			UE_LOG(LogNTEBuildTool, Error, TEXT("%s"), *WriteError);
		}
	}
	if (bApplyMaterials)
	{
		for (const FString& Warning : MaterialWriteResult.Warnings)
		{
			UE_LOG(LogNTEBuildTool, Warning, TEXT("%s"), *Warning);
		}
		for (const FString& WriteError : MaterialWriteResult.Errors)
		{
			UE_LOG(LogNTEBuildTool, Error, TEXT("%s"), *WriteError);
		}
	}
	if (bApplyRuntimeActions)
	{
		for (const FString& Warning : RuntimeActionWriteResult.Warnings)
		{
			UE_LOG(LogNTEBuildTool, Warning, TEXT("%s"), *Warning);
		}
		for (const FString& WriteError : RuntimeActionWriteResult.Errors)
		{
			UE_LOG(LogNTEBuildTool, Error, TEXT("%s"), *WriteError);
		}
	}
	if (bApplyKawaii)
	{
		for (const FString& Warning : KawaiiWriteResult.Warnings)
		{
			UE_LOG(LogNTEBuildTool, Warning, TEXT("%s"), *Warning);
		}
		for (const FString& WriteError : KawaiiWriteResult.Errors)
		{
			UE_LOG(LogNTEBuildTool, Error, TEXT("%s"), *WriteError);
		}
	}
	for (const FString& Warning : KawaiiPlan.Warnings)
	{
		UE_LOG(LogNTEBuildTool, Warning, TEXT("%s"), *Warning);
	}
	for (const FString& PlanError : KawaiiPlan.Errors)
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("%s"), *PlanError);
	}
	if (bWritePackageJob && !bHasPackageJob && !PackageJobError.IsEmpty())
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("%s"), *PackageJobError);
	}
	if (bBuildPackage && !bHasPackageBuild && !PackageBuildError.IsEmpty())
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("%s"), *PackageBuildError);
	}

	UE_LOG(
		LogNTEBuildTool,
		Display,
		TEXT("CharacterModSpec report: %s. Errors=%d Warnings=%d"),
		*OutputFilename,
		ReportErrors.Num(),
		ReportWarnings.Num());

	const bool bFailOnWarnings = FParse::Param(*Params, TEXT("FailOnWarnings"));
	const bool bHasReportWarnings = Validation.HasWarnings()
		|| !MaterialPlan.Warnings.IsEmpty()
		|| !RuntimeActionPlan.Warnings.IsEmpty()
		|| !KawaiiPlan.Warnings.IsEmpty()
		|| (bApplyAppearance && AppearanceWriteResult.Warnings.Num() > 0)
		|| (bApplyMaterials && MaterialWriteResult.Warnings.Num() > 0)
		|| (bApplyRuntimeActions && RuntimeActionWriteResult.Warnings.Num() > 0)
		|| (bApplyKawaii && KawaiiWriteResult.Warnings.Num() > 0);
	if (Validation.HasErrors()
		|| !KawaiiPlan.Errors.IsEmpty()
		|| (bApplyAppearance && AppearanceWriteResult.HasErrors())
		|| (bApplyMaterials && MaterialWriteResult.HasErrors())
		|| (bApplyRuntimeActions && RuntimeActionWriteResult.HasErrors())
		|| (bApplyKawaii && KawaiiWriteResult.HasErrors())
		|| (bWritePackageJob && !bHasPackageJob)
		|| (bBuildPackage && !bHasPackageBuild)
		|| (bFailOnWarnings && bHasReportWarnings))
	{
		return 4;
	}
	return 0;
}
