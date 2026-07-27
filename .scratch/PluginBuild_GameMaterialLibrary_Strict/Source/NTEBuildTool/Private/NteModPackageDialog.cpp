// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteModPackageDialog.h"

#include "NteBuildToolSettings.h"
#include "NteModPackageJob.h"
#include "NteModPackagePlan.h"
#include "NteNotificationUtils.h"

#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NTEBuildTool"

namespace NTEBuildTool::Package
{
namespace
{
struct FPackagePlanRow
{
	int32 CandidateIndex = INDEX_NONE;
	TSharedPtr<SCheckBox> IncludeCheckBox;
};

FString MakeDefaultPackageJobFilename(const FString& ModName)
{
	const FString SafeModName = SanitizeModPackageName(ModName.IsEmpty() ? TEXT("nte_mod_P") : ModName);
	FString Filename = FPaths::ProjectSavedDir() / TEXT("NTEBuildTool/Packages") / SafeModName / (SafeModName + TEXT(".job.json"));
	FPaths::NormalizeFilename(Filename);
	return Filename;
}

FString MakeDefaultPackageModName(const FNtePackagePlan& Plan)
{
	return DeriveModPackageNameFromPackages(GetIncludedPackageNames(Plan));
}

void SetAllRowsChecked(const TArray<TSharedPtr<FPackagePlanRow>>& Rows, const ECheckBoxState State)
{
	for (const TSharedPtr<FPackagePlanRow>& Row : Rows)
	{
		if (Row.IsValid() && Row->IncludeCheckBox.IsValid())
		{
			Row->IncludeCheckBox->SetIsChecked(State);
		}
	}
}
}

bool ShowPackagePlanDialog(FNtePackagePlan& Plan)
{
	FNtePackagePlanDialogResult IgnoredResult;
	return ShowPackagePlanDialog(Plan, IgnoredResult);
}

bool ShowPackagePlanDialog(FNtePackagePlan& Plan, FNtePackagePlanDialogResult& OutResult)
{
	TSharedPtr<SWindow> Window;
	TArray<TSharedPtr<FPackagePlanRow>> Rows;
	bool bAccepted = false;

	const FString InitialModName = MakeDefaultPackageModName(Plan);
	const FString InitialModsDir = NTEBuildTool::Settings::GetDefaultModsOutputDirectory();
	const FString InitialJobFilename = MakeDefaultPackageJobFilename(InitialModName);
	TSharedPtr<SEditableTextBox> ModsDirTextBox;
	TSharedPtr<SEditableTextBox> ModNameTextBox;
	TSharedPtr<SEditableTextBox> JobFilenameTextBox;
	TSharedPtr<SCheckBox> LaunchBuildCheckBox;

	TSharedRef<SVerticalBox> RowList = SNew(SVerticalBox);
	for (int32 Index = 0; Index < Plan.Candidates.Num(); ++Index)
	{
		FNtePackagePlanCandidate& Candidate = Plan.Candidates[Index];
		TSharedPtr<FPackagePlanRow> Row = MakeShared<FPackagePlanRow>();
		Row->CandidateIndex = Index;
		Rows.Add(Row);

		RowList->AddSlot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				[
					SAssignNew(Row->IncludeCheckBox, SCheckBox)
					.IsChecked(Candidate.bDefaultIncluded ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.18f)
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(PackagePlanCandidateKindToString(Candidate.Kind)))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.56f)
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Candidate.PackageName))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.26f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Candidate.Reason))
				]
			];
	}

	SAssignNew(Window, SWindow)
		.Title(LOCTEXT("PackagePlanDialogTitle", "NTE Package Plan"))
		.ClientSize(FVector2D(1160, 820))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 14, 16, 8)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PackagePlanHeader", "Review package candidates and package job settings. Source, Skeleton, Physics, and proxy candidates are visible but off by default unless directly selected."))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 4, 16, 8)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(0.34f)
					.Padding(0, 0, 8, 0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("PackagePlanModNameLabel", "Mod Name"))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)
						[
							SAssignNew(ModNameTextBox, SEditableTextBox)
							.Text(FText::FromString(InitialModName))
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.66f)
					.Padding(0, 0, 8, 0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("PackagePlanModsDirLabel", "Mods Output Directory"))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)
						[
							SAssignNew(ModsDirTextBox, SEditableTextBox)
							.Text(FText::FromString(InitialModsDir))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Bottom)
					[
						SNew(SButton)
						.Text(LOCTEXT("PackagePlanBrowseModsDir", "Browse"))
						.OnClicked_Lambda([&ModsDirTextBox]()
						{
							FString ModsDir;
							if (NTEBuildTool::Editor::ChooseDirectoryWithTitle(
								LOCTEXT("ChoosePackagePlanModsOutputDirectory", "Choose Mods Output Directory"),
								ModsDirTextBox.IsValid() ? ModsDirTextBox->GetText().ToString() : NTEBuildTool::Settings::GetDefaultModsOutputDirectory(),
								ModsDir))
							{
								if (ModsDirTextBox.IsValid())
								{
									ModsDirTextBox->SetText(FText::FromString(ModsDir));
								}
							}
							return FReply::Handled();
						})
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0, 0, 8, 0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("PackagePlanJobFileLabel", "Package Job JSON"))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)
						[
							SAssignNew(JobFilenameTextBox, SEditableTextBox)
							.Text(FText::FromString(InitialJobFilename))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Bottom)
					.Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("PackagePlanDefaultJobFile", "Default"))
						.OnClicked_Lambda([&ModNameTextBox, &JobFilenameTextBox]()
						{
							const FString ModName = ModNameTextBox.IsValid() ? ModNameTextBox->GetText().ToString() : FString();
							if (JobFilenameTextBox.IsValid())
							{
								JobFilenameTextBox->SetText(FText::FromString(MakeDefaultPackageJobFilename(ModName)));
							}
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Bottom)
					.Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("PackagePlanBrowseJobFile", "Browse"))
						.OnClicked_Lambda([&ModNameTextBox, &JobFilenameTextBox]()
						{
							FString JobFilename;
							const FString ModName = ModNameTextBox.IsValid() ? ModNameTextBox->GetText().ToString() : FString();
							if (NTEBuildTool::Editor::ChooseSaveJsonFileWithTitle(
								LOCTEXT("SavePackagePlanJobJson", "Save NTE Mod Package Job JSON"),
								SanitizeModPackageName(ModName.IsEmpty() ? TEXT("nte_mod_P") : ModName) + TEXT(".job.json"),
								JobFilename))
							{
								if (JobFilenameTextBox.IsValid())
								{
									JobFilenameTextBox->SetText(FText::FromString(JobFilename));
								}
							}
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Bottom)
					[
						SAssignNew(LaunchBuildCheckBox, SCheckBox)
						.IsChecked(ECheckBoxState::Checked)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("PackagePlanLaunchBuild", "Build"))
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 4, 16, 4)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 4)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("PackagePlanCheckAll", "All"))
						.OnClicked_Lambda([&Rows]()
						{
							SetAllRowsChecked(Rows, ECheckBoxState::Checked);
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 12, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("PackagePlanCheckNone", "None"))
						.OnClicked_Lambda([&Rows]()
						{
							SetAllRowsChecked(Rows, ECheckBoxState::Unchecked);
							return FReply::Handled();
						})
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[SNew(STextBlock).Text(LOCTEXT("PackagePlanIncludeHeader", "Pack"))]
					+ SHorizontalBox::Slot().FillWidth(0.18f).Padding(0, 0, 8, 0)[SNew(STextBlock).Text(LOCTEXT("PackagePlanKindHeader", "Kind"))]
					+ SHorizontalBox::Slot().FillWidth(0.56f).Padding(0, 0, 8, 0)[SNew(STextBlock).Text(LOCTEXT("PackagePlanPackageHeader", "Package"))]
					+ SHorizontalBox::Slot().FillWidth(0.26f)[SNew(STextBlock).Text(LOCTEXT("PackagePlanReasonHeader", "Reason"))]
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(16, 0, 16, 8)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					RowList
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
					.Text(LOCTEXT("CancelPackagePlan", "Cancel"))
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
					.Text(LOCTEXT("AcceptPackagePlan", "Create Job / Build"))
					.OnClicked_Lambda([&bAccepted, &Window, &ModsDirTextBox, &ModNameTextBox, &JobFilenameTextBox]()
					{
						const FString ModsDir = ModsDirTextBox.IsValid() ? ModsDirTextBox->GetText().ToString().TrimStartAndEnd() : FString();
						const FString ModName = ModNameTextBox.IsValid() ? SanitizeModPackageName(ModNameTextBox->GetText().ToString()) : FString();
						if (ModsDir.IsEmpty())
						{
							NTEBuildTool::Editor::ShowError(LOCTEXT("PackagePlanEmptyModsDir", "Mods Output Directory is empty."));
							return FReply::Handled();
						}
						if (ModName.IsEmpty())
						{
							NTEBuildTool::Editor::ShowError(LOCTEXT("PackagePlanEmptyModName", "Mod Name is empty."));
							return FReply::Handled();
						}
						if (JobFilenameTextBox.IsValid() && JobFilenameTextBox->GetText().ToString().TrimStartAndEnd().IsEmpty())
						{
							JobFilenameTextBox->SetText(FText::FromString(MakeDefaultPackageJobFilename(ModName)));
						}
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

	for (const TSharedPtr<FPackagePlanRow>& Row : Rows)
	{
		if (!Row.IsValid() || !Plan.Candidates.IsValidIndex(Row->CandidateIndex) || !Row->IncludeCheckBox.IsValid())
		{
			continue;
		}
		Plan.Candidates[Row->CandidateIndex].bDefaultIncluded = Row->IncludeCheckBox->IsChecked();
	}

	OutResult = FNtePackagePlanDialogResult();
	OutResult.ModsDir = ModsDirTextBox.IsValid() ? ModsDirTextBox->GetText().ToString().TrimStartAndEnd() : FString();
	OutResult.ModName = ModNameTextBox.IsValid() ? SanitizeModPackageName(ModNameTextBox->GetText().ToString()) : FString();
	OutResult.JobFilename = JobFilenameTextBox.IsValid() ? JobFilenameTextBox->GetText().ToString().TrimStartAndEnd() : FString();
	OutResult.bLaunchBuild = !LaunchBuildCheckBox.IsValid() || LaunchBuildCheckBox->IsChecked();
	if (OutResult.JobFilename.IsEmpty())
	{
		OutResult.JobFilename = MakeDefaultPackageJobFilename(OutResult.ModName);
	}
	return true;
}
}

#undef LOCTEXT_NAMESPACE
