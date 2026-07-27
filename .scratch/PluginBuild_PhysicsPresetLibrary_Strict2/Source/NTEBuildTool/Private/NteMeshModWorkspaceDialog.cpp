// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteMeshModWorkspaceDialog.h"

#include "NteBuildToolSettings.h"
#include "NteCharacterKawaiiAssetSync.h"
#include "NteCharacterKawaiiPlan.h"
#include "NteCharacterKawaiiPresetImporter.h"
#include "NteCharacterKawaiiWriter.h"
#include "NteEditorAssetUtils.h"
#include "NteKawaiiPresetLibraryDialog.h"
#include "NteNotificationUtils.h"

#include "Engine/SkeletalMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

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
	TSharedPtr<SVerticalBox> AttachedMeshRows;
	TSharedPtr<SVerticalBox> MaterialOperationRows;
	TSharedPtr<SVerticalBox> KawaiiPresetRows;
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

void RebuildKawaiiPresetRows(const TSharedRef<FWorkspaceDialogState>& State);
void RebuildAttachedMeshRows(const TSharedRef<FWorkspaceDialogState>& State);

NTEBuildTool::Character::FNteCharacterKawaiiPlan MakeKawaiiApplyPlanForRuntimeGroup(
	const NTEBuildTool::Character::FNteCharacterKawaiiPlan& FullPlan,
	const FString& RuntimeAnimBlueprintPath)
{
	NTEBuildTool::Character::FNteCharacterKawaiiPlan ApplyPlan;
	ApplyPlan.KawaiiAssetRootPath = FullPlan.KawaiiAssetRootPath;
	if (ApplyPlan.KawaiiAssetRootPath.IsEmpty())
	{
		ApplyPlan.Errors.Add(TEXT("Could not derive a /Game Kawaii asset root path from CharacterModSpec."));
	}

	for (const NTEBuildTool::Character::FNteCharacterKawaiiPresetPlanItem& PlanItem : FullPlan.Presets)
	{
		if (!PlanItem.RuntimeAnimBlueprintPath.Equals(RuntimeAnimBlueprintPath, ESearchCase::IgnoreCase))
		{
			continue;
		}

		ApplyPlan.Errors.Append(PlanItem.Errors);
		ApplyPlan.Warnings.Append(PlanItem.Warnings);
		for (const FString& Seed : PlanItem.PackageSeeds)
		{
			ApplyPlan.PackageSeeds.AddUnique(Seed);
		}
		ApplyPlan.Presets.Add(PlanItem);
	}
	ApplyPlan.PackageSeeds.Sort();
	return ApplyPlan;
}

const NTEBuildTool::Character::FNteCharacterKawaiiPresetPlanItem* FindKawaiiPresetPlanItem(
	const NTEBuildTool::Character::FNteCharacterKawaiiPlan& Plan,
	const FString& PresetId)
{
	for (const NTEBuildTool::Character::FNteCharacterKawaiiPresetPlanItem& PlanItem : Plan.Presets)
	{
		if (PlanItem.Id == PresetId)
		{
			return &PlanItem;
		}
	}
	return nullptr;
}

bool CanUseAttachedKawaiiRuntimeAction(
	const NTEBuildTool::Character::FNteCharacterKawaiiPresetPlanItem& PlanItem,
	const FString& PresetId)
{
	if (PlanItem.RuntimeAnimBlueprintPath.IsEmpty())
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Printf(
			TEXT("Kawaii preset has no RuntimeAnimBlueprintPath: %s"),
			*PresetId)));
		return false;
	}
	if (!PlanItem.TargetKind.Equals(TEXT("AttachedMesh"), ESearchCase::IgnoreCase))
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Printf(
			TEXT("Kawaii Runtime AnimBP generation currently supports attached mesh targets only. Preset '%s' targets %s; main-mesh Kawaii still needs a source-pose strategy so the generated graph does not replace the game's character animation."),
			*PresetId,
			*PlanItem.TargetKind)));
		return false;
	}
	return true;
}

