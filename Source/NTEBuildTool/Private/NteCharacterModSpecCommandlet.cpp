// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteCharacterModSpecCommandlet.h"

#include "NTEBuildTool.h"
#include "NteAppearanceAssemblyPlan.h"
#include "NteAppearanceAssemblyWriter.h"
#include "NteCharacterModSpec.h"
#include "NteCharacterRuntimeActionPlan.h"
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
	HelpUsage = TEXT("UnrealEditor-Cmd.exe <Project>.uproject -run=NteCharacterModSpec -Spec=<json> [-Output=<json>] [-ApplyAppearance] [-WritePackageJob] [-BuildPackage] [-FailOnWarnings]");
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

	const NTEBuildTool::Character::FNteCharacterModSpecValidationResult Validation =
		NTEBuildTool::Character::ValidateCharacterModSpec(Spec);
	const TArray<FString> PackageSeeds = NTEBuildTool::Character::CollectCharacterModSpecPackageSeeds(Spec);
	const NTEBuildTool::Character::FNteAppearanceAssemblyPlan AppearancePlan =
		NTEBuildTool::Character::BuildAppearanceAssemblyPlanFromSpec(Spec);
	const NTEBuildTool::Character::FNteCharacterRuntimeActionPlan RuntimeActionPlan =
		NTEBuildTool::Character::BuildCharacterRuntimeActionPlanFromSpec(Spec);
	const bool bApplyAppearance = FParse::Param(*Params, TEXT("ApplyAppearance"));
	NTEBuildTool::Character::FNteAppearanceAssemblyWriteResult AppearanceWriteResult;
	if (bApplyAppearance && !Validation.HasErrors())
	{
		AppearanceWriteResult = NTEBuildTool::Character::WriteAppearanceAssembly(AppearancePlan);
	}

	FString PackagePlanError;
	NTEBuildTool::Package::FNtePackagePlan PackagePlan;
	const bool bHasPackagePlan = NTEBuildTool::Package::BuildPackagePlanFromCharacterModSpec(Spec, PackagePlan, PackagePlanError);
	const bool bBuildPackage = FParse::Param(*Params, TEXT("BuildPackage")) || Spec.Package.bBuildAfterCreate;
	const bool bWritePackageJob = FParse::Param(*Params, TEXT("WritePackageJob")) || bBuildPackage;
	FString PackageJobError;
	NTEBuildTool::Package::FNteModPackageJobCreateResult PackageJobResult;
	bool bHasPackageJob = false;
	if (bWritePackageJob && bHasPackagePlan && !Validation.HasErrors())
	{
		bHasPackageJob = NTEBuildTool::Package::CreateModPackageJobFromCharacterModSpec(Spec, PackagePlan, PackageJobResult, PackageJobError);
	}
	FString PackageBuildError;
	NTEBuildTool::Package::FNteModPackageLaunchResult PackageBuildResult;
	bool bHasPackageBuild = false;
	if (bBuildPackage && bHasPackageJob)
	{
		bHasPackageBuild = NTEBuildTool::Package::LaunchModPackageBuildJob(PackageJobResult.JobFile, PackageBuildResult, PackageBuildError);
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
	Root->SetNumberField(TEXT("ErrorCount"), Validation.Errors.Num());
	Root->SetNumberField(TEXT("WarningCount"), Validation.Warnings.Num());
	Root->SetArrayField(TEXT("Errors"), StringArrayToJsonValues(Validation.Errors));
	Root->SetArrayField(TEXT("Warnings"), StringArrayToJsonValues(Validation.Warnings));
	Root->SetArrayField(TEXT("PackageSeeds"), StringArrayToJsonValues(PackageSeeds));
	Root->SetObjectField(TEXT("AppearanceAssemblyPlan"), NTEBuildTool::Character::AppearanceAssemblyPlanToJson(AppearancePlan));
	Root->SetObjectField(TEXT("RuntimeActionPlan"), NTEBuildTool::Character::CharacterRuntimeActionPlanToJson(RuntimeActionPlan));
	Root->SetBoolField(TEXT("ApplyAppearance"), bApplyAppearance);
	if (bApplyAppearance)
	{
		Root->SetObjectField(TEXT("AppearanceAssemblyWriteResult"), NTEBuildTool::Character::AppearanceAssemblyWriteResultToJson(AppearanceWriteResult));
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
		Validation.Errors.Num(),
		Validation.Warnings.Num());

	const bool bFailOnWarnings = FParse::Param(*Params, TEXT("FailOnWarnings"));
	if (Validation.HasErrors()
		|| (bApplyAppearance && AppearanceWriteResult.HasErrors())
		|| (bWritePackageJob && !bHasPackageJob)
		|| (bBuildPackage && !bHasPackageBuild)
		|| (bFailOnWarnings && (Validation.HasWarnings() || (bApplyAppearance && AppearanceWriteResult.Warnings.Num() > 0))))
	{
		return 4;
	}
	return 0;
}
