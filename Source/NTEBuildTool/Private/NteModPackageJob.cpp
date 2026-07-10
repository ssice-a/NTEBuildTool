// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteModPackageJob.h"

#include "NTEBuildTool.h"
#include "NteBuildToolSettings.h"
#include "NteEditorAssetUtils.h"
#include "NteJsonFileUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace NTEBuildTool::Package
{
using namespace NTEBuildTool::Json;

bool ValidatePackageJob(const FNteModPackageJob& Job, FString& OutError);

namespace
{
FString PackageModeToString(const ENteModPackageMode Mode)
{
	switch (Mode)
	{
	case ENteModPackageMode::CookOnly:
		return TEXT("CookOnly");
	case ENteModPackageMode::PackOnly:
		return TEXT("PackOnly");
	default:
		return TEXT("CookAndPack");
	}
}

void AddUniqueGamePackage(TArray<FString>& Packages, const FString& PackageName)
{
	if (PackageName.StartsWith(TEXT("/Game/")) && !PackageName.Contains(TEXT(".")))
	{
		Packages.AddUnique(PackageName);
	}
}

FString SanitizeModName(FString ModName)
{
	ModName.TrimStartAndEndInline();
	const FString InvalidChars(FPaths::GetInvalidFileSystemChars());
	for (const TCHAR InvalidChar : InvalidChars)
	{
		ModName.ReplaceCharInline(InvalidChar, TEXT('_'));
	}
	return ModName;
}

FString DeriveModNameFromPackages(const TArray<FString>& Packages)
{
	if (Packages.IsEmpty())
	{
		return TEXT("nte_mod_P");
	}

	FString BaseName = FPackageName::GetShortName(Packages[0]);
	if (BaseName.IsEmpty())
	{
		BaseName = TEXT("nte_mod");
	}
	if (!BaseName.EndsWith(TEXT("_P"), ESearchCase::IgnoreCase))
	{
		BaseName += TEXT("_P");
	}
	return SanitizeModName(BaseName);
}

void CollectSelectedAssetPackages(TArray<FString>& Packages)
{
	for (const FAssetData& AssetData : NTEBuildTool::Editor::GetSelectedContentBrowserAssets())
	{
		AddUniqueGamePackage(Packages, AssetData.PackageName.ToString());
	}
}

void CollectSelectedFolderPackages(TArray<FString>& Packages)
{
	const TArray<FString> SelectedFolders = NTEBuildTool::Editor::GetSelectedContentBrowserPaths();
	if (SelectedFolders.IsEmpty())
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	for (const FString& Folder : SelectedFolders)
	{
		if (!NTEBuildTool::Editor::IsGameContentPath(Folder))
		{
			continue;
		}

		TArray<FAssetData> FolderAssets;
		AssetRegistry.GetAssetsByPath(FName(*Folder), FolderAssets, true);
		for (const FAssetData& AssetData : FolderAssets)
		{
			AddUniqueGamePackage(Packages, AssetData.PackageName.ToString());
		}
	}
}

ENteModPackageMode PackageModeFromString(const FString& Value)
{
	if (Value.Equals(TEXT("CookOnly"), ESearchCase::IgnoreCase))
	{
		return ENteModPackageMode::CookOnly;
	}
	if (Value.Equals(TEXT("PackOnly"), ESearchCase::IgnoreCase))
	{
		return ENteModPackageMode::PackOnly;
	}
	return ENteModPackageMode::CookAndPack;
}

bool LooksLikeGeneratedRuntimeBlueprintPackage(const FString& PackageName)
{
	const FString AssetName = FPackageName::GetShortName(PackageName);
	return AssetName.StartsWith(TEXT("ABP_NTE_ModToggle_"))
		|| AssetName.StartsWith(TEXT("WBP_NTE_ModToggle"))
		|| AssetName.StartsWith(TEXT("BP_NTE_ModToggle"))
		|| (PackageName.Contains(TEXT("/mod/Runtime/"), ESearchCase::IgnoreCase)
			&& (AssetName.StartsWith(TEXT("ABP_")) || AssetName.StartsWith(TEXT("WBP_")) || AssetName.StartsWith(TEXT("BP_"))));
}

bool FindGeneratedRuntimeBlueprintPackage(const FNteModPackageJob& Job, FString& OutPackageName)
{
	for (const FString& PackageName : Job.Packages)
	{
		if (LooksLikeGeneratedRuntimeBlueprintPackage(PackageName))
		{
			OutPackageName = PackageName;
			return true;
		}
	}

	return false;
}

void NormalizeCookOptionsForRuntimeBlueprints(FNteModPackageJob& Job)
{
	if (!Job.bUnversioned)
	{
		return;
	}

	FString RuntimeBlueprintPackage;
	if (!FindGeneratedRuntimeBlueprintPackage(Job, RuntimeBlueprintPackage))
	{
		return;
	}

	Job.bUnversioned = false;
	UE_LOG(
		LogNTEBuildTool,
		Warning,
		TEXT("Package job %s contains runtime Blueprint package %s; disabling Unversioned cook to avoid Widget Blueprint serialization errors in the target game."),
		*Job.ModName,
		*RuntimeBlueprintPackage);
}
}

bool LoadModPackageJobJson(const FString& JobFilename, FNteModPackageJob& OutJob, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	if (!LoadJsonObjectFromFile(JobFilename, Root, OutError))
	{
		return false;
	}

	const FString Format = GetStringAny(*Root, TEXT("Format"), TEXT("format"));
	if (!Format.IsEmpty() && Format != TEXT("NTE.ModPackageJob") && Format != TEXT("NTE.ModPackageProfile"))
	{
		OutError = FString::Printf(TEXT("Unsupported package job format: %s"), *Format);
		return false;
	}

	OutJob.ProjectRoot = GetStringAny(*Root, TEXT("ProjectRoot"), TEXT("projectRoot"));
	OutJob.ProjectFile = GetStringAny(*Root, TEXT("ProjectFile"), TEXT("projectFile"));
	OutJob.ProjectName = GetStringAny(*Root, TEXT("ProjectName"), TEXT("projectName"));
	OutJob.EngineRoot = GetStringAny(*Root, TEXT("EngineRoot"), TEXT("engineRoot"));
	OutJob.GameMountName = GetStringAny(*Root, TEXT("GameMountName"), TEXT("GameMount"), TEXT("gameMountName"));
	OutJob.ModsDir = GetStringAny(*Root, TEXT("ModsDir"), TEXT("modsDir"));
	OutJob.ModName = GetStringAny(*Root, TEXT("ModName"), TEXT("modName"));
	OutJob.Mode = PackageModeFromString(GetStringAny(*Root, TEXT("Mode"), TEXT("mode")));
	OutJob.Packages = GetStringArrayAny(*Root, TEXT("Packages"), TEXT("packages"));
	OutJob.NeverPackPackagePrefixes = GetStringArrayAny(*Root, TEXT("NeverPackPackagePrefixes"), TEXT("neverPackPackagePrefixes"));
	GetBoolAny(*Root, OutJob.bUnversioned, TEXT("Unversioned"), TEXT("unversioned"));
	GetBoolAny(*Root, OutJob.bSkipCook, TEXT("SkipCook"), TEXT("skipCook"));
	if (OutJob.bSkipCook && OutJob.Mode == ENteModPackageMode::CookAndPack)
	{
		OutJob.Mode = ENteModPackageMode::PackOnly;
	}

	if (OutJob.GameMountName.IsEmpty())
	{
		OutJob.GameMountName = NTEBuildTool::Settings::GetGameMountName();
	}

	return true;
}

bool SaveModPackageJobJson(const FNteModPackageJob& Job, const FString& JobFilename, FString& OutError)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("Format"), TEXT("NTE.ModPackageJob"));
	Root->SetNumberField(TEXT("Version"), 1.0);
	Root->SetStringField(TEXT("ProjectRoot"), Job.ProjectRoot);
	Root->SetStringField(TEXT("ProjectFile"), Job.ProjectFile);
	Root->SetStringField(TEXT("ProjectName"), Job.ProjectName);
	Root->SetStringField(TEXT("EngineRoot"), Job.EngineRoot);
	Root->SetStringField(TEXT("GameMountName"), Job.GameMountName);
	Root->SetStringField(TEXT("ModsDir"), Job.ModsDir);
	Root->SetStringField(TEXT("ModName"), Job.ModName);
	Root->SetStringField(TEXT("Mode"), PackageModeToString(Job.Mode));
	Root->SetArrayField(TEXT("Packages"), StringArrayToJsonValues(Job.Packages));
	Root->SetArrayField(TEXT("NeverPackPackagePrefixes"), StringArrayToJsonValues(Job.NeverPackPackagePrefixes));
	Root->SetBoolField(TEXT("Unversioned"), Job.bUnversioned);
	Root->SetBoolField(TEXT("SkipCook"), Job.bSkipCook || Job.Mode == ENteModPackageMode::PackOnly);
	return SaveJsonObjectToFile(Root, JobFilename, OutError);
}

