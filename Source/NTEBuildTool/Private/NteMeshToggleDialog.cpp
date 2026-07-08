// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteMeshToggleDialog.h"

#include "NteEditorAssetUtils.h"
#include "NteNotificationUtils.h"

#include "Engine/SkeletalMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
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
	bool bEnabled = false;
	FString KeyName;
	TSharedPtr<SCheckBox> EnabledCheckBox;
	TSharedPtr<SEditableTextBox> KeyTextBox;
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
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SAssignNew(Row->EnabledCheckBox, SCheckBox)
			.IsChecked(Row->bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([Row](ECheckBoxState NewState)
			{
				Row->bEnabled = NewState == ECheckBoxState::Checked;
			})
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(8, 0)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("%d  %s"), Row->SlotIndex, *Row->SlotName)))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(0.45f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(Row->KeyTextBox, SEditableTextBox)
			.Text(FText::FromString(Row->KeyName))
			.HintText(LOCTEXT("ToggleSlotKeyHint", "Up"))
			.OnTextCommitted_Lambda([Row](const FText& NewText, ETextCommit::Type)
			{
				Row->KeyName = NewText.ToString();
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
	bool bAccepted = false;

	TSharedPtr<SEditableTextBox> OutputFolderTextBox;
	TSharedPtr<SEditableTextBox> UiChordTextBox;
	TSharedPtr<SEditableTextBox> TemplatePostProcessTextBox;
	TSharedPtr<SEditableTextBox> TemplateWidgetTextBox;
	TSharedPtr<SEditableTextBox> TemplateSaveGameTextBox;
	TSharedPtr<SWindow> Window;

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

	SAssignNew(Window, SWindow)
		.Title(LOCTEXT("MeshToggleSetupDialogTitle", "NTE Mesh Toggle Runtime"))
		.ClientSize(FVector2D(760, 680))
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
			.FillHeight(1.0f)
			.Padding(16, 8)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SlotList
				]
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
	OutOptions.bCreateBlueprintAssets = true;
	OutOptions.bAssignPostProcessAnimBlueprint = true;
	OutOptions.bOverwriteExistingRuntimeAssets = true;

	for (const TSharedPtr<FMeshToggleSlotRow>& Row : Rows)
	{
		if (!Row->bEnabled)
		{
			continue;
		}

		Row->KeyName = Row->KeyTextBox.IsValid() ? Row->KeyTextBox->GetText().ToString() : Row->KeyName;
		FInputChord GroupChord;
		if (!ParseChord(Row->KeyName, GroupChord, Error))
		{
			NTEBuildTool::Editor::ShowError(FText::FromString(Error));
			return false;
		}

		FNteMeshToggleGroup Group;
		const int32 GroupOrdinal = OutOptions.ToggleGroups.Num() + 1;
		Group.GroupId = FString::Printf(TEXT("toggle_group_%d"), GroupOrdinal);
		Group.Label = FString::Printf(TEXT("Slot %d"), Row->SlotIndex);
		Group.Chord = GroupChord;
		Group.Slots.Add(Row->SlotIndex);
		Group.bDefaultVisible = true;

		FNteMeshToggleSlotBinding Binding;
		Binding.SlotIndex = Row->SlotIndex;
		Binding.SlotName = Row->SlotName;
		Binding.ImportedSlotName = Row->ImportedSlotName;
		Binding.MaterialPath = Row->MaterialPath;
		Group.SlotBindings.Add(Binding);
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
