// Copyright (c) 2026 NTEBuildTool contributors.

#include "NtePakmodProjectEditor.h"

#include "NteBuildToolSettings.h"
#include "NteCharacterKawaiiPresetImporter.h"
#include "NteCharacterModSpec.h"
#include "NteCharacterModSpecMigration.h"
#include "NteCharacterRuntimeActionDialog.h"
#include "NteEditorAssetUtils.h"
#include "NteKawaiiPresetLibraryDialog.h"
#include "NteMaterialInstanceDialog.h"
#include "NteModPackageJob.h"
#include "NteModPackagePlan.h"
#include "NteNotificationUtils.h"
#include "NtePakmodRecipeAdapter.h"
#include "NtePhysicsAssetLibraryDialog.h"

#include "AssetRegistry/AssetData.h"
#include "DesktopPlatformModule.h"
#include "Dom/JsonValue.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/Docking/TabManager.h"
#include "IDesktopPlatform.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NTEBuildToolPakmodProject"

namespace NTEBuildTool::ProjectEditor
{
namespace
{
const FName PakmodProjectTabName(TEXT("NTEBuildTool.PakmodProject"));

FString SanitizeId(FString Value)
{
	Value = FPackageName::GetShortName(Value).ToLower();
	for (TCHAR& Character : Value)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
		{
			Character = TEXT('_');
		}
	}
	return Value.IsEmpty() ? TEXT("item") : Value;
}

FString UniqueAssetId(const NTEBuildTool::Project::FNtePakmodProject& Project, const FString& PackagePath)
{
	FString Result = TEXT("asset_") + SanitizeId(PackagePath);
	const FString Base = Result;
	int32 Suffix = 2;
	while (NTEBuildTool::Project::FindAssetById(Project, Result))
	{
		Result = FString::Printf(TEXT("%s_%d"), *Base, Suffix++);
	}
	return Result;
}

FString UniqueRecipeId(const NTEBuildTool::Project::FNtePakmodProject& Project, const FString& Prefix, const FString& Seed)
{
	FString Result = Prefix + SanitizeId(Seed);
	const FString Base = Result;
	int32 Suffix = 2;
	while (NTEBuildTool::Project::FindRecipeById(Project, Result))
	{
		Result = FString::Printf(TEXT("%s_%d"), *Base, Suffix++);
	}
	return Result;
}

bool ChooseOpenJson(const FText& Title, const FString& DefaultPath, const FString& Filter, FString& OutFilename)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return false;
	}
	TArray<FString> Files;
	if (!DesktopPlatform->OpenFileDialog(nullptr, Title.ToString(), DefaultPath, TEXT(""), Filter, EFileDialogFlags::None, Files) || Files.IsEmpty())
	{
		return false;
	}
	OutFilename = Files[0];
	FPaths::NormalizeFilename(OutFilename);
	return true;
}

bool ChooseSaveJson(const FText& Title, const FString& DefaultPath, const FString& DefaultFilename, FString& OutFilename)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return false;
	}
	TArray<FString> Files;
	if (!DesktopPlatform->SaveFileDialog(nullptr, Title.ToString(), DefaultPath, DefaultFilename, TEXT("Pakmod Project (*.pakmod.json)|*.pakmod.json|JSON (*.json)|*.json"), EFileDialogFlags::None, Files) || Files.IsEmpty())
	{
		return false;
	}
	OutFilename = Files[0];
	if (!OutFilename.EndsWith(TEXT(".json"), ESearchCase::IgnoreCase))
	{
		OutFilename += TEXT(".pakmod.json");
	}
	FPaths::NormalizeFilename(OutFilename);
	return true;
}

const NTEBuildTool::Project::FNteAssetReference* FindMainMeshAsset(const NTEBuildTool::Project::FNtePakmodProject& Project)
{
	const NTEBuildTool::Project::FNteAssetReference* Result = Project.Assets.FindByPredicate([](const NTEBuildTool::Project::FNteAssetReference& Asset)
	{
		return Asset.Intent == NTEBuildTool::Project::ENteAssetIntent::ReplacementAsset && Asset.ClassName.Contains(TEXT("SkeletalMesh"), ESearchCase::IgnoreCase);
	});
	if (!Result)
	{
		Result = Project.Assets.FindByPredicate([](const NTEBuildTool::Project::FNteAssetReference& Asset)
		{
			return Asset.Intent != NTEBuildTool::Project::ENteAssetIntent::ExternalReference && Asset.ClassName.Contains(TEXT("SkeletalMesh"), ESearchCase::IgnoreCase);
		});
	}
	return Result;
}

TSharedPtr<FJsonObject> FirstObjectField(const TSharedRef<FJsonObject>& Root, const TCHAR* Field)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (Root->TryGetArrayField(Field, Values) && Values && !Values->IsEmpty() && (*Values)[0].IsValid())
	{
		return (*Values)[0]->AsObject();
	}
	return nullptr;
}

void AppendResultMessages(const NTEBuildTool::Recipes::FNtePakmodRecipeApplyResult& Result, TArray<FString>& Out)
{
	Out.Append(Result.Errors);
	Out.Append(Result.Warnings);
	if (!Result.AppliedRecipeIds.IsEmpty())
	{
		Out.Add(FString::Printf(TEXT("Recipes: %s"), *FString::Join(Result.AppliedRecipeIds, TEXT(", "))));
	}
	if (!Result.SavedPackages.IsEmpty())
	{
		Out.Add(FString::Printf(TEXT("Saved assets: %s"), *FString::Join(Result.SavedPackages, TEXT(", "))));
	}
}
}

FNtePakmodProjectEditorModel::FNtePakmodProjectEditorModel()
{
	const FString LastProject = NTEBuildTool::Settings::GetLastPakmodProject();
	FString Error;
	if (LastProject.IsEmpty() || !FPaths::FileExists(LastProject) || !Load(LastProject, Error))
	{
		NewProject();
	}
}

