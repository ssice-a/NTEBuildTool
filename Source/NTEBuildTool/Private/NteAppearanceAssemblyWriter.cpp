// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteAppearanceAssemblyWriter.h"

#include "HTAttachedMeshAnimInstance.h"
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
#include "UObject/UnrealType.h"

namespace NTEBuildTool::Character
{
namespace
{
constexpr const TCHAR* GeneratedPresentationComponentPrefix = TEXT("NTE_Attach_");

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

bool ValidateOwnedPropertyOrder(
	const UStruct& Struct,
	const TArray<FName>& ExpectedNames,
	FNteAppearanceAssemblyWriteResult& Result)
{
	TArray<FName> ActualNames;
	for (TFieldIterator<FProperty> PropertyIt(&Struct, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
	{
		ActualNames.Add(PropertyIt->GetFName());
	}

	if (ActualNames == ExpectedNames)
	{
		return true;
	}

	AddError(Result, FString::Printf(
		TEXT("HTGame schema mismatch for %s. Expected own properties [%s], found [%s]."),
		*Struct.GetName(),
		*FString::JoinBy(ExpectedNames, TEXT(", "), [](const FName& Name) { return Name.ToString(); }),
		*FString::JoinBy(ActualNames, TEXT(", "), [](const FName& Name) { return Name.ToString(); })));
	return false;
}

bool ValidateAppearanceSchema(FNteAppearanceAssemblyWriteResult& Result)
{
	bool bValid = true;
	const UClass* AppearanceClass = UHTPlayerAppearance::StaticClass();
	const UClass* NPCAppearanceClass = UHTPlayerNPCAppearance::StaticClass();
	if (!AppearanceClass->GetSuperClass() || AppearanceClass->GetSuperClass()->GetFName() != TEXT("HTAppearanceDataAsset"))
	{
		AddError(Result, TEXT("HTGame schema mismatch: HTPlayerAppearance must inherit HTAppearanceDataAsset."));
		bValid = false;
	}

	bValid &= ValidateOwnedPropertyOrder(*FCharacterMeshData::StaticStruct(),
		{ TEXT("CharacterMesh"), TEXT("AnimInstance") }, Result);
	bValid &= ValidateOwnedPropertyOrder(*FAttachedMeshData::StaticStruct(),
		{
			TEXT("CharacterMesh"), TEXT("AnimInstance"), TEXT("MobileAnimInstance"), TEXT("SocketName"),
			TEXT("MeshComponentOwnedTags"), TEXT("RelativeLocation"), TEXT("RelativeRotation"), TEXT("RelativeScale3D")
		}, Result);
	bValid &= ValidateOwnedPropertyOrder(*AppearanceClass,
		{
			TEXT("FashionMeshData"), TEXT("CapsuleHalfHeight"), TEXT("CapsuleRadius"), TEXT("RelativeLocation"),
			TEXT("SortTriangles"), TEXT("FPSCameraCapsuleTopOffset"), TEXT("VinesIKFootOffsetAdditive"),
			TEXT("ArrayFashionAttachedMeshData"), TEXT("ArrayDynamicAttachedMeshData"), TEXT("ChildMeshDataMap"),
			TEXT("FashionMeshAttachEffects"), TEXT("AppearanceWeapon"), TEXT("UltraSkillSequence"), TEXT("ParticleMap"),
			TEXT("ForceLoadParticleMap"), TEXT("GameplayTagAudioEventMap"), TEXT("SpawnActorMap"),
			TEXT("ParticleMapWithParams"), TEXT("SequenceMap")
		}, Result);
	if (!NPCAppearanceClass->GetSuperClass() || NPCAppearanceClass->GetSuperClass()->GetFName() != TEXT("HTAppearanceDataAsset"))
	{
		AddError(Result, TEXT("HTGame schema mismatch: HTPlayerNPCAppearance must inherit HTAppearanceDataAsset."));
		bValid = false;
	}
	bValid &= ValidateOwnedPropertyOrder(*NPCAppearanceClass,
		{ TEXT("FashionMeshData"), TEXT("ArrayFashionAttachedMeshData") }, Result);
	bValid &= ValidateOwnedPropertyOrder(*UHTAttachedMeshAnimInstance::StaticClass(),
		{
			TEXT("EnablePhysicsWeight"), TEXT("PhysicsCurveInterpSpeed"), TEXT("bUseParentPhysicsWeight"),
			TEXT("ParentDepth"), TEXT("bOverrideByCurveWhenUseParentWeight"), TEXT("bPhysicsWeightBlendMontage"),
			TEXT("SpeedBlendSpeed"), TEXT("MovementReferenceDisplacement"), TEXT("AnimationToPlay"),
			TEXT("MovementSpeed"), TEXT("bIsSitting"), TEXT("SleepWeight"), TEXT("OwnerCharacter")
		}, Result);

	const FStructProperty* MainMeshProperty = FindFProperty<FStructProperty>(AppearanceClass, TEXT("FashionMeshData"));
	if (!MainMeshProperty || MainMeshProperty->Struct != FCharacterMeshData::StaticStruct())
	{
		AddError(Result, TEXT("HTGame schema mismatch: FashionMeshData must use CharacterMeshData."));
		bValid = false;
	}

	const FArrayProperty* AttachedArray = FindFProperty<FArrayProperty>(AppearanceClass, TEXT("ArrayFashionAttachedMeshData"));
	const FStructProperty* AttachedInner = AttachedArray ? CastField<FStructProperty>(AttachedArray->Inner) : nullptr;
	if (!AttachedInner || AttachedInner->Struct != FAttachedMeshData::StaticStruct())
	{
		AddError(Result, TEXT("HTGame schema mismatch: ArrayFashionAttachedMeshData must contain AttachedMeshData."));
		bValid = false;
	}

	const FStructProperty* NPCMainMeshProperty = FindFProperty<FStructProperty>(NPCAppearanceClass, TEXT("FashionMeshData"));
	if (!NPCMainMeshProperty || NPCMainMeshProperty->Struct != FCharacterMeshData::StaticStruct())
	{
		AddError(Result, TEXT("HTGame schema mismatch: HTPlayerNPCAppearance.FashionMeshData must use CharacterMeshData."));
		bValid = false;
	}
	const FArrayProperty* NPCAttachedArray = FindFProperty<FArrayProperty>(NPCAppearanceClass, TEXT("ArrayFashionAttachedMeshData"));
	const FStructProperty* NPCAttachedInner = NPCAttachedArray ? CastField<FStructProperty>(NPCAttachedArray->Inner) : nullptr;
	if (!NPCAttachedInner || NPCAttachedInner->Struct != FAttachedMeshData::StaticStruct())
	{
		AddError(Result, TEXT("HTGame schema mismatch: HTPlayerNPCAppearance.ArrayFashionAttachedMeshData must contain AttachedMeshData."));
		bValid = false;
	}

	for (const UScriptStruct* MeshStruct : { FCharacterMeshData::StaticStruct(), FAttachedMeshData::StaticStruct() })
	{
		const FProperty* AnimProperty = MeshStruct->FindPropertyByName(TEXT("AnimInstance"));
		if (!AnimProperty || !AnimProperty->IsA<FObjectProperty>() || AnimProperty->IsA<FClassProperty>())
		{
			AddError(Result, FString::Printf(
				TEXT("HTGame schema mismatch: %s.AnimInstance must be ObjectProperty, not ClassProperty."),
				*MeshStruct->GetName()));
			bValid = false;
		}
	}

	return bValid;
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

UHTPlayerNPCAppearance* LoadOrCreateNPCAppearanceAsset(const FString& PackagePath, FNteAppearanceAssemblyWriteResult& Result)
{
	const FString NormalizedPath = NormalizePackagePath(PackagePath);
	if (!IsValidGamePackagePath(NormalizedPath))
	{
		AddError(Result, FString::Printf(TEXT("Invalid NPC Appearance AssetPath: %s"), *PackagePath));
		return nullptr;
	}

	const FString ObjectName = FPackageName::GetShortName(NormalizedPath);
	if (UPackage* ExistingPackage = FindPackage(nullptr, *NormalizedPath))
	{
		if (UObject* ExistingAsset = FindObject<UObject>(ExistingPackage, *ObjectName))
		{
			UHTPlayerNPCAppearance* ExistingAppearance = Cast<UHTPlayerNPCAppearance>(ExistingAsset);
			if (!ExistingAppearance)
			{
				AddError(Result, FString::Printf(
					TEXT("Existing asset %s is %s, not HTPlayerNPCAppearance."),
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
			UHTPlayerNPCAppearance* ExistingAppearance = Cast<UHTPlayerNPCAppearance>(ExistingAsset);
			if (!ExistingAppearance)
			{
				AddError(Result, FString::Printf(
					TEXT("Existing asset %s is %s, not HTPlayerNPCAppearance."),
					*NormalizedPath,
					*ExistingAsset->GetClass()->GetPathName()));
			}
			return ExistingAppearance;
		}
	}

	UPackage* Package = CreatePackage(*NormalizedPath);
	if (!Package)
	{
		AddError(Result, FString::Printf(TEXT("Could not create package for NPC Appearance AssetPath: %s"), *NormalizedPath));
		return nullptr;
	}

	UHTPlayerNPCAppearance* NewAppearance = NewObject<UHTPlayerNPCAppearance>(
		Package,
		UHTPlayerNPCAppearance::StaticClass(),
		*ObjectName,
		RF_Public | RF_Standalone | RF_Transactional);

	FAssetRegistryModule::AssetCreated(NewAppearance);
	return NewAppearance;
}

bool FillMainMeshData(
	const FNteAppearanceMeshDataPlan& MainMesh,
	FCharacterMeshData& OutMeshData,
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
	FAttachedMeshData& OutMeshData,
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


	FCharacterMeshData MainMeshData;
	if (!FillMainMeshData(Plan.MainMesh, MainMeshData, Result))
	{
		return;
	}

	TArray<FAttachedMeshData> AttachedMeshDataList;
	for (const FNteAppearanceMeshDataPlan& AttachedMesh : Plan.AttachedMeshes)
	{
		FAttachedMeshData AttachedMeshData;
		if (FillAttachedMeshData(AttachedMesh, AttachedMeshData, Result))
		{
			AttachedMeshDataList.Add(AttachedMeshData);
		}
	}

	Appearance->Modify();
	Appearance->FashionMeshData = MainMeshData;
	if (Plan.CapsuleHalfHeight.IsSet())
	{
		Appearance->CapsuleHalfHeight = Plan.CapsuleHalfHeight.GetValue();
	}
	if (Plan.CapsuleRadius.IsSet())
	{
		Appearance->CapsuleRadius = Plan.CapsuleRadius.GetValue();
	}
	if (Plan.RelativeLocation.IsSet())
	{
		Appearance->RelativeLocation = Plan.RelativeLocation.GetValue();
	}
	Appearance->ArrayFashionAttachedMeshData = MoveTemp(AttachedMeshDataList);
	SaveAsset(*Appearance, Result);
}

void WriteNPCAppearanceAssets(const FNteAppearanceAssemblyPlan& Plan, FNteAppearanceAssemblyWriteResult& Result)
{
	for (const FNteNPCAppearanceTargetPlan& Target : Plan.NPCAppearanceTargets)
	{
		UHTPlayerNPCAppearance* Appearance = LoadOrCreateNPCAppearanceAsset(Target.AssetPath, Result);
		if (!Appearance)
		{
			continue;
		}

		FNteAppearanceMeshDataPlan NPCMainMesh = Plan.MainMesh;
		NPCMainMesh.AnimInstancePath = Target.MainAnimInstancePath;
		FCharacterMeshData MainMeshData;
		if (!FillMainMeshData(NPCMainMesh, MainMeshData, Result))
		{
			continue;
		}

		TArray<FAttachedMeshData> AttachedMeshDataList;
		for (const FNteAppearanceMeshDataPlan& AttachedMesh : Plan.AttachedMeshes)
		{
			FAttachedMeshData AttachedMeshData;
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
}

USkeletalMeshComponent* FindNamedSkeletalMeshComponentTemplate(UBlueprint& Blueprint, const FName ComponentName)
{
	if (!Blueprint.GeneratedClass)
	{
		return nullptr;
	}

	UObject* ClassDefaultObject = Blueprint.GeneratedClass->GetDefaultObject();
	if (!ClassDefaultObject)
	{
		return nullptr;
	}

	TArray<UObject*> DefaultSubobjects;
	ClassDefaultObject->GetDefaultSubobjects(DefaultSubobjects);
	for (UObject* DefaultSubobject : DefaultSubobjects)
	{
		USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(DefaultSubobject);
		if (SkeletalMesh && SkeletalMesh->GetFName() == ComponentName)
		{
			return SkeletalMesh;
		}
	}
	return nullptr;
}

bool ShouldSyncAttachedMeshToTarget(
	const FNteAppearanceMeshDataPlan& AttachedMesh,
	const FNteAppearancePresentationTargetPlan& Target)
{
	if (!AttachedMesh.PresentationTargetIds.IsEmpty())
	{
		return AttachedMesh.PresentationTargetIds.Contains(Target.Id);
	}
	return !Target.Id.Equals(TEXT("ui"), ESearchCase::IgnoreCase) || AttachedMesh.bSyncToUIShow;
}

void ConfigureMainPresentationMesh(
	UBlueprint& Blueprint,
	USkeletalMeshComponent& Component,
	const FNteAppearanceMeshDataPlan& MainMesh,
	const FNteAppearancePresentationTargetPlan& Target,
	FNteAppearanceAssemblyWriteResult& Result)
{
	Component.Modify();
	const FString Context = FString::Printf(TEXT("Presentation target '%s' main mesh"), *Target.Id);
	if (USkeletalMesh* Mesh = LoadSkeletalMesh(MainMesh.CharacterMeshPath, Result, Context))
	{
		Component.SetSkeletalMesh(Mesh);
	}
	if (!Target.MainAnimInstancePath.IsEmpty())
	{
		if (UClass* AnimClass = LoadAnimClass(Target.MainAnimInstancePath, Result, Context, true))
		{
			Component.SetAnimationMode(EAnimationMode::AnimationBlueprint);
			Component.SetAnimInstanceClass(AnimClass);
		}
	}
	Blueprint.MarkPackageDirty();
}

void ConfigureAttachedPresentationMesh(
	UHTSkeletalMeshComponentBudgeted& Component,
	const FNteAppearanceMeshDataPlan& AttachedMesh,
	const FNteAppearancePresentationTargetPlan& Target,
	FNteAppearanceAssemblyWriteResult& Result)
{
	const FString Context = FString::Printf(TEXT("Presentation target '%s' attached mesh '%s'"), *Target.Id, *AttachedMesh.Id);
	if (USkeletalMesh* Mesh = LoadSkeletalMesh(AttachedMesh.CharacterMeshPath, Result, Context))
	{
		Component.SetSkeletalMesh(Mesh);
	}

	const FString AnimInstancePath = Target.Id.Equals(TEXT("ui"), ESearchCase::IgnoreCase)
		&& !AttachedMesh.UIAnimInstancePath.IsEmpty()
		? AttachedMesh.UIAnimInstancePath
		: AttachedMesh.AnimInstancePath;
	if (UClass* AnimClass = LoadAnimClass(AnimInstancePath, Result, Context, false))
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

void SyncPresentationTargetBlueprint(
	const FNteAppearanceAssemblyPlan& Plan,
	const FNteAppearancePresentationTargetPlan& Target,
	FNteAppearanceAssemblyWriteResult& Result)
{
	const FString BlueprintPackagePath = NormalizePackagePath(Target.BlueprintClassPath);
	UBlueprint* Blueprint = NTEBuildTool::Editor::LoadAssetByPath<UBlueprint>(BlueprintPackagePath);
	if (!Blueprint)
	{
		AddError(Result, FString::Printf(TEXT("Presentation target '%s' Blueprint could not be loaded: %s"), *Target.Id, *Target.BlueprintClassPath));
		return;
	}
	if (!Blueprint->SimpleConstructionScript)
	{
		AddError(Result, FString::Printf(TEXT("Presentation target '%s' Blueprint has no SimpleConstructionScript: %s"), *Target.Id, *BlueprintPackagePath));
		return;
	}

	Blueprint->Modify();
	USimpleConstructionScript* SimpleConstructionScript = Blueprint->SimpleConstructionScript;
	SimpleConstructionScript->Modify();
	const FName ParentComponentName(*Target.ParentMeshComponentName);
	USCS_Node* ParentNode = SimpleConstructionScript->FindSCSNode(ParentComponentName);
	USkeletalMeshComponent* ParentComponent = ParentNode
		? Cast<USkeletalMeshComponent>(ParentNode->ComponentTemplate)
		: FindNamedSkeletalMeshComponentTemplate(*Blueprint, ParentComponentName);
	if (!ParentComponent)
	{
		AddError(Result, FString::Printf(
			TEXT("Presentation target '%s' has no skeletal mesh component named '%s': %s"),
			*Target.Id,
			*Target.ParentMeshComponentName,
			*BlueprintPackagePath));
		return;
	}

	if (Target.bConfigureMainMesh)
	{
		ConfigureMainPresentationMesh(*Blueprint, *ParentComponent, Plan.MainMesh, Target, Result);
	}
	TSet<USCS_Node*> KeptGeneratedNodes;
	for (const FNteAppearanceMeshDataPlan& AttachedMesh : Plan.AttachedMeshes)
	{
		if (!ShouldSyncAttachedMeshToTarget(AttachedMesh, Target))
		{
			continue;
		}

		const FString ComponentNameBase = FString::Printf(
			TEXT("%s%s"),
			GeneratedPresentationComponentPrefix,
			*SanitizeObjectName(AttachedMesh.Id));
		USCS_Node* AttachedNode = nullptr;
		TArray<USCS_Node*> DuplicateNodes;
		for (USCS_Node* ExistingNode : SimpleConstructionScript->GetAllNodes())
		{
			if (!ExistingNode || !ExistingNode->GetVariableName().ToString().StartsWith(ComponentNameBase))
			{
				continue;
			}
			if (!AttachedNode)
			{
				AttachedNode = ExistingNode;
			}
			else
			{
				DuplicateNodes.Add(ExistingNode);
			}
		}
		for (USCS_Node* DuplicateNode : DuplicateNodes)
		{
			SimpleConstructionScript->RemoveNode(DuplicateNode, false);
		}

		if (!AttachedNode)
		{
			AttachedNode = SimpleConstructionScript->CreateNode(
				UHTSkeletalMeshComponentBudgeted::StaticClass(),
				FName(*ComponentNameBase));
		}
		if (!AttachedNode)
		{
			AddError(Result, FString::Printf(TEXT("Could not create SCS node for presentation target '%s' attached mesh '%s'."), *Target.Id, *AttachedMesh.Id));
			continue;
		}
		SimpleConstructionScript->RemoveNode(AttachedNode, false);
		AttachedNode->AttachToName = AttachedMesh.PresentationSocketName.IsEmpty()
			? NAME_None
			: FName(*AttachedMesh.PresentationSocketName);
		if (ParentNode)
		{
			AttachedNode->SetParent(ParentNode);
			ParentNode->AddChildNode(AttachedNode);
		}
		else
		{
			SimpleConstructionScript->AddNode(AttachedNode);
			AttachedNode->SetParent(ParentComponent);
		}

		if (UHTSkeletalMeshComponentBudgeted* Component = Cast<UHTSkeletalMeshComponentBudgeted>(AttachedNode->ComponentTemplate))
		{
			ConfigureAttachedPresentationMesh(*Component, AttachedMesh, Target, Result);
		}
		else
		{
			AddError(Result, FString::Printf(TEXT("Generated presentation component is not HTSkeletalMeshComponentBudgeted for target '%s' mesh '%s'."), *Target.Id, *AttachedMesh.Id));
		}

		KeptGeneratedNodes.Add(AttachedNode);
		Result.WrittenPresentationComponents.AddUnique(FString::Printf(
			TEXT("%s:%s"),
			*Target.Id,
			*AttachedNode->GetVariableName().ToString()));
	}

	TArray<USCS_Node*> StaleGeneratedNodes;
	for (USCS_Node* Node : SimpleConstructionScript->GetAllNodes())
	{
		if (Node
			&& Node->GetVariableName().ToString().StartsWith(GeneratedPresentationComponentPrefix)
			&& !KeptGeneratedNodes.Contains(Node))
		{
			StaleGeneratedNodes.Add(Node);
		}
	}
	for (USCS_Node* StaleNode : StaleGeneratedNodes)
	{
		SimpleConstructionScript->RemoveNode(StaleNode, false);
	}
	SimpleConstructionScript->ValidateSceneRootNodes();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	SaveAsset(*Blueprint, Result);
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
	if ((Options.bWritePlayerAppearance || Options.bWriteNPCAppearances) && !ValidateAppearanceSchema(Result))
	{
		return Result;
	}

	if (Options.bWritePlayerAppearance)
	{
		WritePlayerAppearanceAsset(Plan, Result);
	}
	if (Options.bWriteNPCAppearances)
	{
		WriteNPCAppearanceAssets(Plan, Result);
	}

	if (Options.bSyncPresentationTargets)
	{
		for (const FNteAppearancePresentationTargetPlan& Target : Plan.PresentationTargets)
		{
			SyncPresentationTargetBlueprint(Plan, Target, Result);
		}
	}

	return Result;
}

TSharedRef<FJsonObject> AppearanceAssemblyWriteResultToJson(const FNteAppearanceAssemblyWriteResult& Result)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("SavedPackageCount"), Result.SavedPackages.Num());
	Object->SetNumberField(TEXT("WrittenPresentationComponentCount"), Result.WrittenPresentationComponents.Num());
	Object->SetNumberField(TEXT("ErrorCount"), Result.Errors.Num());
	Object->SetNumberField(TEXT("WarningCount"), Result.Warnings.Num());
	Object->SetArrayField(TEXT("SavedPackages"), Json::StringArrayToJsonValues(Result.SavedPackages));
	Object->SetArrayField(TEXT("WrittenPresentationComponents"), Json::StringArrayToJsonValues(Result.WrittenPresentationComponents));
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Result.Errors));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Result.Warnings));
	return Object;
}
}
