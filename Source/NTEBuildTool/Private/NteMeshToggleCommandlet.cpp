// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteMeshToggleCommandlet.h"

#include "NTEBuildTool.h"
#include "NteMeshToggleConfig.h"

#include "Misc/Parse.h"
#include "Misc/Paths.h"

UNteMeshToggleCommandlet::UNteMeshToggleCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
	UseCommandletResultAsExitCode = true;
	HelpDescription = TEXT("Validates and applies an NTE mesh toggle setup JSON.");
	HelpUsage = TEXT("UnrealEditor-Cmd.exe <Project>.uproject -run=NteMeshToggle -Config=<json>");
}

int32 UNteMeshToggleCommandlet::Main(const FString& Params)
{
	FString ConfigFilename;
	if (!FParse::Value(*Params, TEXT("Config="), ConfigFilename) && !FParse::Value(*Params, TEXT("Setup="), ConfigFilename))
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Missing -Config=<json>."));
		return 1;
	}

	FPaths::NormalizeFilename(ConfigFilename);
	FString Error;
	NTEBuildTool::Toggle::FNteMeshToggleSetupOptions Options;
	if (!NTEBuildTool::Toggle::LoadMeshToggleSetupOptionsFromJsonFile(ConfigFilename, Options, Error))
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Mesh toggle setup could not be read: %s"), *Error);
		return 2;
	}

	NTEBuildTool::Toggle::FNteMeshToggleSetupResult Result;
	if (!NTEBuildTool::Toggle::RunMeshToggleUiSetup(Options, Result, Error))
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Mesh toggle setup failed: %s"), *Error);
		return 3;
	}

	for (const FString& Warning : Result.Warnings)
	{
		UE_LOG(LogNTEBuildTool, Warning, TEXT("%s"), *Warning);
	}

	UE_LOG(
		LogNTEBuildTool,
		Display,
		TEXT("Mesh toggle setup applied. Mesh=%s Config=%s PostProcess=%s"),
		*Options.MeshPath,
		*Result.ConfigFilename,
		*Result.PostProcessAnimBlueprintPath);
	return 0;
}
