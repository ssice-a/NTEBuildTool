// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteMaterialInstanceDialog.h"

#include "NteBuildToolSettings.h"
#include "NteEditorAssetUtils.h"
#include "NteJsonFileUtils.h"
#include "NteNotificationUtils.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

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

struct FMaterialSlotRow
{
	int32 SlotIndex = INDEX_NONE;
	FString SlotName;
	FString MaterialPath;
};

struct FGameMaterialCatalogItem
{
	FString CharacterKind;
	FString SourceMaterialJson;
	FString RelativePath;
	FString ParentMaterialPath;
	TArray<FString> TextureParameters;
};

using FGameMaterialCatalogItemPtr = TSharedPtr<FGameMaterialCatalogItem>;

FString ResolveFModelContentRoot(FString ExportRoot)
{
	ExportRoot.TrimStartAndEndInline();
	FPaths::NormalizeFilename(ExportRoot);
	if (ExportRoot.IsEmpty())
	{
		return FString();
	}

	const TArray<FString> Candidates =
	{
		ExportRoot,
		ExportRoot / TEXT("Content"),
		ExportRoot / TEXT("HT/Content")
	};
	for (const FString& Candidate : Candidates)
	{
		if (IFileManager::Get().DirectoryExists(*(Candidate / TEXT("Characters"))))
		{
			return Candidate;
		}
	}
	return FString();
}

