// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteModPackageDialog.h"

#include "NteModPackagePlan.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
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
}

bool ShowPackagePlanDialog(FNtePackagePlan& Plan)
{
	TSharedPtr<SWindow> Window;
	TArray<TSharedPtr<FPackagePlanRow>> Rows;
	bool bAccepted = false;

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
		.ClientSize(FVector2D(1100, 720))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 14, 16, 8)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PackagePlanHeader", "Review package candidates before creating the package job. Source and proxy candidates are visible but off by default."))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 4, 16, 4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[SNew(STextBlock).Text(LOCTEXT("PackagePlanIncludeHeader", "Pack"))]
				+ SHorizontalBox::Slot().FillWidth(0.18f).Padding(0, 0, 8, 0)[SNew(STextBlock).Text(LOCTEXT("PackagePlanKindHeader", "Kind"))]
				+ SHorizontalBox::Slot().FillWidth(0.56f).Padding(0, 0, 8, 0)[SNew(STextBlock).Text(LOCTEXT("PackagePlanPackageHeader", "Package"))]
				+ SHorizontalBox::Slot().FillWidth(0.26f)[SNew(STextBlock).Text(LOCTEXT("PackagePlanReasonHeader", "Reason"))]
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
					.Text(LOCTEXT("AcceptPackagePlan", "Create Job"))
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

	for (const TSharedPtr<FPackagePlanRow>& Row : Rows)
	{
		if (!Row.IsValid() || !Plan.Candidates.IsValidIndex(Row->CandidateIndex) || !Row->IncludeCheckBox.IsValid())
		{
			continue;
		}
		Plan.Candidates[Row->CandidateIndex].bDefaultIncluded = Row->IncludeCheckBox->IsChecked();
	}
	return true;
}
}

#undef LOCTEXT_NAMESPACE