void FNtePakmodProjectEditorModel::NewProject()
{
	Project = NTEBuildTool::Project::FNtePakmodProject();
	Project.Project.Id = TEXT("new_mod");
	Project.Project.DisplayName = TEXT("New Mod");
	Project.PackageManifest.ModName = TEXT("new_mod_P");
	Project.PackageManifest.ModsDirOverride = NTEBuildTool::Settings::GetDefaultModsOutputDirectory();
	Project.PackageManifest.JobFilename = FPaths::ProjectSavedDir() / TEXT("NTEBuildTool/Jobs/new_mod_P.job.json");
	Filename.Reset();
	PackagePlan = NTEBuildTool::Package::FNtePackagePlan();
	Diagnostics.Reset();
	Status = TEXT("New unsaved Pakmod Project");
	bDirty = true;
}

bool FNtePakmodProjectEditorModel::Load(const FString& ProjectFilename, FString& OutError)
{
	NTEBuildTool::Project::FNtePakmodProject Loaded;
	if (!NTEBuildTool::Project::LoadPakmodProjectFromJsonFile(ProjectFilename, Loaded, OutError))
	{
		return false;
	}
	Project = MoveTemp(Loaded);
	Filename = ProjectFilename;
	FPaths::NormalizeFilename(Filename);
	bDirty = false;
	Status = FString::Printf(TEXT("Loaded %s"), *Filename);
	Diagnostics.Reset();
	NTEBuildTool::Settings::RememberPakmodProject(Filename);
	return true;
}

bool FNtePakmodProjectEditorModel::Save(FString& OutError)
{
	if (Filename.IsEmpty())
	{
		OutError = TEXT("Pakmod Project has no filename. Use Save As.");
		return false;
	}
	if (!NTEBuildTool::Project::SavePakmodProjectToJsonFile(Project, Filename, OutError))
	{
		return false;
	}
	bDirty = false;
	Status = FString::Printf(TEXT("Saved %s"), *Filename);
	NTEBuildTool::Settings::RememberPakmodProject(Filename);
	return true;
}

bool FNtePakmodProjectEditorModel::SaveAs(const FString& ProjectFilename, FString& OutError)
{
	Filename = ProjectFilename;
	FPaths::NormalizeFilename(Filename);
	return Save(OutError);
}

bool FNtePakmodProjectEditorModel::AddAssetPath(
	const FString& PackagePath,
	const FString& ClassName,
	const NTEBuildTool::Project::ENteAssetOrigin Origin,
	const NTEBuildTool::Project::ENteAssetIntent Intent,
	FString& OutError)
{
	const FString Normalized = NTEBuildTool::Project::NormalizePakmodPackagePath(PackagePath);
	if (!Normalized.StartsWith(TEXT("/Game/")))
	{
		OutError = FString::Printf(TEXT("Asset path must be a /Game package: %s"), *PackagePath);
		return false;
	}
	NTEBuildTool::Project::FNteAssetReference* Existing = Project.Assets.FindByPredicate([&Normalized](NTEBuildTool::Project::FNteAssetReference& Asset)
	{
		return Asset.PackagePath.Equals(Normalized, ESearchCase::IgnoreCase);
	});
	if (!Existing)
	{
		NTEBuildTool::Project::FNteAssetReference Asset;
		Asset.Id = UniqueAssetId(Project, Normalized);
		Asset.PackagePath = Normalized;
		Asset.ClassName = ClassName;
		Asset.Origin = Origin;
		Asset.Intent = Intent;
		Project.Assets.Add(MoveTemp(Asset));
		Existing = &Project.Assets.Last();
	}
	else
	{
		Existing->ClassName = ClassName.IsEmpty() ? Existing->ClassName : ClassName;
		Existing->Origin = Origin;
		Existing->Intent = Intent;
	}
	if (Intent == NTEBuildTool::Project::ENteAssetIntent::ExternalReference)
	{
		Project.PackageManifest.AssetIds.Remove(Existing->Id);
	}
	else
	{
		Project.PackageManifest.AssetIds.AddUnique(Existing->Id);
	}
	bDirty = true;
	return true;
}

bool FNtePakmodProjectEditorModel::AddSelectedAssets(const NTEBuildTool::Project::ENteAssetIntent Intent, FString& OutError)
{
	const TArray<FAssetData> Selected = NTEBuildTool::Editor::GetSelectedContentBrowserAssets();
	if (Selected.IsEmpty())
	{
		OutError = TEXT("Select one or more assets in the Content Browser first.");
		return false;
	}
	const NTEBuildTool::Project::ENteAssetOrigin Origin = Intent == NTEBuildTool::Project::ENteAssetIntent::ExternalReference
		? NTEBuildTool::Project::ENteAssetOrigin::GameReference
		: NTEBuildTool::Project::ENteAssetOrigin::UserImported;
	for (const FAssetData& Asset : Selected)
	{
		if (!AddAssetPath(Asset.PackageName.ToString(), Asset.AssetClassPath.GetAssetName().ToString(), Origin, Intent, OutError))
		{
			return false;
		}
	}
	Status = FString::Printf(TEXT("Added %d selected asset(s)"), Selected.Num());
	return true;
}

bool FNtePakmodProjectEditorModel::RemoveAsset(const FString& AssetId, FString& OutError)
{
	for (const NTEBuildTool::Project::FNteAuthoringRecipe& Recipe : Project.Recipes)
	{
		if (Recipe.TargetAssetIds.Contains(AssetId) || Recipe.OutputAssetIds.Contains(AssetId))
		{
			OutError = FString::Printf(TEXT("Asset '%s' is still referenced by recipe '%s'. Detach or remove that recipe first."), *AssetId, *Recipe.Id);
			return false;
		}
	}
	Project.PackageManifest.AssetIds.Remove(AssetId);
	const int32 Removed = Project.Assets.RemoveAll([&AssetId](const NTEBuildTool::Project::FNteAssetReference& Asset) { return Asset.Id == AssetId; });
	if (Removed == 0)
	{
		OutError = FString::Printf(TEXT("Asset '%s' does not exist."), *AssetId);
		return false;
	}
	bDirty = true;
	return true;
}