void AddGameMaterialCatalogItems(
	const FString& ContentRoot,
	const FString& RelativeDirectory,
	const FString& CharacterKind,
	TArray<FGameMaterialCatalogItemPtr>& OutItems)
{
	const FString SearchRoot = ContentRoot / RelativeDirectory;
	if (!IFileManager::Get().DirectoryExists(*SearchRoot))
	{
		return;
	}

	TArray<FString> JsonFiles;
	IFileManager::Get().FindFilesRecursive(JsonFiles, *SearchRoot, TEXT("*.json"), true, false);
	for (const FString& JsonFile : JsonFiles)
	{
		if (!FPaths::GetBaseFilename(JsonFile).StartsWith(TEXT("MI_"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		TSharedPtr<FJsonObject> SourceMaterial;
		FString IgnoredError;
		if (!LoadMaterialSourceParametersFromJsonFile(JsonFile, SourceMaterial, IgnoredError))
		{
			continue;
		}

		FGameMaterialCatalogItemPtr Item = MakeShared<FGameMaterialCatalogItem>();
		Item->CharacterKind = CharacterKind;
		Item->SourceMaterialJson = JsonFile;
		Item->RelativePath = JsonFile;
		FPaths::MakePathRelativeTo(Item->RelativePath, *ContentRoot);
		Item->ParentMaterialPath = DeriveParentMaterialPathFromFModelJson(JsonFile);
		if (const FJsonObject* Textures = FindSourceMaterialParameterObject(SourceMaterial.Get(), TEXT("Textures")))
		{
			Textures->Values.GetKeys(Item->TextureParameters);
			Item->TextureParameters.Sort();
		}
		OutItems.Add(MoveTemp(Item));
	}
}

FString MakeCatalogSearchText(const FGameMaterialCatalogItem& Item)
{
	return FString::Printf(TEXT("%s %s %s %s"),
		*Item.CharacterKind,
		*Item.RelativePath,
		*Item.ParentMaterialPath,
		*FString::Join(Item.TextureParameters, TEXT(" ")));
}

FString MakeTextureParameterSummary(const FGameMaterialCatalogItem& Item)
{
	if (Item.TextureParameters.IsEmpty())
	{
		return TEXT("No exported texture parameters");
	}

	const int32 VisibleCount = FMath::Min(Item.TextureParameters.Num(), 6);
	TArray<FString> VisibleParameters;
	for (int32 Index = 0; Index < VisibleCount; ++Index)
	{
		VisibleParameters.Add(Item.TextureParameters[Index]);
	}
	FString Summary = FString::Join(VisibleParameters, TEXT(", "));
	if (Item.TextureParameters.Num() > VisibleCount)
	{
		Summary += FString::Printf(TEXT(" +%d"), Item.TextureParameters.Num() - VisibleCount);
	}
	return Summary;
}

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

bool TryGetSingleSelectedAssetPackagePathOfClass(UClass& RequiredClass, const TCHAR* RequiredLabel, FString& OutPackagePath, FString& OutError)
{
	if (!TryGetSingleSelectedPackagePath(OutPackagePath, OutError))
	{
		return false;
	}

	UObject* SelectedAsset = NTEBuildTool::Editor::LoadAnyAssetByPath(OutPackagePath);
	if (!SelectedAsset || !SelectedAsset->IsA(&RequiredClass))
	{
		OutError = FString::Printf(TEXT("Select exactly one %s asset in the Content Browser."), RequiredLabel);
		return false;
	}
	return true;
}

USkeletalMesh* ResolveInitialSkeletalMesh(const FNteMaterialInstanceOptions& InitialOptions)
{
	if (!InitialOptions.MeshPath.IsEmpty())
	{
		return NTEBuildTool::Editor::LoadAssetByPath<USkeletalMesh>(InitialOptions.MeshPath);
	}

	return NTEBuildTool::Editor::GetSingleSelectedSkeletalMesh();
}
}

bool ShowFModelMaterialLibraryRecipeDialog(
	const FNteMaterialInstanceOptions& InitialOptions,
	FNteMaterialInstanceOptions& OutOptions,
	TSharedPtr<FJsonObject>& OutSourceTextureOverrides)
{
	const FString ExportRoot = NTEBuildTool::Settings::GetFModelExportRoot();
	const FString ContentRoot = ResolveFModelContentRoot(ExportRoot);
	if (ContentRoot.IsEmpty())
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Printf(
			TEXT("FModel Export Root does not contain Content/Characters: %s\nConfigure it in Project Settings > Plugins > NTE Build Tool."),
			*ExportRoot)));
		return false;
	}

	TArray<FGameMaterialCatalogItemPtr> AllItems;
	AddGameMaterialCatalogItems(ContentRoot, TEXT("Characters/Player"), TEXT("Player"), AllItems);
	AddGameMaterialCatalogItems(ContentRoot, TEXT("Characters/Npc"), TEXT("NPC"), AllItems);
	AddGameMaterialCatalogItems(ContentRoot, TEXT("Maps_4N/Characters/Player"), TEXT("Player"), AllItems);
	AddGameMaterialCatalogItems(ContentRoot, TEXT("Maps_4N/Characters/Npc"), TEXT("NPC"), AllItems);
	AllItems.Sort([](const FGameMaterialCatalogItemPtr& A, const FGameMaterialCatalogItemPtr& B)
	{
		const int32 KindCompare = A->CharacterKind.Compare(B->CharacterKind, ESearchCase::IgnoreCase);
		return KindCompare == 0
			? A->RelativePath.Compare(B->RelativePath, ESearchCase::IgnoreCase) < 0
			: KindCompare < 0;
	});

	if (AllItems.IsEmpty())
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Printf(
			TEXT("No Player or NPC material-instance exports (MI_*.json) were found under: %s"),
			*ContentRoot)));
		return false;
	}

	int32 PlayerCount = 0;
	int32 NpcCount = 0;
	for (const FGameMaterialCatalogItemPtr& Item : AllItems)
	{
		Item->CharacterKind == TEXT("Player") ? ++PlayerCount : ++NpcCount;
	}

	TArray<FGameMaterialCatalogItemPtr> FilteredItems = AllItems;
	FGameMaterialCatalogItemPtr SelectedItem;
	TSharedPtr<SListView<FGameMaterialCatalogItemPtr>> ListView;
	TSharedPtr<SWindow> Window;
	bool bAccepted = false;

	auto RefreshFilter = [&AllItems, &FilteredItems, &ListView](const FString& FilterText)
	{
		FilteredItems.Reset();
		const FString TrimmedFilter = FilterText.TrimStartAndEnd();
		for (const FGameMaterialCatalogItemPtr& Item : AllItems)
		{
			if (TrimmedFilter.IsEmpty() || MakeCatalogSearchText(*Item).Contains(TrimmedFilter, ESearchCase::IgnoreCase))
			{
				FilteredItems.Add(Item);
			}
		}
		if (ListView.IsValid())
		{
			ListView->RequestListRefresh();
		}
	};

	SAssignNew(Window, SWindow)
		.Title(LOCTEXT("GameMaterialLibraryTitle", "NTE Game Material Library"))
		.ClientSize(FVector2D(1100, 760))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 14, 16, 6)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(
					TEXT("%d Player materials, %d NPC materials. Select a game material instance as the template."),
					PlayerCount,
					NpcCount)))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 4, 16, 8)
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("GameMaterialLibrarySearchHint", "Search character, material, path, or texture parameter"))
				.OnTextChanged_Lambda([RefreshFilter](const FText& NewText)
				{
					RefreshFilter(NewText.ToString());
				})
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(16, 0, 16, 8)
			[
				SAssignNew(ListView, SListView<FGameMaterialCatalogItemPtr>)
				.ListItemsSource(&FilteredItems)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow_Lambda([](FGameMaterialCatalogItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
				{
					return SNew(STableRow<FGameMaterialCatalogItemPtr>, OwnerTable)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(4, 3, 4, 1)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("[%s] %s"), *Item->CharacterKind, *Item->RelativePath)))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(4, 1, 4, 3)
						[
							SNew(STextBlock)
							.Text(FText::FromString(MakeTextureParameterSummary(*Item)))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						]
					];
				})
				.OnSelectionChanged_Lambda([&SelectedItem](FGameMaterialCatalogItemPtr Item, ESelectInfo::Type)
				{
					SelectedItem = Item;
				})
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
					.Text(LOCTEXT("CancelGameMaterialLibrary", "Cancel"))
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
					.Text(LOCTEXT("UseGameMaterialLibrarySelection", "Choose Material"))
					.IsEnabled_Lambda([&SelectedItem]() { return SelectedItem.IsValid(); })
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
	if (!bAccepted || !SelectedItem.IsValid())
	{
		return false;
	}

	FNteMaterialInstanceOptions RecipeOptions = InitialOptions;
	RecipeOptions.ParentMaterialPath = SelectedItem->ParentMaterialPath;
	return ShowMaterialInstanceRecipeDialog(
		SelectedItem->SourceMaterialJson,
		RecipeOptions,
		OutOptions,
		OutSourceTextureOverrides);
}

