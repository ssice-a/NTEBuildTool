// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteMeshModWorkspaceDialog.h"

#include "NteBuildToolSettings.h"
#include "NteEditorAssetUtils.h"
#include "NteNotificationUtils.h"

#include "Engine/SkeletalMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NTEBuildTool"

namespace NTEBuildTool::Workspace
{
namespace
{
struct FWorkspaceDialogState
{
	NTEBuildTool::Character::FNteCharacterModSpec Spec;
	FString SpecFilename;
	bool bDirty = false;
	bool bRefreshingControls = false;

	TSharedPtr<SEditableTextBox> WorkspaceNameTextBox;
	TSharedPtr<SEditableTextBox> MainMeshPathTextBox;
	TSharedPtr<SEditableTextBox> AppearancePathTextBox;
	TSharedPtr<SEditableTextBox> UIActorPathTextBox;
	TSharedPtr<SEditableTextBox> PackageModNameTextBox;
	TSharedPtr<SEditableTextBox> PackageModsDirTextBox;
	TSharedPtr<SVerticalBox> MaterialOperationRows;
};

FString SanitizeFilenamePart(FString Value)
{
	Value.TrimStartAndEndInline();
	if (Value.IsEmpty())
	{
		Value = TEXT("CharacterModSpec");
	}

	const TCHAR* InvalidChars = TEXT("/\\:*?\"<>|");
	for (const TCHAR* Cursor = InvalidChars; Cursor && *Cursor; ++Cursor)
	{
		Value.ReplaceCharInline(*Cursor, TCHAR('_'));
	}
	return Value;
}

FString MakeDefaultSpecFilename(const NTEBuildTool::Character::FNteCharacterModSpec& Spec)
{
	FString BaseName = Spec.WorkspaceName;
	if (BaseName.IsEmpty())
	{
		BaseName = Spec.Package.ModName;
	}
	if (BaseName.IsEmpty() && !Spec.MainMeshPath.IsEmpty())
	{
		BaseName = FPackageName::GetShortName(Spec.MainMeshPath);
	}
	return SanitizeFilenamePart(BaseName) + TEXT(".spec.json");
}

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

void ApplyTextBoxesToSpec(const TSharedRef<FWorkspaceDialogState>& State)
{
	if (State->WorkspaceNameTextBox.IsValid())
	{
		State->Spec.WorkspaceName = State->WorkspaceNameTextBox->GetText().ToString();
	}
	if (State->MainMeshPathTextBox.IsValid())
	{
		State->Spec.MainMeshPath = NTEBuildTool::Editor::NormalizeAssetPathForText(State->MainMeshPathTextBox->GetText().ToString());
	}
	if (State->AppearancePathTextBox.IsValid())
	{
		State->Spec.Appearance.PlayerAppearanceAssetPath = NTEBuildTool::Editor::NormalizeAssetPathForText(State->AppearancePathTextBox->GetText().ToString());
	}
	if (State->UIActorPathTextBox.IsValid())
	{
		State->Spec.Appearance.UIActorClassPath = NTEBuildTool::Editor::NormalizeAssetPathForText(State->UIActorPathTextBox->GetText().ToString());
	}
	if (State->PackageModNameTextBox.IsValid())
	{
		State->Spec.Package.ModName = State->PackageModNameTextBox->GetText().ToString();
	}
	if (State->PackageModsDirTextBox.IsValid())
	{
		State->Spec.Package.ModsDir = State->PackageModsDirTextBox->GetText().ToString();
	}
}

void RebuildMaterialOperationRows(const TSharedRef<FWorkspaceDialogState>& State)
{
	if (!State->MaterialOperationRows.IsValid())
	{
		return;
	}

	State->MaterialOperationRows->ClearChildren();
	if (State->Spec.MaterialOperations.IsEmpty())
	{
		State->MaterialOperationRows->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("WorkspaceNoMaterialOperations", "No material operations in this spec yet. Use Apply Material on a mesh slot to add one."))
			];
		return;
	}

