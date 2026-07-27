// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteAppearanceAssemblyWriter.h"

#include "HTPlayerAppearance.h"
#include "HTSkeletalMeshComponentBudgeted.h"
#include "NteAppearanceAssemblyPlan.h"
#include "NteEditorAssetUtils.h"
#include "NteJsonFileUtils.h"

#include "Animation/AnimInstance.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace NTEBuildTool::Character
{
namespace
{
constexpr const TCHAR* GeneratedUIShowComponentPrefix = TEXT("NTE_Attach_");

FString NormalizePackagePath(FString Path)
{
	Path.TrimStartAndEndInline();
	Path.TrimQuotesInline();
	Path.RemoveFromStart(TEXT("BlueprintGeneratedClass'"));
	Path.RemoveFromStart(TEXT("Class'"));
	Path.RemoveFromStart(TEXT("AnimBlueprintGeneratedClass'"));
	Path.RemoveFromEnd(TEXT("'"));
	Path.TrimStartAndEndInline();

	int32 DotIndex = INDEX_NONE;
	if (Path.FindLastChar(TEXT('.'), DotIndex))
	{
		const FString PackageName = Path.Left(DotIndex);
		const FString ObjectName = Path.Mid(DotIndex + 1);
		if (FPackageName::GetShortName(PackageName) == ObjectName || ObjectName.EndsWith(TEXT("_C")))
		{
			Path = PackageName;
		}
	}

	return Path;
}

bool IsValidGamePackagePath(const FString& PackagePath)
{
	return PackagePath.StartsWith(TEXT("/Game/")) && !PackagePath.Contains(TEXT("."));
}

FString ToGeneratedClassObjectPath(const FString& BlueprintPath)
{
	const FString PackagePath = NormalizePackagePath(BlueprintPath);
	if (PackagePath.IsEmpty())
	{
		return FString();
	}

	const FString ShortName = FPackageName::GetShortName(PackagePath);
	return PackagePath + TEXT(".") + ShortName + TEXT("_C");
}

FString ToObjectPath(const FString& PackagePath)
{
	const FString NormalizedPath = NormalizePackagePath(PackagePath);
	if (NormalizedPath.IsEmpty())
	{
		return FString();
	}

	return NormalizedPath + TEXT(".") + FPackageName::GetShortName(NormalizedPath);
}

FString SanitizeObjectName(const FString& RawName)
{
	FString Result;
	for (const TCHAR Character : RawName)
	{
		Result.AppendChar(FChar::IsAlnum(Character) ? Character : TEXT('_'));
	}
	return Result.IsEmpty() ? TEXT("Attachment") : Result;
}

void AddError(FNteAppearanceAssemblyWriteResult& Result, const FString& Error)
{
	Result.Errors.Add(Error);
}

void AddWarning(FNteAppearanceAssemblyWriteResult& Result, const FString& Warning)
{
	Result.Warnings.Add(Warning);
}

bool SaveAsset(UObject& Asset, FNteAppearanceAssemblyWriteResult& Result)
{
	UPackage* Package = Asset.GetPackage();
	if (!Package)
	{
		AddError(Result, FString::Printf(TEXT("Asset %s has no package."), *Asset.GetName()));
		return false;
	}

	Package->MarkPackageDirty();
	Asset.MarkPackageDirty();

	const FString PackageName = Package->GetName();
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		PackageName,
		FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;

	if (!UPackage::SavePackage(Package, &Asset, *PackageFilename, SaveArgs))
	{
		AddError(Result, FString::Printf(TEXT("Failed to save package %s to %s."), *PackageName, *PackageFilename));
		return false;
	}

	Result.SavedPackages.AddUnique(PackageName);
	return true;
}

USkeletalMesh* LoadSkeletalMesh(const FString& MeshPath, FNteAppearanceAssemblyWriteResult& Result, const FString& Context)
{
	if (MeshPath.IsEmpty())
	{
		AddError(Result, FString::Printf(TEXT("%s has an empty mesh path."), *Context));
		return nullptr;
	}

	USkeletalMesh* Mesh = NTEBuildTool::Editor::LoadAssetByPath<USkeletalMesh>(NormalizePackagePath(MeshPath));
	if (!Mesh)
	{
		AddError(Result, FString::Printf(TEXT("%s mesh could not be loaded: %s"), *Context, *MeshPath));
	}
	return Mesh;
}

UClass* LoadAnimClass(const FString& AnimBlueprintPath, FNteAppearanceAssemblyWriteResult& Result, const FString& Context, const bool bRequired)
{
	if (AnimBlueprintPath.IsEmpty())
	{
		if (bRequired)
		{
			AddError(Result, FString::Printf(TEXT("%s has an empty AnimBlueprint path."), *Context));
		}
		return nullptr;
	}

	UClass* AnimClass = LoadObject<UClass>(nullptr, *ToGeneratedClassObjectPath(AnimBlueprintPath));
	if (!AnimClass)
	{
		if (UBlueprint* Blueprint = NTEBuildTool::Editor::LoadAssetByPath<UBlueprint>(NormalizePackagePath(AnimBlueprintPath)))
		{
			AnimClass = Blueprint->GeneratedClass;
		}
	}

	if (!AnimClass)
	{
		AddError(Result, FString::Printf(TEXT("%s AnimBlueprint generated class could not be loaded: %s"), *Context, *AnimBlueprintPath));
		return nullptr;
	}
	if (!AnimClass->IsChildOf(UAnimInstance::StaticClass()))
	{
		AddError(Result, FString::Printf(TEXT("%s generated class is not an AnimInstance: %s"), *Context, *AnimClass->GetPathName()));
		return nullptr;
	}
	return AnimClass;
}

UHTPlayerAppearance* LoadOrCreateAppearanceAsset(const FString& PackagePath, FNteAppearanceAssemblyWriteResult& Result)
{
	const FString NormalizedPath = NormalizePackagePath(PackagePath);
	if (!IsValidGamePackagePath(NormalizedPath))
	{
		AddError(Result, FString::Printf(TEXT("Invalid PlayerAppearanceAssetPath: %s"), *PackagePath));
		return nullptr;
	}

	const FString ObjectName = FPackageName::GetShortName(NormalizedPath);
	if (UPackage* ExistingPackage = FindPackage(nullptr, *NormalizedPath))
	{
		if (UObject* ExistingAsset = FindObject<UObject>(ExistingPackage, *ObjectName))
		{
			UHTPlayerAppearance* ExistingAppearance = Cast<UHTPlayerAppearance>(ExistingAsset);
			if (!ExistingAppearance)
			{
				AddError(Result, FString::Printf(
					TEXT("Existing asset %s is %s, not HTPlayerAppearance."),
					*NormalizedPath,
					*ExistingAsset->GetClass()->GetPathName()));
			}
			return ExistingAppearance;
		}
	}

	FString ExistingPackageFilename;
	if (FPackageName::DoesPackageExist(NormalizedPath, &ExistingPackageFilename))
	{
		if (UObject* ExistingAsset = StaticLoadObject(UObject::StaticClass(), nullptr, *ToObjectPath(NormalizedPath)))
		{
			UHTPlayerAppearance* ExistingAppearance = Cast<UHTPlayerAppearance>(ExistingAsset);
			if (!ExistingAppearance)
			{
				AddError(Result, FString::Printf(
					TEXT("Existing asset %s is %s, not HTPlayerAppearance."),
					*NormalizedPath,
					*ExistingAsset->GetClass()->GetPathName()));
			}
			return ExistingAppearance;
		}
	}

	UPackage* Package = CreatePackage(*NormalizedPath);
	if (!Package)
	{
		AddError(Result, FString::Printf(TEXT("Could not create package for PlayerAppearanceAssetPath: %s"), *NormalizedPath));
		return nullptr;
	}

	UHTPlayerAppearance* NewAppearance = NewObject<UHTPlayerAppearance>(
		Package,
		UHTPlayerAppearance::StaticClass(),
		*ObjectName,
		RF_Public | RF_Standalone | RF_Transactional);

	FAssetRegistryModule::AssetCreated(NewAppearance);
	return NewAppearance;
}

bool FillMainMeshData(
	const FNteAppearanceMeshDataPlan& MainMesh,
	FHTFashionMeshData& OutMeshData,
	FNteAppearanceAssemblyWriteResult& Result)
{
	USkeletalMesh* Mesh = LoadSkeletalMesh(MainMesh.CharacterMeshPath, Result, TEXT("Main mesh"));
	UClass* AnimClass = LoadAnimClass(MainMesh.AnimInstancePath, Result, TEXT("Main mesh"), true);
	if (!Mesh || !AnimClass)
	{
		return false;
	}

	OutMeshData.CharacterMesh = Mesh;
	OutMeshData.AnimInstance = AnimClass;
	return true;
}

bool FillAttachedMeshData(
	const FNteAppearanceMeshDataPlan& AttachedMesh,
	FHTFashionAttachedMeshData& OutMeshData,
	FNteAppearanceAssemblyWriteResult& Result)
{
	const FString Context = FString::Printf(TEXT("Attached mesh '%s'"), *AttachedMesh.Id);
	USkeletalMesh* Mesh = LoadSkeletalMesh(AttachedMesh.CharacterMeshPath, Result, Context);
	UClass* AnimClass = LoadAnimClass(AttachedMesh.AnimInstancePath, Result, Context, false);
	UClass* MobileAnimClass = LoadAnimClass(AttachedMesh.MobileAnimInstancePath, Result, Context + TEXT(" mobile"), false);
	if (!Mesh)
	{
		return false;
	}

	OutMeshData.CharacterMesh = Mesh;
	OutMeshData.AnimInstance = AnimClass;
	OutMeshData.MobileAnimInstance = MobileAnimClass ? MobileAnimClass : AnimClass;
	OutMeshData.SocketName = FName(*AttachedMesh.SocketName);
	OutMeshData.MeshComponentOwnedTags.Reset();
	for (const FString& Tag : AttachedMesh.MeshComponentOwnedTags)
	{
		if (!Tag.IsEmpty())
		{
			OutMeshData.MeshComponentOwnedTags.Add(FName(*Tag));
		}
	}
	OutMeshData.RelativeLocation = AttachedMesh.RelativeLocation;
	OutMeshData.RelativeRotation = AttachedMesh.RelativeRotation;
	OutMeshData.RelativeScale3D = AttachedMesh.RelativeScale3D;
	return true;
}

void WritePlayerAppearanceAsset(const FNteAppearanceAssemblyPlan& Plan, FNteAppearanceAssemblyWriteResult& Result)
{
	UHTPlayerAppearance* Appearance = LoadOrCreateAppearanceAsset(Plan.PlayerAppearanceAssetPath, Result);
	if (!Appearance)
	{
		return;
	}

	FHTFashionMeshData MainMeshData;
	if (!FillMainMeshData(Plan.MainMesh, MainMeshData, Result))
	{
		return;
	}

	TArray<FHTFashionAttachedMeshData> AttachedMeshDataList;
	for (const FNteAppearanceMeshDataPlan& AttachedMesh : Plan.AttachedMeshes)
	{
		FHTFashionAttachedMeshData AttachedMeshData;
		if (FillAttachedMeshData(AttachedMesh, AttachedMeshData, Result))
		{
			AttachedMeshDataList.Add(AttachedMeshData);
		}
	}

	Appearance->Modify();
	Appearance->FashionMeshData = MainMeshData;
	Appearance->ArrayFashionAttachedMeshData = MoveTemp(AttachedMeshDataList);
	SaveAsset(*Appearance, Result);
}

USCS_Node* FindMainMeshSCSNode(USimpleConstructionScript& SimpleConstructionScript)
{
	if (USCS_Node* MeshNode = SimpleConstructionScript.FindSCSNode(TEXT("Mesh")))
	{
		return MeshNode;
	}

	for (USCS_Node* Node : SimpleConstructionScript.GetAllNodes())
	{
		if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<USkeletalMeshComponent>())
		{
			return Node;
		}
	}

	return nullptr;
}

void RemovePreviouslyGeneratedUIShowNodes(USimpleConstructionScript& SimpleConstructionScript)
{
	TArray<USCS_Node*> NodesToRemove;
	for (USCS_Node* Node : SimpleConstructionScript.GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString().StartsWith(GeneratedUIShowComponentPrefix))
		{
			NodesToRemove.Add(Node);
		}
	}

	for (USCS_Node* Node : NodesToRemove)
	{
		SimpleConstructionScript.RemoveNode(Node, false);
	}
	SimpleConstructionScript.ValidateSceneRootNodes();
}

