// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteMeshToggleDialog.h"

#include "NteEditorAssetUtils.h"
#include "NteMeshToggleConfig.h"
#include "NteNotificationUtils.h"

#include "Engine/SkeletalMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NTEBuildTool"

namespace NTEBuildTool::Toggle
{
namespace
{
struct FMeshToggleSlotRow
{
	int32 SlotIndex = INDEX_NONE;
	FString SlotName;
	FString ImportedSlotName;
	FString MaterialPath;
};

struct FMeshToggleGroupRow
{
	FString Label;
	FString KeyName;
	FString SlotsText;
	bool bDefaultVisible = true;
	TSharedPtr<SEditableTextBox> LabelTextBox;
	TSharedPtr<SEditableTextBox> KeyTextBox;
	TSharedPtr<SEditableTextBox> SlotsTextBox;
	TSharedPtr<SCheckBox> DefaultVisibleCheckBox;
};

FString ChordToConfigText(const FInputChord& Chord)
{
	FString Result;
	if (Chord.bCtrl)
	{
		Result += TEXT("Ctrl+");
	}
	if (Chord.bAlt)
	{
		Result += TEXT("Alt+");
	}
	if (Chord.bShift)
	{
		Result += TEXT("Shift+");
	}
	if (Chord.bCmd)
	{
		Result += TEXT("Cmd+");
	}
	Result += Chord.Key.IsValid() ? Chord.Key.GetFName().ToString() : FString();
	return Result;
}

bool ParseChord(FString Text, FInputChord& OutChord, FString& OutError)
{
	Text.TrimStartAndEndInline();
	OutChord = FInputChord();
	if (Text.IsEmpty())
	{
		return true;
	}

	Text.ReplaceInline(TEXT(" "), TEXT(""));
	TArray<FString> Parts;
	Text.ParseIntoArray(Parts, TEXT("+"), true);
	if (Parts.IsEmpty())
	{
		return true;
	}

	FString KeyText;
	for (FString Part : Parts)
	{
		Part.TrimStartAndEndInline();
		if (Part.Equals(TEXT("Ctrl"), ESearchCase::IgnoreCase) || Part.Equals(TEXT("Control"), ESearchCase::IgnoreCase))
		{
			OutChord.bCtrl = true;
		}
		else if (Part.Equals(TEXT("Alt"), ESearchCase::IgnoreCase))
		{
			OutChord.bAlt = true;
		}
		else if (Part.Equals(TEXT("Shift"), ESearchCase::IgnoreCase))
		{
			OutChord.bShift = true;
		}
		else if (Part.Equals(TEXT("Cmd"), ESearchCase::IgnoreCase) || Part.Equals(TEXT("Command"), ESearchCase::IgnoreCase))
		{
			OutChord.bCmd = true;
		}
		else
		{
			KeyText = Part;
		}
	}

	if (KeyText.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Input chord '%s' has modifiers but no key."), *Text);
		return false;
	}

	if (KeyText.Equals(TEXT("Numpad8"), ESearchCase::IgnoreCase) || KeyText.Equals(TEXT("Num8"), ESearchCase::IgnoreCase))
	{
		KeyText = TEXT("NumPadEight");
	}
	else if (KeyText.Equals(TEXT("Numpad2"), ESearchCase::IgnoreCase) || KeyText.Equals(TEXT("Num2"), ESearchCase::IgnoreCase))
	{
		KeyText = TEXT("NumPadTwo");
	}

	const FKey Key(*KeyText);
	if (!Key.IsValid())
	{
		OutError = FString::Printf(TEXT("Unknown key '%s'. Use Unreal key names such as Up, Down, NumPadEight, NumPadTwo, F9, Slash."), *KeyText);
		return false;
	}

	OutChord.Key = Key;
	return true;
}

TSharedRef<SWidget> MakeLabeledTextBox(const FText& Label, const TSharedRef<SEditableTextBox>& TextBox)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(Label)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 3, 0, 0)
		[
			TextBox
		];
}

TSharedRef<SWidget> MakeSlotRow(const TSharedPtr<FMeshToggleSlotRow>& Row)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(8, 0)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("%d  %s"), Row->SlotIndex, *Row->SlotName)))
		];
}

FString SlotsToText(const TArray<int32>& Slots)
{
	TArray<FString> Parts;
	for (const int32 SlotIndex : Slots)
	{
		Parts.Add(FString::FromInt(SlotIndex));
	}
	return FString::Join(Parts, TEXT(","));
}

