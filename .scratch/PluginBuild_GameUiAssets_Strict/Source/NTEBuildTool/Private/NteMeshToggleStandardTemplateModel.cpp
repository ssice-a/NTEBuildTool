// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteMeshToggleStandardTemplateModel.h"

#include "Blueprint/WidgetTree.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_Variable.h"
#include "WidgetBlueprint.h"

namespace NTEBuildTool::Toggle
{
namespace
{
bool ParsePositiveIntegerSuffix(const FString& Text, int32& OutValue)
{
	OutValue = INDEX_NONE;
	if (Text.IsEmpty() || !Text.IsNumeric())
	{
		return false;
	}

	OutValue = FCString::Atoi(*Text);
	return OutValue > 0;
}

void CollectBlueprintVariableOrdinals(
	const UBlueprint& Blueprint,
	TSet<int32>& VisibleGroups,
	TSet<int32>& InputGroups)
{
	for (const FBPVariableDescription& Variable : Blueprint.NewVariables)
	{
		int32 Ordinal = INDEX_NONE;
		if (ParseStandardToggleVisibleVariableName(Variable.VarName.ToString(), Ordinal))
		{
			VisibleGroups.Add(Ordinal);
		}
		if (ParseStandardToggleInputVariableName(Variable.VarName.ToString(), Ordinal))
		{
			InputGroups.Add(Ordinal);
		}
	}

	const auto AddFromGraphs = [&VisibleGroups, &InputGroups](const TArray<TObjectPtr<UEdGraph>>& Graphs)
	{
		for (const UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}

			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				const UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node);
				if (!VariableNode)
				{
					continue;
				}

				const FString VariableName = VariableNode->GetVarNameString();
				int32 Ordinal = INDEX_NONE;
				if (ParseStandardToggleVisibleVariableName(VariableName, Ordinal))
				{
					VisibleGroups.Add(Ordinal);
				}
				if (ParseStandardToggleInputVariableName(VariableName, Ordinal))
				{
					InputGroups.Add(Ordinal);
				}
			}
		}
	};

	AddFromGraphs(Blueprint.UbergraphPages);
	AddFromGraphs(Blueprint.FunctionGraphs);
	AddFromGraphs(Blueprint.MacroGraphs);
}

TArray<TSharedPtr<FJsonValue>> OrdinalSetToJsonValues(const TSet<int32>& Ordinals)
{
	TArray<int32> SortedOrdinals = Ordinals.Array();
	SortedOrdinals.Sort();

	TArray<TSharedPtr<FJsonValue>> Values;
	for (const int32 Ordinal : SortedOrdinals)
	{
		Values.Add(MakeShared<FJsonValueNumber>(Ordinal));
	}
	return Values;
}
}

int32 FNteStandardToggleTemplateModel::GetSaveGameVisibleCapacity() const
{
	return GetContiguousStandardGroupCapacity(SaveGameVisibleGroups);
}

int32 FNteStandardToggleTemplateModel::GetPostProcessVisibleCapacity() const
{
	return GetContiguousStandardGroupCapacity(PostProcessVisibleGroups);
}

int32 FNteStandardToggleTemplateModel::GetPostProcessInputCapacity() const
{
	return GetContiguousStandardGroupCapacity(PostProcessInputGroups);
}

int32 FNteStandardToggleTemplateModel::GetWidgetButtonCapacity() const
{
	return GetContiguousStandardGroupCapacity(WidgetButtonGroups);
}

int32 FNteStandardToggleTemplateModel::GetWidgetLabelCapacity() const
{
	return GetContiguousStandardGroupCapacity(WidgetLabelGroups);
}

int32 FNteStandardToggleTemplateModel::GetRuntimeStateCapacity() const
{
	return FMath::Min(GetSaveGameVisibleCapacity(), GetPostProcessVisibleCapacity());
}

TSet<int32> FNteStandardToggleTemplateModel::GetWidgetGroups() const
{
	TSet<int32> Groups = WidgetButtonGroups;
	Groups.Append(WidgetLabelGroups);
	return Groups;
}

bool ParseStandardToggleVisibleVariableName(const FString& VariableName, int32& OutOrdinal)
{
	OutOrdinal = INDEX_NONE;
	if (!VariableName.StartsWith(TEXT("NTE_Toggle_")) || !VariableName.Contains(TEXT("_toggle_group_")) || !VariableName.EndsWith(TEXT("_Visible")))
	{
		return false;
	}

	FString Remaining = VariableName.RightChop(FCString::Strlen(TEXT("NTE_Toggle_")));
	FString OrdinalText;
	if (!Remaining.Split(TEXT("_"), &OrdinalText, &Remaining))
	{
		return false;
	}

	return ParsePositiveIntegerSuffix(OrdinalText, OutOrdinal);
}

bool ParseStandardToggleInputVariableName(const FString& VariableName, int32& OutOrdinal)
{
	OutOrdinal = INDEX_NONE;
	if (!VariableName.StartsWith(TEXT("NTE_Toggle_Input_")))
	{
		return false;
	}

	FString Tail = VariableName.RightChop(FCString::Strlen(TEXT("NTE_Toggle_Input_")));
	FString OrdinalText;
	if (!Tail.Split(TEXT("_"), &OrdinalText, &Tail))
	{
		return false;
	}

	return ParsePositiveIntegerSuffix(OrdinalText, OutOrdinal);
}

