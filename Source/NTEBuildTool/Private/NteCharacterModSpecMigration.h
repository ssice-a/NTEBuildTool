// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "NtePakmodProject.h"

namespace NTEBuildTool::Character
{
struct FNteCharacterModSpec;

struct FNteCharacterModSpecMigrationResult
{
	NTEBuildTool::Project::FNtePakmodProject Project;
	TArray<FString> Warnings;
	TArray<FString> Errors;

	bool HasErrors() const { return !Errors.IsEmpty(); }
};

FNteCharacterModSpecMigrationResult MigrateCharacterModSpecToPakmodProject(const FNteCharacterModSpec& Spec);
bool MigrateCharacterModSpecFileToPakmodProjectFile(
	const FString& CharacterSpecFilename,
	const FString& PakmodProjectFilename,
	FNteCharacterModSpecMigrationResult& OutResult,
	FString& OutError);
}