bool ApplyKawaiiPresetGroup(const TSharedRef<FWorkspaceDialogState>& State, const FString& PresetId)
{
	ApplyTextBoxesToSpec(State);

	const NTEBuildTool::Character::FNteCharacterKawaiiPlan FullPlan =
		NTEBuildTool::Character::BuildCharacterKawaiiPlanFromSpec(State->Spec);
	const NTEBuildTool::Character::FNteCharacterKawaiiPresetPlanItem* SelectedPlanItem =
		FindKawaiiPresetPlanItem(FullPlan, PresetId);
	if (!SelectedPlanItem)
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Printf(
			TEXT("Kawaii preset is not present in the generated KawaiiPlan: %s"),
			*PresetId)));
		return false;
	}
	if (!CanUseAttachedKawaiiRuntimeAction(*SelectedPlanItem, PresetId))
	{
		return false;
	}

	const FString RuntimeAnimBlueprintPath = SelectedPlanItem->RuntimeAnimBlueprintPath;
	NTEBuildTool::Character::FNteCharacterKawaiiPlan ApplyPlan =
		MakeKawaiiApplyPlanForRuntimeGroup(FullPlan, RuntimeAnimBlueprintPath);
	if (ApplyPlan.Presets.IsEmpty())
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Printf(
			TEXT("No Kawaii presets resolve to RuntimeAnimBlueprintPath: %s"),
			*RuntimeAnimBlueprintPath)));
		return false;
	}

	NTEBuildTool::Character::FNteCharacterKawaiiWriteResult WriteResult =
		NTEBuildTool::Character::WriteCharacterKawaiiAssets(ApplyPlan);
	if (WriteResult.HasErrors())
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Join(WriteResult.Errors, TEXT("\n"))));
		return false;
	}

	NTEBuildTool::Editor::ShowSuccessNotification(FText::Format(
		LOCTEXT("AppliedKawaiiPreset", "Applied Kawaii preset group '{0}' ({1} asset result(s), {2} warning(s)). Open the AnimBP/DataAssets to edit with native Kawaii tools."),
		FText::FromString(PresetId),
		FText::AsNumber(WriteResult.Assets.Num()),
		FText::AsNumber(WriteResult.Warnings.Num())));
	return true;
}

enum class EKawaiiWorkspaceOpenTarget
{
	RuntimeAnimBlueprint,
	LimitsDataAsset,
	BoneConstraintsDataAsset
};

FString ResolveKawaiiOpenTargetPath(
	const NTEBuildTool::Character::FNteCharacterKawaiiPresetPlanItem& PlanItem,
	const EKawaiiWorkspaceOpenTarget Target)
{
	switch (Target)
	{
	case EKawaiiWorkspaceOpenTarget::RuntimeAnimBlueprint:
		return PlanItem.RuntimeAnimBlueprintPath;
	case EKawaiiWorkspaceOpenTarget::LimitsDataAsset:
		return PlanItem.OutputLimitsDataAssetPath;
	case EKawaiiWorkspaceOpenTarget::BoneConstraintsDataAsset:
		return PlanItem.OutputBoneConstraintsDataAssetPath;
	default:
		return FString();
	}
}

bool OpenKawaiiPresetAsset(
	const TSharedRef<FWorkspaceDialogState>& State,
	const FString& PresetId,
	const EKawaiiWorkspaceOpenTarget Target)
{
	ApplyTextBoxesToSpec(State);

	const NTEBuildTool::Character::FNteCharacterKawaiiPlan FullPlan =
		NTEBuildTool::Character::BuildCharacterKawaiiPlanFromSpec(State->Spec);
	const NTEBuildTool::Character::FNteCharacterKawaiiPresetPlanItem* SelectedPlanItem =
		FindKawaiiPresetPlanItem(FullPlan, PresetId);
	if (!SelectedPlanItem)
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Printf(
			TEXT("Kawaii preset is not present in the generated KawaiiPlan: %s"),
			*PresetId)));
		return false;
	}

	const FString AssetPath = ResolveKawaiiOpenTargetPath(*SelectedPlanItem, Target);
	if (AssetPath.IsEmpty())
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Printf(
			TEXT("Kawaii preset '%s' has no asset path for this open action. Apply Kawaii first if the path is derived."),
			*PresetId)));
		return false;
	}

	FString OpenError;
	if (!NTEBuildTool::Editor::OpenAssetEditorByPath(AssetPath, OpenError))
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(OpenError));
		return false;
	}
	return true;
}

bool SyncKawaiiPresetFromGeneratedAssets(const TSharedRef<FWorkspaceDialogState>& State, const FString& PresetId)
{
	ApplyTextBoxesToSpec(State);

	int32 PresetIndex = INDEX_NONE;
	for (int32 Index = 0; Index < State->Spec.KawaiiPresets.Num(); ++Index)
	{
		if (State->Spec.KawaiiPresets[Index].Id == PresetId)
		{
			PresetIndex = Index;
			break;
		}
	}
	if (PresetIndex == INDEX_NONE)
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Printf(
			TEXT("Kawaii preset does not exist in CharacterModSpec: %s"),
			*PresetId)));
		return false;
	}

	const NTEBuildTool::Character::FNteCharacterKawaiiPlan FullPlan =
		NTEBuildTool::Character::BuildCharacterKawaiiPlanFromSpec(State->Spec);
	const NTEBuildTool::Character::FNteCharacterKawaiiPresetPlanItem* SelectedPlanItem =
		FindKawaiiPresetPlanItem(FullPlan, PresetId);
	if (!SelectedPlanItem)
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Printf(
			TEXT("Kawaii preset is not present in the generated KawaiiPlan: %s"),
			*PresetId)));
		return false;
	}

	NTEBuildTool::Character::FNteCharacterKawaiiAssetSyncResult SyncResult =
		NTEBuildTool::Character::SyncKawaiiPresetSpecFromGeneratedAssets(
			*SelectedPlanItem,
			State->Spec.KawaiiPresets[PresetIndex]);
	if (SyncResult.HasErrors())
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Join(SyncResult.Errors, TEXT("\n"))));
		return false;
	}

	State->bDirty = true;
	RebuildKawaiiPresetRows(State);
	NTEBuildTool::Editor::ShowSuccessNotification(FText::Format(
		LOCTEXT("SyncedKawaiiPresetFromAssets", "Synced Kawaii preset '{0}' from generated assets. Updated fields: {1}. Warnings: {2}. Save the CharacterModSpec to persist it."),
		FText::FromString(PresetId),
		FText::FromString(SyncResult.UpdatedFields.IsEmpty() ? FString(TEXT("<none>")) : FString::Join(SyncResult.UpdatedFields, TEXT(", "))),
		FText::AsNumber(SyncResult.Warnings.Num())));
	return true;
}

