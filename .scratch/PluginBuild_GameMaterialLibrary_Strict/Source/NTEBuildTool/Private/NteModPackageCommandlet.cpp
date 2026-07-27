// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteModPackageCommandlet.h"

#include "NTEBuildTool.h"
#include "NteModPackageJob.h"

#include "Misc/Parse.h"
#include "Misc/Paths.h"

UNteModPackageCommandlet::UNteModPackageCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
	UseCommandletResultAsExitCode = true;
	HelpDescription = TEXT("Builds an NTE mod package from a package job JSON.");
	HelpUsage = TEXT("UnrealEditor-Cmd.exe <Project>.uproject -run=NteModPackage -Job=<json>");
}

int32 UNteModPackageCommandlet::Main(const FString& Params)
{
	FString JobFilename;
	if (!FParse::Value(*Params, TEXT("Job="), JobFilename) && !FParse::Value(*Params, TEXT("Config="), JobFilename))
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Missing -Job=<json>."));
		return 1;
	}

	FPaths::NormalizeFilename(JobFilename);
	FString Error;
	NTEBuildTool::Package::FNteModPackageLaunchResult Result;
	if (!NTEBuildTool::Package::LaunchModPackageBuildJob(JobFilename, Result, Error))
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Package build failed: %s"), *Error);
		return 2;
	}

	UE_LOG(LogNTEBuildTool, Display, TEXT("Package build finished. Job=%s WorkRoot=%s"), *Result.JobFile, *Result.WorkRoot);
	return 0;
}