bool ParseSlotsText(FString Text, const int32 MaterialCount, TArray<int32>& OutSlots, FString& OutError)
{
	Text.TrimStartAndEndInline();
	OutSlots.Reset();
	if (Text.IsEmpty())
	{
		OutError = TEXT("Each toggle item needs at least one material slot.");
		return false;
	}

	Text.ReplaceInline(TEXT(";"), TEXT(","));
	Text.ReplaceInline(TEXT(" "), TEXT(","));
	TArray<FString> Parts;
	Text.ParseIntoArray(Parts, TEXT(","), true);
	for (FString Part : Parts)
	{
		Part.TrimStartAndEndInline();
		if (Part.IsEmpty())
		{
			continue;
		}

		if (!Part.IsNumeric())
		{
			OutError = FString::Printf(TEXT("Material slot '%s' is not a number."), *Part);
			return false;
		}

		const int32 SlotIndex = FCString::Atoi(*Part);
		if (SlotIndex < 0 || SlotIndex >= MaterialCount)
		{
			OutError = FString::Printf(TEXT("Material slot %d is out of range. Valid range is 0-%d."), SlotIndex, MaterialCount - 1);
			return false;
		}
		OutSlots.AddUnique(SlotIndex);
	}

	if (OutSlots.IsEmpty())
	{
		OutError = TEXT("Each toggle item needs at least one material slot.");
		return false;
	}
	return true;
}