void ConfigureSkeletalMeshComponent(
	UHTSkeletalMeshComponentBudgeted& Component,
	const FNteAppearanceMeshDataPlan& AttachedMesh,
	FNteAppearanceAssemblyWriteResult& Result)
{
	if (USkeletalMesh* Mesh = LoadSkeletalMesh(AttachedMesh.CharacterMeshPath, Result, FString::Printf(TEXT("UIShow attached mesh '%s'"), *AttachedMesh.Id)))
	{
		Component.SetSkeletalMesh(Mesh);
	}

	if (UClass* AnimClass = LoadAnimClass(AttachedMesh.UIAnimInstancePath.IsEmpty() ? AttachedMesh.AnimInstancePath : AttachedMesh.UIAnimInstancePath, Result, FString::Printf(TEXT("UIShow attached mesh '%s'"), *AttachedMesh.Id), false))
	{
		Component.SetAnimationMode(EAnimationMode::AnimationBlueprint);
		Component.SetAnimInstanceClass(AnimClass);
	}

	Component.SetRelativeLocation(AttachedMesh.RelativeLocation);
	Component.SetRelativeRotation(AttachedMesh.RelativeRotation);
	Component.SetRelativeScale3D(AttachedMesh.RelativeScale3D);
	Component.ComponentTags.Reset();
	for (const FString& Tag : AttachedMesh.MeshComponentOwnedTags)
	{
		if (!Tag.IsEmpty())
		{
			Component.ComponentTags.Add(FName(*Tag));
		}
	}
}