void RebuildKawaiiPresetRows(const TSharedRef<FWorkspaceDialogState>& State)
{
	if (!State->KawaiiPresetRows.IsValid())
	{
		return;
	}

	State->KawaiiPresetRows->ClearChildren();
	if (State->Spec.KawaiiPresets.IsEmpty())
	{
		State->KawaiiPresetRows->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("WorkspaceNoKawaiiPresets", "No Kawaii presets in this spec yet. Import FModel AnimBP/AnimLayer JSON or create one in the physics editor."))
			];
		return;
	}

	const NTEBuildTool::Character::FNteCharacterKawaiiPlan KawaiiPlan =
		NTEBuildTool::Character::BuildCharacterKawaiiPlanFromSpec(State->Spec);
	TMap<FString, const NTEBuildTool::Character::FNteCharacterKawaiiPresetPlanItem*> PresetPlansById;
	for (const NTEBuildTool::Character::FNteCharacterKawaiiPresetPlanItem& PlanItem : KawaiiPlan.Presets)
	{
		if (!PlanItem.Id.IsEmpty())
		{
			PresetPlansById.Add(PlanItem.Id, &PlanItem);
		}
	}

	for (const NTEBuildTool::Character::FNteCharacterKawaiiPresetSpec& Preset : State->Spec.KawaiiPresets)
	{
		const NTEBuildTool::Character::FNteCharacterKawaiiPresetPlanItem* const* PlanItemPtr = PresetPlansById.Find(Preset.Id);
		const NTEBuildTool::Character::FNteCharacterKawaiiPresetPlanItem* PlanItem = PlanItemPtr ? *PlanItemPtr : nullptr;
		const FString SourceText = Preset.SourceKind.IsEmpty()
			? TEXT("Manual")
			: (Preset.SourceNodeName.IsEmpty() ? Preset.SourceKind : FString::Printf(TEXT("%s / %s"), *Preset.SourceKind, *Preset.SourceNodeName));
		const FString CountsText = FString::Printf(
			TEXT("roots %d  limits %d  curves %d"),
			Preset.AdditionalRootBones.Num(),
			Preset.CollisionLimits.Num(),
			Preset.Curves.Num());
		const FString TargetText = PlanItem
			? FString::Printf(
				TEXT("%s / %s"),
				*(PlanItem->TargetMeshId.IsEmpty() ? FString(TEXT("main")) : PlanItem->TargetMeshId),
				*(PlanItem->TargetKind.IsEmpty() ? FString(TEXT("?")) : PlanItem->TargetKind))
			: (Preset.TargetMeshId.IsEmpty() ? FString(TEXT("main")) : Preset.TargetMeshId);
		const FString SkeletonText = PlanItem
			? (PlanItem->bTargetMeshLoaded
				? (PlanItem->TargetSkeletonPath.IsEmpty() ? FString(TEXT("<no skeleton>")) : FPackageName::GetShortName(PlanItem->TargetSkeletonPath))
				: FString(TEXT("<mesh missing>")))
			: FString(TEXT("<not planned>"));
		const FString DiagnosticsText = PlanItem
			? FString::Printf(
				TEXT("%s%s"),
				PlanItem->MissingBones.IsEmpty()
					? TEXT("bones ok")
					: *FString::Printf(TEXT("missing %d bone(s)"), PlanItem->MissingBones.Num()),
				PlanItem->bKawaiiPhysicsTagChecked
					? (PlanItem->bKawaiiPhysicsTagValid ? TEXT(" / tag ok") : TEXT(" / tag invalid"))
					: TEXT(""))
			: FString(TEXT("not planned"));
		const FText DiagnosticsTooltip = PlanItem
			? FText::FromString(FString::Printf(
				TEXT("TargetMesh: %s\nSkeleton: %s\nReferencedBones: %d\nMissingBones: %s\nKawaiiPhysicsTag: %s\nRuntimeAnimBP: %s"),
				*PlanItem->TargetMeshPath,
				*PlanItem->TargetSkeletonPath,
				PlanItem->ReferencedBones.Num(),
				PlanItem->MissingBones.IsEmpty() ? TEXT("<none>") : *FString::Join(PlanItem->MissingBones, TEXT(", ")),
				PlanItem->KawaiiPhysicsTag.IsEmpty()
					? TEXT("<none>")
					: *FString::Printf(TEXT("%s (%s)"), *PlanItem->KawaiiPhysicsTag, PlanItem->bKawaiiPhysicsTagValid ? TEXT("valid") : TEXT("invalid")),
				*PlanItem->RuntimeAnimBlueprintPath))
			: LOCTEXT("WorkspaceKawaiiNotPlannedTooltip", "This preset was not present in the generated KawaiiPlan.");
		const bool bCanUseNativeKawaiiAssets = PlanItem
			&& PlanItem->TargetKind.Equals(TEXT("AttachedMesh"), ESearchCase::IgnoreCase)
			&& !PlanItem->RuntimeAnimBlueprintPath.IsEmpty();
		const FText ApplyTooltip = PlanItem
			? (bCanUseNativeKawaiiAssets
				? FText::FromString(FString::Printf(
					TEXT("Write this Runtime AnimBP group from CharacterModSpec. This overwrites generated Kawaii assets:\n%s"),
					*PlanItem->RuntimeAnimBlueprintPath))
				: LOCTEXT("WorkspaceKawaiiApplyUnsupportedTooltip", "Apply is currently available for attached-mesh Kawaii Runtime AnimBPs. Main-mesh Kawaii needs a source-pose strategy first."))
			: LOCTEXT("WorkspaceKawaiiApplyNotPlannedTooltip", "This preset is not present in KawaiiPlan.");
		const FText OpenAnimTooltip = PlanItem
			? FText::FromString(FString::Printf(TEXT("Open the generated Runtime AnimBP without applying/overwriting:\n%s"), *PlanItem->RuntimeAnimBlueprintPath))
			: LOCTEXT("WorkspaceKawaiiOpenAnimNotPlannedTooltip", "This preset is not present in KawaiiPlan.");
		const FText OpenLimitsTooltip = PlanItem
			? FText::FromString(FString::Printf(TEXT("Open the generated Kawaii Limits DataAsset without applying/overwriting:\n%s"), *PlanItem->OutputLimitsDataAssetPath))
			: LOCTEXT("WorkspaceKawaiiOpenLimitsNotPlannedTooltip", "This preset is not present in KawaiiPlan.");
		const FText OpenConstraintsTooltip = PlanItem
			? FText::FromString(FString::Printf(TEXT("Open the generated Kawaii BoneConstraints DataAsset without applying/overwriting:\n%s"), *PlanItem->OutputBoneConstraintsDataAssetPath))
			: LOCTEXT("WorkspaceKawaiiOpenConstraintsNotPlannedTooltip", "This preset is not present in KawaiiPlan.");
		const FText SyncTooltip = PlanItem
			? (bCanUseNativeKawaiiAssets
				? LOCTEXT("WorkspaceKawaiiSyncTooltip", "Read the current generated AnimBP/DataAsset values back into this CharacterModSpec preset after editing with native Kawaii tools.")
				: LOCTEXT("WorkspaceKawaiiSyncUnsupportedTooltip", "Sync is currently available for attached-mesh Kawaii Runtime AnimBPs. Main-mesh Kawaii needs a source-pose strategy first."))
			: LOCTEXT("WorkspaceKawaiiSyncNotPlannedTooltip", "This preset is not present in KawaiiPlan.");
		const FString PresetId = Preset.Id;

		State->KawaiiPresetRows->AddSlot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.22f)
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Preset.Id.IsEmpty() ? TEXT("<missing id>") : Preset.Id))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.16f)
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TargetText))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.14f)
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Preset.RootBone.IsEmpty() ? TEXT("<no root bone>") : Preset.RootBone))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.14f)
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(CountsText))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.14f)
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(SkeletonText))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.18f)
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(DiagnosticsText))
					.ToolTipText(DiagnosticsTooltip)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.22f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(SourceText))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 4, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("WorkspaceKawaiiApplyButton", "Apply"))
						.ToolTipText(ApplyTooltip)
						.IsEnabled(bCanUseNativeKawaiiAssets)
						.OnClicked_Lambda([State, PresetId]()
						{
							ApplyKawaiiPresetGroup(State, PresetId);
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 4, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("WorkspaceKawaiiOpenAnimButton", "AnimBP"))
						.ToolTipText(OpenAnimTooltip)
						.IsEnabled(PlanItem && !PlanItem->RuntimeAnimBlueprintPath.IsEmpty())
						.OnClicked_Lambda([State, PresetId]()
						{
							OpenKawaiiPresetAsset(State, PresetId, EKawaiiWorkspaceOpenTarget::RuntimeAnimBlueprint);
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 4, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("WorkspaceKawaiiOpenLimitsButton", "Limits"))
						.ToolTipText(OpenLimitsTooltip)
						.IsEnabled(PlanItem && !PlanItem->OutputLimitsDataAssetPath.IsEmpty())
						.OnClicked_Lambda([State, PresetId]()
						{
							OpenKawaiiPresetAsset(State, PresetId, EKawaiiWorkspaceOpenTarget::LimitsDataAsset);
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 4, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("WorkspaceKawaiiOpenConstraintsButton", "Constraints"))
						.ToolTipText(OpenConstraintsTooltip)
						.IsEnabled(PlanItem && !PlanItem->OutputBoneConstraintsDataAssetPath.IsEmpty())
						.OnClicked_Lambda([State, PresetId]()
						{
							OpenKawaiiPresetAsset(State, PresetId, EKawaiiWorkspaceOpenTarget::BoneConstraintsDataAsset);
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("WorkspaceKawaiiSyncButton", "Sync"))
						.ToolTipText(SyncTooltip)
						.IsEnabled(bCanUseNativeKawaiiAssets)
						.OnClicked_Lambda([State, PresetId]()
						{
							SyncKawaiiPresetFromGeneratedAssets(State, PresetId);
							return FReply::Handled();
						})
					]
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
	RebuildAttachedMeshRows(State);
	RebuildKawaiiPresetRows(State);
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

TSharedRef<SWidget> MakeEditableSpecRow(
	const FText& Label,
	const FString& InitialValue,
	TSharedPtr<SEditableTextBox>& OutTextBox,
	TFunction<void(const FString&)> OnValueChanged);

bool ShowAttachedMeshDialog(NTEBuildTool::Character::FNteCharacterAttachedMeshSpec& OutAttachedMesh)
{
	NTEBuildTool::Character::FNteCharacterAttachedMeshSpec Draft;
	Draft.Id = TEXT("physics_part");
	Draft.Label = TEXT("Physics Part");
	Draft.SocketName = TEXT("root");

	TSharedPtr<SEditableTextBox> IdTextBox;
	TSharedPtr<SEditableTextBox> LabelTextBox;
	TSharedPtr<SEditableTextBox> MeshPathTextBox;
	TSharedPtr<SEditableTextBox> SocketTextBox;
	TSharedPtr<SWindow> Window;
	bool bAccepted = false;
	SAssignNew(Window, SWindow)
		.Title(LOCTEXT("AddAttachedMeshTitle", "Add Attached Skeletal Mesh"))
		.ClientSize(FVector2D(760, 270))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(14, 12, 14, 8)
			[
				SNew(STextBlock).Text(LOCTEXT("AddAttachedMeshHint", "Register a separate clothing or physics mesh. Kawaii will generate its CopyPose -> Kawaii AnimBP on this mesh without replacing the main character AnimBP."))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(14, 0, 14, 6)[MakeEditableSpecRow(LOCTEXT("AttachedMeshIdLabel", "Mesh Id"), Draft.Id, IdTextBox, [](const FString&) {})]
			+ SVerticalBox::Slot().AutoHeight().Padding(14, 0, 14, 6)[MakeEditableSpecRow(LOCTEXT("AttachedMeshLabelLabel", "Label"), Draft.Label, LabelTextBox, [](const FString&) {})]
			+ SVerticalBox::Slot().AutoHeight().Padding(14, 0, 14, 6)[MakeEditableSpecRow(LOCTEXT("AttachedMeshPathLabel", "Skeletal Mesh"), Draft.MeshPath, MeshPathTextBox, [](const FString&) {})]
			+ SVerticalBox::Slot().AutoHeight().Padding(14, 0, 14, 10)[MakeEditableSpecRow(LOCTEXT("AttachedMeshSocketLabel", "Attach Socket"), Draft.SocketName, SocketTextBox, [](const FString&) {})]
			+ SVerticalBox::Slot().AutoHeight().Padding(14, 0, 14, 14).HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[SNew(SButton).Text(LOCTEXT("CancelAddAttachedMesh", "Cancel")).OnClicked_Lambda([&Window]() { Window->RequestDestroyWindow(); return FReply::Handled(); })]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).Text(LOCTEXT("ConfirmAddAttachedMesh", "Add Mesh")).OnClicked_Lambda([&]()
					{
						Draft.Id = IdTextBox.IsValid() ? IdTextBox->GetText().ToString() : FString();
						Draft.Label = LabelTextBox.IsValid() ? LabelTextBox->GetText().ToString() : FString();
						Draft.MeshPath = NTEBuildTool::Editor::NormalizeAssetPathForText(MeshPathTextBox.IsValid() ? MeshPathTextBox->GetText().ToString() : FString());
						Draft.SocketName = SocketTextBox.IsValid() ? SocketTextBox->GetText().ToString() : FString();
						Draft.Id.TrimStartAndEndInline();
						Draft.Label.TrimStartAndEndInline();
						Draft.SocketName.TrimStartAndEndInline();
						if (Draft.Id.IsEmpty() || Draft.MeshPath.IsEmpty() || Draft.SocketName.IsEmpty())
						{
							NTEBuildTool::Editor::ShowError(LOCTEXT("AddAttachedMeshMissingFields", "Mesh Id, Skeletal Mesh, and Attach Socket are required."));
							return FReply::Handled();
						}
						if (!NTEBuildTool::Editor::LoadAssetByPath<USkeletalMesh>(Draft.MeshPath))
						{
							NTEBuildTool::Editor::ShowError(FText::FromString(FString::Printf(TEXT("Attached SkeletalMesh cannot be loaded: %s"), *Draft.MeshPath)));
							return FReply::Handled();
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
	OutAttachedMesh = MoveTemp(Draft);
	return true;
}

bool IsAttachedMeshReferenced(const NTEBuildTool::Character::FNteCharacterModSpec& Spec, const FString& MeshId)
{
	for (const NTEBuildTool::Character::FNteCharacterMaterialOperationSpec& Operation : Spec.MaterialOperations)
	{
		if (Operation.TargetMeshId == MeshId)
		{
			return true;
		}
	}
	for (const NTEBuildTool::Character::FNteCharacterRuntimeActionSpec& Action : Spec.RuntimeActions)
	{
		if (Action.TargetMeshId == MeshId || Action.HostMeshId == MeshId)
		{
			return true;
		}
	}
	for (const NTEBuildTool::Character::FNteCharacterKawaiiPresetSpec& Preset : Spec.KawaiiPresets)
	{
		if (Preset.TargetMeshId == MeshId)
		{
			return true;
		}
	}
	return false;
}

void RebuildAttachedMeshRows(const TSharedRef<FWorkspaceDialogState>& State)
{
	if (!State->AttachedMeshRows.IsValid())
	{
		return;
	}
	State->AttachedMeshRows->ClearChildren();
	if (State->Spec.AttachedMeshes.IsEmpty())
	{
		State->AttachedMeshRows->AddSlot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("NoAttachedMeshes", "No attached meshes yet. Add the separate skirt/accessory mesh before importing its Kawaii chain."))];
		return;
	}
	for (const NTEBuildTool::Character::FNteCharacterAttachedMeshSpec& AttachedMesh : State->Spec.AttachedMeshes)
	{
		const FString MeshId = AttachedMesh.Id;
		const bool bReferenced = IsAttachedMeshReferenced(State->Spec, MeshId);
		State->AttachedMeshRows->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.20f).Padding(0, 0, 8, 0).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(MeshId))]
			+ SHorizontalBox::Slot().FillWidth(0.20f).Padding(0, 0, 8, 0).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(AttachedMesh.Label.IsEmpty() ? TEXT("<no label>") : AttachedMesh.Label))]
			+ SHorizontalBox::Slot().FillWidth(0.50f).Padding(0, 0, 8, 0).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s  @ %s"), *AttachedMesh.MeshPath, *AttachedMesh.SocketName)))]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("RemoveAttachedMesh", "Remove")).ToolTipText(bReferenced ? LOCTEXT("RemoveAttachedMeshReferenced", "This mesh is used by a material operation, runtime action, or Kawaii preset and cannot be removed until those references are cleared.") : FText::GetEmpty()).IsEnabled(!bReferenced).OnClicked_Lambda([State, MeshId]()
				{
					State->Spec.AttachedMeshes.RemoveAll([&MeshId](const NTEBuildTool::Character::FNteCharacterAttachedMeshSpec& Item) { return Item.Id == MeshId; });
					State->bDirty = true;
					RebuildAttachedMeshRows(State);
					return FReply::Handled();
				})
			]
		];
	}
}