	for (const NTEBuildTool::Character::FNteCharacterMaterialOperationSpec& Operation : State->Spec.MaterialOperations)
	{
		const FString SlotText = Operation.SlotName.IsEmpty()
			? FString::Printf(TEXT("Slot %d"), Operation.SlotIndex)
			: FString::Printf(TEXT("Slot %d / %s"), Operation.SlotIndex, *Operation.SlotName);
		const FString OutputText = Operation.OutputMaterialPath.IsEmpty() ? TEXT("<output material not set>") : Operation.OutputMaterialPath;

		State->MaterialOperationRows->AddSlot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.24f)
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Operation.Id.IsEmpty() ? TEXT("<missing id>") : Operation.Id))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.18f)
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Operation.TargetMeshId.IsEmpty() ? TEXT("main") : Operation.TargetMeshId))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.22f)
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(SlotText))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.36f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(OutputText))
				]
			];
	}
}

void RefreshWorkspaceEditor(const TSharedRef<FWorkspaceDialogState>& State)
{
	State->bRefreshingControls = true;
	if (State->WorkspaceNameTextBox.IsValid())
	{
		State->WorkspaceNameTextBox->SetText(FText::FromString(State->Spec.WorkspaceName));
	}
	if (State->MainMeshPathTextBox.IsValid())
	{
		State->MainMeshPathTextBox->SetText(FText::FromString(State->Spec.MainMeshPath));
	}
	if (State->AppearancePathTextBox.IsValid())
	{
		State->AppearancePathTextBox->SetText(FText::FromString(State->Spec.Appearance.PlayerAppearanceAssetPath));
	}
	if (State->UIActorPathTextBox.IsValid())
	{
		State->UIActorPathTextBox->SetText(FText::FromString(State->Spec.Appearance.UIActorClassPath));
	}
	if (State->PackageModNameTextBox.IsValid())
	{
		State->PackageModNameTextBox->SetText(FText::FromString(State->Spec.Package.ModName));
	}
	if (State->PackageModsDirTextBox.IsValid())
	{
		State->PackageModsDirTextBox->SetText(FText::FromString(State->Spec.Package.ModsDir));
	}
	State->bRefreshingControls = false;
	RebuildMaterialOperationRows(State);
}

bool SaveWorkspaceSpec(const TSharedRef<FWorkspaceDialogState>& State, const bool bSaveAs)
{
	ApplyTextBoxesToSpec(State);

	FString Filename = State->SpecFilename;
	if (bSaveAs || Filename.IsEmpty())
	{
		if (!NTEBuildTool::Editor::ChooseSaveJsonFileWithTitle(
			LOCTEXT("SaveCharacterModSpecJson", "Save NTE CharacterModSpec JSON"),
			MakeDefaultSpecFilename(State->Spec),
			Filename))
		{
			return false;
		}
	}

	FString Error;
	if (!NTEBuildTool::Character::SaveCharacterModSpecToJsonFile(State->Spec, Filename, Error))
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(Error));
		return false;
	}

	State->SpecFilename = Filename;
	State->bDirty = false;
	NTEBuildTool::Editor::ShowSuccessNotification(FText::Format(
		LOCTEXT("SavedCharacterModSpec", "Saved CharacterModSpec: {0}"),
		FText::FromString(Filename)));
	return true;
}