TSharedRef<SWidget> MakeGroupRow(
	const TSharedPtr<FMeshToggleGroupRow>& Row,
	const FSimpleDelegate& OnRemove)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(0.75f)
		.Padding(0, 0, 8, 0)
		.VAlign(VAlign_Center)
		[
			SAssignNew(Row->LabelTextBox, SEditableTextBox)
			.Text(FText::FromString(Row->Label))
			.HintText(LOCTEXT("ToggleGroupLabelHint", "UI button label"))
			.OnTextCommitted_Lambda([Row](const FText& NewText, ETextCommit::Type)
			{
				Row->Label = NewText.ToString();
			})
		]
		+ SHorizontalBox::Slot()
		.FillWidth(0.55f)
		.Padding(0, 0, 8, 0)
		.VAlign(VAlign_Center)
		[
			SAssignNew(Row->KeyTextBox, SEditableTextBox)
			.Text(FText::FromString(Row->KeyName))
			.HintText(LOCTEXT("ToggleGroupKeyHint", "optional hotkey"))
			.OnTextCommitted_Lambda([Row](const FText& NewText, ETextCommit::Type)
			{
				Row->KeyName = NewText.ToString();
			})
		]
		+ SHorizontalBox::Slot()
		.FillWidth(0.75f)
		.Padding(0, 0, 8, 0)
		.VAlign(VAlign_Center)
		[
			SAssignNew(Row->SlotsTextBox, SEditableTextBox)
			.Text(FText::FromString(Row->SlotsText))
			.HintText(LOCTEXT("ToggleGroupSlotsHint", "1,13,15"))
			.OnTextCommitted_Lambda([Row](const FText& NewText, ETextCommit::Type)
			{
				Row->SlotsText = NewText.ToString();
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, 8, 0)
		.VAlign(VAlign_Center)
		[
			SAssignNew(Row->DefaultVisibleCheckBox, SCheckBox)
			.IsChecked(Row->bDefaultVisible ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([Row](ECheckBoxState NewState)
			{
				Row->bDefaultVisible = NewState == ECheckBoxState::Checked;
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.Text(LOCTEXT("RemoveToggleGroup", "Remove"))
			.OnClicked_Lambda([OnRemove]()
			{
				OnRemove.ExecuteIfBound();
				return FReply::Handled();
			})
		];
}
}

bool ShowMeshToggleSetupDialog(USkeletalMesh& SkeletalMesh, FNteMeshToggleSetupOptions& OutOptions)
{
	const FString MeshPath = SkeletalMesh.GetPackage()->GetName();
	TArray<TSharedPtr<FMeshToggleSlotRow>> Rows;
	const TArray<FSkeletalMaterial>& Materials = SkeletalMesh.GetMaterials();
	for (int32 Index = 0; Index < Materials.Num(); ++Index)
	{
		const FSkeletalMaterial& Material = Materials[Index];
		TSharedPtr<FMeshToggleSlotRow> Row = MakeShared<FMeshToggleSlotRow>();
		Row->SlotIndex = Index;
		Row->SlotName = Material.MaterialSlotName.ToString();
#if WITH_EDITORONLY_DATA
		Row->ImportedSlotName = Material.ImportedMaterialSlotName.ToString();
#endif
		Row->MaterialPath = Material.MaterialInterface ? Material.MaterialInterface->GetPackage()->GetName() : FString();
		Rows.Add(Row);
	}

	FString OutputFolder = FPackageName::GetLongPackagePath(MeshPath) / TEXT("mod/Runtime");
	FString UiChordText = TEXT("Ctrl+Slash");
	FString TemplatePostProcess;
	FString TemplateWidget;
	FString TemplateSaveGame;
	bool bCreateRuntimeAssets = false;
	bool bAccepted = false;

	FString ExistingSetupFilename = FPackageName::LongPackageNameToFilename(
		NTEBuildTool::Editor::JoinAssetPath(OutputFolder, TEXT("NTE_ModToggleSetup")),
		TEXT(".json"));
	FNteMeshToggleSetupOptions ExistingOptions;
	FString ExistingLoadError;
	const bool bHasExistingSetup = FPaths::FileExists(ExistingSetupFilename)
		&& LoadMeshToggleSetupOptionsFromJsonFile(ExistingSetupFilename, ExistingOptions, ExistingLoadError)
		&& ExistingOptions.TargetMeshPath == MeshPath;
	if (bHasExistingSetup)
	{
		OutputFolder = ExistingOptions.OutputFolder;
		UiChordText = ChordToConfigText(ExistingOptions.UiChord);
		TemplatePostProcess = ExistingOptions.TemplatePostProcessAnimBlueprintPath;
		TemplateWidget = ExistingOptions.TemplateWidgetBlueprintPath;
		TemplateSaveGame = ExistingOptions.TemplateSaveGameBlueprintPath;
	}

	TSharedPtr<SEditableTextBox> OutputFolderTextBox;
	TSharedPtr<SEditableTextBox> UiChordTextBox;
	TSharedPtr<SEditableTextBox> TemplatePostProcessTextBox;
	TSharedPtr<SEditableTextBox> TemplateWidgetTextBox;
	TSharedPtr<SEditableTextBox> TemplateSaveGameTextBox;
	TSharedPtr<SCheckBox> CreateRuntimeAssetsCheckBox;
	TSharedPtr<SWindow> Window;
	TArray<TSharedPtr<FMeshToggleGroupRow>> GroupRows;
	TSharedPtr<SVerticalBox> GroupList;

	TSharedRef<SVerticalBox> SlotList = SNew(SVerticalBox);
	for (const TSharedPtr<FMeshToggleSlotRow>& Row : Rows)
	{
		SlotList->AddSlot()
			.AutoHeight()
			.Padding(0, 2)
			[
				MakeSlotRow(Row)
			];
	}

	const auto AddGroupRowFromValues = [&GroupRows](const FString& Label, const FString& KeyName, const FString& SlotsText, const bool bDefaultVisible)
	{
		TSharedPtr<FMeshToggleGroupRow> Row = MakeShared<FMeshToggleGroupRow>();
		Row->Label = Label;
		Row->KeyName = KeyName;
		Row->SlotsText = SlotsText;
		Row->bDefaultVisible = bDefaultVisible;
		GroupRows.Add(Row);
	};

	if (bHasExistingSetup)
	{
		for (const FNteMeshToggleGroup& Group : ExistingOptions.ToggleGroups)
		{
			AddGroupRowFromValues(Group.Label, ChordToConfigText(Group.Chord), SlotsToText(Group.Slots), Group.bDefaultVisible);
		}
	}
	if (GroupRows.IsEmpty())
	{
		AddGroupRowFromValues(TEXT("Toggle 1"), FString(), FString(), true);
	}

	TFunction<void()> RebuildGroupList;
	RebuildGroupList = [&GroupRows, &GroupList, &RebuildGroupList]()
	{
		if (!GroupList.IsValid())
		{
			return;
		}

		GroupList->ClearChildren();
		GroupList->AddSlot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.75f).Padding(0, 0, 8, 0)[SNew(STextBlock).Text(LOCTEXT("ToggleGroupHeaderLabel", "UI Button"))]
				+ SHorizontalBox::Slot().FillWidth(0.55f).Padding(0, 0, 8, 0)[SNew(STextBlock).Text(LOCTEXT("ToggleGroupHeaderKey", "Hotkey"))]
				+ SHorizontalBox::Slot().FillWidth(0.75f).Padding(0, 0, 8, 0)[SNew(STextBlock).Text(LOCTEXT("ToggleGroupHeaderSlots", "Material Slots"))]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[SNew(STextBlock).Text(LOCTEXT("ToggleGroupHeaderDefault", "On"))]
				+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::GetEmpty())]
			];

		for (int32 Index = 0; Index < GroupRows.Num(); ++Index)
		{
			const TSharedPtr<FMeshToggleGroupRow> Row = GroupRows[Index];
			GroupList->AddSlot()
				.AutoHeight()
				.Padding(0, 3)
				[
					MakeGroupRow(Row, FSimpleDelegate::CreateLambda([&GroupRows, &RebuildGroupList, Row]()
					{
						GroupRows.Remove(Row);
						if (GroupRows.IsEmpty())
						{
							TSharedPtr<FMeshToggleGroupRow> NewRow = MakeShared<FMeshToggleGroupRow>();
							NewRow->Label = TEXT("Toggle 1");
							NewRow->bDefaultVisible = true;
							GroupRows.Add(NewRow);
						}
						RebuildGroupList();
					}))
				];
		}
	};

	SAssignNew(Window, SWindow)
		.Title(LOCTEXT("MeshToggleSetupDialogTitle", "NTE Mesh Toggle Runtime"))
		.ClientSize(FVector2D(900, 760))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 14, 16, 8)
			[
				SNew(STextBlock)
				.Text(FText::FromString(MeshPath))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0, 0, 8, 0)
				[
					MakeLabeledTextBox(
						LOCTEXT("OutputFolderLabel", "Runtime Output Folder"),
						SAssignNew(OutputFolderTextBox, SEditableTextBox).Text(FText::FromString(OutputFolder)))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.35f)
				[
					MakeLabeledTextBox(
						LOCTEXT("UiHotkeyLabel", "UI Hotkey"),
						SAssignNew(UiChordTextBox, SEditableTextBox).Text(FText::FromString(UiChordText)))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 8)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ToggleGroupsTitle", "Toggle Items"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 0, 16, 8)
			[
				SAssignNew(GroupList, SVerticalBox)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 0, 16, 8)
			[
				SNew(SButton)
				.Text(LOCTEXT("AddToggleGroup", "Add Toggle Item"))
				.OnClicked_Lambda([&GroupRows, &RebuildGroupList]()
				{
					TSharedPtr<FMeshToggleGroupRow> Row = MakeShared<FMeshToggleGroupRow>();
					Row->Label = FString::Printf(TEXT("Toggle %d"), GroupRows.Num() + 1);
					Row->bDefaultVisible = true;
					GroupRows.Add(Row);
					RebuildGroupList();
					return FReply::Handled();
				})
			]
			+ SVerticalBox::Slot()
			.FillHeight(0.35f)
			.Padding(16, 4)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SlotList
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 8)
			[
				SAssignNew(CreateRuntimeAssetsCheckBox, SCheckBox)
				.IsChecked(bCreateRuntimeAssets ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged_Lambda([&bCreateRuntimeAssets](ECheckBoxState NewState)
				{
					bCreateRuntimeAssets = NewState == ECheckBoxState::Checked;
				})
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CreateRuntimeAssetsLabel", "Create or overwrite runtime Blueprint assets from templates"))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 8)
			[
				MakeLabeledTextBox(
					LOCTEXT("TemplatePostProcessLabel", "Template Post Process Anim Blueprint"),
					SAssignNew(TemplatePostProcessTextBox, SEditableTextBox).Text(FText::FromString(TemplatePostProcess)))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 8)
			[
				MakeLabeledTextBox(
					LOCTEXT("TemplateWidgetLabel", "Template Widget Blueprint"),
					SAssignNew(TemplateWidgetTextBox, SEditableTextBox).Text(FText::FromString(TemplateWidget)))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 8)
			[
				MakeLabeledTextBox(
					LOCTEXT("TemplateSaveGameLabel", "Template SaveGame Blueprint"),
					SAssignNew(TemplateSaveGameTextBox, SEditableTextBox).Text(FText::FromString(TemplateSaveGame)))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 8, 16, 16)
			.HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 8, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("CancelMeshToggleSetup", "Cancel"))
					.OnClicked_Lambda([&bAccepted, &Window]()
					{
						bAccepted = false;
						Window->RequestDestroyWindow();
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("CreateMeshToggleSetup", "Generate"))
					.OnClicked_Lambda([&bAccepted, &Window]()
					{
						bAccepted = true;
						Window->RequestDestroyWindow();
						return FReply::Handled();
					})
				]
			]
		];

	RebuildGroupList();
	FSlateApplication::Get().AddModalWindow(Window.ToSharedRef(), nullptr);
	if (!bAccepted)
	{
		return false;
	}

	OutputFolder = OutputFolderTextBox->GetText().ToString();
	UiChordText = UiChordTextBox->GetText().ToString();
	TemplatePostProcess = TemplatePostProcessTextBox->GetText().ToString();
	TemplateWidget = TemplateWidgetTextBox->GetText().ToString();
	TemplateSaveGame = TemplateSaveGameTextBox->GetText().ToString();
	bCreateRuntimeAssets = CreateRuntimeAssetsCheckBox.IsValid() && CreateRuntimeAssetsCheckBox->IsChecked();

	FString Error;
	FInputChord UiChord;
	if (!ParseChord(UiChordText, UiChord, Error) || !UiChord.Key.IsValid())
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(Error.IsEmpty() ? TEXT("UI hotkey must contain a key.") : Error));
		return false;
	}

	OutOptions = FNteMeshToggleSetupOptions();
	OutOptions.MeshPath = MeshPath;
	OutOptions.TargetMeshPath = MeshPath;
	OutOptions.RuntimeAnchorMeshPath = MeshPath;
	OutOptions.OutputFolder = NTEBuildTool::Editor::NormalizeAssetPathForText(OutputFolder);
	OutOptions.UiChord = UiChord;
	OutOptions.TemplatePostProcessAnimBlueprintPath = NTEBuildTool::Editor::NormalizeAssetPathForText(TemplatePostProcess);
	OutOptions.TemplateWidgetBlueprintPath = NTEBuildTool::Editor::NormalizeAssetPathForText(TemplateWidget);
	OutOptions.TemplateSaveGameBlueprintPath = NTEBuildTool::Editor::NormalizeAssetPathForText(TemplateSaveGame);
	OutOptions.RuntimeMode = TEXT("StandardPostProcessTemplate");
	OutOptions.bCreateBlueprintAssets = bCreateRuntimeAssets;
	OutOptions.bAssignPostProcessAnimBlueprint = true;
	OutOptions.bOverwriteExistingRuntimeAssets = true;

	for (const TSharedPtr<FMeshToggleGroupRow>& Row : GroupRows)
	{
		Row->Label = Row->LabelTextBox.IsValid() ? Row->LabelTextBox->GetText().ToString() : Row->Label;
		Row->KeyName = Row->KeyTextBox.IsValid() ? Row->KeyTextBox->GetText().ToString() : Row->KeyName;
		Row->SlotsText = Row->SlotsTextBox.IsValid() ? Row->SlotsTextBox->GetText().ToString() : Row->SlotsText;
		Row->bDefaultVisible = Row->DefaultVisibleCheckBox.IsValid() ? Row->DefaultVisibleCheckBox->IsChecked() : Row->bDefaultVisible;

		FInputChord GroupChord;
		if (!ParseChord(Row->KeyName, GroupChord, Error))
		{
			NTEBuildTool::Editor::ShowError(FText::FromString(Error));
			return false;
		}

		TArray<int32> Slots;
		if (!ParseSlotsText(Row->SlotsText, Rows.Num(), Slots, Error))
		{
			NTEBuildTool::Editor::ShowError(FText::FromString(Error));
			return false;
		}

		FNteMeshToggleGroup Group;
		const int32 GroupOrdinal = OutOptions.ToggleGroups.Num() + 1;
		Group.GroupId = FString::Printf(TEXT("toggle_group_%d"), GroupOrdinal);
		Group.Label = Row->Label.IsEmpty() ? FString::Printf(TEXT("Toggle %d"), GroupOrdinal) : Row->Label;
		Group.Chord = GroupChord;
		Group.Slots = Slots;
		Group.bDefaultVisible = Row->bDefaultVisible;

		for (const int32 SlotIndex : Slots)
		{
			const TSharedPtr<FMeshToggleSlotRow>& SlotRow = Rows[SlotIndex];
			FNteMeshToggleSlotBinding Binding;
			Binding.SlotIndex = SlotRow->SlotIndex;
			Binding.SlotName = SlotRow->SlotName;
			Binding.ImportedSlotName = SlotRow->ImportedSlotName;
			Binding.MaterialPath = SlotRow->MaterialPath;
			Group.SlotBindings.Add(Binding);
		}
		OutOptions.ToggleGroups.Add(Group);
	}

	if (OutOptions.ToggleGroups.IsEmpty())
	{
		NTEBuildTool::Editor::ShowError(LOCTEXT("NoToggleSlotsSelected", "Select at least one material slot to generate toggle runtime assets."));
		return false;
	}

	return true;
}
}

#undef LOCTEXT_NAMESPACE
