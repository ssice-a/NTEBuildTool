// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace NTEBuildTool::Character
{
struct FNteAppearanceAssemblyPlan;

struct FNteAppearanceAssemblyWriteOptions
{
	bool bWritePlayerAppearance = true;
	bool bSyncPlayerUIShow = true;
};

struct FNteAppearanceAssemblyWriteResult
{
	TArray<FString> SavedPackages;
	TArray<FString> WrittenUIShowComponents;
	TArray<FString> Errors;
	TArray<FString> Warnings;

	bool HasErrors() const { return !Errors.IsEmpty(); }
};

FNteAppearanceAssemblyWriteResult WriteAppearanceAssembly(
	const FNteAppearanceAssemblyPlan& Plan,
	const FNteAppearanceAssemblyWriteOptions& Options = FNteAppearanceAssemblyWriteOptions());

TSharedRef<FJsonObject> AppearanceAssemblyWriteResultToJson(const FNteAppearanceAssemblyWriteResult& Result);
}
