// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteCharacterRuntimeUiPresentation.h"

#include "NteEditorAssetUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Engine/Font.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

namespace NTEBuildTool::Character
{
namespace
{
void AddPresentationError(FNteCharacterRuntimeActionAssetWriteResult& AssetResult, const FString& Error)
{
	AssetResult.Errors.Add(Error);
}

FString SanitizeWidgetNameSuffix(const FString& RawValue)
{
	FString Result;
	for (const TCHAR Character : RawValue)
	{
		Result.AppendChar(FChar::IsAlnum(Character) ? Character : TEXT('_'));
	}
	while (Result.Contains(TEXT("__")))
	{
		Result.ReplaceInline(TEXT("__"), TEXT("_"));
	}
	Result.TrimStartAndEndInline();
	Result.RemoveFromStart(TEXT("_"));
	Result.RemoveFromEnd(TEXT("_"));
	return Result.IsEmpty() ? TEXT("Action") : Result;
}

UFont* LoadOrCreateSourceGameFontProxy(
	const FString& FontPath,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	if (!FontPath.StartsWith(TEXT("/Game/")) || FontPath.Contains(TEXT(".")))
	{
		AddPresentationError(AssetResult, FString::Printf(
			TEXT("Runtime UI FontPath must be a /Game package path: %s"),
			*FontPath));
		return nullptr;
	}

	if (UFont* ExistingFont = NTEBuildTool::Editor::LoadAssetByPath<UFont>(FontPath))
	{
		AssetResult.Actions.Add(FString::Printf(TEXT("uses source-game font %s"), *FontPath));
		return ExistingFont;
	}

	FString ExistingPackageFilename;
	if (FPackageName::DoesPackageExist(FontPath, &ExistingPackageFilename))
	{
		AddPresentationError(AssetResult, FString::Printf(
			TEXT("Runtime UI font package exists but its UFont could not be loaded: %s (%s)"),
			*FontPath,
			*ExistingPackageFilename));
		return nullptr;
	}

	UPackage* Package = FindPackage(nullptr, *FontPath);
	if (!Package)
	{
		Package = CreatePackage(*FontPath);
	}
	if (!Package)
	{
		AddPresentationError(AssetResult, FString::Printf(TEXT("Could not create source-game font proxy package: %s"), *FontPath));
		return nullptr;
	}

	const FName FontName(*FPackageName::GetShortName(FontPath));
	if (UObject* ExistingObject = FindObject<UObject>(Package, *FontName.ToString()))
	{
		if (UFont* ExistingFont = Cast<UFont>(ExistingObject))
		{
			return ExistingFont;
		}
		AddPresentationError(AssetResult, FString::Printf(
			TEXT("Source-game font proxy path is occupied by %s: %s"),
			*ExistingObject->GetClass()->GetPathName(),
			*FontPath));
		return nullptr;
	}

	UFont* FontProxy = NewObject<UFont>(Package, FontName, RF_Public | RF_Standalone | RF_Transactional);
	if (!FontProxy)
	{
		AddPresentationError(AssetResult, FString::Printf(TEXT("Could not create source-game font proxy: %s"), *FontPath));
		return nullptr;
	}

	FontProxy->FontCacheType = EFontCacheType::Runtime;
	FontProxy->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(FontProxy);
	Package->MarkPackageDirty();

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(FontPath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, FontProxy, *PackageFilename, SaveArgs))
	{
		AddPresentationError(AssetResult, FString::Printf(TEXT("Could not save source-game font proxy: %s"), *PackageFilename));
		return nullptr;
	}