void FNtePakmodProjectEditorModel::ToggleManifestAsset(const FString& AssetId)
{
	if (Project.PackageManifest.AssetIds.Contains(AssetId))
	{
		Project.PackageManifest.AssetIds.Remove(AssetId);
	}
	else if (const NTEBuildTool::Project::FNteAssetReference* Asset = NTEBuildTool::Project::FindAssetById(Project, AssetId))
	{
		if (Asset->Intent != NTEBuildTool::Project::ENteAssetIntent::ExternalReference)
		{
			Project.PackageManifest.AssetIds.Add(AssetId);
		}
	}
	bDirty = true;
}

bool FNtePakmodProjectEditorModel::RefreshPackagePlan(FString& OutError)
{
	PackagePlan = NTEBuildTool::Package::FNtePackagePlan();
	if (!NTEBuildTool::Package::BuildPackagePlanFromManifest(Project, PackagePlan, OutError))
	{
		return false;
	}
	Status = FString::Printf(TEXT("Package plan contains %d candidates"), PackagePlan.Candidates.Num());
	return true;
}

bool FNtePakmodProjectEditorModel::Validate(FString& OutError) const
{
	const NTEBuildTool::Project::FNtePakmodProjectValidationResult Validation = NTEBuildTool::Project::ValidatePakmodProject(Project);
	if (Validation.HasErrors())
	{
		OutError = FString::Join(Validation.Errors, LINE_TERMINATOR);
		return false;
	}
	return true;
}

namespace
{
enum class EPakmodPage : uint8
{
	Project,
	Assets,
	Materials,
	Runtime,
	Physics,
	Kawaii,
	Package,
	Build,
	Count
};

class SNtePakmodProjectEditor final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNtePakmodProjectEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&)
	{
		Model = MakeShared<FNtePakmodProjectEditorModel>();
		SAssignNew(PageSwitcher, SWidgetSwitcher);
		PageSwitcher->AddSlot()[BuildProjectPage()];
		PageSwitcher->AddSlot()[BuildAssetsPage()];
		PageSwitcher->AddSlot()[BuildRecipePage(TEXT("MaterialInstance"), LOCTEXT("MaterialPageTitle", "Materials"), LOCTEXT("MaterialPageHint", "Create material instances from game references and explicit texture overrides."), EPakmodPage::Materials)];
		PageSwitcher->AddSlot()[BuildRecipePage(TEXT("RuntimeActions"), LOCTEXT("RuntimePageTitle", "Runtime UI and Hotkeys"), LOCTEXT("RuntimePageHint", "Configure persistent visibility actions and the generated in-game control panel."), EPakmodPage::Runtime)];
		PageSwitcher->AddSlot()[BuildRecipePage(TEXT("PhysicsAsset"), LOCTEXT("PhysicsPageTitle", "PhysicsAsset"), LOCTEXT("PhysicsPageHint", "Copy a complete source-game PhysicsAsset or one matching bone chain."), EPakmodPage::Physics)];
		PageSwitcher->AddSlot()[BuildRecipePage(TEXT("Kawaii"), LOCTEXT("KawaiiPageTitle", "Kawaii and Post Process"), LOCTEXT("KawaiiPageHint", "Import source-game Kawaii nodes and apply the merged-main-mesh Post Process AnimBP route."), EPakmodPage::Kawaii)];
		PageSwitcher->AddSlot()[BuildPackagePage()];
		PageSwitcher->AddSlot()[BuildBuildPage()];

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[BuildToolbar()]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(6)[BuildNavigation()]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(6)[PageSwitcher.ToSharedRef()]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 4)
			[
				SNew(STextBlock).Text(this, &SNtePakmodProjectEditor::GetStatusText).AutoWrapText(true)
			]
		];
		RefreshAll();
	}

