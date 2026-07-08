// Copyright (c) 2026 NTEBuildTool contributors.

#include "NtePakModAuditCommandlet.h"

#include "NTEBuildTool.h"
#include "NteEditorAssetUtils.h"
#include "NteJsonFileUtils.h"
#include "NteMaterialInstanceTool.h"
#include "NteMeshToggleConfig.h"
#include "NteModPackageJob.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "StaticParameterSet.h"

namespace
{
using namespace NTEBuildTool::Editor;
using namespace NTEBuildTool::Json;

struct FNtePakModAuditContext
{
	TSet<FString> JobPackages;
	TArray<FString> NeverPackPackagePrefixes;
	TArray<FString> DependencyPackagePrefixes;
	TArray<FString> IgnoreDependencyPackagePrefixes = { TEXT("/Game/CoreMaterials"), TEXT("/Engine") };
	TArray<FString> RequiredPackages;
	TArray<TSharedPtr<FJsonValue>> Findings;
	bool bAuditSourceTextureDependencies = false;
	int32 ErrorCount = 0;
	int32 WarningCount = 0;
};

void AddFinding(FNtePakModAuditContext& Context, const FString& Severity, const FString& Area, const FString& Message, const FString& Subject = FString())
{
	const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
	Entry->SetStringField(TEXT("Severity"), Severity);
	Entry->SetStringField(TEXT("Area"), Area);
	Entry->SetStringField(TEXT("Message"), Message);
	if (!Subject.IsEmpty())
	{
		Entry->SetStringField(TEXT("Subject"), Subject);
	}
	Context.Findings.Add(MakeShared<FJsonValueObject>(Entry));

	if (Severity == TEXT("Error"))
	{
		++Context.ErrorCount;
	}
	else if (Severity == TEXT("Warning"))
	{
		++Context.WarningCount;
	}
}

bool StartsWithAnyPrefix(const FString& Value, const TArray<FString>& Prefixes)
{
	for (const FString& Prefix : Prefixes)
	{
		if (!Prefix.IsEmpty() && Value.StartsWith(Prefix))
		{
			return true;
		}
	}
	return false;
}

bool ShouldAuditDependencyPackage(const FNtePakModAuditContext& Context, const FString& PackageName)
{
	if (!PackageName.StartsWith(TEXT("/Game/")))
	{
		return false;
	}
	if (StartsWithAnyPrefix(PackageName, Context.IgnoreDependencyPackagePrefixes))
	{
		return false;
	}
	return Context.DependencyPackagePrefixes.IsEmpty() || StartsWithAnyPrefix(PackageName, Context.DependencyPackagePrefixes);
}

TArray<TSharedPtr<FJsonValue>> StringSetToJsonValues(const TSet<FString>& Values)
{
	TArray<FString> SortedValues = Values.Array();
	SortedValues.Sort();
	return StringArrayToJsonValues(SortedValues);
}

FString BoolToString(const bool bValue)
{
	return bValue ? TEXT("true") : TEXT("false");
}

TSet<FString> CollectObjectStringKeys(const FJsonObject* Object)
{
	TSet<FString> Keys;
	if (!Object)
	{
		return Keys;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Object->Values)
	{
		Keys.Add(Entry.Key);
	}
	return Keys;
}

TSet<FString> CollectSourceTexturePackages(const FJsonObject* SourceTextures)
{
	TSet<FString> Packages;
	for (const NTEBuildTool::Material::FNteMaterialSourceTextureUsage& Usage : NTEBuildTool::Material::BuildSourceTextureUsage(SourceTextures))
	{
		if (Usage.SourceTexturePath.StartsWith(TEXT("/Game/")))
		{
			Packages.Add(Usage.SourceTexturePath);
		}
	}
	return Packages;
}

TSet<FString> CollectStaticSwitchNames(const UMaterialInstanceConstant& MaterialInstance)
{
	TSet<FString> Names;
	const FStaticParameterSet StaticParameters = MaterialInstance.GetStaticParameters();
	for (const FStaticSwitchParameter& StaticSwitchParameter : StaticParameters.StaticSwitchParameters)
	{
		Names.Add(StaticSwitchParameter.ParameterInfo.Name.ToString());
	}
	return Names;
}

FString MakeBlueprintPackageFilename(const FString& BlueprintPath)
{
	FString PackageFilename;
	FPackageName::TryConvertLongPackageNameToFilename(BlueprintPath, PackageFilename, FPackageName::GetAssetPackageExtension());
	FPaths::NormalizeFilename(PackageFilename);
	return PackageFilename;
}

bool BlueprintPackageContainsAscii(const FString& BlueprintPath, const ANSICHAR* Needle)
{
	const FString PackageFilename = MakeBlueprintPackageFilename(BlueprintPath);
	if (PackageFilename.IsEmpty())
	{
		return false;
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *PackageFilename))
	{
		return false;
	}

