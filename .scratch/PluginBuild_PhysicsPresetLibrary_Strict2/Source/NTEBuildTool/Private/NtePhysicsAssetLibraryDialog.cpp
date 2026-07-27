// Copyright (c) 2026 NTEBuildTool contributors.

#include "NtePhysicsAssetLibraryDialog.h"

#include "FModelJsonUtils.h"
#include "FModelPhysicsAssetAnalysis.h"
#include "NteBuildToolSettings.h"
#include "NteNotificationUtils.h"

#include "ReferenceSkeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "NTEBuildTool"

namespace NTEBuildTool::Physics
{
namespace
{
struct FPhysicsAssetCatalogItem
{
	FString CharacterKind;
	FString SourceJson;
	FString RelativePath;
	TArray<FName> BodyBones;
	int32 ConstraintCount = 0;
};

using FPhysicsAssetCatalogItemPtr = TSharedPtr<FPhysicsAssetCatalogItem>;

struct FPhysicsChainChoice
{
	FName RootBone = NAME_None;
	FString Label;
};

using FPhysicsChainChoicePtr = TSharedPtr<FPhysicsChainChoice>;

FString ResolveFModelContentRoot(FString ExportRoot)
{
	ExportRoot.TrimStartAndEndInline();
	FPaths::NormalizeFilename(ExportRoot);
	if (ExportRoot.IsEmpty())
	{
		return FString();
	}

	const TArray<FString> Candidates = { ExportRoot, ExportRoot / TEXT("Content"), ExportRoot / TEXT("HT/Content") };
	for (const FString& Candidate : Candidates)
	{
		if (IFileManager::Get().DirectoryExists(*(Candidate / TEXT("Characters"))))
		{
			return Candidate;
		}
	}
	return FString();
}

bool CollectBodyBones(const FFModelPhysicsAssetAnalysis& Analysis, TArray<FName>& OutBones)
{
	OutBones.Reset();
	for (const TSharedPtr<FJsonObject>& Body : Analysis.OrderedBodies)
	{
		TSharedPtr<FJsonObject> Properties;
		if (!Body.IsValid() || !FModelJson::TryGetObject(*Body, TEXT("Properties"), Properties))
		{
			continue;
		}
		const FName BoneName = FModelJson::GetOptionalName(*Properties, TEXT("BoneName"));
		if (BoneName != NAME_None)
		{
			OutBones.AddUnique(BoneName);
		}
	}
	OutBones.Sort([](const FName A, const FName B)
	{
		return A.ToString() < B.ToString();
	});
	return !OutBones.IsEmpty();
}

void AddPhysicsCatalogItems(const FString& ContentRoot, const FString& RelativeDirectory, const FString& CharacterKind, TArray<FPhysicsAssetCatalogItemPtr>& OutItems)
{
	const FString SearchRoot = ContentRoot / RelativeDirectory;
	if (!IFileManager::Get().DirectoryExists(*SearchRoot))
	{
		return;
	}

	TArray<FString> JsonFiles;
	IFileManager::Get().FindFilesRecursive(JsonFiles, *SearchRoot, TEXT("*PhysicsAsset.json"), true, false);
	for (const FString& JsonFile : JsonFiles)
	{
		TArray<TSharedPtr<FJsonValue>> RootArray;
		FString IgnoredError;
		if (!FModelJson::LoadJsonArrayFromFile(JsonFile, RootArray, IgnoredError))
		{
			continue;
		}

		FFModelPhysicsAssetAnalysis Analysis;
		if (!AnalyzeFModelPhysicsAssetJson(RootArray, Analysis, IgnoredError))
		{
			continue;
		}

		FPhysicsAssetCatalogItemPtr Item = MakeShared<FPhysicsAssetCatalogItem>();
		Item->CharacterKind = CharacterKind;
		Item->SourceJson = JsonFile;
		Item->RelativePath = JsonFile;
		FPaths::MakePathRelativeTo(Item->RelativePath, *ContentRoot);
		CollectBodyBones(Analysis, Item->BodyBones);
		Item->ConstraintCount = Analysis.OrderedConstraints.Num();
		OutItems.Add(MoveTemp(Item));
	}
}

bool IsDescendantOf(const FReferenceSkeleton& Skeleton, const FName Candidate, const FName Root)
{
	const int32 RootIndex = Skeleton.FindBoneIndex(Root);
	int32 CurrentIndex = Skeleton.FindBoneIndex(Candidate);
	if (RootIndex == INDEX_NONE || CurrentIndex == INDEX_NONE)
	{
		return false;
	}
	while (CurrentIndex != INDEX_NONE)
	{
		if (CurrentIndex == RootIndex)
		{
			return true;
		}
		CurrentIndex = Skeleton.GetParentIndex(CurrentIndex);
	}
	return false;
}

TArray<FPhysicsChainChoicePtr> BuildChainChoices(const USkeletalMesh& TargetMesh, const FPhysicsAssetCatalogItem& Source)
{
	const FReferenceSkeleton& Skeleton = TargetMesh.GetRefSkeleton();
	TArray<FPhysicsChainChoicePtr> Choices;
	FPhysicsChainChoicePtr FullAsset = MakeShared<FPhysicsChainChoice>();
	FullAsset->Label = FString::Printf(TEXT("Entire PhysicsAsset (%d bodies, %d constraints)"), Source.BodyBones.Num(), Source.ConstraintCount);
	Choices.Add(FullAsset);

	for (const FName Bone : Source.BodyBones)
	{
		if (Skeleton.FindBoneIndex(Bone) == INDEX_NONE)
		{
			continue;
		}
		int32 ChainBodyCount = 0;
		for (const FName Candidate : Source.BodyBones)
		{
			ChainBodyCount += IsDescendantOf(Skeleton, Candidate, Bone) ? 1 : 0;
		}
		FPhysicsChainChoicePtr Choice = MakeShared<FPhysicsChainChoice>();
		Choice->RootBone = Bone;
		Choice->Label = FString::Printf(TEXT("Chain from %s (%d matching bodies)"), *Bone.ToString(), ChainBodyCount);
		Choices.Add(Choice);
	}
	return Choices;
}

bool ShowChainChoiceDialog(const USkeletalMesh& TargetMesh, const FPhysicsAssetCatalogItem& Source, FName& OutChainRootBone)
{
	TArray<FPhysicsChainChoicePtr> Choices = BuildChainChoices(TargetMesh, Source);
	FPhysicsChainChoicePtr SelectedChoice = Choices[0];
	TSharedPtr<SWindow> Window;
	bool bAccepted = false;
	SAssignNew(Window, SWindow)
		.Title(LOCTEXT("PhysicsChainChoiceTitle", "Choose Physics Bodies"))
		.ClientSize(FVector2D(720, 620))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(16, 14, 16, 8)
			[
				SNew(STextBlock).Text(FText::FromString(Source.RelativePath))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(16, 0, 16, 8)
			[
				SNew(STextBlock).Text(LOCTEXT("PhysicsChainChoiceHint", "Choose the full asset for a complete body setup, or one root bone to import only that bone chain."))
			]
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(16, 0, 16, 8)
			[
				SNew(SListView<FPhysicsChainChoicePtr>)
				.ListItemsSource(&Choices)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow_Lambda([](FPhysicsChainChoicePtr Item, const TSharedRef<STableViewBase>& OwnerTable)
				{
					return SNew(STableRow<FPhysicsChainChoicePtr>, OwnerTable)
					[
						SNew(STextBlock).Text(FText::FromString(Item->Label))
					];
				})
				.OnSelectionChanged_Lambda([&SelectedChoice](FPhysicsChainChoicePtr Item, ESelectInfo::Type) { if (Item.IsValid()) { SelectedChoice = Item; } })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(16, 8, 16, 16).HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
				[
					SNew(SButton).Text(LOCTEXT("CancelPhysicsChainChoice", "Cancel"))
					.OnClicked_Lambda([&bAccepted, &Window]() { bAccepted = false; Window->RequestDestroyWindow(); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).Text(LOCTEXT("ApplyPhysicsChainChoice", "Use Selection"))
					.OnClicked_Lambda([&bAccepted, &Window]() { bAccepted = true; Window->RequestDestroyWindow(); return FReply::Handled(); })
				]
			]
		];

	FSlateApplication::Get().AddModalWindow(Window.ToSharedRef(), nullptr);
	if (!bAccepted || !SelectedChoice.IsValid())
	{
		return false;
	}
	OutChainRootBone = SelectedChoice->RootBone;
	return true;
}
}

bool ShowFModelPhysicsAssetLibraryDialog(const USkeletalMesh& TargetMesh, FNteFModelPhysicsAssetLibrarySelection& OutSelection)
{
	OutSelection = FNteFModelPhysicsAssetLibrarySelection();
	const FString ExportRoot = NTEBuildTool::Settings::GetFModelExportRoot();
	const FString ContentRoot = ResolveFModelContentRoot(ExportRoot);
	if (ContentRoot.IsEmpty())
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Printf(TEXT("FModel Export Root does not contain Content/Characters: %s"), *ExportRoot)));
		return false;
	}

	TArray<FPhysicsAssetCatalogItemPtr> AllItems;
	AddPhysicsCatalogItems(ContentRoot, TEXT("Characters/Player"), TEXT("Player"), AllItems);
	AddPhysicsCatalogItems(ContentRoot, TEXT("Characters/Npc"), TEXT("NPC"), AllItems);
	AddPhysicsCatalogItems(ContentRoot, TEXT("Maps_4N/Characters/Player"), TEXT("Player"), AllItems);
	AddPhysicsCatalogItems(ContentRoot, TEXT("Maps_4N/Characters/Npc"), TEXT("NPC"), AllItems);
	AllItems.Sort([](const FPhysicsAssetCatalogItemPtr& A, const FPhysicsAssetCatalogItemPtr& B) { return A->RelativePath < B->RelativePath; });
	if (AllItems.IsEmpty())
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Printf(TEXT("No Player or NPC PhysicsAsset exports were found under: %s"), *ContentRoot)));
		return false;
	}

	TArray<FPhysicsAssetCatalogItemPtr> FilteredItems = AllItems;
	FPhysicsAssetCatalogItemPtr SelectedItem;
	TSharedPtr<SListView<FPhysicsAssetCatalogItemPtr>> ListView;
	TSharedPtr<SWindow> Window;
	bool bAccepted = false;
	auto RefreshFilter = [&AllItems, &FilteredItems, &ListView](const FString& FilterText)
	{
		FilteredItems.Reset();
		const FString Trimmed = FilterText.TrimStartAndEnd();
		for (const FPhysicsAssetCatalogItemPtr& Item : AllItems)
		{
			if (Trimmed.IsEmpty() || (Item->CharacterKind + TEXT(" ") + Item->RelativePath).Contains(Trimmed, ESearchCase::IgnoreCase))
			{
				FilteredItems.Add(Item);
			}
		}
		if (ListView.IsValid()) { ListView->RequestListRefresh(); }
	};

	SAssignNew(Window, SWindow)
		.Title(LOCTEXT("GamePhysicsLibraryTitle", "Game PhysicsAsset Library"))
		.ClientSize(FVector2D(1080, 720))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(16, 14, 16, 8)
			[
				SNew(STextBlock).Text(LOCTEXT("GamePhysicsLibraryHint", "Select a Player or NPC PhysicsAsset preset. The next step can limit the import to one matching bone chain."))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(16, 0, 16, 8)
			[
				SNew(SEditableTextBox).HintText(LOCTEXT("GamePhysicsLibrarySearch", "Search character or source path"))
				.OnTextChanged_Lambda([RefreshFilter](const FText& NewText) { RefreshFilter(NewText.ToString()); })
			]
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(16, 0, 16, 8)
			[
				SAssignNew(ListView, SListView<FPhysicsAssetCatalogItemPtr>)
				.ListItemsSource(&FilteredItems)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow_Lambda([](FPhysicsAssetCatalogItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
				{
					return SNew(STableRow<FPhysicsAssetCatalogItemPtr>, OwnerTable)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(4, 3, 4, 1)[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("[%s] %s"), *Item->CharacterKind, *Item->RelativePath)))]
						+ SVerticalBox::Slot().AutoHeight().Padding(4, 1, 4, 3)[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%d bodies, %d constraints"), Item->BodyBones.Num(), Item->ConstraintCount))).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
					];
				})
				.OnSelectionChanged_Lambda([&SelectedItem](FPhysicsAssetCatalogItemPtr Item, ESelectInfo::Type) { SelectedItem = Item; })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(16, 8, 16, 16).HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[SNew(SButton).Text(LOCTEXT("CancelGamePhysicsLibrary", "Cancel")).OnClicked_Lambda([&bAccepted, &Window]() { bAccepted = false; Window->RequestDestroyWindow(); return FReply::Handled(); })]
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("ChooseGamePhysicsLibrary", "Choose PhysicsAsset")).IsEnabled_Lambda([&SelectedItem]() { return SelectedItem.IsValid(); }).OnClicked_Lambda([&bAccepted, &Window]() { bAccepted = true; Window->RequestDestroyWindow(); return FReply::Handled(); })]
			]
		];

	FSlateApplication::Get().AddModalWindow(Window.ToSharedRef(), nullptr);
	if (!bAccepted || !SelectedItem.IsValid())
	{
		return false;
	}
	if (!ShowChainChoiceDialog(TargetMesh, *SelectedItem, OutSelection.ChainRootBone))
	{
		return false;
	}
	OutSelection.SourcePhysicsAssetJson = SelectedItem->SourceJson;
	return true;
}
}

#undef LOCTEXT_NAMESPACE
