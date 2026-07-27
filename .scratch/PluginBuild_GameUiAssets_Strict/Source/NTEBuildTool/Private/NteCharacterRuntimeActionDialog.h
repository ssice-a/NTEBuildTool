// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "NteCharacterModSpec.h"

namespace NTEBuildTool::Character
{
struct FNteCharacterRuntimeActionDialogDefaults
{
	FString TargetMeshId = TEXT("main");
	TArray<int32> MaterialSlots;
	FString SlotName;
};

bool ShowCharacterRuntimeActionDialog(
	const FNteCharacterModSpec& Spec,
	const FNteCharacterRuntimeActionDialogDefaults& Defaults,
	FNteCharacterRuntimeActionSpec& OutAction);
}