void SyncPlayerUIShowBlueprint(const FNteAppearanceAssemblyPlan& Plan, FNteAppearanceAssemblyWriteResult& Result)
{
	if (Plan.UIActorClassPath.IsEmpty())
	{
		AddWarning(Result, TEXT("UIActorClassPath is empty; skipped PlayerUIShow SCS sync."));
		return;
	}

	const FString UIShowPackagePath = NormalizePackagePath(Plan.UIActorClassPath);
	UBlueprint* UIShowBlueprint = NTEBuildTool::Editor::LoadAssetByPath<UBlueprint>(UIShowPackagePath);
	if (!UIShowBlueprint)
	{
		AddError(Result, FString::Printf(TEXT("PlayerUIShow Blueprint could not be loaded for SCS sync: %s"), *Plan.UIActorClassPath));
		return;
	}
	if (!UIShowBlueprint->SimpleConstructionScript)
	{
		AddError(Result, FString::Printf(TEXT("PlayerUIShow Blueprint has no SimpleConstructionScript: %s"), *UIShowPackagePath));
		return;
	}

	UIShowBlueprint->Modify();
	USimpleConstructionScript* SimpleConstructionScript = UIShowBlueprint->SimpleConstructionScript;
	SimpleConstructionScript->Modify();

	USCS_Node* MainMeshNode = FindMainMeshSCSNode(*SimpleConstructionScript);
	if (!MainMeshNode)
	{
		AddError(Result, FString::Printf(TEXT("PlayerUIShow Blueprint has no Mesh SCS node to attach generated components: %s"), *UIShowPackagePath));
		return;
	}

	RemovePreviouslyGeneratedUIShowNodes(*SimpleConstructionScript);

	for (const FNteAppearanceMeshDataPlan& AttachedMesh : Plan.AttachedMeshes)
	{
		if (!AttachedMesh.bSyncToUIShow)
		{
			continue;
		}

		const FName ComponentName(*FString::Printf(
			TEXT("%s%s"),
			GeneratedUIShowComponentPrefix,
			*SanitizeObjectName(AttachedMesh.Id)));
		USCS_Node* AttachedNode = SimpleConstructionScript->CreateNode(
			UHTSkeletalMeshComponentBudgeted::StaticClass(),
			ComponentName);
		if (!AttachedNode)
		{
			AddError(Result, FString::Printf(TEXT("Could not create SCS node for UIShow attached mesh '%s'."), *AttachedMesh.Id));
			continue;
		}

		AttachedNode->SetParent(MainMeshNode);
		AttachedNode->AttachToName = FName(*AttachedMesh.SocketName);
		MainMeshNode->AddChildNode(AttachedNode);

		if (UHTSkeletalMeshComponentBudgeted* Component = Cast<UHTSkeletalMeshComponentBudgeted>(AttachedNode->ComponentTemplate))
		{
			ConfigureSkeletalMeshComponent(*Component, AttachedMesh, Result);
		}
		else
		{
			AddError(Result, FString::Printf(TEXT("Generated UIShow component template is not HTSkeletalMeshComponentBudgeted for '%s'."), *AttachedMesh.Id));
		}

		Result.WrittenUIShowComponents.AddUnique(ComponentName.ToString());
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(UIShowBlueprint);
	FKismetEditorUtilities::CompileBlueprint(UIShowBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	SaveAsset(*UIShowBlueprint, Result);
}

void AppendPlanIssues(const FNteAppearanceAssemblyPlan& Plan, FNteAppearanceAssemblyWriteResult& Result)
{
	for (const FString& Error : Plan.Errors)
	{
		AddError(Result, Error);
	}
	for (const FString& Warning : Plan.Warnings)
	{
		AddWarning(Result, Warning);
	}
}
}

FNteAppearanceAssemblyWriteResult WriteAppearanceAssembly(
	const FNteAppearanceAssemblyPlan& Plan,
	const FNteAppearanceAssemblyWriteOptions& Options)
{
	FNteAppearanceAssemblyWriteResult Result;
	AppendPlanIssues(Plan, Result);
	if (Result.HasErrors())
	{
		return Result;
	}

	if (Options.bWritePlayerAppearance)
	{
		WritePlayerAppearanceAsset(Plan, Result);
	}

	if (Options.bSyncPlayerUIShow)
	{
		SyncPlayerUIShowBlueprint(Plan, Result);
	}

	return Result;
}

TSharedRef<FJsonObject> AppearanceAssemblyWriteResultToJson(const FNteAppearanceAssemblyWriteResult& Result)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("SavedPackageCount"), Result.SavedPackages.Num());
	Object->SetNumberField(TEXT("WrittenUIShowComponentCount"), Result.WrittenUIShowComponents.Num());
	Object->SetNumberField(TEXT("ErrorCount"), Result.Errors.Num());
	Object->SetNumberField(TEXT("WarningCount"), Result.Warnings.Num());
	Object->SetArrayField(TEXT("SavedPackages"), Json::StringArrayToJsonValues(Result.SavedPackages));
	Object->SetArrayField(TEXT("WrittenUIShowComponents"), Json::StringArrayToJsonValues(Result.WrittenUIShowComponents));
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Result.Errors));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Result.Warnings));
	return Object;
}
}
