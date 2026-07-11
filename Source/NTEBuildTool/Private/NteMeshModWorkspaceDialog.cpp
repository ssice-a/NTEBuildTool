// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteMeshModWorkspaceDialog.h"

#include "NteBuildToolSettings.h"
#include "NteEditorAssetUtils.h"

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
		return LOCTEXT("WorkspaceNoMeshSelected", "NTE Character Mod Workspace");
	}

	return FText::FromString(FString::Printf(TEXT("NTE Character Mod Workspace - %s"), *Mesh->GetPackage()->GetName()));
}

NTEBuildTool::Character::FNteCharacterModSpec MakeDraftCharacterSpec(USkeletalMesh* Mesh)
{
	NTEBuildTool::Character::FNteCharacterModSpec Spec;
	Spec.WorkspaceName = Mesh ? Mesh->GetName() : TEXT("NewCharacterMod");
	Spec.MainMeshPath = Mesh ? NTEBuildTool::Editor::GetAssetPackagePath(Mesh) : FString();
	Spec.Package.ModsDir = NTEBuildTool::Settings::GetDefaultModsOutputDirectory();
	if (Mesh)
	{
		Spec.Package.ModName = Mesh->GetName() + TEXT("_mod_P");
	}
	return Spec;
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

TSharedRef<SWidget> MakeCharacterSpecSummary(const NTEBuildTool::Character::FNteCharacterModSpec& Spec)
{
	const NTEBuildTool::Character::FNteCharacterModSpecValidationResult Validation =
		NTEBuildTool::Character::ValidateCharacterModSpec(Spec);

	const FString AppearanceText = Spec.Appearance.PlayerAppearanceAssetPath.IsEmpty()
		? TEXT("Appearance: not selected yet")
		: FString::Printf(TEXT("Appearance: %s"), *Spec.Appearance.PlayerAppearanceAssetPath);
	const FString UIShowText = Spec.Appearance.UIActorClassPath.IsEmpty()
		? TEXT("UI Preview: not selected yet")
		: FString::Printf(TEXT("UI Preview: %s"), *Spec.Appearance.UIActorClassPath);
	const FString CountsText = FString::Printf(
		TEXT("Attached: %d   Materials: %d   Runtime Actions: %d   Kawaii Presets: %d"),
		Spec.AttachedMeshes.Num(),
		Spec.MaterialOperations.Num(),
		Spec.RuntimeActions.Num(),
		Spec.KawaiiPresets.Num());
	const FString ValidationText = FString::Printf(
		TEXT("Spec validation: %d error(s), %d warning(s)"),
		Validation.Errors.Num(),
		Validation.Warnings.Num());

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(AppearanceText))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 3, 0, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(UIShowText))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 3, 0, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(CountsText))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 3, 0, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(ValidationText))
		];
}

