// Copyright (c) 2026 NTEBuildTool contributors.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "NteModPackagePlan.h"
#include "NteCharacterModSpec.h"
#include "NteCharacterModSpecMigration.h"
#include "NtePakmodProject.h"

namespace NTEBuildTool::Tests
{
namespace
{
Project::FNtePakmodProject MakeProject()
{
	Project::FNtePakmodProject Result;
	Result.Project.Id = TEXT("test_mod");
	Result.Project.DisplayName = TEXT("Test Mod");
	Result.PackageManifest.ModName = TEXT("test_mod");

	Project::FNteSourceReference Source;
	Source.Id = TEXT("game_material");
	Source.Kind = TEXT("FModelMaterial");
	Source.GamePackagePath = TEXT("/Game/Test/MI_Source");
	Source.Locator = TEXT("Exports/HT/Content/Test/MI_Source.json");
	Result.Sources.Add(Source);

	Project::FNteAssetReference Replacement;
	Replacement.Id = TEXT("main_mesh");
	Replacement.PackagePath = TEXT("/Game/Test/SK_Main");
	Replacement.ClassName = TEXT("SkeletalMesh");
	Replacement.Origin = Project::ENteAssetOrigin::UserImported;
	Replacement.Intent = Project::ENteAssetIntent::ReplacementAsset;
	Result.Assets.Add(Replacement);

	Project::FNteAssetReference Generated;
	Generated.Id = TEXT("generated_mi");
	Generated.PackagePath = TEXT("/Game/Test/mod/MI_Generated");
	Generated.ClassName = TEXT("MaterialInstanceConstant");
	Generated.Origin = Project::ENteAssetOrigin::ToolGenerated;
	Generated.Intent = Project::ENteAssetIntent::AddedAsset;
	Generated.OwnerRecipeId = TEXT("material_recipe");
	Result.Assets.Add(Generated);

	Project::FNteAssetReference External;
	External.Id = TEXT("source_mi");
	External.PackagePath = TEXT("/Game/Test/MI_Source");
	External.ClassName = TEXT("MaterialInstanceConstant");
	External.Origin = Project::ENteAssetOrigin::GameReference;
	External.Intent = Project::ENteAssetIntent::ExternalReference;
	Result.Assets.Add(External);

	Project::FNteAuthoringRecipe Recipe;
	Recipe.Id = TEXT("material_recipe");
	Recipe.Type = TEXT("MaterialInstance");
	Recipe.SourceIds.Add(TEXT("game_material"));
	Recipe.TargetAssetIds.Add(TEXT("main_mesh"));
	Recipe.OutputAssetIds.Add(TEXT("generated_mi"));
	Recipe.Deltas->SetStringField(TEXT("SlotName"), TEXT("body"));
	Result.Recipes.Add(Recipe);

	Result.PackageManifest.AssetIds = { TEXT("main_mesh"), TEXT("generated_mi") };
	return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNtePakmodProjectRoundTripTest,
	"NTEBuildTool.PakmodProject.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNtePakmodProjectRoundTripTest::RunTest(const FString& Parameters)
{
	const Project::FNtePakmodProject Source = MakeProject();
	const TSharedRef<FJsonObject> Json = Project::PakmodProjectToJson(Source);
	Project::FNtePakmodProject Loaded;
	FString Error;
	TestTrue(TEXT("v1 project parses"), Project::PakmodProjectFromJson(*Json, Loaded, Error));
	TestEqual(TEXT("project id"), Loaded.Project.Id, Source.Project.Id);
	TestEqual(TEXT("source count"), Loaded.Sources.Num(), 1);
	TestEqual(TEXT("asset count"), Loaded.Assets.Num(), 3);
	TestEqual(TEXT("recipe count"), Loaded.Recipes.Num(), 1);
	TestEqual(TEXT("manifest count"), Loaded.PackageManifest.AssetIds.Num(), 2);
	TestEqual(TEXT("origin survives"), static_cast<int32>(Loaded.Assets[1].Origin), static_cast<int32>(Project::ENteAssetOrigin::ToolGenerated));
	TestEqual(TEXT("intent survives"), static_cast<int32>(Loaded.Assets[0].Intent), static_cast<int32>(Project::ENteAssetIntent::ReplacementAsset));
	TestEqual(TEXT("recipe delta survives"), Loaded.Recipes[0].Deltas->GetStringField(TEXT("SlotName")), FString(TEXT("body")));
	TestFalse(TEXT("round-tripped project validates"), Project::ValidatePakmodProject(Loaded).HasErrors());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNtePakmodProjectVersionTest,
	"NTEBuildTool.PakmodProject.RejectsUnknownVersion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNtePakmodProjectVersionTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FJsonObject> Json = Project::PakmodProjectToJson(MakeProject());
	Json->SetNumberField(TEXT("Version"), 2);
	Project::FNtePakmodProject Loaded;
	FString Error;
	TestFalse(TEXT("version 2 is rejected"), Project::PakmodProjectFromJson(*Json, Loaded, Error));
	TestTrue(TEXT("error mentions version"), Error.Contains(TEXT("version"), ESearchCase::IgnoreCase));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNtePackageManifestResolutionTest,
	"NTEBuildTool.Package.ManifestResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNtePackageManifestResolutionTest::RunTest(const FString& Parameters)
{
	Project::FNtePakmodProject Source = MakeProject();
	Source.PackageManifest.AssetIds.Add(TEXT("main_mesh"));
	Package::FNtePackageManifestResolution Resolution = Package::ResolvePackageManifest(Source);
	TestFalse(TEXT("valid manifest resolves"), Resolution.HasErrors());
	TestEqual(TEXT("duplicate asset id is deduplicated by package"), Resolution.PackageNames.Num(), 2);
	TestEqual(TEXT("deterministic first path"), Resolution.PackageNames[0], FString(TEXT("/Game/Test/SK_Main")));

	Source.PackageManifest.ExplicitExclusions.Add(TEXT("/Game/Test/mod/MI_Generated"));
	Resolution = Package::ResolvePackageManifest(Source);
	TestEqual(TEXT("explicit exclusion removes package"), Resolution.PackageNames.Num(), 1);
	TestTrue(TEXT("explicit exclusion is reported"), Resolution.Warnings.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNtePackageManifestExternalReferenceTest,
	"NTEBuildTool.Package.RejectsExternalReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNtePackageManifestExternalReferenceTest::RunTest(const FString& Parameters)
{
	Project::FNtePakmodProject Source = MakeProject();
	Source.PackageManifest.AssetIds.Add(TEXT("source_mi"));
	const Package::FNtePackageManifestResolution Resolution = Package::ResolvePackageManifest(Source);
	TestTrue(TEXT("external reference is rejected"), Resolution.HasErrors());
	TestFalse(TEXT("external package is excluded"), Resolution.PackageNames.Contains(TEXT("/Game/Test/MI_Source")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNteCharacterModSpecMigrationTest,
	"NTEBuildTool.PakmodProject.CharacterModSpecMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNteCharacterModSpecMigrationTest::RunTest(const FString& Parameters)
{
	Character::FNteCharacterModSpec Legacy;
	Legacy.WorkspaceName = TEXT("Legacy Test");
	Legacy.MainMeshPath = TEXT("/Game/Test/SK_Main");
	Legacy.MainAnimBlueprintPath = TEXT("/Game/Test/ABP_GameSource");
	Legacy.MainPostProcessAnimBlueprintPath = TEXT("/Game/Test/mod/Runtime/ABP_Post");
	Legacy.Package.ModName = TEXT("legacy_test_P");

	Character::FNteCharacterPackageAssetSpec External;
	External.AssetPath = TEXT("/Game/Test/MI_GameSource");
	External.Intent = Character::ENteCharacterPackageAssetIntent::ExternalReference;
	Legacy.Package.Assets.Add(External);

	Character::FNteCharacterPackageAssetSpec Generated;
	Generated.AssetPath = TEXT("/Game/Test/mod/Runtime/WBP_Runtime");
	Generated.Intent = Character::ENteCharacterPackageAssetIntent::GeneratedAsset;
	Legacy.Package.Assets.Add(Generated);

	Character::FNteCharacterMaterialOperationSpec Material;
	Material.Id = TEXT("body");
	Material.TargetMeshId = TEXT("main");
	Material.OutputMaterialPath = TEXT("/Game/Test/mod/Materials/MI_Body");
	Material.ParentMaterialPath = TEXT("/Game/Test/MI_GameSource");
	Material.bAssignToSlot = false;
	Legacy.MaterialOperations.Add(Material);

	Legacy.RuntimeUi.bEnableUi = true;
	Legacy.RuntimeUi.ToggleUiHotkey = TEXT("Slash");

	Character::FNteCharacterKawaiiPresetSpec Kawaii;
	Kawaii.Id = TEXT("skirt");
	Kawaii.TargetMeshId = TEXT("main");
	Kawaii.RootBone = TEXT("Bn_l_qunB_002_001");
	Kawaii.RuntimeAnimBlueprintPath = TEXT("/Game/Test/mod/Runtime/ABP_Post");
	Legacy.KawaiiPresets.Add(Kawaii);

	const Character::FNteCharacterModSpecMigrationResult Migration =
		Character::MigrateCharacterModSpecToPakmodProject(Legacy);
	TestFalse(TEXT("migration succeeds"), Migration.HasErrors());

	const Project::FNteAssetReference* GeneratedAsset = Migration.Project.Assets.FindByPredicate([](const Project::FNteAssetReference& Asset)
	{
		return Asset.PackagePath == TEXT("/Game/Test/mod/Runtime/WBP_Runtime");
	});
	TestNotNull(TEXT("legacy generated asset is retained"), GeneratedAsset);
	if (GeneratedAsset)
	{
		TestEqual(TEXT("generated origin"), static_cast<int32>(GeneratedAsset->Origin), static_cast<int32>(Project::ENteAssetOrigin::ToolGenerated));
		TestEqual(TEXT("generated intent"), static_cast<int32>(GeneratedAsset->Intent), static_cast<int32>(Project::ENteAssetIntent::AddedAsset));
		TestTrue(TEXT("generated asset enters manifest"), Migration.Project.PackageManifest.AssetIds.Contains(GeneratedAsset->Id));
	}

	const Project::FNteAssetReference* ExternalAsset = Migration.Project.Assets.FindByPredicate([](const Project::FNteAssetReference& Asset)
	{
		return Asset.PackagePath == TEXT("/Game/Test/MI_GameSource");
	});
	TestNotNull(TEXT("external asset is retained as evidence"), ExternalAsset);
	if (ExternalAsset)
	{
		TestEqual(TEXT("external intent"), static_cast<int32>(ExternalAsset->Intent), static_cast<int32>(Project::ENteAssetIntent::ExternalReference));
		TestFalse(TEXT("external asset is not packaged"), Migration.Project.PackageManifest.AssetIds.Contains(ExternalAsset->Id));
	}

	int32 MaterialRecipes = 0;
	int32 RuntimeRecipes = 0;
	int32 KawaiiRecipes = 0;
	for (const Project::FNteAuthoringRecipe& Recipe : Migration.Project.Recipes)
	{
		MaterialRecipes += Recipe.Type == TEXT("MaterialInstance") ? 1 : 0;
		RuntimeRecipes += Recipe.Type == TEXT("RuntimeActions") ? 1 : 0;
		KawaiiRecipes += Recipe.Type.StartsWith(TEXT("Kawaii")) ? 1 : 0;
	}
	TestEqual(TEXT("material recipe count"), MaterialRecipes, 1);
	TestEqual(TEXT("runtime recipe count"), RuntimeRecipes, 1);
	TestEqual(TEXT("kawaii recipe count"), KawaiiRecipes, 1);
	return true;
}
}

#endif
