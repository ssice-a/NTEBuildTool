// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteCharacterRuntimeActionDialog.h"

#include "NteNotificationUtils.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NTEBuildTool"

namespace NTEBuildTool::Character
{
namespace
{
FString JoinMaterialSlots(const TArray<int32>& MaterialSlots)
{
	TArray<FString> Parts;
	for (const int32 SlotIndex : MaterialSlots)
	{
		Parts.Add(FString::FromInt(SlotIndex));
	}
	return FString::Join(Parts, TEXT(","));
}

FString MakeDefaultRuntimeActionId(const FNteCharacterRuntimeActionDialogDefaults& Defaults)
{
	if (Defaults.MaterialSlots.Num() == 1)
	{
		return FString::Printf(TEXT("%s_slot_%d_visibility"), *Defaults.TargetMeshId, Defaults.MaterialSlots[0]);
	}
	return Defaults.TargetMeshId + TEXT("_material_visibility");
}

FString MakeDefaultRuntimeActionLabel(const FNteCharacterRuntimeActionDialogDefaults& Defaults)
{
	if (!Defaults.SlotName.IsEmpty())
	{
		return FString::Printf(TEXT("Toggle %s"), *Defaults.SlotName);
	}
	if (Defaults.MaterialSlots.Num() == 1)
	{
		return FString::Printf(TEXT("Toggle Slot %d"), Defaults.MaterialSlots[0]);
	}
	return TEXT("Toggle Material Slots");
}

bool ParseMaterialSlots(const FString& RawText, TArray<int32>& OutMaterialSlots, FString& OutError)
{
	FString Normalized = RawText;
	Normalized.ReplaceInline(TEXT(";"), TEXT(","));
	Normalized.ReplaceInline(TEXT(" "), TEXT(","));

	TArray<FString> Tokens;
	Normalized.ParseIntoArray(Tokens, TEXT(","), true);
	if (Tokens.IsEmpty())
	{
		OutError = TEXT("Material Slots must contain at least one slot index.");
		return false;
	}

	TSet<int32> SeenSlots;
	OutMaterialSlots.Reset();
	for (FString Token : Tokens)
	{
		Token.TrimStartAndEndInline();
		if (Token.IsEmpty())
		{
			continue;
		}

		if (!Token.IsNumeric())
		{
			OutError = FString::Printf(TEXT("Material slot '%s' is not an integer."), *Token);
			return false;
		}

		const int32 SlotIndex = FCString::Atoi(*Token);
		if (SlotIndex < 0)
		{
			OutError = FString::Printf(TEXT("Material slot '%s' is negative."), *Token);
			return false;
		}
		if (SeenSlots.Contains(SlotIndex))
		{
			OutError = FString::Printf(TEXT("Material slot %d appears more than once."), SlotIndex);
			return false;
		}

		SeenSlots.Add(SlotIndex);
		OutMaterialSlots.Add(SlotIndex);
	}

	if (OutMaterialSlots.IsEmpty())
	{
		OutError = TEXT("Material Slots must contain at least one slot index.");
		return false;
	}
	return true;
}

TSharedRef<SWidget> MakeTextRow(
	const FText& Label,
	const FString& InitialValue,
	TSharedPtr<SEditableTextBox>& OutTextBox)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(0.24f)
		.Padding(0, 0, 8, 0)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(Label)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(0.76f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(OutTextBox, SEditableTextBox)
			.Text(FText::FromString(InitialValue))
		];
}
}