bool CreateModPackageJobFromSelection(const FNteModPackageJobCreateOptions& Options, FNteModPackageJobCreateResult& OutResult, FString& OutError)
{
	OutResult = FNteModPackageJobCreateResult();

	TArray<FString> Packages = Options.Packages;
	if (Options.bCollectContentBrowserSelection)
	{
		CollectSelectedAssetPackages(Packages);
		CollectSelectedFolderPackages(Packages);
	}
	Packages.Sort();

	if (Packages.IsEmpty())
	{
		OutError = TEXT("Select at least one Content Browser asset or /Game folder before creating a package job.");
		return false;
	}

	const FString ModName = SanitizeModName(!Options.ModName.IsEmpty() ? Options.ModName : DeriveModNameFromPackages(Packages));
	if (ModName.IsEmpty())
	{
		OutError = TEXT("ModName is empty after sanitization.");
		return false;
	}

	FNteModPackageJob Job;
	Job.ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeFilename(Job.ProjectRoot);
	Job.ProjectFile = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	FPaths::NormalizeFilename(Job.ProjectFile);
	Job.ProjectName = FApp::GetProjectName();
	Job.EngineRoot = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT(".."));
	FPaths::NormalizeFilename(Job.EngineRoot);
	Job.GameMountName = Options.GameMountName.IsEmpty() ? NTEBuildTool::Settings::GetGameMountName() : Options.GameMountName;
	Job.ModsDir = Options.ModsDir.IsEmpty() ? NTEBuildTool::Settings::GetDefaultModsOutputDirectory() : Options.ModsDir;
	Job.ModName = ModName;
	Job.Mode = Options.Mode;
	Job.Packages = Packages;
	Job.NeverPackPackagePrefixes = Options.NeverPackPackagePrefixes;
	Job.bUnversioned = Options.bUnversioned;
	Job.bSkipCook = Options.bSkipCook || Options.Mode == ENteModPackageMode::PackOnly;
	NormalizeCookOptionsForRuntimeBlueprints(Job);

	if (!ValidatePackageJob(Job, OutError))
	{
		return false;
	}

	FString JobFilename = Options.JobFilename;
	if (JobFilename.IsEmpty())
	{
		JobFilename = FPaths::ProjectSavedDir() / TEXT("NTEBuildTool/Packages") / ModName / (ModName + TEXT(".job.json"));
	}
	FPaths::NormalizeFilename(JobFilename);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(JobFilename), true);

	if (!SaveModPackageJobJson(Job, JobFilename, OutError))
	{
		return false;
	}

	OutResult.JobFile = JobFilename;
	OutResult.ModName = ModName;
	OutResult.PackageCount = Packages.Num();
	OutResult.Job = Job;
	return true;
}

