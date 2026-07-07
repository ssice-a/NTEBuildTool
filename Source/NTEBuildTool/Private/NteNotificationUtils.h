// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"

namespace NTEBuildTool::Editor
{
void ShowError(const FText& Message);
void ShowInfo(const FText& Message);
void ShowSuccessNotification(const FText& Message);

bool ChooseJsonFileWithTitle(const FText& Title, const FString& DefaultFilename, FString& OutFilename);
bool ChooseSaveJsonFileWithTitle(const FText& Title, const FString& DefaultFilename, FString& OutFilename);
bool ChooseDirectoryWithTitle(const FText& Title, const FString& DefaultDirectory, FString& OutDirectory);
}
