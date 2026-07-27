// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteKawaiiPresetLibraryDialog.h"

#include "FModelJsonUtils.h"
#include "FModelKawaiiAnimLayerAnalysis.h"
#include "NteBuildToolSettings.h"
#include "NteNotificationUtils.h"

#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"
#include "Containers/StringConv.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/UnrealMemory.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "NTEBuildTool"

namespace NTEBuildTool::Kawaii
{
namespace
{
struct FKawaiiPresetCatalogItem
{
	FString SourceJson;
	FString RelativePath;
	FString SourceNodeName;
	FName RootBone;
	TArray<FName> AdditionalRootBones;
	int32 MatchingAdditionalRootBoneCount = 0;
	int32 CollisionLimitCount = 0;
};

using FKawaiiPresetCatalogItemPtr = TSharedPtr<FKawaiiPresetCatalogItem>;

struct FSkeletonSourceCatalogItem
{
	FString PskFilename;
	FString RelativePath;
};

using FSkeletonSourceCatalogItemPtr = TSharedPtr<FSkeletonSourceCatalogItem>;

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

FString InferKawaiiFamily(const FName RootBone)
{
	FString Value = RootBone.ToString();
	Value.ToLowerInline();
	for (const TCHAR* Family : { TEXT("qun"), TEXT("hair"), TEXT("tail"), TEXT("tie"), TEXT("piaodai"), TEXT("ear") })
	{
		if (Value.Contains(Family))
		{
			return Family;
		}
	}
	return RootBone.IsNone() ? TEXT("kawaii") : RootBone.ToString();
}

bool HasBone(const USkeletalMesh& Mesh, const FName Bone)
{
	return Bone != NAME_None && Mesh.GetRefSkeleton().FindBoneIndex(Bone) != INDEX_NONE;
}

bool ContainsUtf8String(const TArray<uint8>& Bytes, const FString& Value)
{
	FTCHARToUTF8 Needle(*Value);
	const int32 NeedleLength = Needle.Length();
	if (NeedleLength == 0 || Bytes.Num() < NeedleLength)
	{
		return false;
	}
	for (int32 Index = 0; Index <= Bytes.Num() - NeedleLength; ++Index)
	{
		if (FMemory::Memcmp(Bytes.GetData() + Index, Needle.Get(), NeedleLength) == 0)
		{
			return true;
		}
	}
	return false;
}

bool ShowSkeletonSourceDialog(const FString& ContentRoot, FString& OutPskFilename)
{
	TArray<FString> PskFiles;
	const TArray<FString> SearchRoots =
	{
		ContentRoot / TEXT("Characters/Player"),
		ContentRoot / TEXT("Characters/Npc"),
		ContentRoot / TEXT("Maps_4N/Characters/Player"),
		ContentRoot / TEXT("Maps_4N/Characters/Npc")
	};
	for (const FString& SearchRoot : SearchRoots)
	{
		if (IFileManager::Get().DirectoryExists(*SearchRoot))
		{
			IFileManager::Get().FindFilesRecursive(PskFiles, *SearchRoot, TEXT("*.psk"), true, false);
		}
	}
	if (PskFiles.IsEmpty())
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Printf(TEXT("No Player or NPC PSK exports were found below: %s"), *ContentRoot)));
		return false;
	}

	TArray<FSkeletonSourceCatalogItemPtr> Items;
	for (const FString& PskFile : PskFiles)
	{
		FSkeletonSourceCatalogItemPtr Item = MakeShared<FSkeletonSourceCatalogItem>();
		Item->PskFilename = PskFile;
		Item->RelativePath = PskFile;
		FPaths::MakePathRelativeTo(Item->RelativePath, *ContentRoot);
		Items.Add(Item);
	}
	Items.Sort([](const FSkeletonSourceCatalogItemPtr& A, const FSkeletonSourceCatalogItemPtr& B) { return A->RelativePath < B->RelativePath; });

	FSkeletonSourceCatalogItemPtr SelectedItem;
	TSharedPtr<SWindow> Window;
	bool bAccepted = false;
	SAssignNew(Window, SWindow)
		.Title(LOCTEXT("KawaiiSkeletonSourceTitle", "Choose Game Physics-Bone Source"))
		.ClientSize(FVector2D(920, 620))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(16, 14, 16, 8)
			[
				SNew(STextBlock).Text(LOCTEXT("KawaiiSkeletonSourceHint", "Choose the Player or NPC clothing skeleton from which you copied the extra physics-bone chain."))
			]
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(16, 0, 16, 8)
			[
				SNew(SListView<FSkeletonSourceCatalogItemPtr>)
				.ListItemsSource(&Items)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow_Lambda([](FSkeletonSourceCatalogItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
				{
					return SNew(STableRow<FSkeletonSourceCatalogItemPtr>, OwnerTable)
					[
						SNew(STextBlock).Text(FText::FromString(Item->RelativePath))
					];
				})
				.OnSelectionChanged_Lambda([&SelectedItem](FSkeletonSourceCatalogItemPtr Item, ESelectInfo::Type) { SelectedItem = Item; })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(16, 8, 16, 16).HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[SNew(SButton).Text(LOCTEXT("CancelKawaiiSkeletonSource", "Cancel")).OnClicked_Lambda([&bAccepted, &Window]() { bAccepted = false; Window->RequestDestroyWindow(); return FReply::Handled(); })]
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("UseKawaiiSkeletonSource", "Use Source")).IsEnabled_Lambda([&SelectedItem]() { return SelectedItem.IsValid(); }).OnClicked_Lambda([&bAccepted, &Window]() { bAccepted = true; Window->RequestDestroyWindow(); return FReply::Handled(); })]
			]
		];

	FSlateApplication::Get().AddModalWindow(Window.ToSharedRef(), nullptr);
	if (!bAccepted || !SelectedItem.IsValid())
	{
		return false;
	}
	OutPskFilename = SelectedItem->PskFilename;
	return true;
}