bool ValidatePackageJob(const FNteModPackageJob& Job, FString& OutError)
{
	if (Job.ProjectRoot.IsEmpty() || !FPaths::DirectoryExists(Job.ProjectRoot))
	{
		OutError = FString::Printf(TEXT("ProjectRoot does not exist: %s"), *Job.ProjectRoot);
		return false;
	}
	if (Job.ProjectFile.IsEmpty() || !FPaths::FileExists(Job.ProjectFile))
	{
		OutError = FString::Printf(TEXT("ProjectFile does not exist: %s"), *Job.ProjectFile);
		return false;
	}
	if (Job.EngineRoot.IsEmpty() || !FPaths::DirectoryExists(Job.EngineRoot))
	{
		OutError = FString::Printf(TEXT("EngineRoot does not exist: %s"), *Job.EngineRoot);
		return false;
	}
	if (Job.ModsDir.IsEmpty())
	{
		OutError = TEXT("ModsDir is empty.");
		return false;
	}
	if (Job.ModName.IsEmpty())
	{
		OutError = TEXT("ModName is empty.");
		return false;
	}
	if (Job.Packages.IsEmpty())
	{
		OutError = TEXT("Package job has no Packages.");
		return false;
	}

	for (const FString& PackageName : Job.Packages)
	{
		if (!PackageName.StartsWith(TEXT("/Game/")) || PackageName.Contains(TEXT(".")))
		{
			OutError = FString::Printf(TEXT("Package must be a /Game package path without object suffix: %s"), *PackageName);
			return false;
		}
		for (const FString& Prefix : Job.NeverPackPackagePrefixes)
		{
			if (!Prefix.IsEmpty() && PackageName.StartsWith(Prefix))
			{
				OutError = FString::Printf(TEXT("Refusing to pack %s because it matches forbidden prefix %s."), *PackageName, *Prefix);
				return false;
			}
		}
	}

	return true;
}