TSharedRef<SWidget> MakeWorkspaceStageList()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("WorkspaceStageAppearance", "1. Appearance Assembly: MeshAsset + PlayerUIShow sync from CharacterModSpec"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 3, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("WorkspaceStageMaterial", "2. Materials: source texture groups -> material instances -> mesh slots"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 3, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("WorkspaceStageRuntime", "3. Runtime Actions: hotkeys + UI buttons from one action model"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 3, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("WorkspaceStagePhysics", "4. Physics: UE-edited Kawaii presets seeded from source-game JSON"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 3, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("WorkspaceStagePackage", "5. Package: replaced assets + referenced added assets -> pak/utoc/ucas"))
		];
}

void CloseWithAction(
	const TSharedPtr<SWindow>& Window,
	FNteMeshModWorkspaceResult& Result,
	const ENteMeshModWorkspaceAction Action,
	USkeletalMesh* Mesh,
	const int32 SlotIndex = INDEX_NONE,
	const FString& SlotName = FString(),
	const FString& MaterialPath = FString())
{
	Result.Action = Action;
	Result.CharacterSpec = MakeDraftCharacterSpec(Mesh);
	Result.MeshPath = Mesh ? Mesh->GetPackage()->GetName() : FString();
	Result.SlotIndex = SlotIndex;
	Result.SlotName = SlotName;
	Result.MaterialPath = MaterialPath;
	if (Window.IsValid())
	{
		Window->RequestDestroyWindow();
	}
}

TSharedRef<SWidget> MakeMaterialSlotList(USkeletalMesh* Mesh, TSharedPtr<SWindow>& Window, FNteMeshModWorkspaceResult& OutResult)
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
		const FString SlotName = Material.MaterialSlotName.ToString();
		const FString MaterialPath = Material.MaterialInterface ? Material.MaterialInterface->GetPackage()->GetName() : FString();
		Rows->AddSlot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.20f)
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%d  %s"), Index, *SlotName)))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.70f)
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(MaterialPath.IsEmpty() ? TEXT("<none>") : MaterialPath))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Text(LOCTEXT("WorkspaceSlotMaterialButton", "Apply Material"))
					.OnClicked_Lambda([&Window, &OutResult, Mesh, Index, SlotName, MaterialPath]()
					{
						CloseWithAction(Window, OutResult, ENteMeshModWorkspaceAction::ApplyMaterialOperation, Mesh, Index, SlotName, MaterialPath);
						return FReply::Handled();
					})
				]
			];
	}

	return Rows;
}
}

bool ShowMeshModWorkspaceDialog(USkeletalMesh* SelectedMesh, ENteMeshModWorkspaceAction& OutAction)
{
	FNteMeshModWorkspaceResult Result;
	const bool bAccepted = ShowMeshModWorkspaceDialog(SelectedMesh, Result);
	OutAction = Result.Action;
	return bAccepted;
}

bool ShowMeshModWorkspaceDialog(USkeletalMesh* SelectedMesh, FNteMeshModWorkspaceResult& OutResult)
{
	OutResult = FNteMeshModWorkspaceResult();
	OutResult.CharacterSpec = MakeDraftCharacterSpec(SelectedMesh);

	TSharedPtr<SWindow> Window;
	SAssignNew(Window, SWindow)
		.Title(LOCTEXT("MeshModWorkspaceTitle", "NTE Character Mod Workspace"))
		.ClientSize(FVector2D(1080, 780))
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
				MakeCharacterSpecSummary(OutResult.CharacterSpec)
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
				.Text(LOCTEXT("WorkspacePipelineHeader", "Workspace Pipeline"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 0, 16, 8)
			[
				MakeWorkspaceStageList()
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
					MakeMaterialSlotList(SelectedMesh, Window, OutResult)
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
					.Text(LOCTEXT("WorkspaceMaterialButton", "Material Operation"))
					.IsEnabled(SelectedMesh != nullptr)
					.OnClicked_Lambda([&OutResult, &Window, SelectedMesh]()
					{
						CloseWithAction(Window, OutResult, ENteMeshModWorkspaceAction::ApplyMaterialOperation, SelectedMesh);
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
					.OnClicked_Lambda([&OutResult, &Window, SelectedMesh]()
					{
						CloseWithAction(Window, OutResult, ENteMeshModWorkspaceAction::ConfigureToggleRuntime, SelectedMesh);
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0, 0, 8, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("WorkspacePackageButton", "Package"))
					.OnClicked_Lambda([&OutResult, &Window, SelectedMesh]()
					{
						CloseWithAction(Window, OutResult, ENteMeshModWorkspaceAction::BuildPackage, SelectedMesh);
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("WorkspaceCloseButton", "Close"))
					.OnClicked_Lambda([&OutResult, &Window]()
					{
						OutResult.Action = ENteMeshModWorkspaceAction::None;
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
	return OutResult.Action != ENteMeshModWorkspaceAction::None;
}
}

#undef LOCTEXT_NAMESPACE