	const int32 NeedleLen = FCStringAnsi::Strlen(Needle);
	if (NeedleLen <= 0 || Bytes.Num() < NeedleLen)
	{
		return false;
	}

	for (int32 Index = 0; Index <= Bytes.Num() - NeedleLen; ++Index)
	{
		if (FMemory::Memcmp(Bytes.GetData() + Index, Needle, NeedleLen) == 0)
		{
			return true;
		}
	}
	return false;
}

bool BlueprintPackageContainsString(const FString& BlueprintPath, const FString& Needle)
{
	return !Needle.IsEmpty() && BlueprintPackageContainsAscii(BlueprintPath, TCHAR_TO_UTF8(*Needle));
}

bool IsPlaceholderImportedMaterial(const UMaterialInterface& Material)
{
	const UMaterial* BaseMaterial = Cast<UMaterial>(&Material);
	if (!BaseMaterial)
	{
		return false;
	}

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(BaseMaterial->GetPackage()->GetName(), FPackageName::GetAssetPackageExtension());
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *PackageFilename))
	{
		return false;
	}

	const auto ContainsAscii = [&Bytes](const ANSICHAR* Needle)
	{
		const int32 NeedleLen = FCStringAnsi::Strlen(Needle);
		if (NeedleLen <= 0 || Bytes.Num() < NeedleLen)
		{
			return false;
		}

		for (int32 Index = 0; Index <= Bytes.Num() - NeedleLen; ++Index)
		{
			if (FMemory::Memcmp(Bytes.GetData() + Index, Needle, NeedleLen) == 0)
			{
				return true;
			}
		}
		return false;
	};

	return ContainsAscii("InterchangeAssetImportData") || ContainsAscii("MF_PhongToMetalRoughness");
}

TArray<TSharedPtr<FJsonValue>> AuditPackageJob(const FString& JobFilename, FNtePakModAuditContext& Context)
{
	TArray<TSharedPtr<FJsonValue>> Results;
	NTEBuildTool::Package::FNteModPackageJob Job;
	FString Error;
	if (!NTEBuildTool::Package::LoadModPackageJobJson(JobFilename, Job, Error))
	{
		AddFinding(Context, TEXT("Error"), TEXT("PackageJob"), Error, JobFilename);
		return Results;
	}

	const TSharedRef<FJsonObject> JobObject = MakeShared<FJsonObject>();
	JobObject->SetStringField(TEXT("JobFile"), JobFilename);
	JobObject->SetStringField(TEXT("ModName"), Job.ModName);
	JobObject->SetStringField(TEXT("ModsDir"), Job.ModsDir);
	JobObject->SetNumberField(TEXT("PackageCount"), Job.Packages.Num());

	for (const FString& Prefix : Job.NeverPackPackagePrefixes)
	{
		Context.NeverPackPackagePrefixes.AddUnique(Prefix);
	}

	TArray<TSharedPtr<FJsonValue>> Packages;
	for (FString PackageName : Job.Packages)
	{
		PackageName = NormalizeAssetPathForText(PackageName);
		Context.JobPackages.Add(PackageName);

		const TSharedRef<FJsonObject> PackageObject = MakeShared<FJsonObject>();
		PackageObject->SetStringField(TEXT("Package"), PackageName);
		const bool bValidPackageName = PackageName.StartsWith(TEXT("/Game/")) && !PackageName.Contains(TEXT("."));
		PackageObject->SetBoolField(TEXT("ValidPackageName"), bValidPackageName);
		if (!bValidPackageName)
		{
			AddFinding(Context, TEXT("Error"), TEXT("PackageJob"), TEXT("Package must be a /Game package path without object suffix."), PackageName);
		}

		const bool bLoads = LoadAnyAssetByPath(PackageName) != nullptr;
		PackageObject->SetBoolField(TEXT("Loads"), bLoads);
		if (!bLoads)
		{
			AddFinding(Context, TEXT("Error"), TEXT("PackageJob"), TEXT("Package listed in job does not load in the mirror project."), PackageName);
		}

		if (StartsWithAnyPrefix(PackageName, Job.NeverPackPackagePrefixes))
		{
			AddFinding(Context, TEXT("Error"), TEXT("PackageJob"), TEXT("Package matches a NeverPackPackagePrefixes entry."), PackageName);
		}

		Packages.Add(MakeShared<FJsonValueObject>(PackageObject));
	}
	JobObject->SetArrayField(TEXT("Packages"), Packages);
	Results.Add(MakeShared<FJsonValueObject>(JobObject));
	return Results;
}

