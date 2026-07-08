// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UWidgetBlueprint;
class FJsonObject;

namespace NTEBuildTool::Toggle
{
struct FNteStandardToggleTemplateModel
{
	TSet<int32> SaveGameVisibleGroups;
	TSet<int32> PostProcessVisibleGroups;
	TSet<int32> PostProcessInputGroups;
	TSet<int32> WidgetButtonGroups;
	TSet<int32> WidgetLabelGroups;

	int32 GetSaveGameVisibleCapacity() const;
	int32 GetPostProcessVisibleCapacity() const;
	int32 GetPostProcessInputCapacity() const;
	int32 GetWidgetButtonCapacity() const;
	int32 GetWidgetLabelCapacity() const;
	int32 GetRuntimeStateCapacity() const;
	TSet<int32> GetWidgetGroups() const;
};

bool ParseStandardToggleVisibleVariableName(const FString& VariableName, int32& OutOrdinal);
bool ParseStandardToggleInputVariableName(const FString& VariableName, int32& OutOrdinal);
bool ParseStandardToggleButtonWidgetName(const FString& WidgetName, int32& OutOrdinal);
bool ParseStandardToggleButtonLabelWidgetName(const FString& WidgetName, int32& OutOrdinal);

FString MakeStandardToggleButtonWidgetName(int32 TemplateGroupOrdinal);
FString MakeStandardToggleButtonLabelWidgetName(int32 TemplateGroupOrdinal);

int32 GetContiguousStandardGroupCapacity(const TSet<int32>& Ordinals);
FNteStandardToggleTemplateModel BuildStandardToggleTemplateModel(
	const UBlueprint* SaveGameBlueprint,
	const UBlueprint* PostProcessAnimBlueprint,
	const UWidgetBlueprint* WidgetBlueprint);
void AddStandardToggleTemplateModelJson(const FNteStandardToggleTemplateModel& Model, FJsonObject& Object);
}
