// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"

namespace NTEBuildTool::Character
{
struct FNteCurveFloatJsonImportResult
{
	TArray<FString> SavedPackages;
	TArray<FString> Warnings;
	TArray<FString> Errors;

	bool HasErrors() const { return !Errors.IsEmpty(); }
};

FNteCurveFloatJsonImportResult ImportCurveFloatAssetsFromFModelJsonFiles(const TArray<FString>& SourceJsonFiles);
}