bool ParseStandardToggleButtonWidgetName(const FString& WidgetName, int32& OutOrdinal)
{
	OutOrdinal = INDEX_NONE;
	if (!WidgetName.StartsWith(TEXT("NTE_Toggle_Button_")) || WidgetName.EndsWith(TEXT("_Label")))
	{
		return false;
	}

	FString Prefix;
	FString OrdinalText;
	if (!WidgetName.Split(TEXT("_toggle_group_"), &Prefix, &OrdinalText))
	{
		return false;
	}

	return ParsePositiveIntegerSuffix(OrdinalText, OutOrdinal);
}

bool ParseStandardToggleButtonLabelWidgetName(const FString& WidgetName, int32& OutOrdinal)
{
	OutOrdinal = INDEX_NONE;
	if (!WidgetName.StartsWith(TEXT("NTE_Toggle_Button_")) || !WidgetName.EndsWith(TEXT("_Label")))
	{
		return false;
	}

	FString Prefix;
	FString OrdinalText;
	if (!WidgetName.LeftChop(FCString::Strlen(TEXT("_Label"))).Split(TEXT("_toggle_group_"), &Prefix, &OrdinalText))
	{
		return false;
	}

	return ParsePositiveIntegerSuffix(OrdinalText, OutOrdinal);
}

FString MakeStandardToggleButtonWidgetName(const int32 TemplateGroupOrdinal)
{
	return FString::Printf(
		TEXT("NTE_Toggle_Button_%02d_toggle_group_%d"),
		TemplateGroupOrdinal,
		TemplateGroupOrdinal);
}

FString MakeStandardToggleButtonLabelWidgetName(const int32 TemplateGroupOrdinal)
{
	return FString::Printf(
		TEXT("NTE_Toggle_Button_%02d_toggle_group_%d_Label"),
		TemplateGroupOrdinal,
		TemplateGroupOrdinal);
}

int32 GetContiguousStandardGroupCapacity(const TSet<int32>& Ordinals)
{
	int32 Capacity = 0;
	while (Ordinals.Contains(Capacity + 1))
	{
		++Capacity;
	}
	return Capacity;
}

FNteStandardToggleTemplateModel BuildStandardToggleTemplateModel(
	const UBlueprint* SaveGameBlueprint,
	const UBlueprint* PostProcessAnimBlueprint,
	const UWidgetBlueprint* WidgetBlueprint)
{
	FNteStandardToggleTemplateModel Model;
	if (SaveGameBlueprint)
	{
		TSet<int32> IgnoredInputGroups;
		CollectBlueprintVariableOrdinals(*SaveGameBlueprint, Model.SaveGameVisibleGroups, IgnoredInputGroups);
	}
	if (PostProcessAnimBlueprint)
	{
		CollectBlueprintVariableOrdinals(*PostProcessAnimBlueprint, Model.PostProcessVisibleGroups, Model.PostProcessInputGroups);
	}
	if (WidgetBlueprint && WidgetBlueprint->WidgetTree)
	{
		TArray<UWidget*> Widgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(Widgets);
		for (const UWidget* Widget : Widgets)
		{
			if (!Widget)
			{
				continue;
			}

			int32 Ordinal = INDEX_NONE;
			const FString WidgetName = Widget->GetName();
			if (ParseStandardToggleButtonWidgetName(WidgetName, Ordinal))
			{
				Model.WidgetButtonGroups.Add(Ordinal);
			}
			if (ParseStandardToggleButtonLabelWidgetName(WidgetName, Ordinal))
			{
				Model.WidgetLabelGroups.Add(Ordinal);
			}
		}
	}
	return Model;
}

void AddStandardToggleTemplateModelJson(const FNteStandardToggleTemplateModel& Model, FJsonObject& Object)
{
	Object.SetNumberField(TEXT("SaveGameVisibleCapacity"), Model.GetSaveGameVisibleCapacity());
	Object.SetNumberField(TEXT("PostProcessVisibleCapacity"), Model.GetPostProcessVisibleCapacity());
	Object.SetNumberField(TEXT("PostProcessInputCapacity"), Model.GetPostProcessInputCapacity());
	Object.SetNumberField(TEXT("WidgetButtonCapacity"), Model.GetWidgetButtonCapacity());
	Object.SetNumberField(TEXT("WidgetLabelCapacity"), Model.GetWidgetLabelCapacity());
	Object.SetNumberField(TEXT("RuntimeStateCapacity"), Model.GetRuntimeStateCapacity());
	Object.SetArrayField(TEXT("SaveGameVisibleGroups"), OrdinalSetToJsonValues(Model.SaveGameVisibleGroups));
	Object.SetArrayField(TEXT("PostProcessVisibleGroups"), OrdinalSetToJsonValues(Model.PostProcessVisibleGroups));
	Object.SetArrayField(TEXT("PostProcessInputGroups"), OrdinalSetToJsonValues(Model.PostProcessInputGroups));
	Object.SetArrayField(TEXT("WidgetButtonGroups"), OrdinalSetToJsonValues(Model.WidgetButtonGroups));
	Object.SetArrayField(TEXT("WidgetLabelGroups"), OrdinalSetToJsonValues(Model.WidgetLabelGroups));
}
}