FString QuoteArgument(const FString& Value)
{
	return TEXT("\"") + Value.Replace(TEXT("\""), TEXT("\\\"")) + TEXT("\"");
}

FString ToPlatformFilename(FString Filename)
{
	FPaths::MakePlatformFilename(Filename);
	return Filename;
}

FString ResolveSystemExecutable(const FString& RelativeSystem32Path, const FString& FallbackName)
{
	const FString SystemRoot = FPlatformMisc::GetEnvironmentVariable(TEXT("SystemRoot"));
	if (!SystemRoot.IsEmpty())
	{
		const FString Candidate = FPaths::Combine(SystemRoot, RelativeSystem32Path);
		if (FPaths::FileExists(Candidate))
		{
			return ToPlatformFilename(Candidate);
		}
	}

	const FString WindowsCandidate = FPaths::Combine(TEXT("C:/Windows"), RelativeSystem32Path);
	if (FPaths::FileExists(WindowsCandidate))
	{
		return ToPlatformFilename(WindowsCandidate);
	}

	return FallbackName;
}

FString GetPowerShellExe()
{
	return ResolveSystemExecutable(TEXT("System32/WindowsPowerShell/v1.0/powershell.exe"), TEXT("powershell.exe"));
}

FString GetCmdExe()
{
	return ResolveSystemExecutable(TEXT("System32/cmd.exe"), TEXT("cmd.exe"));
}

bool IsFreshNonEmptyFile(const FString& Filename, const FDateTime BuildStartTime)
{
	if (!FPaths::FileExists(Filename))
	{
		return false;
	}

	if (IFileManager::Get().FileSize(*Filename) <= 0)
	{
		return false;
	}

	const FDateTime Timestamp = IFileManager::Get().GetTimeStamp(*Filename);
	return Timestamp >= BuildStartTime - FTimespan::FromSeconds(2.0);
}

void DeleteKnownPackageOutputs(const FNteModPackageJob& Job, const FString& WorkRoot)
{
	const FString ContainerBase = FPaths::Combine(WorkRoot, Job.ModName);
	for (const TCHAR* Extension : { TEXT(".pak"), TEXT(".utoc"), TEXT(".ucas"), TEXT(".response.txt"), TEXT(".iostore.txt"), TEXT(".empty-pak.txt") })
	{
		IFileManager::Get().Delete(*(ContainerBase + Extension), false, true);
	}

	const FString GlobalContainerBase = FPaths::Combine(WorkRoot, TEXT("global"));
	for (const TCHAR* Extension : { TEXT(".utoc"), TEXT(".ucas") })
	{
		IFileManager::Get().Delete(*(GlobalContainerBase + Extension), false, true);
	}

	for (const TCHAR* Extension : { TEXT(".pak"), TEXT(".utoc"), TEXT(".ucas") })
	{
		IFileManager::Get().Delete(*FPaths::Combine(Job.ModsDir, Job.ModName + Extension), false, true);
	}
}