bool LoadWorkspaceSpec(const TSharedRef<FWorkspaceDialogState>& State, USkeletalMesh* SelectedMesh)
{
	FString Filename;
	if (!NTEBuildTool::Editor::ChooseJsonFileWithTitle(
		LOCTEXT("OpenCharacterModSpecJson", "Open NTE CharacterModSpec JSON"),
		TEXT("CharacterModSpec.spec.json"),
		Filename))
	{
		return false;
	}

	NTEBuildTool::Character::FNteCharacterModSpec LoadedSpec;
	FString Error;
	if (!NTEBuildTool::Character::LoadCharacterModSpecFromJsonFile(Filename, LoadedSpec, Error))
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(Error));
		return false;
	}

	if (LoadedSpec.WorkspaceName.IsEmpty())
	{
		LoadedSpec.WorkspaceName = SelectedMesh ? SelectedMesh->GetName() : TEXT("NewCharacterMod");
	}
	if (LoadedSpec.MainMeshPath.IsEmpty() && SelectedMesh)
	{
		LoadedSpec.MainMeshPath = NTEBuildTool::Editor::GetAssetPackagePath(SelectedMesh);
	}
	if (LoadedSpec.Package.ModsDir.IsEmpty())
	{
		LoadedSpec.Package.ModsDir = NTEBuildTool::Settings::GetDefaultModsOutputDirectory();
	}
	if (LoadedSpec.Package.ModName.IsEmpty())
	{
		LoadedSpec.Package.ModName = LoadedSpec.WorkspaceName + TEXT("_mod_P");
	}

	State->Spec = MoveTemp(LoadedSpec);
	State->SpecFilename = Filename;
	State->bDirty = false;
	RefreshWorkspaceEditor(State);

	NTEBuildTool::Editor::ShowSuccessNotification(FText::Format(
		LOCTEXT("LoadedCharacterModSpec", "Loaded CharacterModSpec: {0}"),
		FText::FromString(Filename)));
	return true;
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

TSharedRef<SWidget> MakeCharacterSpecSummary(const TSharedRef<FWorkspaceDialogState>& State)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text_Lambda([State]()
			{
				return FText::FromString(State->SpecFilename.IsEmpty()
					? TEXT("Spec file: <unsaved>")
					: FString::Printf(TEXT("Spec file: %s"), *State->SpecFilename));
			})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 3, 0, 0)
		[
			SNew(STextBlock)
			.Text_Lambda([State]()
			{
				return FText::FromString(State->Spec.Appearance.PlayerAppearanceAssetPath.IsEmpty()
					? TEXT("Appearance: not selected yet")
					: FString::Printf(TEXT("Appearance: %s"), *State->Spec.Appearance.PlayerAppearanceAssetPath));
			})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 3, 0, 0)
		[
			SNew(STextBlock)
			.Text_Lambda([State]()
			{
				return FText::FromString(State->Spec.Appearance.UIActorClassPath.IsEmpty()
					? TEXT("UI Preview: not selected yet")
					: FString::Printf(TEXT("UI Preview: %s"), *State->Spec.Appearance.UIActorClassPath));
			})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 3, 0, 0)
		[
			SNew(STextBlock)
			.Text_Lambda([State]()
			{
				return FText::FromString(FString::Printf(
					TEXT("Attached: %d   Materials: %d   Runtime Actions: %d   Kawaii Presets: %d"),
					State->Spec.AttachedMeshes.Num(),
					State->Spec.MaterialOperations.Num(),
					State->Spec.RuntimeActions.Num(),
					State->Spec.KawaiiPresets.Num()));
			})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 3, 0, 0)
		[
			SNew(STextBlock)
			.Text_Lambda([State]()
			{
				const NTEBuildTool::Character::FNteCharacterModSpecValidationResult Validation =
					NTEBuildTool::Character::ValidateCharacterModSpec(State->Spec);
				return FText::FromString(FString::Printf(
					TEXT("Spec validation: %d error(s), %d warning(s)%s"),
					Validation.Errors.Num(),
					Validation.Warnings.Num(),
					State->bDirty ? TEXT("   Unsaved changes") : TEXT("")));
			})
		];
}

TSharedRef<SWidget> MakeEditableSpecRow(
	const FText& Label,
	const FString& InitialValue,
	TSharedPtr<SEditableTextBox>& OutTextBox,
	TFunction<void(const FString&)> OnValueChanged)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(0.20f)
		.Padding(0, 0, 8, 0)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(Label)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(0.80f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(OutTextBox, SEditableTextBox)
			.Text(FText::FromString(InitialValue))
			.OnTextChanged_Lambda([OnValueChanged](const FText& Text)
			{
				OnValueChanged(Text.ToString());
			})
		];
}