	AssetResult.Actions.Add(FString::Printf(TEXT("created editor-only source-game font proxy %s"), *FontPath));
	return FontProxy;
}

UWidgetTree* EnsureWidgetTree(UWidgetBlueprint& WidgetBlueprint)
{
	if (!WidgetBlueprint.WidgetTree)
	{
		WidgetBlueprint.Modify();
		WidgetBlueprint.WidgetTree = NewObject<UWidgetTree>(&WidgetBlueprint, TEXT("WidgetTree"), RF_Transactional);
	}
	return WidgetBlueprint.WidgetTree;
}

void DetachWidgetFromParent(UWidget& Widget)
{
	if (UPanelWidget* Parent = Widget.GetParent())
	{
		Parent->RemoveChild(&Widget);
	}
}

template <typename WidgetType>
WidgetType* FindOrCreateRuntimeWidget(
	UWidgetBlueprint& WidgetBlueprint,
	UWidgetTree& WidgetTree,
	const FName WidgetName,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	if (UWidget* ExistingWidget = WidgetTree.FindWidget(WidgetName))
	{
		WidgetType* ExistingTypedWidget = Cast<WidgetType>(ExistingWidget);
		if (!ExistingTypedWidget)
		{
			AddPresentationError(AssetResult, FString::Printf(
				TEXT("Widget '%s' exists but is %s, not %s."),
				*WidgetName.ToString(),
				*ExistingWidget->GetClass()->GetName(),
				*WidgetType::StaticClass()->GetName()));
			return nullptr;
		}
		ExistingTypedWidget->bIsVariable = true;
		return ExistingTypedWidget;
	}

	WidgetType* NewWidget = WidgetTree.ConstructWidget<WidgetType>(WidgetType::StaticClass(), WidgetName);
	if (!NewWidget)
	{
		AddPresentationError(AssetResult, FString::Printf(TEXT("Could not create widget '%s'."), *WidgetName.ToString()));
		return nullptr;
	}

	NewWidget->bIsVariable = true;
	WidgetBlueprint.OnVariableRemoved(WidgetName);
	WidgetBlueprint.OnVariableAdded(WidgetName);
	AssetResult.Actions.Add(FString::Printf(
		TEXT("created runtime widget %s:%s"),
		*WidgetName.ToString(),
		*NewWidget->GetClass()->GetName()));
	return NewWidget;
}
}

FName MakeRuntimeActionButtonWidgetName(const FNteCharacterRuntimeActionPlanItem& Action)
{
	return FName(*(TEXT("NTE_ActionButton_") + SanitizeWidgetNameSuffix(Action.Id)));
}

FName MakeRuntimeActionLabelWidgetName(const FNteCharacterRuntimeActionPlanItem& Action)
{
	return FName(*(TEXT("NTE_ActionLabel_") + SanitizeWidgetNameSuffix(Action.Id)));
}