TArray<TSharedPtr<FJsonValue>> AuditMaterialRecipe(const FString& RecipeFilename, FNtePakModAuditContext& Context)
{
	TArray<TSharedPtr<FJsonValue>> Results;
	TSharedPtr<FJsonObject> Recipe;
	FString Error;
	if (!LoadJsonObjectFromFile(RecipeFilename, Recipe, Error))
	{
		AddFinding(Context, TEXT("Error"), TEXT("Material"), Error, RecipeFilename);
		return Results;
	}

	const FString SourceMaterialJson = GetStringAny(*Recipe, TEXT("SourceMaterialJson"), TEXT("sourceMaterialJson"), TEXT("FModelMaterialJson"));
	const FString OutputMaterialPath = NormalizeAssetPathForText(GetStringAny(*Recipe, TEXT("OutputMaterial"), TEXT("outputMaterial")));
	const FString ExplicitParentMaterialPath = NormalizeAssetPathForText(GetStringAny(*Recipe, TEXT("ParentMaterial"), TEXT("parentMaterial")));
	const FString DerivedParentMaterialPath = NTEBuildTool::Material::DeriveParentMaterialPathFromFModelJson(SourceMaterialJson);
	const FString EffectiveParentMaterialPath = !ExplicitParentMaterialPath.IsEmpty() ? ExplicitParentMaterialPath : DerivedParentMaterialPath;
	bool bCopySourceParameters = false;
	bool bCopySourceTextures = false;
	bool bAllowStaticSwitchOverrides = false;
	GetBoolAny(*Recipe, bCopySourceParameters, TEXT("CopySourceParameters"), TEXT("copySourceParameters"));
	GetBoolAny(*Recipe, bCopySourceTextures, TEXT("CopySourceTextures"), TEXT("copySourceTextures"));
	GetBoolAny(*Recipe, bAllowStaticSwitchOverrides, TEXT("AllowStaticSwitchOverrides"), TEXT("allowStaticSwitchOverrides"));

	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("RecipeFile"), RecipeFilename);
	Result->SetStringField(TEXT("SourceMaterialJson"), SourceMaterialJson);
	Result->SetStringField(TEXT("OutputMaterial"), OutputMaterialPath);
	Result->SetStringField(TEXT("ExplicitParentMaterial"), ExplicitParentMaterialPath);
	Result->SetStringField(TEXT("DerivedParentMaterial"), DerivedParentMaterialPath);
	Result->SetStringField(TEXT("EffectiveParentMaterial"), EffectiveParentMaterialPath);
	Result->SetBoolField(TEXT("CopySourceParameters"), bCopySourceParameters);
	Result->SetBoolField(TEXT("CopySourceTextures"), bCopySourceTextures);
	Result->SetBoolField(TEXT("AllowStaticSwitchOverrides"), bAllowStaticSwitchOverrides);

	if (!ExplicitParentMaterialPath.IsEmpty() && !DerivedParentMaterialPath.IsEmpty() && ExplicitParentMaterialPath != DerivedParentMaterialPath)
	{
		AddFinding(
			Context,
			TEXT("Warning"),
			TEXT("Material"),
			FString::Printf(TEXT("ParentMaterial differs from the source-derived FModel path. This can bypass the original MaterialInstance chain. Explicit=%s Derived=%s"), *ExplicitParentMaterialPath, *DerivedParentMaterialPath),
			RecipeFilename);
	}

	if (!EffectiveParentMaterialPath.IsEmpty() && Context.JobPackages.Contains(EffectiveParentMaterialPath))
	{
		AddFinding(
			Context,
			TEXT("Error"),
			TEXT("Material"),
			TEXT("Recipe parent material is listed in a package job. Pak-only material overrides should reference the base-game parent but must not pack the placeholder parent asset."),
			EffectiveParentMaterialPath);
	}

	if (!EffectiveParentMaterialPath.IsEmpty() && StartsWithAnyPrefix(EffectiveParentMaterialPath, Context.NeverPackPackagePrefixes))
	{
		Result->SetBoolField(TEXT("ParentProtectedByNeverPack"), true);
	}

	TSharedPtr<FJsonObject> SourceMaterial;
	if (!SourceMaterialJson.IsEmpty())
	{
		if (!LoadJsonObjectFromFile(SourceMaterialJson, SourceMaterial, Error))
		{
			AddFinding(Context, TEXT("Error"), TEXT("Material"), Error, SourceMaterialJson);
		}
	}

	const FJsonObject* SourceTextures = NTEBuildTool::Material::FindSourceMaterialParameterObject(SourceMaterial.Get(), TEXT("Textures"));
	const FJsonObject* SourceSwitches = NTEBuildTool::Material::FindSourceMaterialParameterObject(SourceMaterial.Get(), TEXT("Switches"));
	const TSet<FString> SourceSwitchNames = CollectObjectStringKeys(SourceSwitches);
	const TSet<FString> SourceTexturePackages = CollectSourceTexturePackages(SourceTextures);
	Result->SetNumberField(TEXT("SourceTextureDependencyCount"), SourceTexturePackages.Num());
	Result->SetNumberField(TEXT("SourceStaticSwitchCount"), SourceSwitchNames.Num());

	if (!SourceSwitchNames.IsEmpty() && bCopySourceParameters && !bAllowStaticSwitchOverrides)
	{
		AddFinding(
			Context,
			TEXT("Warning"),
			TEXT("Material"),
			TEXT("Source material has static switches but the recipe does not allow static switch overrides; dye/ramp/material feature branches may not match the source."),
			RecipeFilename);
	}

	TArray<TSharedPtr<FJsonValue>> DependencyObjects;
	if (Context.bAuditSourceTextureDependencies)
	{
		for (const FString& DependencyPackage : SourceTexturePackages)
		{
			if (!ShouldAuditDependencyPackage(Context, DependencyPackage))
			{
				continue;
			}

			const TSharedRef<FJsonObject> DependencyObject = MakeShared<FJsonObject>();
			DependencyObject->SetStringField(TEXT("Package"), DependencyPackage);
			DependencyObject->SetBoolField(TEXT("Loads"), LoadAnyAssetByPath(DependencyPackage) != nullptr);
			DependencyObject->SetBoolField(TEXT("InPackageJob"), Context.JobPackages.Contains(DependencyPackage));
			DependencyObject->SetBoolField(TEXT("MatchesNeverPackPrefix"), StartsWithAnyPrefix(DependencyPackage, Context.NeverPackPackagePrefixes));
			DependencyObjects.Add(MakeShared<FJsonValueObject>(DependencyObject));

			if (!Context.JobPackages.Contains(DependencyPackage))
			{
				AddFinding(
					Context,
					TEXT("Warning"),
					TEXT("Material"),
					TEXT("Source texture dependency is not explicitly packaged. This is only safe when the unchanged base-game asset will be used at runtime."),
					DependencyPackage);
			}
		}
	}
	Result->SetArrayField(TEXT("AuditedSourceTextureDependencies"), DependencyObjects);

	if (!OutputMaterialPath.IsEmpty())
	{
		UMaterialInstanceConstant* MaterialInstance = LoadAssetByPath<UMaterialInstanceConstant>(OutputMaterialPath);
		Result->SetBoolField(TEXT("OutputMaterialLoads"), MaterialInstance != nullptr);
		if (!MaterialInstance)
		{
			AddFinding(Context, TEXT("Error"), TEXT("Material"), TEXT("Output material instance does not load."), OutputMaterialPath);
		}
		else
		{
			const FString ActualParent = MaterialInstance->Parent ? MaterialInstance->Parent->GetPackage()->GetName() : FString();
			Result->SetStringField(TEXT("ActualParentMaterial"), ActualParent);
			Result->SetStringField(TEXT("ActualParentClass"), MaterialInstance->Parent ? MaterialInstance->Parent->GetClass()->GetName() : FString());
			if (!EffectiveParentMaterialPath.IsEmpty() && ActualParent != EffectiveParentMaterialPath)
			{
				AddFinding(
					Context,
					TEXT("Error"),
					TEXT("Material"),
					FString::Printf(TEXT("Output material parent does not match the recipe. Expected=%s Actual=%s"), *EffectiveParentMaterialPath, *ActualParent),
					OutputMaterialPath);
			}
			if (MaterialInstance->Parent && IsPlaceholderImportedMaterial(*MaterialInstance->Parent))
			{
				AddFinding(
					Context,
					TEXT("Error"),
					TEXT("Material"),
					TEXT("Output material parent is an imported placeholder material. It will not consume the original Toon parameter set; use the source-derived FModel parent path and keep that parent out of the package job."),
					ActualParent);
			}

			const TSet<FString> ActualSwitchNames = CollectStaticSwitchNames(*MaterialInstance);
			TArray<FString> MissingSwitchNames;
			if (bCopySourceParameters && bAllowStaticSwitchOverrides)
			{
				for (const FString& SourceSwitchName : SourceSwitchNames)
				{
					if (!ActualSwitchNames.Contains(SourceSwitchName))
					{
						MissingSwitchNames.Add(SourceSwitchName);
					}
				}
			}
			MissingSwitchNames.Sort();
			Result->SetArrayField(TEXT("MissingSourceStaticSwitchOverrides"), StringArrayToJsonValues(MissingSwitchNames));
			if (!MissingSwitchNames.IsEmpty())
			{
				AddFinding(
					Context,
					TEXT("Warning"),
				TEXT("Material"),
				TEXT("Output material instance does not override every source static switch. This is risky for dye/ramp/material feature compatibility."),
				OutputMaterialPath);
			}

			if (!bCopySourceParameters)
			{
				if (MaterialInstance->ScalarParameterValues.Num() > 0 || MaterialInstance->VectorParameterValues.Num() > 0 || ActualSwitchNames.Num() > 0)
				{
					AddFinding(
						Context,
						TEXT("Warning"),
						TEXT("Material"),
						TEXT("Recipe is configured to inherit source parameters, but the output material still contains scalar/vector/static overrides. Re-run the recipe with ResetForPakTextureOnly=true to keep only explicit texture overrides."),
						OutputMaterialPath);
				}
			}
		}
	}

	Results.Add(MakeShared<FJsonValueObject>(Result));
	return Results;
}