TSharedRef<SWidget> MakeCharacterSpecEditor(const TSharedRef<FWorkspaceDialogState>& State, USkeletalMesh* SelectedMesh)
{
	const auto MarkDirty = [State]()
	{
		if (!State->bRefreshingControls)
		{
			State->bDirty = true;
		}
	};

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 6)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.20f)
			.Padding(0, 0, 8, 0)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("WorkspaceSpecFileLabel", "Spec JSON"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0, 0, 8, 0)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([State]()
				{
					return FText::FromString(State->SpecFilename.IsEmpty() ? TEXT("<unsaved>") : State->SpecFilename);
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 6, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("WorkspaceLoadSpecButton", "Load"))
				.OnClicked_Lambda([State, SelectedMesh]()
				{
					LoadWorkspaceSpec(State, SelectedMesh);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 6, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("WorkspaceSaveSpecButton", "Save"))
				.OnClicked_Lambda([State]()
				{
					SaveWorkspaceSpec(State, false);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("WorkspaceSaveSpecAsButton", "Save As"))
				.OnClicked_Lambda([State]()
				{
					SaveWorkspaceSpec(State, true);
					return FReply::Handled();
				})
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 6)
		[
			MakeEditableSpecRow(
				LOCTEXT("WorkspaceNameLabel", "Workspace"),
				State->Spec.WorkspaceName,
				State->WorkspaceNameTextBox,
				[State, MarkDirty](const FString& Value)
				{
					State->Spec.WorkspaceName = Value;
					MarkDirty();
				})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 6)
		[
			MakeEditableSpecRow(
				LOCTEXT("WorkspaceMainMeshLabel", "Main Mesh"),
				State->Spec.MainMeshPath,
				State->MainMeshPathTextBox,
				[State, MarkDirty](const FString& Value)
				{
					State->Spec.MainMeshPath = NTEBuildTool::Editor::NormalizeAssetPathForText(Value);
					MarkDirty();
				})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 6)
		[
			MakeEditableSpecRow(
				LOCTEXT("WorkspaceAppearanceLabel", "Appearance"),
				State->Spec.Appearance.PlayerAppearanceAssetPath,
				State->AppearancePathTextBox,
				[State, MarkDirty](const FString& Value)
				{
					State->Spec.Appearance.PlayerAppearanceAssetPath = NTEBuildTool::Editor::NormalizeAssetPathForText(Value);
					MarkDirty();
				})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 6)
		[
			MakeEditableSpecRow(
				LOCTEXT("WorkspaceUIActorLabel", "UI Preview"),
				State->Spec.Appearance.UIActorClassPath,
				State->UIActorPathTextBox,
				[State, MarkDirty](const FString& Value)
				{
					State->Spec.Appearance.UIActorClassPath = NTEBuildTool::Editor::NormalizeAssetPathForText(Value);
					MarkDirty();
				})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 6)
		[
			MakeEditableSpecRow(
				LOCTEXT("WorkspacePackageModNameLabel", "Mod Name"),
				State->Spec.Package.ModName,
				State->PackageModNameTextBox,
				[State, MarkDirty](const FString& Value)
				{
					State->Spec.Package.ModName = Value;
					MarkDirty();
				})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.20f)
			.Padding(0, 0, 8, 0)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("WorkspacePackageModsDirLabel", "Mods Dir"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.80f)
			.Padding(0, 0, 8, 0)
			.VAlign(VAlign_Center)
			[
				SAssignNew(State->PackageModsDirTextBox, SEditableTextBox)
				.Text(FText::FromString(State->Spec.Package.ModsDir))
				.OnTextChanged_Lambda([State, MarkDirty](const FText& Text)
				{
					State->Spec.Package.ModsDir = Text.ToString();
					MarkDirty();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("WorkspaceBrowseModsDirButton", "Browse"))
				.OnClicked_Lambda([State, MarkDirty]()
				{
					FString ModsDir;
					if (NTEBuildTool::Editor::ChooseDirectoryWithTitle(
						LOCTEXT("ChooseWorkspaceModsDir", "Choose Mods Output Directory"),
						State->Spec.Package.ModsDir.IsEmpty() ? NTEBuildTool::Settings::GetDefaultModsOutputDirectory() : State->Spec.Package.ModsDir,
						ModsDir))
					{
						State->Spec.Package.ModsDir = ModsDir;
						if (State->PackageModsDirTextBox.IsValid())
						{
							State->PackageModsDirTextBox->SetText(FText::FromString(ModsDir));
						}
						MarkDirty();
					}
					return FReply::Handled();
				})
			]
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
	const TSharedRef<FWorkspaceDialogState>& State,
	USkeletalMesh* Mesh,
	const int32 SlotIndex = INDEX_NONE,
	const FString& SlotName = FString(),
	const FString& MaterialPath = FString())
{
	ApplyTextBoxesToSpec(State);
	Result.Action = Action;
	Result.CharacterSpec = State->Spec;
	Result.SpecFilename = State->SpecFilename;
	Result.MeshPath = !State->Spec.MainMeshPath.IsEmpty()
		? State->Spec.MainMeshPath
		: (Mesh ? Mesh->GetPackage()->GetName() : FString());
	Result.SlotIndex = SlotIndex;
	Result.SlotName = SlotName;
	Result.MaterialPath = MaterialPath;
	if (Window.IsValid())
	{
		Window->RequestDestroyWindow();
	}
}

TSharedRef<SWidget> MakeMaterialSlotList(
	USkeletalMesh* Mesh,
	const TSharedRef<FWorkspaceDialogState>& State,
	TSharedPtr<SWindow>& Window,
	FNteMeshModWorkspaceResult& OutResult)
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
				.FillWidth(0.60f)
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
					.OnClicked_Lambda([&Window, &OutResult, State, Mesh, Index, SlotName, MaterialPath]()
					{
						CloseWithAction(Window, OutResult, ENteMeshModWorkspaceAction::ApplyMaterialOperation, State, Mesh, Index, SlotName, MaterialPath);
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6, 0, 0, 0)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Text(LOCTEXT("WorkspaceSlotRuntimeActionButton", "Add Toggle"))
					.OnClicked_Lambda([&Window, &OutResult, State, Mesh, Index, SlotName, MaterialPath]()
					{
						CloseWithAction(Window, OutResult, ENteMeshModWorkspaceAction::ConfigureToggleRuntime, State, Mesh, Index, SlotName, MaterialPath);
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
	TSharedRef<FWorkspaceDialogState> State = MakeShared<FWorkspaceDialogState>();
	State->Spec = MakeDraftCharacterSpec(SelectedMesh);
	OutResult.CharacterSpec = State->Spec;

	TSharedPtr<SWindow> Window;
	SAssignNew(Window, SWindow)
		.Title(LOCTEXT("MeshModWorkspaceTitle", "NTE Character Mod Workspace"))
		.ClientSize(FVector2D(1180, 860))
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
				MakeCharacterSpecSummary(State)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 4, 16, 10)
			[
				MakeCharacterSpecEditor(State, SelectedMesh)
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
				.Text(LOCTEXT("WorkspaceMaterialOperationsHeader", "Material Operations in Spec"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 0, 16, 8)
			[
				SAssignNew(State->MaterialOperationRows, SVerticalBox)
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
					MakeMaterialSlotList(SelectedMesh, State, Window, OutResult)
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
					.OnClicked_Lambda([&OutResult, &Window, State, SelectedMesh]()
					{
						CloseWithAction(Window, OutResult, ENteMeshModWorkspaceAction::ApplyMaterialOperation, State, SelectedMesh);
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0, 0, 8, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("WorkspaceRuntimeActionButton", "Runtime Action"))
					.IsEnabled(SelectedMesh != nullptr)
					.OnClicked_Lambda([&OutResult, &Window, State, SelectedMesh]()
					{
						CloseWithAction(Window, OutResult, ENteMeshModWorkspaceAction::ConfigureToggleRuntime, State, SelectedMesh);
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0, 0, 8, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("WorkspacePackageButton", "Package"))
					.OnClicked_Lambda([&OutResult, &Window, State, SelectedMesh]()
					{
						CloseWithAction(Window, OutResult, ENteMeshModWorkspaceAction::BuildPackage, State, SelectedMesh);
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

	RebuildMaterialOperationRows(State);
	FSlateApplication::Get().AddModalWindow(Window.ToSharedRef(), nullptr);
	return OutResult.Action != ENteMeshModWorkspaceAction::None;
}
}

#undef LOCTEXT_NAMESPACE
