// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteMeshModWorkspaceDialog.h"

#include "NteBuildToolSettings.h"

#include "Engine/SkeletalMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInterface.h"
#include "UObject/Package.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NTEBuildTool"

namespace NTEBuildTool::Workspace
{
namespace
{
FText MakeMeshHeader(USkeletalMesh* Mesh)
{
	if (!Mesh)
	{
		return LOCTEXT("WorkspaceNoMeshSelected", "No SkeletalMesh selected");
	}

	return FText::FromString(Mesh->GetPackage()->GetName());
}

TSharedRef<SWidget> MakeSettingsSummary()
{
	const FString GameMount = NTEBuildTool::Settings::GetGameMountName();
	const FString ModsDir = NTEBuildTool::Settings::GetDefaultModsOutputDirectory();
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("Game Mount: %s"), *GameMount)))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 3, 0, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("Mods Output: %s"), *ModsDir)))
		];
}

TSharedRef<SWidget> MakeMaterialSlotList(USkeletalMesh* Mesh)
{
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	if (!Mesh)
	{
		Rows->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("WorkspaceSelectMeshHint", "Select one SkeletalMesh in the Content Browser to inspect material slots."))
			];
		return Rows;
	}

	const TArray<FSkeletalMaterial>& Materials = Mesh->GetMaterials();
	if (Materials.IsEmpty())
	{
		Rows->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("WorkspaceNoSlots", "This mesh has no material slots."))
			];
		return Rows;
	}

	for (int32 Index = 0; Index < Materials.Num(); ++Index)
	{
		const FSkeletalMaterial& Material = Materials[Index];
		const FString MaterialPath = Material.MaterialInterface ? Material.MaterialInterface->GetPackage()->GetName() : TEXT("<none>");
		Rows->AddSlot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.22f)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%d  %s"), Index, *Material.MaterialSlotName.ToString())))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.78f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(MaterialPath))
				]
			];
	}

	return Rows;
}
}

bool ShowMeshModWorkspaceDialog(USkeletalMesh* SelectedMesh, ENteMeshModWorkspaceAction& OutAction)
{
	OutAction = ENteMeshModWorkspaceAction::None;

	TSharedPtr<SWindow> Window;
	SAssignNew(Window, SWindow)
		.Title(LOCTEXT("MeshModWorkspaceTitle", "NTE Mesh Mod Workspace"))
		.ClientSize(FVector2D(980, 700))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 14, 16, 8)
			[
				SNew(STextBlock)
				.Text(MakeMeshHeader(SelectedMesh))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 4, 16, 8)
			[
				MakeSettingsSummary()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 8, 16, 4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("WorkspaceMaterialSlotsHeader", "Material Slots"))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(16, 0, 16, 12)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					MakeMaterialSlotList(SelectedMesh)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 8, 16, 16)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0, 0, 8, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("WorkspaceMaterialButton", "Material"))
					.IsEnabled(SelectedMesh != nullptr)
					.OnClicked_Lambda([&OutAction, &Window]()
					{
						OutAction = ENteMeshModWorkspaceAction::CreateMaterialInstance;
						Window->RequestDestroyWindow();
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0, 0, 8, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("WorkspaceToggleButton", "Toggle"))
					.IsEnabled(SelectedMesh != nullptr)
					.OnClicked_Lambda([&OutAction, &Window]()
					{
						OutAction = ENteMeshModWorkspaceAction::ConfigureToggleRuntime;
						Window->RequestDestroyWindow();
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0, 0, 8, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("WorkspacePackageButton", "Package"))
					.OnClicked_Lambda([&OutAction, &Window]()
					{
						OutAction = ENteMeshModWorkspaceAction::BuildPackage;
						Window->RequestDestroyWindow();
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("WorkspaceCloseButton", "Close"))
					.OnClicked_Lambda([&OutAction, &Window]()
					{
						OutAction = ENteMeshModWorkspaceAction::None;
						Window->RequestDestroyWindow();
						return FReply::Handled();
					})
				]
			]
		];

	FSlateApplication::Get().AddModalWindow(Window.ToSharedRef(), nullptr);
	return OutAction != ENteMeshModWorkspaceAction::None;
}
}

#undef LOCTEXT_NAMESPACE