bool ShowCharacterRuntimeActionDialog(
	const FNteCharacterModSpec& Spec,
	const FNteCharacterRuntimeActionDialogDefaults& Defaults,
	FNteCharacterRuntimeActionSpec& OutAction)
{
	TSharedPtr<SEditableTextBox> IdTextBox;
	TSharedPtr<SEditableTextBox> LabelTextBox;
	TSharedPtr<SEditableTextBox> HotkeyTextBox;
	TSharedPtr<SEditableTextBox> TargetMeshTextBox;
	TSharedPtr<SEditableTextBox> MaterialSlotsTextBox;
	TSharedPtr<SCheckBox> DefaultVisibleCheckBox;

	bool bAccepted = false;
	TSharedPtr<SWindow> Window;
	SAssignNew(Window, SWindow)
		.Title(LOCTEXT("RuntimeActionDialogTitle", "Add Character Runtime Action"))
		.ClientSize(FVector2D(720, 430))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 14, 16, 8)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("RuntimeActionDialogIntro", "Create a CharacterModSpec RuntimeAction. First slice supports MaterialSlotVisibility; Blueprint generation will consume the same data model."))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(16, 0, 16, 8)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 0, 0, 8)
					[
						MakeTextRow(LOCTEXT("RuntimeActionIdLabel", "Id"), MakeDefaultRuntimeActionId(Defaults), IdTextBox)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 0, 0, 8)
					[
						MakeTextRow(LOCTEXT("RuntimeActionLabelLabel", "Label"), MakeDefaultRuntimeActionLabel(Defaults), LabelTextBox)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 0, 0, 8)
					[
						MakeTextRow(LOCTEXT("RuntimeActionHotkeyLabel", "Hotkey"), FString(), HotkeyTextBox)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 0, 0, 8)
					[
						MakeTextRow(LOCTEXT("RuntimeActionTargetMeshLabel", "Target Mesh Id"), Defaults.TargetMeshId, TargetMeshTextBox)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 0, 0, 8)
					[
						MakeTextRow(LOCTEXT("RuntimeActionMaterialSlotsLabel", "Material Slots"), JoinMaterialSlots(Defaults.MaterialSlots), MaterialSlotsTextBox)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 4, 0, 0)
					[
						SAssignNew(DefaultVisibleCheckBox, SCheckBox)
						.IsChecked(ECheckBoxState::Checked)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("RuntimeActionDefaultVisibleLabel", "Default visible / enabled"))
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 12, 0, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RuntimeActionHotkeyHint", "Hotkey is optional. Use Unreal key names such as M, Slash, F9, NumPadOne, or chords like Ctrl+M. Leave blank for UI-only actions."))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 8, 16, 16)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(
						TEXT("Current spec actions: %d"),
						Spec.RuntimeActions.Num())))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 8, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("RuntimeActionOkButton", "Add / Update"))
					.OnClicked_Lambda([&OutAction, &bAccepted, &Window, IdTextBox, LabelTextBox, HotkeyTextBox, TargetMeshTextBox, MaterialSlotsTextBox, DefaultVisibleCheckBox]()
					{
						FNteCharacterRuntimeActionSpec Action;
						Action.Id = IdTextBox.IsValid() ? IdTextBox->GetText().ToString() : FString();
						Action.Label = LabelTextBox.IsValid() ? LabelTextBox->GetText().ToString() : FString();
						Action.Hotkey = HotkeyTextBox.IsValid() ? HotkeyTextBox->GetText().ToString() : FString();
						Action.TargetMeshId = TargetMeshTextBox.IsValid() ? TargetMeshTextBox->GetText().ToString() : TEXT("main");
						Action.ActionType = TEXT("MaterialSlotVisibility");
						Action.bDefaultEnabled = !DefaultVisibleCheckBox.IsValid() || DefaultVisibleCheckBox->IsChecked();

						Action.Id.TrimStartAndEndInline();
						Action.Label.TrimStartAndEndInline();
						Action.Hotkey.TrimStartAndEndInline();
						Action.TargetMeshId.TrimStartAndEndInline();
						if (Action.TargetMeshId.IsEmpty())
						{
							Action.TargetMeshId = TEXT("main");
						}

						if (Action.Id.IsEmpty())
						{
							NTEBuildTool::Editor::ShowError(LOCTEXT("RuntimeActionMissingId", "Runtime action Id is required."));
							return FReply::Handled();
						}
						if (Action.Label.IsEmpty())
						{
							NTEBuildTool::Editor::ShowError(LOCTEXT("RuntimeActionMissingLabel", "Runtime action Label is required."));
							return FReply::Handled();
						}

						FString SlotParseError;
						if (!ParseMaterialSlots(MaterialSlotsTextBox.IsValid() ? MaterialSlotsTextBox->GetText().ToString() : FString(), Action.MaterialSlots, SlotParseError))
						{
							NTEBuildTool::Editor::ShowError(FText::FromString(SlotParseError));
							return FReply::Handled();
						}

						OutAction = MoveTemp(Action);
						bAccepted = true;
						if (Window.IsValid())
						{
							Window->RequestDestroyWindow();
						}
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("RuntimeActionCancelButton", "Cancel"))
					.OnClicked_Lambda([&Window]()
					{
						if (Window.IsValid())
						{
							Window->RequestDestroyWindow();
						}
						return FReply::Handled();
					})
				]
			]
		];

	FSlateApplication::Get().AddModalWindow(Window.ToSharedRef(), nullptr);
	return bAccepted;
}
}

#undef LOCTEXT_NAMESPACE