bool ValidatePackageBuildOutputs(const FNteModPackageJob& Job, const FString& WorkRoot, const FDateTime BuildStartTime, FString& OutError)
{
	const FString CookedRoot = FPaths::Combine(WorkRoot, TEXT("Cooked"));
	if (Job.Mode == ENteModPackageMode::CookOnly)
	{
		if (!FPaths::DirectoryExists(CookedRoot))
		{
			OutError = FString::Printf(TEXT("CookOnly job finished but cooked output directory was not found: %s"), *CookedRoot);
			return false;
		}

		return true;
	}

	const FString ResponseFile = FPaths::Combine(WorkRoot, Job.ModName + TEXT(".response.txt"));
	if (!IsFreshNonEmptyFile(ResponseFile, BuildStartTime))
	{
		OutError = FString::Printf(TEXT("Package response file was not produced or is stale: %s"), *ResponseFile);
		return false;
	}

	for (const TCHAR* Extension : { TEXT(".pak"), TEXT(".utoc"), TEXT(".ucas") })
	{
		const FString WorkOutput = FPaths::Combine(WorkRoot, Job.ModName + Extension);
		if (!IsFreshNonEmptyFile(WorkOutput, BuildStartTime))
		{
			OutError = FString::Printf(TEXT("Package output was not produced or is stale: %s"), *WorkOutput);
			return false;
		}

		const FString ModsOutput = FPaths::Combine(Job.ModsDir, Job.ModName + Extension);
		if (!IsFreshNonEmptyFile(ModsOutput, BuildStartTime))
		{
			OutError = FString::Printf(TEXT("Package output was not copied to ModsDir or is stale: %s"), *ModsOutput);
			return false;
		}
	}

	return true;
}

FString TailForError(FString Text)
{
	Text.TrimStartAndEndInline();
	constexpr int32 MaxChars = 4000;
	return Text.Len() > MaxChars ? TEXT("...") + Text.Right(MaxChars) : Text;
}

FString MakeProcessFailureMessage(const FString& BaseMessage, const FString& StdOut, const FString& StdErr)
{
	FString Message = BaseMessage;
	const FString StdErrTail = TailForError(StdErr);
	const FString StdOutTail = TailForError(StdOut);
	if (!StdErrTail.IsEmpty())
	{
		Message += LINE_TERMINATOR TEXT("stderr: ") + StdErrTail;
	}
	if (!StdOutTail.IsEmpty())
	{
		Message += LINE_TERMINATOR TEXT("stdout: ") + StdOutTail;
	}
	return Message;
}

bool WritePackageCommandScript(
	const FString& CommandScript,
	const FString& PowerShellExe,
	const FString& PackageScript,
	const FString& JobFile,
	const FString& StdOutLog,
	const FString& StdErrLog,
	FString& OutError)
{
	const FString ScriptBody =
		TEXT("@echo off\r\n")
		TEXT("setlocal\r\n") +
		QuoteArgument(PowerShellExe) +
		TEXT(" -NoProfile -ExecutionPolicy Bypass -File ") +
		QuoteArgument(ToPlatformFilename(PackageScript)) +
		TEXT(" -JobFile ") +
		QuoteArgument(ToPlatformFilename(JobFile)) +
		TEXT(" > ") +
		QuoteArgument(ToPlatformFilename(StdOutLog)) +
		TEXT(" 2> ") +
		QuoteArgument(ToPlatformFilename(StdErrLog)) +
		TEXT("\r\nset EXITCODE=%ERRORLEVEL%\r\n")
		TEXT("exit /b %EXITCODE%\r\n");

	if (!FFileHelper::SaveStringToFile(ScriptBody, *CommandScript, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("Could not write package command script: %s"), *CommandScript);
		return false;
	}

	return true;
}