bool RebuildRuntimeActionWidgetPresentation(
	UWidgetBlueprint& WidgetBlueprint,
	const FNteCharacterRuntimeActionPlan& Plan,
	const TArray<const FNteCharacterRuntimeActionPlanItem*>& Actions,
	FNteCharacterRuntimeActionAssetWriteResult& AssetResult)
{
	if (!Plan.RuntimeUi.bEnableUi)
	{
		AssetResult.Actions.Add(TEXT("RuntimeUi disabled; widget layout generation skipped"));
		return false;
	}

	UWidgetTree* WidgetTree = EnsureWidgetTree(WidgetBlueprint);
	if (!WidgetTree)
	{
		AddPresentationError(AssetResult, TEXT("WidgetBlueprint has no WidgetTree and one could not be created."));
		return false;
	}

	UFont* RuntimeFont = LoadOrCreateSourceGameFontProxy(Plan.RuntimeUi.FontPath, AssetResult);
	if (!RuntimeFont)
	{
		return false;
	}

	WidgetBlueprint.Modify();
	WidgetTree->Modify();

	UCanvasPanel* Root = FindOrCreateRuntimeWidget<UCanvasPanel>(WidgetBlueprint, *WidgetTree, TEXT("NTE_CharacterActions_Root"), AssetResult);
	UVerticalBox* WindowPanel = FindOrCreateRuntimeWidget<UVerticalBox>(WidgetBlueprint, *WidgetTree, TEXT("NTE_CharacterActions_WindowPanel"), AssetResult);
	UTextBlock* TitleText = FindOrCreateRuntimeWidget<UTextBlock>(WidgetBlueprint, *WidgetTree, TEXT("NTE_CharacterActions_Title"), AssetResult);
	UVerticalBox* ButtonList = FindOrCreateRuntimeWidget<UVerticalBox>(WidgetBlueprint, *WidgetTree, TEXT("NTE_CharacterActions_ButtonList"), AssetResult);
	if (!Root || !WindowPanel || !TitleText || !ButtonList)
	{
		return false;
	}

	struct FRuntimeActionWidgetPair { const FNteCharacterRuntimeActionPlanItem* Action = nullptr; UButton* Button = nullptr; UTextBlock* Label = nullptr; };
	TArray<FRuntimeActionWidgetPair> ActionWidgets;
	for (const FNteCharacterRuntimeActionPlanItem* Action : Actions)
	{
		if (!Action)
		{
			continue;
		}
		FRuntimeActionWidgetPair& Pair = ActionWidgets.AddDefaulted_GetRef();
		Pair.Action = Action;
		Pair.Button = FindOrCreateRuntimeWidget<UButton>(WidgetBlueprint, *WidgetTree, MakeRuntimeActionButtonWidgetName(*Action), AssetResult);
		Pair.Label = FindOrCreateRuntimeWidget<UTextBlock>(WidgetBlueprint, *WidgetTree, MakeRuntimeActionLabelWidgetName(*Action), AssetResult);
		if (!Pair.Button || !Pair.Label)
		{
			return false;
		}
	}

	UTextBlock* EmptyText = nullptr;
	if (Actions.IsEmpty())
	{
		EmptyText = FindOrCreateRuntimeWidget<UTextBlock>(WidgetBlueprint, *WidgetTree, TEXT("NTE_CharacterActions_EmptyLabel"), AssetResult);
		if (!EmptyText)
		{
			return false;
		}
	}

	Root->ClearChildren();
	WindowPanel->ClearChildren();
	ButtonList->ClearChildren();
	WidgetTree->RootWidget = Root;
	DetachWidgetFromParent(*WindowPanel);
	if (UCanvasPanelSlot* WindowSlot = Root->AddChildToCanvas(WindowPanel))
	{
		WindowSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		WindowSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		WindowSlot->SetPosition(FVector2D(80.0f, 80.0f));
		WindowSlot->SetSize(FVector2D(360.0f, 72.0f + FMath::Max(1, Actions.Num()) * 44.0f));
		WindowSlot->SetAutoSize(false);
	}

	const FString Title = Plan.RuntimeUi.Title.IsEmpty() ? TEXT("NTE Character Actions") : Plan.RuntimeUi.Title;
	TitleText->SetText(FText::FromString(Title));
	TitleText->SetFont(FSlateFontInfo(RuntimeFont, static_cast<float>(Plan.RuntimeUi.TitleFontSize), FName(*Plan.RuntimeUi.TitleFontTypeface)));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	DetachWidgetFromParent(*TitleText);
	if (UVerticalBoxSlot* TitleSlot = WindowPanel->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(12.0f, 8.0f, 12.0f, 6.0f));
		TitleSlot->SetHorizontalAlignment(HAlign_Fill);
		TitleSlot->SetVerticalAlignment(VAlign_Center);
	}

	DetachWidgetFromParent(*ButtonList);
	if (UVerticalBoxSlot* ButtonListSlot = WindowPanel->AddChildToVerticalBox(ButtonList))
	{
		ButtonListSlot->SetPadding(FMargin(8.0f, 0.0f, 8.0f, 8.0f));
		ButtonListSlot->SetHorizontalAlignment(HAlign_Fill);
		ButtonListSlot->SetVerticalAlignment(VAlign_Fill);
	}

	if (EmptyText)
	{
		EmptyText->SetText(FText::FromString(TEXT("No runtime actions configured")));
		EmptyText->SetFont(FSlateFontInfo(RuntimeFont, static_cast<float>(Plan.RuntimeUi.BodyFontSize), FName(*Plan.RuntimeUi.BodyFontTypeface)));
		EmptyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f)));
		DetachWidgetFromParent(*EmptyText);
		if (UVerticalBoxSlot* EmptySlot = ButtonList->AddChildToVerticalBox(EmptyText))
		{
			EmptySlot->SetPadding(FMargin(8.0f, 6.0f, 8.0f, 6.0f));
			EmptySlot->SetHorizontalAlignment(HAlign_Fill);
			EmptySlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	for (const FRuntimeActionWidgetPair& Pair : ActionWidgets)
	{
		if (!Pair.Button || !Pair.Label)
		{
			return false;
		}
		const FString LabelText = Pair.Action->Label.IsEmpty() ? Pair.Action->Id : Pair.Action->Label;
		Pair.Label->SetText(FText::FromString(LabelText));
		Pair.Label->SetFont(FSlateFontInfo(RuntimeFont, static_cast<float>(Plan.RuntimeUi.BodyFontSize), FName(*Plan.RuntimeUi.BodyFontTypeface)));
		Pair.Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Pair.Button->IsFocusable = false;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		Pair.Button->SetColorAndOpacity(FLinearColor::White);
		Pair.Button->SetBackgroundColor(FLinearColor(0.08f, 0.08f, 0.08f, 0.92f));
		DetachWidgetFromParent(*Pair.Label);
		Pair.Button->SetContent(Pair.Label);
		DetachWidgetFromParent(*Pair.Button);
		if (UVerticalBoxSlot* ButtonSlot = ButtonList->AddChildToVerticalBox(Pair.Button))
		{
			ButtonSlot->SetPadding(FMargin(4.0f, 4.0f, 4.0f, 4.0f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
			ButtonSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&WidgetBlueprint);
	if (UPackage* Package = WidgetBlueprint.GetPackage())
	{
		Package->MarkPackageDirty();
	}
	AssetResult.bUpdated = true;
	AssetResult.Actions.Add(FString::Printf(TEXT("rebuilt runtime widget layout with %d action button(s)"), Actions.Num()));
	return true;
}
}
