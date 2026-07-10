// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteMaterialInstanceDialog.h"

#include "NteEditorAssetUtils.h"
#include "NteJsonFileUtils.h"
#include "NteNotificationUtils.h"

#include "Engine/SkeletalMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NTEBuildTool"

namespace NTEBuildTool::Material
{
namespace
{
struct FSourceTextureRow
{
	FNteMaterialSourceTextureUsage Usage;
	FString ReplacementTexturePath;
	TSharedPtr<SEditableTextBox> ReplacementTextBox;
};

FString MakeParameterSummary(const TArray<FString>& ParameterNames)
{
	return FString::Join(ParameterNames, TEXT(", "));
}

bool TryGetSingleSelectedPackagePath(FString& OutPackagePath, FString& OutError)
{
	const TArray<FAssetData> SelectedAssets = NTEBuildTool::Editor::GetSelectedContentBrowserAssets();
	if (SelectedAssets.Num() != 1)
	{
		OutError = TEXT("Select exactly one asset in the Content Browser.");
		return false;
	}

	OutPackagePath = SelectedAssets[0].PackageName.ToString();
	return true;
}
}

bool ShowMaterialInstanceRecipeDialog(const FString& SourceMaterialJson, FNteMaterialInstanceOptions& OutOptions, TSharedPtr<FJsonObject>& OutSourceTextureOverrides)
{
	TSharedPtr<FJsonObject> SourceMaterial;
	FString Error;
	if (!NTEBuildTool::Json::LoadJsonObjectFromFile(SourceMaterialJson, SourceMaterial, Error))
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(Error));
		return false;
	}

	const FString ParentMaterialPath = DeriveParentMaterialPathFromFModelJson(SourceMaterialJson);
	FString OutputFolder = DeriveModMaterialFolderFromParentPath(ParentMaterialPath, NTEBuildTool::Editor::GetSelectedContentBrowserPath());
	FString OutputMaterialPath = NTEBuildTool::Editor::JoinAssetPath(OutputFolder, MakeModMaterialNameFromFModelJson(SourceMaterialJson));
	FString MeshPath;
	if (USkeletalMesh* SelectedMesh = NTEBuildTool::Editor::GetSingleSelectedSkeletalMesh())
	{
		MeshPath = NTEBuildTool::Editor::GetAssetPackagePath(SelectedMesh);
	}
	FString SlotText;
	bool bAssignToMeshSlot = false;
	bool bResetForPakTextureOnly = true;
	bool bEnsureParentPlaceholder = true;
	bool bAccepted = false;

	const FJsonObject* SourceTextures = FindSourceMaterialParameterObject(SourceMaterial.Get(), TEXT("Textures"));
	TArray<TSharedPtr<FSourceTextureRow>> SourceTextureRows;
	for (const FNteMaterialSourceTextureUsage& Usage : BuildSourceTextureUsage(SourceTextures))
	{
		TSharedPtr<FSourceTextureRow> Row = MakeShared<FSourceTextureRow>();
		Row->Usage = Usage;
		SourceTextureRows.Add(Row);
	}

	TSharedPtr<SEditableTextBox> OutputMaterialTextBox;
	TSharedPtr<SEditableTextBox> MeshTextBox;
	TSharedPtr<SEditableTextBox> SlotTextBox;
	TSharedPtr<SCheckBox> AssignToMeshSlotCheckBox;
	TSharedPtr<SCheckBox> ResetForPakTextureOnlyCheckBox;
	TSharedPtr<SCheckBox> EnsureParentPlaceholderCheckBox;
	TSharedPtr<SWindow> Window;

	TSharedRef<SVerticalBox> TextureRows = SNew(SVerticalBox);
	for (const TSharedPtr<FSourceTextureRow>& Row : SourceTextureRows)
	{
		TextureRows->AddSlot()
			.AutoHeight()
			.Padding(0, 3)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.36f)
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Row->Usage.SourceTexturePath))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.24f)
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(MakeParameterSummary(Row->Usage.ParameterNames)))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.31f)
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				[
					SAssignNew(Row->ReplacementTextBox, SEditableTextBox)
					.Text(FText::FromString(Row->ReplacementTexturePath))
					.HintText(LOCTEXT("ReplacementTextureHint", "/Game/.../texture"))
					.OnTextCommitted_Lambda([Row](const FText& NewText, ETextCommit::Type)
					{
						Row->ReplacementTexturePath = NewText.ToString();
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Text(LOCTEXT("UseSelectedReplacementTexture", "Use Selected"))
					.OnClicked_Lambda([Row]()
					{
						FString PackagePath;
						FString Error;
						if (!TryGetSingleSelectedPackagePath(PackagePath, Error))
						{
							NTEBuildTool::Editor::ShowError(FText::FromString(Error));
							return FReply::Handled();
						}

						Row->ReplacementTexturePath = PackagePath;
						if (Row->ReplacementTextBox.IsValid())
						{
							Row->ReplacementTextBox->SetText(FText::FromString(Row->ReplacementTexturePath));
						}
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Text(LOCTEXT("BrowseReplacementTexture", "..."))
					.OnClicked_Lambda([Row]()
					{
						FString TextureFilename;
						if (NTEBuildTool::Editor::ChooseAssetFileWithTitle(
							LOCTEXT("ChooseReplacementTexture", "Choose Replacement Texture Asset"),
							TEXT(""),
							TEXT("Unreal assets (*.uasset)|*.uasset"),
							TextureFilename))
						{
							const FString PackagePath = NTEBuildTool::Editor::TryConvertFilenameToGamePackagePath(TextureFilename);
							Row->ReplacementTexturePath = PackagePath.IsEmpty() ? TextureFilename : PackagePath;
							if (Row->ReplacementTextBox.IsValid())
							{
								Row->ReplacementTextBox->SetText(FText::FromString(Row->ReplacementTexturePath));
							}
						}
						return FReply::Handled();
					})
				]
			];
	}

	SAssignNew(Window, SWindow)
		.Title(LOCTEXT("MaterialRecipeDialogTitle", "NTE Material Instance Recipe"))
		.ClientSize(FVector2D(1120, 760))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 14, 16, 8)
			[
				SNew(STextBlock)
				.Text(FText::FromString(SourceMaterialJson))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 4)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("Parent: %s"), *ParentMaterialPath)))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 8)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("OutputMaterialLabel", "Output Material Instance"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 3, 0, 0)
				[
					SAssignNew(OutputMaterialTextBox, SEditableTextBox)
					.Text(FText::FromString(OutputMaterialPath))
				]
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
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("MeshPathLabel", "Optional Mesh"))]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)[SAssignNew(MeshTextBox, SEditableTextBox).Text(FText::FromString(MeshPath))]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.2f)
				.Padding(0, 0, 8, 0)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("SlotIndexLabel", "Slot"))]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)[SAssignNew(SlotTextBox, SEditableTextBox).Text(FText::FromString(SlotText))]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				.Padding(0, 0, 8, 0)
				[
					SAssignNew(AssignToMeshSlotCheckBox, SCheckBox)
					.IsChecked(bAssignToMeshSlot ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AssignToMeshSlotLabel", "Assign"))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				.Padding(0, 0, 8, 0)
				[
					SAssignNew(ResetForPakTextureOnlyCheckBox, SCheckBox)
					.IsChecked(bResetForPakTextureOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ResetForPakTextureOnlyLabel", "Reset"))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				[
					SAssignNew(EnsureParentPlaceholderCheckBox, SCheckBox)
					.IsChecked(bEnsureParentPlaceholder ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("EnsureProxyLabel", "Proxy"))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 8, 16, 4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.36f).Padding(0, 0, 8, 0)[SNew(STextBlock).Text(LOCTEXT("SourceTextureHeader", "Source Texture"))]
				+ SHorizontalBox::Slot().FillWidth(0.24f).Padding(0, 0, 8, 0)[SNew(STextBlock).Text(LOCTEXT("SourceTextureParamsHeader", "Parameters"))]
				+ SHorizontalBox::Slot().FillWidth(0.31f).Padding(0, 0, 8, 0)[SNew(STextBlock).Text(LOCTEXT("ReplacementTextureHeader", "Replacement Texture"))]
				+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::GetEmpty())]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(16, 0, 16, 8)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					TextureRows
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
					.Text(LOCTEXT("CancelMaterialRecipe", "Cancel"))
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
					.Text(LOCTEXT("ApplyMaterialRecipe", "Apply"))
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

	OutOptions = FNteMaterialInstanceOptions();
	OutOptions.SourceMaterialJson = SourceMaterialJson;
	OutOptions.ParentMaterialPath = ParentMaterialPath;
	OutOptions.OutputMaterialPath = NTEBuildTool::Editor::NormalizeAssetPathForText(OutputMaterialTextBox->GetText().ToString());
	OutOptions.MeshPath = NTEBuildTool::Editor::NormalizeAssetPathForText(MeshTextBox->GetText().ToString());
	OutOptions.bAssignToMeshSlot = AssignToMeshSlotCheckBox.IsValid() && AssignToMeshSlotCheckBox->IsChecked();
	OutOptions.bResetForPakTextureOnly = !ResetForPakTextureOnlyCheckBox.IsValid() || ResetForPakTextureOnlyCheckBox->IsChecked();
	OutOptions.bEnsureParentPlaceholder = !EnsureParentPlaceholderCheckBox.IsValid() || EnsureParentPlaceholderCheckBox->IsChecked();

	const FString SlotValue = SlotTextBox->GetText().ToString().TrimStartAndEnd();
	if (!SlotValue.IsEmpty())
	{
		if (!SlotValue.IsNumeric())
		{
			NTEBuildTool::Editor::ShowError(LOCTEXT("MaterialSlotMustBeNumeric", "Slot must be numeric."));
			return false;
		}
		OutOptions.SlotIndex = FCString::Atoi(*SlotValue);
	}
	OutOptions.bAssignToMeshSlot = OutOptions.bAssignToMeshSlot || (!OutOptions.MeshPath.IsEmpty() && OutOptions.SlotIndex != INDEX_NONE);

	OutSourceTextureOverrides = MakeShared<FJsonObject>();
	for (const TSharedPtr<FSourceTextureRow>& Row : SourceTextureRows)
	{
		if (!Row.IsValid())
		{
			continue;
		}

		Row->ReplacementTexturePath = Row->ReplacementTextBox.IsValid() ? Row->ReplacementTextBox->GetText().ToString() : Row->ReplacementTexturePath;
		const FString ReplacementTexturePath = NTEBuildTool::Editor::NormalizeAssetPathForText(Row->ReplacementTexturePath);
		if (!ReplacementTexturePath.IsEmpty())
		{
			OutSourceTextureOverrides->SetStringField(Row->Usage.SourceTexturePath, ReplacementTexturePath);
		}
	}
	return true;
}
}

#undef LOCTEXT_NAMESPACE