void AuditRequiredPackages(FNtePakModAuditContext& Context)
{
	for (FString RequiredPackage : Context.RequiredPackages)
	{
		RequiredPackage = NormalizeAssetPathForText(RequiredPackage);
		if (!Context.JobPackages.Contains(RequiredPackage))
		{
			AddFinding(Context, TEXT("Error"), TEXT("PackageJob"), TEXT("Required package is missing from the package job set."), RequiredPackage);
		}
		if (StartsWithAnyPrefix(RequiredPackage, Context.NeverPackPackagePrefixes))
		{
			AddFinding(Context, TEXT("Error"), TEXT("PackageJob"), TEXT("Required package matches a NeverPackPackagePrefixes entry."), RequiredPackage);
		}
	}
}

TArray<TSharedPtr<FJsonValue>> AuditToggleSetup(const FString& SetupFilename, FNtePakModAuditContext& Context)
{
	TArray<TSharedPtr<FJsonValue>> Results;
	NTEBuildTool::Toggle::FNteMeshToggleSetupOptions Options;
	FString Error;
	if (!NTEBuildTool::Toggle::LoadMeshToggleSetupOptionsFromJsonFile(SetupFilename, Options, Error))
	{
		AddFinding(Context, TEXT("Error"), TEXT("Toggle"), Error, SetupFilename);
		return Results;
	}

	const FString TargetMeshPath = !Options.TargetMeshPath.IsEmpty() ? Options.TargetMeshPath : Options.MeshPath;
	const FString RuntimeAnchorMeshPath = !Options.RuntimeAnchorMeshPath.IsEmpty() ? Options.RuntimeAnchorMeshPath : TargetMeshPath;
	const FString PostProcessPath = JoinAssetPath(Options.OutputFolder, Options.PostProcessAnimBlueprintName);
	const FString ControllerPath = Options.ControllerBlueprintName.IsEmpty() ? FString() : JoinAssetPath(Options.OutputFolder, Options.ControllerBlueprintName);
	const FString WidgetPath = JoinAssetPath(Options.OutputFolder, Options.WidgetBlueprintName);
	const FString SaveGamePath = JoinAssetPath(Options.OutputFolder, Options.SaveGameBlueprintName);

	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("SetupFile"), SetupFilename);
	Result->SetStringField(TEXT("TargetMesh"), TargetMeshPath);
	Result->SetStringField(TEXT("RuntimeAnchorMesh"), RuntimeAnchorMeshPath);
	Result->SetStringField(TEXT("RuntimeMode"), Options.RuntimeMode);
	Result->SetStringField(TEXT("StaticMeshVisibilityAdapter"), Options.StaticMeshVisibilityAdapter);
	Result->SetStringField(TEXT("HiddenMaterial"), Options.HiddenMaterialPath);
	Result->SetStringField(TEXT("PostProcessAnimBlueprint"), PostProcessPath);
	Result->SetStringField(TEXT("ControllerBlueprint"), ControllerPath);
	Result->SetStringField(TEXT("WidgetBlueprint"), WidgetPath);
	Result->SetStringField(TEXT("SaveGameBlueprint"), SaveGamePath);

	UObject* TargetMesh = LoadAnyAssetByPath(TargetMeshPath);
	UObject* RuntimeAnchorMesh = LoadAnyAssetByPath(RuntimeAnchorMeshPath);
	Result->SetBoolField(TEXT("TargetMeshLoads"), TargetMesh != nullptr);
	Result->SetStringField(TEXT("TargetMeshClass"), TargetMesh ? TargetMesh->GetClass()->GetName() : FString());
	Result->SetBoolField(TEXT("RuntimeAnchorMeshLoads"), RuntimeAnchorMesh != nullptr);
	Result->SetStringField(TEXT("RuntimeAnchorMeshClass"), RuntimeAnchorMesh ? RuntimeAnchorMesh->GetClass()->GetName() : FString());
	if (!TargetMesh)
	{
		AddFinding(Context, TEXT("Error"), TEXT("Toggle"), TEXT("TargetMesh does not load."), TargetMeshPath);
	}
	if (!RuntimeAnchorMesh)
	{
		AddFinding(Context, TEXT("Error"), TEXT("Toggle"), TEXT("RuntimeAnchorMesh does not load."), RuntimeAnchorMeshPath);
	}

	USkeletalMesh* AnchorSkeletalMesh = Cast<USkeletalMesh>(RuntimeAnchorMesh);
	if (!AnchorSkeletalMesh)
	{
		AddFinding(Context, TEXT("Error"), TEXT("Toggle"), TEXT("RuntimeAnchorMesh must be a SkeletalMesh; Skeleton and StaticMesh assets cannot host a Post Process Anim Blueprint tick."), RuntimeAnchorMeshPath);
	}

	UAnimBlueprint* PostProcessAnimBlueprint = LoadAssetByPath<UAnimBlueprint>(PostProcessPath);
	UBlueprint* ControllerBlueprint = ControllerPath.IsEmpty() ? nullptr : LoadAssetByPath<UBlueprint>(ControllerPath);
	Result->SetBoolField(TEXT("PostProcessAnimBlueprintLoads"), PostProcessAnimBlueprint != nullptr);
	Result->SetBoolField(TEXT("ControllerBlueprintLoads"), ControllerBlueprint != nullptr);
	Result->SetBoolField(TEXT("WidgetBlueprintLoads"), LoadAnyAssetByPath(WidgetPath) != nullptr);
	Result->SetBoolField(TEXT("SaveGameBlueprintLoads"), LoadAnyAssetByPath(SaveGamePath) != nullptr);
	if (!PostProcessAnimBlueprint)
	{
		AddFinding(Context, TEXT("Error"), TEXT("Toggle"), TEXT("PostProcessAnimBlueprint does not load."), PostProcessPath);
	}

	const bool bThinAnchorMode = Options.RuntimeMode.Equals(TEXT("ThinAnchorController"), ESearchCase::IgnoreCase);
	const bool bStandardTemplateMode = Options.RuntimeMode.IsEmpty() || Options.RuntimeMode.Equals(TEXT("StandardPostProcessTemplate"), ESearchCase::IgnoreCase);
	if (bThinAnchorMode && !ControllerBlueprint)
	{
		AddFinding(Context, TEXT("Error"), TEXT("Toggle"), TEXT("RuntimeMode is ThinAnchorController but ControllerBlueprint is missing. StandardPostProcessTemplate assets are not a thin-controller runtime."), ControllerPath);
	}

	const bool bTargetIsStaticMesh = Cast<UStaticMesh>(TargetMesh) != nullptr;
	if (bTargetIsStaticMesh)
	{
		if (!bThinAnchorMode || !ControllerBlueprint)
		{
			AddFinding(Context, TEXT("Error"), TEXT("Toggle"), TEXT("TargetMesh is a StaticMesh. StandardPostProcessTemplate uses SkinnedMeshComponent::ShowMaterialSection, so a ThinAnchorController with StaticMeshVisibilityAdapter support is required."), TargetMeshPath);
		}
		if (Options.StaticMeshVisibilityAdapter.Equals(TEXT("MaterialSwap"), ESearchCase::IgnoreCase) && Options.HiddenMaterialPath.IsEmpty())
		{
			AddFinding(Context, TEXT("Error"), TEXT("Toggle"), TEXT("StaticMesh MaterialSwap adapter requires HiddenMaterial."), SetupFilename);
		}
	}

	if (AnchorSkeletalMesh && PostProcessAnimBlueprint && PostProcessAnimBlueprint->GeneratedClass)
	{
		const UClass* AssignedPostProcessClass = AnchorSkeletalMesh->GetPostProcessAnimBlueprint();
		Result->SetStringField(TEXT("AssignedPostProcessAnimBlueprintClass"), AssignedPostProcessClass ? AssignedPostProcessClass->GetPathName() : FString());
		Result->SetStringField(TEXT("ExpectedPostProcessAnimBlueprintClass"), PostProcessAnimBlueprint->GeneratedClass->GetPathName());
		if (AssignedPostProcessClass != PostProcessAnimBlueprint->GeneratedClass)
		{
			AddFinding(Context, TEXT("Error"), TEXT("Toggle"), TEXT("RuntimeAnchorMesh is not assigned to the configured PostProcessAnimBlueprint class."), RuntimeAnchorMeshPath);
		}
	}

	const bool bLooksStandardTemplateRuntime = BlueprintPackageContainsAscii(PostProcessPath, "IsInputKeyDown") && BlueprintPackageContainsAscii(PostProcessPath, "ShowMaterialSection");
	Result->SetBoolField(TEXT("LooksLikeStandardPostProcessTemplateRuntime"), bLooksStandardTemplateRuntime);
	if (bStandardTemplateMode && !bLooksStandardTemplateRuntime)
	{
		AddFinding(Context, TEXT("Warning"), TEXT("Toggle"), TEXT("RuntimeMode is StandardPostProcessTemplate, but the PostProcessAnimBlueprint does not contain the expected input/UI/ShowMaterialSection markers."), PostProcessPath);
	}
	if (bThinAnchorMode && bLooksStandardTemplateRuntime)
	{
		AddFinding(Context, TEXT("Warning"), TEXT("Toggle"), TEXT("RuntimeMode is ThinAnchorController, but the PostProcessAnimBlueprint still looks like a StandardPostProcessTemplate runtime."), PostProcessPath);
	}

	TArray<TSharedPtr<FJsonValue>> GroupObjects;
	for (const NTEBuildTool::Toggle::FNteMeshToggleGroup& Group : Options.ToggleGroups)
	{
		const TSharedRef<FJsonObject> GroupObject = MakeShared<FJsonObject>();
		GroupObject->SetStringField(TEXT("GroupId"), Group.GroupId);
		GroupObject->SetStringField(TEXT("Key"), Group.Chord.Key.IsValid() ? Group.Chord.Key.GetFName().ToString() : FString());
		GroupObject->SetStringField(TEXT("Label"), Group.Label);
		GroupObject->SetBoolField(TEXT("DefaultVisible"), Group.bDefaultVisible);
		TArray<TSharedPtr<FJsonValue>> SlotValues;
		for (const int32 SlotIndex : Group.Slots)
		{
			SlotValues.Add(MakeShared<FJsonValueNumber>(SlotIndex));
		}
		GroupObject->SetArrayField(TEXT("MaterialSlots"), SlotValues);

		if (Group.Chord.Key.IsValid() && PostProcessAnimBlueprint)
		{
			const FString KeyName = Group.Chord.Key.GetFName().ToString();
			const bool bKeyNamePresent = BlueprintPackageContainsString(PostProcessPath, KeyName);
			GroupObject->SetBoolField(TEXT("KeyNamePresentInPostProcessAsset"), bKeyNamePresent);
			if (bStandardTemplateMode && bLooksStandardTemplateRuntime && !bKeyNamePresent)
			{
				AddFinding(Context, TEXT("Warning"), TEXT("Toggle"), TEXT("Configured hotkey name was not found in the StandardPostProcessTemplate asset."), KeyName);
			}
		}
		else if (!Group.Chord.Key.IsValid())
		{
			AddFinding(Context, TEXT("Warning"), TEXT("Toggle"), TEXT("Toggle group has no configured hotkey. It can only be controlled through UI or external runtime logic."), Group.GroupId);
		}

		GroupObjects.Add(MakeShared<FJsonValueObject>(GroupObject));
	}
	Result->SetArrayField(TEXT("Groups"), GroupObjects);

	Results.Add(MakeShared<FJsonValueObject>(Result));
	return Results;
}

