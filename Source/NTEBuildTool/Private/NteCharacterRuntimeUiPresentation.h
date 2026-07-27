// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "NteCharacterRuntimeActionWriter.h"

class UWidgetBlueprint;

namespace NTEBuildTool::Character
{
FName MakeRuntimeActionButtonWidgetName(const FNteCharacterRuntimeActionPlanItem& Action);
FName MakeRuntimeActionLabelWidgetName(const FNteCharacterRuntimeActionPlanItem& Action);

bool RebuildRuntimeActionWidgetPresentation(
	UWidgetBlueprint& WidgetBlueprint,
	const FNteCharacterRuntimeActionPlan& Plan,
	const TArray<const FNteCharacterRuntimeActionPlanItem*>& Actions,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult);
}