private:
	TSharedPtr<FNtePakmodProjectEditorModel> Model;
	TSharedPtr<SWidgetSwitcher> PageSwitcher;
	TSharedPtr<SVerticalBox> AssetRows;
	TSharedPtr<SVerticalBox> MaterialRows;
	TSharedPtr<SVerticalBox> RuntimeRows;
	TSharedPtr<SVerticalBox> PhysicsRows;
	TSharedPtr<SVerticalBox> KawaiiRows;
	TSharedPtr<SVerticalBox> PackageRows;
	TSharedPtr<SVerticalBox> BuildRows;
	TSharedPtr<SEditableTextBox> ProjectNameBox;
	TSharedPtr<SEditableTextBox> ProjectIdBox;
	TSharedPtr<SEditableTextBox> ModNameBox;
	TSharedPtr<SEditableTextBox> ModsDirBox;
	TSharedPtr<SEditableTextBox> JobFileBox;

	FText GetStatusText() const
	{
		return FText::FromString(Model->Status + (Model->bDirty ? TEXT("  * unsaved") : FString()));
	}

	void PullProjectFields()
	{
		if (ProjectNameBox) Model->Project.Project.DisplayName = ProjectNameBox->GetText().ToString();
		if (ProjectIdBox) Model->Project.Project.Id = ProjectIdBox->GetText().ToString();
		if (ModNameBox) Model->Project.PackageManifest.ModName = ModNameBox->GetText().ToString();
		if (ModsDirBox) Model->Project.PackageManifest.ModsDirOverride = ModsDirBox->GetText().ToString();
		if (JobFileBox) Model->Project.PackageManifest.JobFilename = JobFileBox->GetText().ToString();
	}

	void PushProjectFields()
	{
		if (ProjectNameBox) ProjectNameBox->SetText(FText::FromString(Model->Project.Project.DisplayName));
		if (ProjectIdBox) ProjectIdBox->SetText(FText::FromString(Model->Project.Project.Id));
		if (ModNameBox) ModNameBox->SetText(FText::FromString(Model->Project.PackageManifest.ModName));
		if (ModsDirBox) ModsDirBox->SetText(FText::FromString(Model->Project.PackageManifest.ModsDirOverride));
		if (JobFileBox) JobFileBox->SetText(FText::FromString(Model->Project.PackageManifest.JobFilename));
	}

	void RefreshAll()
	{
		PushProjectFields();
		RefreshAssetRows();
		RefreshRecipeRows();
		RefreshPackageRows();
		RefreshBuildRows();
	}

	TSharedRef<SWidget> BuildToolbar()
	{
		return SNew(SBorder).Padding(6)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(LOCTEXT("NewProject", "New")).OnClicked(this, &SNtePakmodProjectEditor::OnNew)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(LOCTEXT("OpenProject", "Open")).OnClicked(this, &SNtePakmodProjectEditor::OnOpen)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(LOCTEXT("MigrateSpec", "Migrate CharacterModSpec")).OnClicked(this, &SNtePakmodProjectEditor::OnMigrate)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(LOCTEXT("SaveProject", "Save")).OnClicked(this, &SNtePakmodProjectEditor::OnSave)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(LOCTEXT("SaveProjectAs", "Save As")).OnClicked(this, &SNtePakmodProjectEditor::OnSaveAs)]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(10, 2).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(Model->Filename.IsEmpty() ? TEXT("<unsaved .pakmod.json>") : Model->Filename); })
			]
		];
	}

	TSharedRef<SWidget> BuildNavigation()
	{
		const TArray<TPair<EPakmodPage, FText>> Pages = {
			{ EPakmodPage::Project, LOCTEXT("NavProject", "Project") },
			{ EPakmodPage::Assets, LOCTEXT("NavAssets", "Assets") },
			{ EPakmodPage::Materials, LOCTEXT("NavMaterials", "Materials") },
			{ EPakmodPage::Runtime, LOCTEXT("NavRuntime", "Runtime") },
			{ EPakmodPage::Physics, LOCTEXT("NavPhysics", "Physics") },
			{ EPakmodPage::Kawaii, LOCTEXT("NavKawaii", "Kawaii") },
			{ EPakmodPage::Package, LOCTEXT("NavPackage", "Package") },
			{ EPakmodPage::Build, LOCTEXT("NavBuild", "Build") }
		};
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		for (const TPair<EPakmodPage, FText>& Page : Pages)
		{
			Box->AddSlot().AutoHeight().Padding(1)
			[
				SNew(SButton).Text(Page.Value).HAlign(HAlign_Left).OnClicked_Lambda([this, Index = static_cast<int32>(Page.Key)]()
				{
					PageSwitcher->SetActiveWidgetIndex(Index);
					return FReply::Handled();
				})
			];
		}
		return SNew(SBorder).Padding(4)[SNew(SBox).WidthOverride(150.0f)[Box]];
	}

	TSharedRef<SWidget> LabeledField(const FText& Label, TSharedPtr<SEditableTextBox>& OutBox)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(170)[SNew(STextBlock).Text(Label)]]
			+ SHorizontalBox::Slot().FillWidth(1.0f)[SAssignNew(OutBox, SEditableTextBox).OnTextChanged_Lambda([this](const FText&) { Model->bDirty = true; })];
	}

	TSharedRef<SWidget> BuildProjectPage()
	{
		return SNew(SScrollBox)
		+ SScrollBox::Slot()[SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).Text(LOCTEXT("ProjectTitle", "Pakmod Project"))]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)[LabeledField(LOCTEXT("ProjectName", "Display name"), ProjectNameBox)]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)[LabeledField(LOCTEXT("ProjectId", "Project ID"), ProjectIdBox)]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)[LabeledField(LOCTEXT("ModName", "Mod name"), ModNameBox)]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)[LabeledField(LOCTEXT("ModsDir", "Mods output directory"), ModsDirBox)]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)[LabeledField(LOCTEXT("JobFile", "Package job JSON"), JobFileBox)]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("ProjectHelp", "Project data is persistent. Apply Changes writes recipe outputs; Build Package only reads the saved UE assets listed by the Package Manifest."))]
		];
	}

	TSharedRef<SWidget> BuildAssetsPage()
	{
		return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(LOCTEXT("AddReplacement", "Add selected as Replacement")).OnClicked_Lambda([this]() { return OnAddSelected(NTEBuildTool::Project::ENteAssetIntent::ReplacementAsset); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(LOCTEXT("AddAdded", "Add selected as Added")).OnClicked_Lambda([this]() { return OnAddSelected(NTEBuildTool::Project::ENteAssetIntent::AddedAsset); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(LOCTEXT("AddExternal", "Add selected as Game Reference")).OnClicked_Lambda([this]() { return OnAddSelected(NTEBuildTool::Project::ENteAssetIntent::ExternalReference); })]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(AssetRows, SVerticalBox)]];
	}

	TSharedPtr<SVerticalBox>& RecipeRowsForPage(const EPakmodPage Page)
	{
		switch (Page)
		{
		case EPakmodPage::Materials: return MaterialRows;
		case EPakmodPage::Runtime: return RuntimeRows;
		case EPakmodPage::Physics: return PhysicsRows;
		default: return KawaiiRows;
		}
	}

	TSharedRef<SWidget> BuildRecipePage(const FString& RecipeType, const FText& Title, const FText& Hint, const EPakmodPage Page)
	{
		TSharedPtr<SVerticalBox>& Rows = RecipeRowsForPage(Page);
		return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).Text(Title)]
		+ SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).Text(Hint).AutoWrapText(true)]
		+ SVerticalBox::Slot().AutoHeight().Padding(2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(LOCTEXT("AddRecipe", "Add recipe")).OnClicked_Lambda([this, RecipeType]() { return OnAddRecipe(RecipeType); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(LOCTEXT("ApplyRecipe", "Apply enabled")).OnClicked_Lambda([this, RecipeType]() { return OnApplyRecipes(RecipeType); })]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(Rows, SVerticalBox)]];
	}

	TSharedRef<SWidget> BuildPackagePage()
	{
		return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).Text(LOCTEXT("PackageTitle", "Package Manifest"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(LOCTEXT("RefreshPlan", "Refresh Package Plan")).OnClicked(this, &SNtePakmodProjectEditor::OnRefreshPlan)]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(LOCTEXT("ValidateProject", "Validate")).OnClicked(this, &SNtePakmodProjectEditor::OnValidate)]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(PackageRows, SVerticalBox)]];
	}

	TSharedRef<SWidget> BuildBuildPage()
	{
		return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).Text(LOCTEXT("BuildTitle", "Build Package"))]
		+ SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("BuildHelp", "Build reads only current disk assets in Package Manifest. It never runs Apply Changes."))]
		+ SVerticalBox::Slot().AutoHeight().Padding(2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(LOCTEXT("CreateJob", "Create Package Job")).OnClicked_Lambda([this]() { return OnBuild(false); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(LOCTEXT("BuildNow", "Build Package")).OnClicked_Lambda([this]() { return OnBuild(true); })]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(BuildRows, SVerticalBox)]];
	}

	void RefreshAssetRows()
	{
		if (!AssetRows) return;
		AssetRows->ClearChildren();
		for (const NTEBuildTool::Project::FNteAssetReference& Asset : Model->Project.Assets)
		{
			const FString AssetId = Asset.Id;
			const bool bInManifest = Model->Project.PackageManifest.AssetIds.Contains(Asset.Id);
			AssetRows->AddSlot().AutoHeight().Padding(2)
			[
				SNew(SBorder).Padding(4)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2)
					[
						SNew(SCheckBox).IsEnabled(Asset.Intent != NTEBuildTool::Project::ENteAssetIntent::ExternalReference).IsChecked(bInManifest ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
						.OnCheckStateChanged_Lambda([this, AssetId](ECheckBoxState) { Model->ToggleManifestAsset(AssetId); RefreshAssetRows(); RefreshPackageRows(); })
					]
					+ SHorizontalBox::Slot().FillWidth(0.18f).Padding(4).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(Asset.Id))]
					+ SHorizontalBox::Slot().FillWidth(0.47f).Padding(4).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(Asset.PackagePath))]
					+ SHorizontalBox::Slot().FillWidth(0.13f).Padding(4).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(NTEBuildTool::Project::AssetOriginToString(Asset.Origin)))]
					+ SHorizontalBox::Slot().FillWidth(0.15f).Padding(4).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(NTEBuildTool::Project::AssetIntentToString(Asset.Intent)))]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(LOCTEXT("RemoveAsset", "Remove")).OnClicked_Lambda([this, AssetId]()
					{
						FString Error;
						if (!Model->RemoveAsset(AssetId, Error)) Model->Status = Error;
						RefreshAll();
						return FReply::Handled();
					})]
				]
			];
		}
	}

	void AddRecipeRow(const NTEBuildTool::Project::FNteAuthoringRecipe& Recipe, const TSharedPtr<SVerticalBox>& Rows)
	{
		if (!Rows) return;
		const FString RecipeId = Recipe.Id;
		Rows->AddSlot().AutoHeight().Padding(2)
		[
			SNew(SBorder).Padding(4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(2).VAlign(VAlign_Center)
				[
					SNew(SCheckBox).IsChecked(Recipe.bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked).OnCheckStateChanged_Lambda([this, RecipeId](ECheckBoxState State)
					{
						if (NTEBuildTool::Project::FNteAuthoringRecipe* Item = Model->Project.Recipes.FindByPredicate([&RecipeId](NTEBuildTool::Project::FNteAuthoringRecipe& Candidate) { return Candidate.Id == RecipeId; }))
						{
							Item->bEnabled = State == ECheckBoxState::Checked; Model->bDirty = true;
						}
					})
				]
				+ SHorizontalBox::Slot().FillWidth(0.25f).Padding(4)[SNew(STextBlock).Text(FText::FromString(Recipe.Id))]
				+ SHorizontalBox::Slot().FillWidth(0.25f).Padding(4)[SNew(STextBlock).Text(FText::FromString(Recipe.Type))]
				+ SHorizontalBox::Slot().FillWidth(0.25f).Padding(4)[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("targets %d / outputs %d"), Recipe.TargetAssetIds.Num(), Recipe.OutputAssetIds.Num())))]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(LOCTEXT("ApplyOne", "Apply")).OnClicked_Lambda([this, RecipeId]() { return ApplyRecipeIds({ RecipeId }); })]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(LOCTEXT("SyncOne", "Sync")).IsEnabled(Recipe.Type.StartsWith(TEXT("Kawaii"))).OnClicked_Lambda([this, RecipeId]() { return SyncRecipe(RecipeId); })]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(LOCTEXT("DetachOne", "Detach")).OnClicked_Lambda([this, RecipeId]() { return DetachRecipe(RecipeId); })]
			]
		];
	}

	void RefreshRecipeRows()
	{
		for (TSharedPtr<SVerticalBox> Rows : { MaterialRows, RuntimeRows, PhysicsRows, KawaiiRows }) if (Rows) Rows->ClearChildren();
		for (const NTEBuildTool::Project::FNteAuthoringRecipe& Recipe : Model->Project.Recipes)
		{
			if (Recipe.Type.Equals(TEXT("MaterialInstance"), ESearchCase::IgnoreCase)) AddRecipeRow(Recipe, MaterialRows);
			else if (Recipe.Type.Equals(TEXT("RuntimeActions"), ESearchCase::IgnoreCase)) AddRecipeRow(Recipe, RuntimeRows);
			else if (Recipe.Type.Equals(TEXT("PhysicsAsset"), ESearchCase::IgnoreCase)) AddRecipeRow(Recipe, PhysicsRows);
			else if (Recipe.Type.StartsWith(TEXT("Kawaii"), ESearchCase::IgnoreCase) || Recipe.Type.Equals(TEXT("PostProcessBlueprint"), ESearchCase::IgnoreCase)) AddRecipeRow(Recipe, KawaiiRows);
		}
	}

	void RefreshPackageRows()
	{
		if (!PackageRows) return;
		PackageRows->ClearChildren();
		for (const NTEBuildTool::Package::FNtePackagePlanCandidate& Candidate : Model->PackagePlan.Candidates)
		{
			PackageRows->AddSlot().AutoHeight().Padding(2)[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s  |  %s  |  %s"), Candidate.bDefaultIncluded ? TEXT("IN") : TEXT("OUT"), *Candidate.PackageName, *Candidate.Reason)))];
		}
		if (Model->PackagePlan.Candidates.IsEmpty()) PackageRows->AddSlot().AutoHeight().Padding(4)[SNew(STextBlock).Text(LOCTEXT("PlanEmpty", "Refresh the plan to resolve Package Manifest assets and hard dependencies."))];
	}

	void RefreshBuildRows()
	{
		if (!BuildRows) return;
		BuildRows->ClearChildren();
		BuildRows->AddSlot().AutoHeight().Padding(2)[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Manifest assets: %d"), Model->Project.PackageManifest.AssetIds.Num())))];
		BuildRows->AddSlot().AutoHeight().Padding(2)[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Plan candidates: %d"), Model->PackagePlan.Candidates.Num())))];
		BuildRows->AddSlot().AutoHeight().Padding(2)[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Output: %s"), *Model->Project.PackageManifest.ModsDirOverride)))];
	}

	FReply OnNew()
	{
		Model->NewProject(); RefreshAll(); return FReply::Handled();
	}

	FReply OnOpen()
	{
		FString Filename;
		if (ChooseOpenJson(LOCTEXT("ChoosePakmodProject", "Open Pakmod Project"), FPaths::GetPath(Model->Filename), TEXT("Pakmod Project (*.pakmod.json)|*.pakmod.json|JSON (*.json)|*.json"), Filename))
		{
			FString Error;
			if (!Model->Load(Filename, Error)) Model->Status = Error;
			RefreshAll();
		}
		return FReply::Handled();
	}

	FReply OnMigrate()
	{
		FString Filename;
		if (!ChooseOpenJson(LOCTEXT("ChooseLegacySpec", "Choose CharacterModSpec JSON"), FPaths::ProjectDir(), TEXT("CharacterModSpec (*.json)|*.json"), Filename)) return FReply::Handled();
		NTEBuildTool::Character::FNteCharacterModSpec Spec;
		FString Error;
		if (!NTEBuildTool::Character::LoadCharacterModSpecFromJsonFile(Filename, Spec, Error))
		{
			Model->Status = Error; return FReply::Handled();
		}
		NTEBuildTool::Character::FNteCharacterModSpecMigrationResult Migration = NTEBuildTool::Character::MigrateCharacterModSpecToPakmodProject(Spec);
		if (Migration.HasErrors())
		{
			Model->Status = FString::Join(Migration.Errors, TEXT("; ")); return FReply::Handled();
		}
		Model->Project = MoveTemp(Migration.Project); Model->Filename.Reset(); Model->bDirty = true;
		Model->Status = FString::Printf(TEXT("Migrated %s. Save as .pakmod.json."), *Filename);
		RefreshAll(); return FReply::Handled();
	}

	FReply OnSave()
	{
		PullProjectFields();
		if (Model->Filename.IsEmpty()) return OnSaveAs();
		FString Error; if (!Model->Save(Error)) Model->Status = Error; RefreshAll(); return FReply::Handled();
	}

	FReply OnSaveAs()
	{
		PullProjectFields();
		FString Filename;
		if (ChooseSaveJson(LOCTEXT("SavePakmodProject", "Save Pakmod Project"), Model->Filename.IsEmpty() ? FPaths::ProjectDir() : FPaths::GetPath(Model->Filename), Model->Project.Project.Id + TEXT(".pakmod.json"), Filename))
		{
			FString Error; if (!Model->SaveAs(Filename, Error)) Model->Status = Error; RefreshAll();
		}
		return FReply::Handled();
	}

	FReply OnAddSelected(const NTEBuildTool::Project::ENteAssetIntent Intent)
	{
		FString Error; if (!Model->AddSelectedAssets(Intent, Error)) Model->Status = Error; RefreshAll(); return FReply::Handled();
	}

	FReply OnAddRecipe(const FString RecipeType)
	{
		PullProjectFields();
		const NTEBuildTool::Project::FNteAssetReference* MainMeshAsset = FindMainMeshAsset(Model->Project);
		USkeletalMesh* MainMesh = MainMeshAsset ? NTEBuildTool::Editor::LoadAssetByPath<USkeletalMesh>(MainMeshAsset->PackagePath) : nullptr;
		if (!MainMesh)
		{
			Model->Status = TEXT("Add the main SkeletalMesh as a Replacement or Added asset before creating this recipe."); return FReply::Handled();
		}

		NTEBuildTool::Project::FNteAuthoringRecipe Recipe;
		Recipe.TargetAssetIds.Add(MainMeshAsset->Id);
		if (RecipeType.Equals(TEXT("MaterialInstance")))
		{
			NTEBuildTool::Material::FNteMaterialInstanceOptions Initial; Initial.MeshPath = MainMeshAsset->PackagePath;
			NTEBuildTool::Material::FNteMaterialInstanceOptions Options; TSharedPtr<FJsonObject> Overrides;
			if (!NTEBuildTool::Material::ShowFModelMaterialLibraryRecipeDialog(Initial, Options, Overrides)) return FReply::Handled();
			NTEBuildTool::Character::FNteCharacterModSpec Projection; Projection.MainMeshPath = MainMeshAsset->PackagePath;
			NTEBuildTool::Character::FNteCharacterMaterialOperationSpec Operation;
			Operation.Id = UniqueRecipeId(Model->Project, TEXT("material_"), Options.OutputMaterialPath); Operation.TargetMeshId = TEXT("main"); Operation.SlotIndex = Options.SlotIndex; Operation.ParentMaterialPath = Options.ParentMaterialPath; Operation.SourceMaterialJson = Options.SourceMaterialJson; Operation.OutputMaterialPath = Options.OutputMaterialPath; Operation.bAssignToSlot = Options.bAssignToMeshSlot;
			if (Overrides) for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Overrides->Values) if (Pair.Value) Operation.SourceTextureOverrides.Add(Pair.Key, Pair.Value->AsString());
			Projection.MaterialOperations.Add(Operation); Recipe.Id = Operation.Id; Recipe.Type = TEXT("MaterialInstance"); Recipe.Deltas = FirstObjectField(NTEBuildTool::Character::CharacterModSpecToJson(Projection), TEXT("MaterialOperations"));
			if (!Options.OutputMaterialPath.IsEmpty())
			{
				FString Error; Model->AddAssetPath(Options.OutputMaterialPath, TEXT("MaterialInstanceConstant"), NTEBuildTool::Project::ENteAssetOrigin::ToolGenerated, NTEBuildTool::Project::ENteAssetIntent::AddedAsset, Error);
				if (NTEBuildTool::Project::FNteAssetReference* Output = Model->Project.Assets.FindByPredicate([&Options](NTEBuildTool::Project::FNteAssetReference& Asset) { return Asset.PackagePath == NTEBuildTool::Project::NormalizePakmodPackagePath(Options.OutputMaterialPath); })) { Output->OwnerRecipeId = Recipe.Id; Recipe.OutputAssetIds.Add(Output->Id); }
			}
		}
		else if (RecipeType.Equals(TEXT("RuntimeActions")))
		{
			NTEBuildTool::Character::FNteCharacterModSpec Projection; Projection.MainMeshPath = MainMeshAsset->PackagePath; Projection.MainPostProcessAnimBlueprintPath = MainMesh->GetPostProcessAnimBlueprint() ? MainMesh->GetPostProcessAnimBlueprint()->GetPathName() : FString();
			NTEBuildTool::Character::FNteCharacterRuntimeActionDialogDefaults Defaults; NTEBuildTool::Character::FNteCharacterRuntimeActionSpec Action;
			if (!NTEBuildTool::Character::ShowCharacterRuntimeActionDialog(Projection, Defaults, Action)) return FReply::Handled();
			Projection.RuntimeActions.Add(Action); Projection.RuntimeUi.bEnableUi = true; Projection.RuntimeUi.ToggleUiHotkey = TEXT("Slash"); Projection.RuntimeUi.Title = Model->Project.Project.DisplayName;
			const TSharedRef<FJsonObject> Json = NTEBuildTool::Character::CharacterModSpecToJson(Projection);
			Recipe.Id = UniqueRecipeId(Model->Project, TEXT("runtime_"), Action.Id); Recipe.Type = TEXT("RuntimeActions"); Recipe.Deltas = MakeShared<FJsonObject>();
			const TArray<TSharedPtr<FJsonValue>>* Actions = nullptr; if (Json->TryGetArrayField(TEXT("RuntimeActions"), Actions) && Actions) Recipe.Deltas->SetArrayField(TEXT("Actions"), *Actions);
			const TSharedPtr<FJsonObject>* Ui = nullptr; if (Json->TryGetObjectField(TEXT("RuntimeUi"), Ui) && Ui && Ui->IsValid()) Recipe.Deltas->SetObjectField(TEXT("RuntimeUi"), *Ui);
		}
		else if (RecipeType.Equals(TEXT("PhysicsAsset")))
		{
			NTEBuildTool::Physics::FNteFModelPhysicsAssetLibrarySelection Selection; if (!NTEBuildTool::Physics::ShowFModelPhysicsAssetLibraryDialog(*MainMesh, Selection)) return FReply::Handled();
			Recipe.Id = UniqueRecipeId(Model->Project, TEXT("physics_"), Selection.SourcePhysicsAssetJson); Recipe.Type = TEXT("PhysicsAsset"); Recipe.Deltas = MakeShared<FJsonObject>(); Recipe.Deltas->SetStringField(TEXT("SourcePhysicsAssetJson"), Selection.SourcePhysicsAssetJson); Recipe.Deltas->SetStringField(TEXT("TargetMeshPath"), MainMeshAsset->PackagePath); if (Selection.ChainRootBone != NAME_None) Recipe.Deltas->SetStringField(TEXT("ChainRootBone"), Selection.ChainRootBone.ToString());
		}
		else
		{
			NTEBuildTool::Kawaii::FNteFModelKawaiiPresetLibrarySelection Selection; if (!NTEBuildTool::Kawaii::ShowFModelKawaiiPresetLibraryDialog(*MainMesh, Selection)) return FReply::Handled();
			NTEBuildTool::Character::FNteCharacterKawaiiImportOptions Options; Options.SourceJsonPath = Selection.SourceAnimLayerJson; Options.SourceNodeNames.Add(Selection.SourceNodeName); Options.TargetMeshId = TEXT("main"); Options.PresetIdPrefix = Selection.SuggestedPresetPrefix;
			NTEBuildTool::Character::FNteCharacterKawaiiImportResult Import = NTEBuildTool::Character::ImportKawaiiPresetsFromFModelJson(Options); if (Import.HasErrors() || Import.ImportedPresets.IsEmpty()) { Model->Status = FString::Join(Import.Errors, TEXT("; ")); return FReply::Handled(); }
			NTEBuildTool::Character::FNteCharacterModSpec Projection; Projection.MainMeshPath = MainMeshAsset->PackagePath; Projection.MainPostProcessAnimBlueprintPath = MainMesh->GetPostProcessAnimBlueprint() ? MainMesh->GetPostProcessAnimBlueprint()->GetPathName() : FString(); Projection.KawaiiPresets.Add(Import.ImportedPresets[0]);
			Recipe.Id = UniqueRecipeId(Model->Project, TEXT("kawaii_"), Import.ImportedPresets[0].Id); Recipe.Type = TEXT("KawaiiPostProcess"); Recipe.Deltas = FirstObjectField(NTEBuildTool::Character::CharacterModSpecToJson(Projection), TEXT("KawaiiPresets"));
		}
		if (Recipe.Deltas.IsValid()) { Model->Project.Recipes.Add(MoveTemp(Recipe)); Model->bDirty = true; Model->Status = TEXT("Recipe added. Save the project, then Apply when ready."); RefreshAll(); }
		return FReply::Handled();
	}

	FReply OnApplyRecipes(const FString RecipeType)
	{
		TArray<FString> RecipeIds;
		for (const NTEBuildTool::Project::FNteAuthoringRecipe& Recipe : Model->Project.Recipes)
		{
			const bool bMatches = RecipeType.Equals(TEXT("Kawaii")) ? Recipe.Type.StartsWith(TEXT("Kawaii")) : Recipe.Type.Equals(RecipeType);
			if (bMatches && Recipe.bEnabled) RecipeIds.Add(Recipe.Id);
		}
		return ApplyRecipeIds(RecipeIds);
	}

	FReply ApplyRecipeIds(const TArray<FString>& RecipeIds)
	{
		NTEBuildTool::Recipes::FNtePakmodRecipeApplyResult Result;
		NTEBuildTool::Recipes::ApplyPakmodRecipes(Model->Project, RecipeIds, Result);
		Model->Diagnostics.Reset(); AppendResultMessages(Result, Model->Diagnostics);
		Model->Status = Result.HasErrors() ? FString::Join(Result.Errors, TEXT("; ")) : TEXT("Apply Changes completed. Build remains a separate action.");
		Model->bDirty = true; RefreshAll(); return FReply::Handled();
	}

	FReply SyncRecipe(const FString RecipeId)
	{
		NTEBuildTool::Recipes::FNtePakmodRecipeApplyResult Result; NTEBuildTool::Recipes::SyncPakmodKawaiiRecipe(Model->Project, RecipeId, Result);
		Model->Diagnostics.Reset(); AppendResultMessages(Result, Model->Diagnostics); Model->Status = Result.HasErrors() ? FString::Join(Result.Errors, TEXT("; ")) : TEXT("Recipe synchronized from generated assets."); Model->bDirty = true; RefreshAll(); return FReply::Handled();
	}

	FReply DetachRecipe(const FString RecipeId)
	{
		NTEBuildTool::Recipes::FNtePakmodRecipeApplyResult Result; NTEBuildTool::Recipes::DetachPakmodRecipeOutputs(Model->Project, RecipeId, Result);
		Model->Status = Result.HasErrors() ? FString::Join(Result.Errors, TEXT("; ")) : TEXT("Recipe outputs detached; assets were not deleted."); Model->bDirty = true; RefreshAll(); return FReply::Handled();
	}

	FReply OnRefreshPlan()
	{
		PullProjectFields(); FString Error; if (!Model->RefreshPackagePlan(Error)) Model->Status = Error; RefreshPackageRows(); RefreshBuildRows(); return FReply::Handled();
	}

	FReply OnValidate()
	{
		PullProjectFields(); FString Error; Model->Status = Model->Validate(Error) ? TEXT("Pakmod Project validation passed.") : Error; return FReply::Handled();
	}

	FReply OnBuild(const bool bLaunch)
	{
		PullProjectFields();
		FString Error;
		if (!Model->RefreshPackagePlan(Error)) { Model->Status = Error; RefreshAll(); return FReply::Handled(); }
		NTEBuildTool::Package::FNteModPackageJobCreateResult CreateResult;
		if (!NTEBuildTool::Package::CreateModPackageJobFromManifest(Model->Project, Model->PackagePlan, CreateResult, Error)) { Model->Status = Error; RefreshAll(); return FReply::Handled(); }
		if (!bLaunch) Model->Status = FString::Printf(TEXT("Created package job: %s"), *CreateResult.JobFile);
		else
		{
			NTEBuildTool::Package::FNteModPackageLaunchResult LaunchResult;
			Model->Status = NTEBuildTool::Package::LaunchModPackageBuildJob(CreateResult.JobFile, LaunchResult, Error) ? FString::Printf(TEXT("Package build finished from job: %s"), *LaunchResult.JobFile) : Error;
		}
		RefreshAll(); return FReply::Handled();
	}
};

TSharedRef<SDockTab> SpawnPakmodProjectTab(const FSpawnTabArgs&)
{
	return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SNtePakmodProjectEditor)];
}
}

TSharedRef<SWidget> MakePakmodProjectEditorWidget()
{
	return SNew(SNtePakmodProjectEditor);
}

void OpenPakmodProjectEditorTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(PakmodProjectTabName);
}

void RegisterPakmodProjectEditorTab()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PakmodProjectTabName, FOnSpawnTab::CreateStatic(&SpawnPakmodProjectTab))
		.SetDisplayName(LOCTEXT("PakmodProjectTab", "NTE Pakmod Project"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void UnregisterPakmodProjectEditorTab()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PakmodProjectTabName);
}
}

#undef LOCTEXT_NAMESPACE