struct FKawaiiTargetMeshChoice
{
	FString Id;
	FString Label;
	FString MeshPath;
	USkeletalMesh* Mesh = nullptr;
};

using FKawaiiTargetMeshChoicePtr = TSharedPtr<FKawaiiTargetMeshChoice>;

bool ShowKawaiiTargetMeshDialog(
	const TSharedRef<FWorkspaceDialogState>& State,
	USkeletalMesh* SelectedMesh,
	FString& OutTargetMeshId,
	USkeletalMesh*& OutTargetMesh)

{
	OutTargetMeshId.Reset();
	OutTargetMesh = nullptr;

	TArray<FKawaiiTargetMeshChoicePtr> Choices;
	FKawaiiTargetMeshChoicePtr MainChoice = MakeShared<FKawaiiTargetMeshChoice>();
	MainChoice->Id = TEXT("main");
	MainChoice->Label = TEXT("Main mesh");
	MainChoice->MeshPath = State->Spec.MainMeshPath;
	MainChoice->Mesh = !MainChoice->MeshPath.IsEmpty()
		? NTEBuildTool::Editor::LoadAssetByPath<USkeletalMesh>(MainChoice->MeshPath)
		: SelectedMesh;
	if (MainChoice->Mesh && MainChoice->MeshPath.IsEmpty())
	{
		MainChoice->MeshPath = NTEBuildTool::Editor::GetAssetPackagePath(MainChoice->Mesh);
	}
	Choices.Add(MainChoice);

	for (const NTEBuildTool::Character::FNteCharacterAttachedMeshSpec& AttachedMesh : State->Spec.AttachedMeshes)
	{
		FKawaiiTargetMeshChoicePtr Choice = MakeShared<FKawaiiTargetMeshChoice>();
		Choice->Id = AttachedMesh.Id;
		Choice->Label = AttachedMesh.Label.IsEmpty() ? AttachedMesh.Id : AttachedMesh.Label;
		Choice->MeshPath = AttachedMesh.MeshPath;
		Choice->Mesh = NTEBuildTool::Editor::LoadAssetByPath<USkeletalMesh>(Choice->MeshPath);
		Choices.Add(Choice);
	}

	FKawaiiTargetMeshChoicePtr SelectedChoice;
	TSharedPtr<SWindow> Window;
	bool bAccepted = false;
	SAssignNew(Window, SWindow)
		.Title(LOCTEXT("KawaiiTargetMeshTitle", "Choose Kawaii Target Mesh"))
		.ClientSize(FVector2D(900, 520))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(16, 14, 16, 8)
			[
				SNew(STextBlock).Text(LOCTEXT("KawaiiTargetMeshHint", "For added physics bones, choose the attached mesh that contains that copied bone chain. Main-mesh Kawaii import is retained for inspection, but generated Kawaii AnimBPs only support attached meshes."))
			]
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(16, 0, 16, 8)
			[
				SNew(SListView<FKawaiiTargetMeshChoicePtr>)
				.ListItemsSource(&Choices)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow_Lambda([](FKawaiiTargetMeshChoicePtr Item, const TSharedRef<STableViewBase>& OwnerTable)
				{
					const FString Status = Item->Mesh ? TEXT("loaded") : TEXT("mesh not found");
					return SNew(STableRow<FKawaiiTargetMeshChoicePtr>, OwnerTable)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(4, 3, 4, 1)[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("[%s] %s"), *Item->Id, *Item->Label)))]
						+ SVerticalBox::Slot().AutoHeight().Padding(4, 1, 4, 3)[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s (%s)"), *Item->MeshPath, *Status))).ColorAndOpacity(Item->Mesh ? FSlateColor::UseSubduedForeground() : FSlateColor(FLinearColor(1.0f, 0.35f, 0.25f)))]
					];
				})
				.OnSelectionChanged_Lambda([&SelectedChoice](FKawaiiTargetMeshChoicePtr Item, ESelectInfo::Type) { SelectedChoice = Item; })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(16, 8, 16, 16).HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[SNew(SButton).Text(LOCTEXT("CancelKawaiiTargetMesh", "Cancel")).OnClicked_Lambda([&bAccepted, &Window]() { bAccepted = false; Window->RequestDestroyWindow(); return FReply::Handled(); })]
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("UseKawaiiTargetMesh", "Use Target")).IsEnabled_Lambda([&SelectedChoice]() { return SelectedChoice.IsValid() && SelectedChoice->Mesh != nullptr; }).OnClicked_Lambda([&bAccepted, &Window]() { bAccepted = true; Window->RequestDestroyWindow(); return FReply::Handled(); })]
			]
		];
	FSlateApplication::Get().AddModalWindow(Window.ToSharedRef(), nullptr);
	if (!bAccepted || !SelectedChoice.IsValid() || !SelectedChoice->Mesh)
	{
		return false;
	}
	OutTargetMeshId = SelectedChoice->Id;
	OutTargetMesh = SelectedChoice->Mesh;
	return true;
}