bool LaunchModPackageBuildJob(const FString& JobFilename, FNteModPackageLaunchResult& OutResult, FString& OutError)
{
	OutResult = FNteModPackageLaunchResult();

	FNteModPackageJob Job;
	if (!LoadModPackageJobJson(JobFilename, Job, OutError))
	{
		return false;
	}
	if (!ValidatePackageJob(Job, OutError))
	{
		return false;
	}

	if (Job.ProjectName.IsEmpty())
	{
		Job.ProjectName = FPaths::GetBaseFilename(Job.ProjectFile);
	}
	if (Job.GameMountName.IsEmpty())
	{
		Job.GameMountName = NTEBuildTool::Settings::GetGameMountName();
	}
	Job.bSkipCook = Job.bSkipCook || Job.Mode == ENteModPackageMode::PackOnly;
	NormalizeCookOptionsForRuntimeBlueprints(Job);

	const FString WorkRoot = FPaths::Combine(Job.ProjectRoot, TEXT("Saved/NTEBuildTool/Packages"), Job.ModName);
	const FString ScriptOutputDir = FPaths::Combine(WorkRoot, TEXT("Scripts"));
	const FString CopiedScript = FPaths::Combine(ScriptOutputDir, TEXT("BuildNteMod.ps1"));
	const FString NormalizedJobFile = FPaths::Combine(WorkRoot, Job.ModName + TEXT(".job.json"));
	IFileManager::Get().MakeDirectory(*ScriptOutputDir, true);

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("NTEBuildTool"));
	if (!Plugin.IsValid())
	{
		OutError = TEXT("Could not locate NTEBuildTool plugin.");
		return false;
	}

	const FString SourceScript = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/Scripts/BuildNteMod.ps1"));
	if (!FPaths::FileExists(SourceScript))
	{
		OutError = FString::Printf(TEXT("Package build script is missing: %s"), *SourceScript);
		return false;
	}
	if (IFileManager::Get().Copy(*CopiedScript, *SourceScript, true, true) != COPY_OK)
	{
		OutError = FString::Printf(TEXT("Could not copy package build script to: %s"), *CopiedScript);
		return false;
	}
	if (!SaveModPackageJobJson(Job, NormalizedJobFile, OutError))
	{
		return false;
	}

	const FDateTime BuildStartTime = FDateTime::UtcNow();
	DeleteKnownPackageOutputs(Job, WorkRoot);
	const FString StdOutLog = FPaths::Combine(WorkRoot, TEXT("package.stdout.log"));
	const FString StdErrLog = FPaths::Combine(WorkRoot, TEXT("package.stderr.log"));
	IFileManager::Get().Delete(*StdOutLog, false, true);
	IFileManager::Get().Delete(*StdErrLog, false, true);

	int32 ReturnCode = 0;
	FString StdOut;
	FString StdErr;
	OutResult.JobFile = NormalizedJobFile;
	OutResult.WorkRoot = WorkRoot;

	const FString PowerShellExe = GetPowerShellExe();
	const FString CmdExe = GetCmdExe();
	const FString CommandScript = FPaths::Combine(WorkRoot, Job.ModName + TEXT(".run-package.cmd"));
	if (!WritePackageCommandScript(CommandScript, PowerShellExe, CopiedScript, NormalizedJobFile, StdOutLog, StdErrLog, OutError))
	{
		return false;
	}

	const FString CmdArguments = FString::Printf(TEXT("/d /c call %s"), *QuoteArgument(ToPlatformFilename(CommandScript)));
	OutResult.CommandLine = QuoteArgument(CmdExe) + TEXT(" ") + CmdArguments;

	if (!FPlatformProcess::ExecProcess(*CmdExe, *CmdArguments, &ReturnCode, nullptr, nullptr, *ToPlatformFilename(WorkRoot)))
	{
		OutError = FString::Printf(TEXT("Could not start package process: %s"), *OutResult.CommandLine);
		return false;
	}

	FFileHelper::LoadFileToString(StdOut, *StdOutLog);
	FFileHelper::LoadFileToString(StdErr, *StdErrLog);
	if (ReturnCode != 0)
	{
		OutError = MakeProcessFailureMessage(
			FString::Printf(TEXT("Package process failed with exit code %d. Log: %s"), ReturnCode, *StdErrLog),
			StdOut,
			StdErr);
		return false;
	}

	if (!ValidatePackageBuildOutputs(Job, WorkRoot, BuildStartTime, OutError))
	{
		OutError = MakeProcessFailureMessage(
			TEXT("Package process exited successfully but did not produce verified outputs: ") + OutError,
			StdOut,
			StdErr);
		return false;
	}

	return true;
}
}