void AddKawaiiCatalogItems(const FString& ContentRoot, const TArray<uint8>& SourceSkeletonBytes, const USkeletalMesh& TargetMesh, TArray<FKawaiiPresetCatalogItemPtr>& OutItems)
{
	const FString SearchRoot = ContentRoot / TEXT("Characters/AnimInterface/Physics");
	if (!IFileManager::Get().DirectoryExists(*SearchRoot))
	{
		return;
	}

	TArray<FString> JsonFiles;
	IFileManager::Get().FindFilesRecursive(JsonFiles, *SearchRoot, TEXT("*.json"), true, false);
	for (const FString& JsonFile : JsonFiles)
	{
		TArray<TSharedPtr<FJsonValue>> RootArray;
		FString IgnoredError;
		if (!FModelJson::LoadJsonArrayFromFile(JsonFile, RootArray, IgnoredError))
		{
			continue;
		}

		FFModelKawaiiAnimLayerAnalysis Analysis;
		if (!AnalyzeFModelKawaiiAnimLayerJson(RootArray, Analysis, IgnoredError))
		{
			continue;
		}

		for (const FFModelKawaiiNodeAnalysis& Node : Analysis.KawaiiNodes)
		{
			if (Node.RootBone == NAME_None || !ContainsUtf8String(SourceSkeletonBytes, Node.RootBone.ToString()))
			{
				continue;
			}

			FKawaiiPresetCatalogItemPtr Item = MakeShared<FKawaiiPresetCatalogItem>();
			Item->SourceJson = JsonFile;
			Item->RelativePath = JsonFile;
			FPaths::MakePathRelativeTo(Item->RelativePath, *ContentRoot);
			Item->SourceNodeName = Node.GraphNodeName;
			Item->RootBone = Node.RootBone;
			for (const FFModelKawaiiRootBoneSetting& AdditionalRoot : Node.AdditionalRootBones)
			{
				if (AdditionalRoot.RootBone != NAME_None)
				{
					Item->AdditionalRootBones.Add(AdditionalRoot.RootBone);
					Item->MatchingAdditionalRootBoneCount += HasBone(TargetMesh, AdditionalRoot.RootBone) ? 1 : 0;
				}
			}
			Item->CollisionLimitCount = Node.SphericalLimits.Num() + Node.CapsuleLimits.Num() + Node.BoxLimits.Num() + Node.PlanarLimits.Num();
			OutItems.Add(Item);
		}
	}
}

FString MakeSearchText(const FKawaiiPresetCatalogItem& Item)
{
	FString AdditionalRoots;
	for (const FName Bone : Item.AdditionalRootBones)
	{
		AdditionalRoots += TEXT(" ") + Bone.ToString();
	}
	return Item.RelativePath + TEXT(" ") + Item.SourceNodeName + TEXT(" ") + Item.RootBone.ToString() + TEXT(" ") + InferKawaiiFamily(Item.RootBone) + AdditionalRoots;
}
}