bool ShowKawaiiImportOptionsDialog(const FString& SourceJsonPath, const FString& TargetDescription, FString& OutPresetPrefix)
{
	if (OutPresetPrefix.IsEmpty())
	{
		OutPresetPrefix = FPaths::GetBaseFilename(SourceJsonPath);
	}

	bool bAccepted = false;
	TSharedPtr<SEditableTextBox> PrefixTextBox;
	TSharedPtr<SWindow> Window;

	SAssignNew(Window, SWindow)
		.Title(LOCTEXT("KawaiiImportOptionsTitle", "Add Game Kawaii Preset"))
		.ClientSize(FVector2D(620, 154))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(14, 12, 14, 8)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("Target: %s\nSource: %s"), *TargetDescription, *SourceJsonPath)))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(14, 0, 14, 12)
			[
				MakeEditableSpecRow(
					LOCTEXT("KawaiiImportPresetPrefix", "Preset Prefix"),
					OutPresetPrefix,
					PrefixTextBox,
					[](const FString&) {})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(14, 0, 14, 14)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("KawaiiImportOptionsHint", "Import creates editable CharacterModSpec KawaiiPresets. Apply Kawaii writes DataAssets and supported attached-mesh AnimGraphs from the same preset model."))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8, 0, 6, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("KawaiiImportOptionsOk", "Import"))
					.OnClicked_Lambda([&]()
					{
						OutPresetPrefix = PrefixTextBox.IsValid() ? PrefixTextBox->GetText().ToString() : FString();
						OutPresetPrefix.TrimStartAndEndInline();
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
					.Text(LOCTEXT("KawaiiImportOptionsCancel", "Cancel"))
					.OnClicked_Lambda([&]()
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

bool ImportKawaiiJsonIntoWorkspace(const TSharedRef<FWorkspaceDialogState>& State, USkeletalMesh* SelectedMesh)
{
	ApplyTextBoxesToSpec(State);

	FString TargetMeshId;
	USkeletalMesh* TargetMesh = nullptr;
	if (!ShowKawaiiTargetMeshDialog(State, SelectedMesh, TargetMeshId, TargetMesh))
	{
		return false;
	}

	NTEBuildTool::Kawaii::FNteFModelKawaiiPresetLibrarySelection Selection;
	if (!NTEBuildTool::Kawaii::ShowFModelKawaiiPresetLibraryDialog(*TargetMesh, Selection))
	{
		return false;
	}

	const FString SourceJsonPath = Selection.SourceAnimLayerJson;
	FString PresetPrefix = Selection.SuggestedPresetPrefix;
	const FString TargetDescription = FString::Printf(TEXT("%s (%s)"), *TargetMeshId, *NTEBuildTool::Editor::GetAssetPackagePath(TargetMesh));
	if (!ShowKawaiiImportOptionsDialog(SourceJsonPath, TargetDescription, PresetPrefix))
	{
		return false;
	}

	NTEBuildTool::Character::FNteCharacterKawaiiImportOptions Options;
	Options.SourceJsonPath = SourceJsonPath;
	Options.TargetMeshId = TargetMeshId;
	Options.PresetIdPrefix = PresetPrefix;
	Options.SourceNodeNames.Add(Selection.SourceNodeName);
	Options.bReplaceExistingById = true;

	NTEBuildTool::Character::FNteCharacterKawaiiImportResult Result =
		NTEBuildTool::Character::ImportKawaiiPresetsFromFModelJson(Options);
	if (!Result.HasErrors())
	{
		NTEBuildTool::Character::UpsertKawaiiPresets(State->Spec, Result.ImportedPresets, Options.bReplaceExistingById, Result);
	}

	if (Result.HasErrors())
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Join(Result.Errors, TEXT("\n"))));
		return false;
	}

	State->bDirty = true;
	RebuildKawaiiPresetRows(State);
	NTEBuildTool::Editor::ShowSuccessNotification(FText::Format(
		LOCTEXT("ImportedKawaiiPresets", "Imported {0} Kawaii preset(s): {1} added, {2} replaced."),
		FText::AsNumber(Result.ImportedPresets.Num()),
		FText::AsNumber(Result.AddedPresetCount),
		FText::AsNumber(Result.ReplacedPresetCount)));
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
			.Padding(16, 8, 16, 4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("WorkspaceAttachedMeshesHeader", "Attached Skeletal Meshes"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 0, 16, 8)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(State->AttachedMeshRows, SVerticalBox)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0).HAlign(HAlign_Right)
				[
					SNew(SButton).Text(LOCTEXT("WorkspaceAddAttachedMeshButton", "Add Attached Mesh"))
					.OnClicked_Lambda([State]()
					{
						ApplyTextBoxesToSpec(State);
						NTEBuildTool::Character::FNteCharacterAttachedMeshSpec AttachedMesh;
						if (!ShowAttachedMeshDialog(AttachedMesh))
						{
							return FReply::Handled();
						}
						const bool bDuplicateId = State->Spec.AttachedMeshes.ContainsByPredicate([&AttachedMesh](const NTEBuildTool::Character::FNteCharacterAttachedMeshSpec& Item) { return Item.Id == AttachedMesh.Id; });
						if (bDuplicateId)
						{
							NTEBuildTool::Editor::ShowError(FText::FromString(FString::Printf(TEXT("An attached mesh already uses id '%s'."), *AttachedMesh.Id)));
							return FReply::Handled();
						}
						State->Spec.AttachedMeshes.Add(MoveTemp(AttachedMesh));
						State->bDirty = true;
						RebuildAttachedMeshRows(State);
						return FReply::Handled();
					})
				]
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
				.Text(LOCTEXT("WorkspaceKawaiiPresetsHeader", "Kawaii Presets in Spec"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 0, 16, 8)
			[
				SAssignNew(State->KawaiiPresetRows, SVerticalBox)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 0, 16, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("WorkspaceKawaiiImportHint", "Choose one game Kawaii preset for the copied physics-bone chain. The list shows target skeleton, missing bones, and tag diagnostics from KawaiiPlan."))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8, 0, 0, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("WorkspaceImportKawaiiJsonButton", "Add Game Kawaii Preset"))
					.OnClicked_Lambda([State, SelectedMesh]()
					{
						ImportKawaiiJsonIntoWorkspace(State, SelectedMesh);
						return FReply::Handled();
					})
				]
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
	RebuildAttachedMeshRows(State);
	RebuildKawaiiPresetRows(State);
	FSlateApplication::Get().AddModalWindow(Window.ToSharedRef(), nullptr);
	return OutResult.Action != ENteMeshModWorkspaceAction::None;
}
}

#undef LOCTEXT_NAMESPACE
