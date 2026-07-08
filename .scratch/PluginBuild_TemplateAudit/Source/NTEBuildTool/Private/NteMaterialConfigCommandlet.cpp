// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteMaterialConfigCommandlet.h"

#include "NTEBuildTool.h"
#include "NteMaterialInstanceTool.h"

#include "Misc/Parse.h"
#include "Misc/Paths.h"

UNteMaterialConfigCommandlet::UNteMaterialConfigCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
	UseCommandletResultAsExitCode = true;
	HelpDescription = TEXT("Applies an NTE material instance recipe JSON.");
	HelpUsage = TEXT("UnrealEditor-Cmd.exe <Project>.uproject -run=NteMaterialConfig -Config=<json>");
}

int32 UNteMaterialConfigCommandlet::Main(const FString& Params)
{
	FString ConfigFilename;
	if (!FParse::Value(*Params, TEXT("Config="), ConfigFilename) && !FParse::Value(*Params, TEXT("NTEBuildToolMaterialConfig="), ConfigFilename))
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Missing -Config=<json>."));
		return 1;
	}

	FPaths::NormalizeFilename(ConfigFilename);
	FString Error;
	NTEBuildTool::Material::FNteMaterialConfigApplyResult Result;
	if (!NTEBuildTool::Material::ApplyModMaterialConfigFromFile(ConfigFilename, Result, Error))
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Material config failed: %s"), *Error);
		return 2;
	}

	UE_LOG(LogNTEBuildTool, Display, TEXT("Material config applied. Output=%s Report=%s"), *Result.OutputMaterialPath, *Result.ReportFilename);
	return 0;
}