bool ShowFModelKawaiiPresetLibraryDialog(const USkeletalMesh& TargetMesh, FNteFModelKawaiiPresetLibrarySelection& OutSelection)
{
	OutSelection = FNteFModelKawaiiPresetLibrarySelection();
	const FString ContentRoot = ResolveFModelContentRoot(NTEBuildTool::Settings::GetFModelExportRoot());
	if (ContentRoot.IsEmpty())
	{
		NTEBuildTool::Editor::ShowError(LOCTEXT("KawaiiLibraryMissingFModelRoot", "Set FModel Export Root to the exported HT Content folder before opening the Kawaii preset library."));
		return false;
	}

	FString SourcePskFilename;
	if (!ShowSkeletonSourceDialog(ContentRoot, SourcePskFilename))
	{
		return false;
	}
	TArray<uint8> SourceSkeletonBytes;
	if (!FFileHelper::LoadFileToArray(SourceSkeletonBytes, *SourcePskFilename))
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Printf(TEXT("Could not read selected source skeleton: %s"), *SourcePskFilename)));
		return false;
	}

	TArray<FKawaiiPresetCatalogItemPtr> AllItems;
	AddKawaiiCatalogItems(ContentRoot, SourceSkeletonBytes, TargetMesh, AllItems);
	AllItems.Sort([](const FKawaiiPresetCatalogItemPtr& A, const FKawaiiPresetCatalogItemPtr& B)
	{
		const int32 PathCompare = A->RelativePath.Compare(B->RelativePath, ESearchCase::IgnoreCase);
		return PathCompare == 0 ? A->SourceNodeName < B->SourceNodeName : PathCompare < 0;
	});
	if (AllItems.IsEmpty())
	{
		NTEBuildTool::Editor::ShowError(FText::FromString(FString::Printf(
			TEXT("No game Kawaii node has a root bone present in the selected source skeleton: %s"),
			*SourcePskFilename)));
		return false;
	}

	TArray<FKawaiiPresetCatalogItemPtr> FilteredItems = AllItems;
	FKawaiiPresetCatalogItemPtr SelectedItem;
	TSharedPtr<SListView<FKawaiiPresetCatalogItemPtr>> ListView;
	TSharedPtr<SWindow> Window;
	bool bAccepted = false;
	auto RefreshFilter = [&AllItems, &FilteredItems, &ListView](const FString& FilterText)
	{
		FilteredItems.Reset();
		const FString Trimmed = FilterText.TrimStartAndEnd();
		for (const FKawaiiPresetCatalogItemPtr& Item : AllItems)
		{
			if (Trimmed.IsEmpty() || MakeSearchText(*Item).Contains(Trimmed, ESearchCase::IgnoreCase))
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
		.Title(LOCTEXT("KawaiiLibraryTitle", "Game Kawaii Preset Library"))
		.ClientSize(FVector2D(1120, 720))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(16, 14, 16, 8)
			[
				SNew(STextBlock).Text(LOCTEXT("KawaiiLibraryHint", "Search a copied physics-bone family such as qun, hair, tail, or tie. The target mesh is checked against the preset root bones."))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(16, 0, 16, 8)
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("KawaiiLibrarySearch", "Search bone family, root bone, or source AnimLayer"))
				.OnTextChanged_Lambda([RefreshFilter](const FText& NewText) { RefreshFilter(NewText.ToString()); })
			]
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(16, 0, 16, 8)
			[
				SAssignNew(ListView, SListView<FKawaiiPresetCatalogItemPtr>)
				.ListItemsSource(&FilteredItems)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow_Lambda([&TargetMesh](FKawaiiPresetCatalogItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
				{
					const bool bRootMatches = HasBone(TargetMesh, Item->RootBone);
					const FString Summary = FString::Printf(
						TEXT("root %s: %s | additional roots %d/%d match | %d collision limits"),
						*Item->RootBone.ToString(),
						bRootMatches ? TEXT("matches") : TEXT("missing"),
						Item->MatchingAdditionalRootBoneCount,
						Item->AdditionalRootBones.Num(),
						Item->CollisionLimitCount);
					return SNew(STableRow<FKawaiiPresetCatalogItemPtr>, OwnerTable)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(4, 3, 4, 1)
						[
							SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("[%s] %s / %s"), *InferKawaiiFamily(Item->RootBone), *Item->RelativePath, *Item->SourceNodeName)))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(4, 1, 4, 3)
						[
							SNew(STextBlock).Text(FText::FromString(Summary)).ColorAndOpacity(bRootMatches ? FSlateColor::UseSubduedForeground() : FSlateColor(FLinearColor(1.0f, 0.35f, 0.25f)))
						]
					];
				})
				.OnSelectionChanged_Lambda([&SelectedItem](FKawaiiPresetCatalogItemPtr Item, ESelectInfo::Type) { SelectedItem = Item; })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(16, 8, 16, 16).HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
				[
					SNew(SButton).Text(LOCTEXT("CancelKawaiiLibrary", "Cancel"))
					.OnClicked_Lambda([&bAccepted, &Window]() { bAccepted = false; Window->RequestDestroyWindow(); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).Text(LOCTEXT("UseKawaiiLibraryPreset", "Use Preset"))
					.IsEnabled_Lambda([&SelectedItem, &TargetMesh]() { return SelectedItem.IsValid() && HasBone(TargetMesh, SelectedItem->RootBone); })
					.OnClicked_Lambda([&bAccepted, &Window]() { bAccepted = true; Window->RequestDestroyWindow(); return FReply::Handled(); })
				]
			]
		];

	FSlateApplication::Get().AddModalWindow(Window.ToSharedRef(), nullptr);
	if (!bAccepted || !SelectedItem.IsValid())
	{
		return false;
	}
	OutSelection.SourceAnimLayerJson = SelectedItem->SourceJson;
	OutSelection.SourceNodeName = SelectedItem->SourceNodeName;
	OutSelection.SuggestedPresetPrefix = InferKawaiiFamily(SelectedItem->RootBone);
	return true;
}
}

#undef LOCTEXT_NAMESPACE