TArray<FString> GetConfigArrayPlusParam(const FJsonObject& Config, const TCHAR* FieldName, const FString& ParamValue)
{
	TArray<FString> Values = GetStringArrayAny(Config, FieldName);
	if (!ParamValue.IsEmpty())
	{
		Values.Add(ParamValue);
	}
	return Values;
}
}

UNtePakModAuditCommandlet::UNtePakModAuditCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
	UseCommandletResultAsExitCode = true;
	HelpDescription = TEXT("Audits NTE pakmod material, toggle, and package job readiness.");
	HelpUsage = TEXT("UnrealEditor-Cmd.exe <Project>.uproject -run=NtePakModAudit -Config=<json> [-Output=<json>]");
}

int32 UNtePakModAuditCommandlet::Main(const FString& Params)
{
	FString ConfigFilename;
	if (!FParse::Value(*Params, TEXT("Config="), ConfigFilename))
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Missing -Config=<json>."));
		return 1;
	}
	FPaths::NormalizeFilename(ConfigFilename);

	FString Error;
	TSharedPtr<FJsonObject> Config;
	if (!LoadJsonObjectFromFile(ConfigFilename, Config, Error))
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Audit config could not be read: %s"), *Error);
		return 2;
	}

	const FString Format = GetStringAny(*Config, TEXT("Format"), TEXT("format"));
	if (!Format.IsEmpty() && Format != TEXT("NTE.PakModAudit"))
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Unsupported audit config format: %s"), *Format);
		return 3;
	}

	FNtePakModAuditContext Context;
	Context.DependencyPackagePrefixes = GetStringArrayAny(*Config, TEXT("DependencyPackagePrefixes"), TEXT("dependencyPackagePrefixes"));
	const TArray<FString> IgnorePrefixes = GetStringArrayAny(*Config, TEXT("IgnoreDependencyPackagePrefixes"), TEXT("ignoreDependencyPackagePrefixes"));
	if (!IgnorePrefixes.IsEmpty())
	{
		Context.IgnoreDependencyPackagePrefixes = IgnorePrefixes;
	}
	Context.RequiredPackages = GetStringArrayAny(*Config, TEXT("RequiredPackages"), TEXT("requiredPackages"));
	GetBoolAny(*Config, Context.bAuditSourceTextureDependencies, TEXT("AuditSourceTextureDependencies"), TEXT("auditSourceTextureDependencies"));

	bool bFailOnWarnings = false;
	GetBoolAny(*Config, bFailOnWarnings, TEXT("FailOnWarnings"), TEXT("failOnWarnings"));

	FString JobParam;
	FString MaterialParam;
	FString ToggleParam;
	FParse::Value(*Params, TEXT("Job="), JobParam);
	FParse::Value(*Params, TEXT("MaterialConfig="), MaterialParam);
	FParse::Value(*Params, TEXT("ToggleSetup="), ToggleParam);
	FPaths::NormalizeFilename(JobParam);
	FPaths::NormalizeFilename(MaterialParam);
	FPaths::NormalizeFilename(ToggleParam);

	const TArray<FString> PackageJobs = GetConfigArrayPlusParam(*Config, TEXT("PackageJobs"), JobParam);
	const TArray<FString> MaterialRecipes = GetConfigArrayPlusParam(*Config, TEXT("MaterialRecipes"), MaterialParam);
	const TArray<FString> ToggleSetups = GetConfigArrayPlusParam(*Config, TEXT("ToggleSetups"), ToggleParam);

	TArray<TSharedPtr<FJsonValue>> PackageJobResults;
	for (FString JobFilename : PackageJobs)
	{
		FPaths::NormalizeFilename(JobFilename);
		PackageJobResults.Append(AuditPackageJob(JobFilename, Context));
	}

	AuditRequiredPackages(Context);

	TArray<TSharedPtr<FJsonValue>> MaterialResults;
	for (FString RecipeFilename : MaterialRecipes)
	{
		FPaths::NormalizeFilename(RecipeFilename);
		MaterialResults.Append(AuditMaterialRecipe(RecipeFilename, Context));
	}

	TArray<TSharedPtr<FJsonValue>> ToggleResults;
	for (FString SetupFilename : ToggleSetups)
	{
		FPaths::NormalizeFilename(SetupFilename);
		ToggleResults.Append(AuditToggleSetup(SetupFilename, Context));
	}

	FString OutputFilename;
	if (!FParse::Value(*Params, TEXT("Output="), OutputFilename) || OutputFilename.IsEmpty())
	{
		OutputFilename = FPaths::ProjectSavedDir() / TEXT("NTEBuildTool/PakModAudit.json");
	}
	FPaths::NormalizeFilename(OutputFilename);

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("Format"), TEXT("NTE.PakModAuditReport"));
	Root->SetNumberField(TEXT("Version"), 1.0);
	Root->SetStringField(TEXT("Config"), ConfigFilename);
	Root->SetNumberField(TEXT("ErrorCount"), Context.ErrorCount);
	Root->SetNumberField(TEXT("WarningCount"), Context.WarningCount);
	Root->SetArrayField(TEXT("Findings"), Context.Findings);
	Root->SetArrayField(TEXT("PackageJobs"), PackageJobResults);
	Root->SetArrayField(TEXT("MaterialRecipes"), MaterialResults);
	Root->SetArrayField(TEXT("ToggleSetups"), ToggleResults);
	Root->SetArrayField(TEXT("AllJobPackages"), StringSetToJsonValues(Context.JobPackages));

	if (!SaveJsonObjectToFile(Root, OutputFilename, Error))
	{
		UE_LOG(LogNTEBuildTool, Error, TEXT("Could not write audit report: %s"), *Error);
		return 4;
	}

	for (const TSharedPtr<FJsonValue>& FindingValue : Context.Findings)
	{
		const TSharedPtr<FJsonObject> FindingObject = FindingValue.IsValid() ? FindingValue->AsObject() : nullptr;
		if (!FindingObject.IsValid())
		{
			continue;
		}
		const FString Severity = FindingObject->GetStringField(TEXT("Severity"));
		const FString Area = FindingObject->GetStringField(TEXT("Area"));
		const FString Message = FindingObject->GetStringField(TEXT("Message"));
		const FString Subject = GetStringAny(*FindingObject, TEXT("Subject"));
		UE_LOG(LogNTEBuildTool, Warning, TEXT("[%s][%s] %s %s"), *Severity, *Area, *Message, *Subject);
	}

	UE_LOG(LogNTEBuildTool, Display, TEXT("Pakmod audit report: %s"), *OutputFilename);
	if (Context.ErrorCount > 0 || (bFailOnWarnings && Context.WarningCount > 0))
	{
		return 5;
	}
	return 0;
}