bool ShowMaterialInstanceRecipeDialog(const FString& SourceMaterialJson, const FNteMaterialInstanceOptions& InitialOptions, FNteMaterialInstanceOptions& OutOptions, TSharedPtr<FJsonObject>& OutSourceTextureOverrides)
{
	TSharedPtr<FJsonObject> SourceMaterial;
	FString Error;
	if (!LoadMaterialSourceParametersFromJsonFile(SourceMaterialJson, SourceMaterial, Error))
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(Error));
		return false;
	}

	FString ParentMaterialPath = InitialOptions.ParentMaterialPath.IsEmpty() ? DeriveParentMaterialPathFromFModelJson(SourceMaterialJson) : InitialOptions.ParentMaterialPath;
	FString OutputFolder = DeriveModMaterialFolderFromParentPath(ParentMaterialPath, NTEBuildTool::Editor::GetSelectedContentBrowserPath());
	FString OutputMaterialPath = InitialOptions.OutputMaterialPath.IsEmpty()
		? NTEBuildTool::Editor::JoinAssetPath(OutputFolder, MakeModMaterialNameFromFModelJson(SourceMaterialJson))
		: InitialOptions.OutputMaterialPath;
	FString MeshPath = InitialOptions.MeshPath;
	TArray<TSharedPtr<FMaterialSlotRow>> SlotRows;
	if (USkeletalMesh* SelectedMesh = ResolveInitialSkeletalMesh(InitialOptions))
	{
		MeshPath = NTEBuildTool::Editor::GetAssetPackagePath(SelectedMesh);
		const TArray<FSkeletalMaterial>& Materials = SelectedMesh->GetMaterials();
		for (int32 Index = 0; Index < Materials.Num(); ++Index)
		{
			const FSkeletalMaterial& Material = Materials[Index];
			TSharedPtr<FMaterialSlotRow> Row = MakeShared<FMaterialSlotRow>();
			Row->SlotIndex = Index;
			Row->SlotName = Material.MaterialSlotName.ToString();
			Row->MaterialPath = Material.MaterialInterface ? Material.MaterialInterface->GetPackage()->GetName() : FString();
			SlotRows.Add(Row);
		}
	}
	FString SlotText = InitialOptions.SlotIndex != INDEX_NONE ? FString::FromInt(InitialOptions.SlotIndex) : FString();
	bool bAssignToMeshSlot = InitialOptions.bAssignToMeshSlot || (!MeshPath.IsEmpty() && InitialOptions.SlotIndex != INDEX_NONE);
	bool bResetForPakTextureOnly = InitialOptions.bResetForPakTextureOnly;
	bool bEnsureParentPlaceholder = InitialOptions.bEnsureParentPlaceholder;
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
	TSharedPtr<SEditableTextBox> ParentMaterialTextBox;
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
						if (!TryGetSingleSelectedAssetPackagePathOfClass(*UTexture::StaticClass(), TEXT("Texture"), PackagePath, Error))
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

	TSharedRef<SVerticalBox> SlotPickerRows = SNew(SVerticalBox);
	if (!SlotRows.IsEmpty())
	{
		SlotPickerRows->AddSlot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.18f).Padding(0, 0, 8, 0)[SNew(STextBlock).Text(LOCTEXT("MaterialSlotPickerSlotHeader", "Slot"))]
				+ SHorizontalBox::Slot().FillWidth(0.30f).Padding(0, 0, 8, 0)[SNew(STextBlock).Text(LOCTEXT("MaterialSlotPickerMaterialHeader", "Current Material"))]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[SNew(STextBlock).Text(FText::GetEmpty())]
				+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::GetEmpty())]
			];

		for (const TSharedPtr<FMaterialSlotRow>& Row : SlotRows)
		{
			SlotPickerRows->AddSlot()
				.AutoHeight()
				.Padding(0, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(0.18f)
					.Padding(0, 0, 8, 0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("%d  %s"), Row->SlotIndex, *Row->SlotName)))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.30f)
					.Padding(0, 0, 8, 0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Row->MaterialPath.IsEmpty() ? TEXT("<none>") : Row->MaterialPath))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 8, 0)
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.Text(LOCTEXT("UseSlotForAssignment", "Assign Slot"))
						.OnClicked_Lambda([Row, MeshPath, &MeshTextBox, &SlotTextBox, &AssignToMeshSlotCheckBox]()
						{
							if (MeshTextBox.IsValid())
							{
								MeshTextBox->SetText(FText::FromString(MeshPath));
							}
							if (SlotTextBox.IsValid())
							{
								SlotTextBox->SetText(FText::AsNumber(Row->SlotIndex));
							}
							if (AssignToMeshSlotCheckBox.IsValid())
							{
								AssignToMeshSlotCheckBox->SetIsChecked(ECheckBoxState::Checked);
							}
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.Text(LOCTEXT("UseSlotMaterialAsParent", "Use As Parent"))
						.IsEnabled(!Row->MaterialPath.IsEmpty())
						.OnClicked_Lambda([Row, &ParentMaterialTextBox]()
						{
							if (ParentMaterialTextBox.IsValid())
							{
								ParentMaterialTextBox->SetText(FText::FromString(Row->MaterialPath));
							}
							return FReply::Handled();
						})
					]
				];
		}
	}

	SAssignNew(Window, SWindow)
		.Title(LOCTEXT("MaterialRecipeDialogTitle", "NTE Material Instance Recipe"))
		.ClientSize(FVector2D(1120, 820))
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
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0, 0, 8, 0)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("ParentMaterialLabel", "Parent Material"))]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)[SAssignNew(ParentMaterialTextBox, SEditableTextBox).Text(FText::FromString(ParentMaterialPath))]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				[
					SNew(SButton)
					.Text(LOCTEXT("UseSelectedParentMaterial", "Use Selected"))
					.OnClicked_Lambda([&ParentMaterialTextBox]()
					{
						FString PackagePath;
						FString Error;
						if (!TryGetSingleSelectedAssetPackagePathOfClass(*UMaterialInterface::StaticClass(), TEXT("Material or MaterialInstance"), PackagePath, Error))
						{
							NTEBuildTool::Editor::ShowError(FText::FromString(Error));
							return FReply::Handled();
						}
						if (ParentMaterialTextBox.IsValid())
						{
							ParentMaterialTextBox->SetText(FText::FromString(PackagePath));
						}
						return FReply::Handled();
					})
				]
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
				SNew(STextBlock)
				.Text(LOCTEXT("SelectedMeshSlotsLabel", "Selected Mesh Slots"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16, 0, 16, 8)
			[
				SlotPickerRows
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
	OutOptions.ParentMaterialPath = NTEBuildTool::Editor::NormalizeAssetPathForText(ParentMaterialTextBox->GetText().ToString());
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
			if (!NTEBuildTool::Editor::LoadAssetByPath<UTexture>(ReplacementTexturePath))
			{
				NTEBuildTool::Editor::ShowError(FText::FromString(FString::Printf(TEXT("Replacement texture does not load as a Texture asset: %s"), *ReplacementTexturePath)));
				return false;
			}
			OutSourceTextureOverrides->SetStringField(Row->Usage.SourceTexturePath, ReplacementTexturePath);
		}
	}
	return true;
}
}

#undef LOCTEXT_NAMESPACE
