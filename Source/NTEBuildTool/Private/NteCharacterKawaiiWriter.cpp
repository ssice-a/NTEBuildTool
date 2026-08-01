// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteCharacterKawaiiWriter.h"

#include "NteEditorAssetUtils.h"
#include "NteJsonFileUtils.h"

#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_LocalRefPose.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Curves/CurveFloat.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

namespace NTEBuildTool::Character
{
namespace
{
constexpr const TCHAR* LimitsDataAssetClassPath = TEXT("/Script/KawaiiPhysics.KawaiiPhysicsLimitsDataAsset");
constexpr const TCHAR* BoneConstraintsDataAssetClassPath = TEXT("/Script/KawaiiPhysics.KawaiiPhysicsBoneConstraintsDataAsset");
constexpr const TCHAR* AnimNodeStructPath = TEXT("/Script/KawaiiPhysics.AnimNode_KawaiiPhysics");
constexpr const TCHAR* PhysicsSettingsStructPath = TEXT("/Script/KawaiiPhysics.KawaiiPhysicsSettings");
constexpr const TCHAR* CapsuleLimitStructPath = TEXT("/Script/KawaiiPhysics.CapsuleLimit");
constexpr const TCHAR* CollisionLimitBaseStructPath = TEXT("/Script/KawaiiPhysics.CollisionLimitBase");
constexpr const TCHAR* KawaiiAnimGraphNodeClassPath = TEXT("/Script/KawaiiPhysicsEd.AnimGraphNode_KawaiiPhysics");
constexpr const TCHAR* CopyPoseAnimGraphNodeClassPath = TEXT("/Script/AnimGraph.AnimGraphNode_CopyPoseFromMesh");
constexpr const TCHAR* LinkedInputPoseAnimGraphNodeClassPath = TEXT("/Script/AnimGraph.AnimGraphNode_LinkedInputPose");
constexpr const TCHAR* LocalRefPoseAnimGraphNodeClassPath = TEXT("/Script/AnimGraph.AnimGraphNode_LocalRefPose");
constexpr const TCHAR* LayeredBoneBlendAnimGraphNodeClassPath = TEXT("/Script/AnimGraph.AnimGraphNode_LayeredBoneBlend");
constexpr const TCHAR* LocalToComponentAnimGraphNodeClassPath = TEXT("/Script/AnimGraph.AnimGraphNode_LocalToComponentSpace");
constexpr const TCHAR* ComponentToLocalAnimGraphNodeClassPath = TEXT("/Script/AnimGraph.AnimGraphNode_ComponentToLocalSpace");
constexpr const TCHAR* RootAnimGraphNodeClassPath = TEXT("/Script/AnimGraph.AnimGraphNode_Root");
constexpr const TCHAR* GeneratedKawaiiNodeComment = TEXT("NTE Character Kawaii Generated");

void AddStringIfNotEmpty(const TSharedRef<FJsonObject>& Object, const TCHAR* FieldName, const FString& Value)
{
	if (!Value.IsEmpty())
	{
		Object->SetStringField(FieldName, Value);
	}
}

FString NormalizePackagePath(FString Path)
{
	Path = NTEBuildTool::Editor::NormalizeAssetPathForText(Path);
	if (!Path.StartsWith(TEXT("/Game/")))
	{
		return FString();
	}

	int32 DotIndex = INDEX_NONE;
	if (Path.FindLastChar(TEXT('.'), DotIndex))
	{
		Path.LeftInline(DotIndex);
	}
	return Path;
}

bool IsValidGamePackagePath(const FString& Path)
{
	return Path.StartsWith(TEXT("/Game/")) && !Path.Contains(TEXT("."));
}

void AddError(FNteCharacterKawaiiAssetWriteResult& Result, const FString& Error)
{
	Result.Errors.Add(Error);
}

void AddError(FNteCharacterKawaiiWriteResult& Result, const FString& Error)
{
	Result.Errors.Add(Error);
}

void AddWarning(FNteCharacterKawaiiAssetWriteResult& Result, const FString& Warning)
{
	Result.Warnings.Add(Warning);
}

void AddWarning(FNteCharacterKawaiiWriteResult& Result, const FString& Warning)
{
	Result.Warnings.Add(Warning);
}

bool SaveAsset(UObject& Asset, FNteCharacterKawaiiWriteResult& Result, FNteCharacterKawaiiAssetWriteResult& AssetResult)
{
	UPackage* Package = Asset.GetPackage();
	if (!Package)
	{
		const FString Error = FString::Printf(TEXT("Asset %s has no package."), *Asset.GetName());
		AddError(AssetResult, Error);
		AddError(Result, Error);
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
		const FString Error = FString::Printf(TEXT("Failed to save package %s to %s."), *PackageName, *PackageFilename);
		AddError(AssetResult, Error);
		AddError(Result, Error);
		return false;
	}

	Result.SavedPackages.AddUnique(PackageName);
	AssetResult.Actions.Add(FString::Printf(TEXT("saved %s"), *PackageName));
	return true;
}

UObject* CreateOrLoadAssetOfClass(
	const FString& AssetPath,
	UClass* AssetClass,
	FNteCharacterKawaiiAssetWriteResult& AssetResult)
{
	const FString NormalizedPath = NormalizePackagePath(AssetPath);
	if (!IsValidGamePackagePath(NormalizedPath))
	{
		AddError(AssetResult, FString::Printf(TEXT("Asset path must be a /Game package path: %s"), *AssetPath));
		return nullptr;
	}
	if (!AssetClass)
	{
		AddError(AssetResult, FString::Printf(TEXT("Asset class is missing for %s."), *NormalizedPath));
		return nullptr;
	}

	const FString ObjectName = FPackageName::GetShortName(NormalizedPath);
	auto ValidateExistingAsset = [&AssetResult, AssetClass, &NormalizedPath](UObject* ExistingAsset) -> UObject*
	{
		if (!ExistingAsset)
		{
			return nullptr;
		}
		if (!ExistingAsset->IsA(AssetClass))
		{
			AddError(AssetResult, FString::Printf(
				TEXT("Existing asset is %s, not %s: %s"),
				*ExistingAsset->GetClass()->GetPathName(),
				*AssetClass->GetPathName(),
				*NormalizedPath));
			return nullptr;
		}

		AssetResult.bUpdated = true;
		AssetResult.Actions.Add(FString::Printf(TEXT("loaded existing asset %s"), *NormalizedPath));
		return ExistingAsset;
	};

	if (UPackage* ExistingPackage = FindPackage(nullptr, *NormalizedPath))
	{
		if (UObject* ExistingAsset = FindObject<UObject>(ExistingPackage, *ObjectName))
		{
			return ValidateExistingAsset(ExistingAsset);
		}
	}

	FString ExistingPackageFilename;
	if (FPackageName::DoesPackageExist(NormalizedPath, &ExistingPackageFilename))
	{
		if (UObject* ExistingAsset = NTEBuildTool::Editor::LoadAnyAssetByPath(NormalizedPath))
		{
			return ValidateExistingAsset(ExistingAsset);
		}

		AddError(AssetResult, FString::Printf(
			TEXT("Package exists but asset could not be loaded: %s (%s)"),
			*NormalizedPath,
			*ExistingPackageFilename));
		return nullptr;
	}

	UPackage* Package = CreatePackage(*NormalizedPath);
	if (!Package)
	{
		AddError(AssetResult, FString::Printf(TEXT("Could not create package: %s"), *NormalizedPath));
		return nullptr;
	}

	UObject* Asset = NewObject<UObject>(
		Package,
		AssetClass,
		FName(*ObjectName),
		RF_Public | RF_Standalone | RF_Transactional);
	if (!Asset)
	{
		AddError(AssetResult, FString::Printf(TEXT("Could not create asset: %s"), *NormalizedPath));
		return nullptr;
	}

	FAssetRegistryModule::AssetCreated(Asset);
	Package->MarkPackageDirty();
	AssetResult.bCreated = true;
	AssetResult.bUpdated = true;
	AssetResult.Actions.Add(FString::Printf(
		TEXT("created %s %s"),
		*AssetClass->GetPathName(),
		*NormalizedPath));
	return Asset;
}

FProperty* FindExactProperty(UStruct* Struct, const TCHAR* FieldName)
{
	if (!Struct)
	{
		return nullptr;
	}

	for (TFieldIterator<FProperty> It(Struct, EFieldIterationFlags::IncludeSuper); It; ++It)
	{
		FProperty* Property = *It;
		if (Property && Property->GetAuthoredName() == FieldName)
		{
			return Property;
		}
	}
	return nullptr;
}

bool StructHasField(UScriptStruct* Struct, const TCHAR* FieldName)
{
	return FindExactProperty(Struct, FieldName) != nullptr;
}

UScriptStruct* LoadScriptStruct(const TCHAR* StructPath)
{
	return LoadObject<UScriptStruct>(nullptr, StructPath);
}

void AddPresentType(FNteCharacterKawaiiSchemaProbeResult& Result, const FString& TypePath)
{
	Result.PresentTypes.AddUnique(TypePath);
}

void AddMissingType(FNteCharacterKawaiiSchemaProbeResult& Result, const FString& TypePath)
{
	Result.MissingTypes.AddUnique(TypePath);
	Result.Errors.Add(FString::Printf(TEXT("Missing KawaiiPhysics reflected type: %s"), *TypePath));
}

void CheckType(UObject* TypeObject, const TCHAR* TypePath, FNteCharacterKawaiiSchemaProbeResult& Result)
{
	if (TypeObject)
	{
		AddPresentType(Result, TypePath);
	}
	else
	{
		AddMissingType(Result, TypePath);
	}
}

void CheckField(UScriptStruct* Struct, const TCHAR* StructPath, const TCHAR* FieldName, FNteCharacterKawaiiSchemaProbeResult& Result)
{
	const FString FieldPath = FString::Printf(TEXT("%s.%s"), StructPath, FieldName);
	if (StructHasField(Struct, FieldName))
	{
		Result.PresentFields.AddUnique(FieldPath);
	}
	else
	{
		Result.MissingFields.AddUnique(FieldPath);
		Result.Errors.Add(FString::Printf(TEXT("Missing NTE Kawaii field: %s"), *FieldPath));
	}
}

void TrackPublicOnlyField(UScriptStruct* Struct, const TCHAR* StructPath, const TCHAR* FieldName, FNteCharacterKawaiiSchemaProbeResult& Result)
{
	if (StructHasField(Struct, FieldName))
	{
		Result.PublicOnlyFields.AddUnique(FString::Printf(TEXT("%s.%s"), StructPath, FieldName));
	}
}

FProperty* FindProperty(UStruct* Struct, const TCHAR* FieldName)
{
	return FindExactProperty(Struct, FieldName);
}

FProperty* FindPropertyByAnyName(UStruct* Struct, const TArray<FName>& FieldNames)
{
	if (!Struct)
	{
		return nullptr;
	}
	for (const FName FieldName : FieldNames)
	{
		if (FProperty* Property = FindProperty(Struct, *FieldName.ToString()))
		{
			return Property;
		}
	}
	return nullptr;
}

void SetBoolProperty(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName, const bool Value)
{
	if (FBoolProperty* Property = CastField<FBoolProperty>(FindProperty(ContainerStruct, FieldName)))
	{
		Property->SetPropertyValue_InContainer(Container, Value);
	}
}

void SetFloatProperty(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName, const float Value)
{
	if (FNumericProperty* Property = CastField<FNumericProperty>(FindProperty(ContainerStruct, FieldName)))
	{
		if (Property->IsFloatingPoint())
		{
			Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Container), Value);
		}
		else if (Property->IsInteger())
		{
			Property->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Container), static_cast<int64>(Value));
		}
	}
}

void SetIntProperty(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName, const int32 Value)
{
	if (FNumericProperty* Property = CastField<FNumericProperty>(FindProperty(ContainerStruct, FieldName)))
	{
		if (Property->IsInteger())
		{
			Property->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Container), static_cast<int64>(Value));
		}
		else if (Property->IsFloatingPoint())
		{
			Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Container), static_cast<double>(Value));
		}
	}
}

void SetNameProperty(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName, const FName Value)
{
	if (FNameProperty* Property = CastField<FNameProperty>(FindProperty(ContainerStruct, FieldName)))
	{
		Property->SetPropertyValue_InContainer(Container, Value);
	}
}

void SetGameplayTagProperty(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName, const FString& TagName)
{
	if (TagName.IsEmpty())
	{
		return;
	}

	if (FStructProperty* Property = CastField<FStructProperty>(FindProperty(ContainerStruct, FieldName)))
	{
		SetNameProperty(Property->ContainerPtrToValuePtr<void>(Container), Property->Struct, TEXT("TagName"), FName(*TagName));
	}
}

void SetVectorProperty(void* Container, UStruct* ContainerStruct, const TArray<FName>& FieldNames, const FVector& Value)
{
	if (FStructProperty* Property = CastField<FStructProperty>(FindPropertyByAnyName(ContainerStruct, FieldNames)))
	{
		if (Property->Struct == TBaseStructure<FVector>::Get())
		{
			*Property->ContainerPtrToValuePtr<FVector>(Container) = Value;
		}
	}
}

void SetRotatorProperty(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName, const FRotator& Value)
{
	if (FStructProperty* Property = CastField<FStructProperty>(FindProperty(ContainerStruct, FieldName)))
	{
		if (Property->Struct == TBaseStructure<FRotator>::Get())
		{
			*Property->ContainerPtrToValuePtr<FRotator>(Container) = Value;
		}
	}
}

void SetPlaneProperty(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName, const FPlane& Value)
{
	if (FStructProperty* Property = CastField<FStructProperty>(FindProperty(ContainerStruct, FieldName)))
	{
		if (Property->Struct == TBaseStructure<FPlane>::Get())
		{
			*Property->ContainerPtrToValuePtr<FPlane>(Container) = Value;
		}
	}
}

int64 ResolveEnumValue(UEnum* Enum, const FString& RawValue)
{
	if (!Enum || RawValue.IsEmpty())
	{
		return INDEX_NONE;
	}

	int64 Value = Enum->GetValueByNameString(RawValue);
	if (Value != INDEX_NONE)
	{
		return Value;
	}

	FString ShortValue = RawValue;
	ShortValue.Split(TEXT("::"), nullptr, &ShortValue, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
	{
		if (Enum->GetNameStringByIndex(Index).Equals(ShortValue, ESearchCase::IgnoreCase))
		{
			return Enum->GetValueByIndex(Index);
		}
	}
	return INDEX_NONE;
}

void SetEnumProperty(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName, const FString& RawValue)
{
	if (RawValue.IsEmpty())
	{
		return;
	}

	FProperty* Property = FindProperty(ContainerStruct, FieldName);
	if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		const int64 Value = ResolveEnumValue(EnumProperty->GetEnum(), RawValue);
		if (Value != INDEX_NONE)
		{
			EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(EnumProperty->ContainerPtrToValuePtr<void>(Container), Value);
		}
	}
	else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		const int64 Value = ResolveEnumValue(ByteProperty->Enum, RawValue);
		if (Value != INDEX_NONE)
		{
			ByteProperty->SetIntPropertyValue(ByteProperty->ContainerPtrToValuePtr<void>(Container), Value);
		}
	}
}

void SetObjectProperty(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName, UObject* Value)
{
	if (FObjectPropertyBase* Property = CastField<FObjectPropertyBase>(FindProperty(ContainerStruct, FieldName)))
	{
		Property->SetObjectPropertyValue_InContainer(Container, Value);
	}
	else if (FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(FindProperty(ContainerStruct, FieldName)))
	{
		FSoftObjectPtr SoftObjectPtr(Value);
		SoftObjectProperty->SetPropertyValue_InContainer(Container, SoftObjectPtr);
	}
}

void SetBoneReferenceStruct(void* BoneReferenceContainer, UScriptStruct* BoneReferenceStruct, const FString& BoneName)
{
	if (!BoneReferenceContainer || !BoneReferenceStruct || BoneName.IsEmpty())
	{
		return;
	}
	SetNameProperty(BoneReferenceContainer, BoneReferenceStruct, TEXT("BoneName"), FName(*BoneName));
}

void SetBoneReferenceProperty(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName, const FString& BoneName)
{
	if (FStructProperty* Property = CastField<FStructProperty>(FindProperty(ContainerStruct, FieldName)))
	{
		SetBoneReferenceStruct(Property->ContainerPtrToValuePtr<void>(Container), Property->Struct, BoneName);
	}
}

void AssignBoneReferenceArray(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName, const TArray<FString>& BoneNames)
{
	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(FindProperty(ContainerStruct, FieldName)))
	{
		if (FStructProperty* StructProperty = CastField<FStructProperty>(ArrayProperty->Inner))
		{
			FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Container));
			Helper.EmptyValues();
			for (const FString& BoneName : BoneNames)
			{
				if (BoneName.IsEmpty())
				{
					continue;
				}
				const int32 Index = Helper.AddValue();
				SetBoneReferenceStruct(Helper.GetRawPtr(Index), StructProperty->Struct, BoneName);
			}
		}
	}
}

void AssignNameArray(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName, const TArray<FString>& Names)
{
	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(FindProperty(ContainerStruct, FieldName)))
	{
		if (FNameProperty* NameProperty = CastField<FNameProperty>(ArrayProperty->Inner))
		{
			FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Container));
			Helper.EmptyValues();
			for (const FString& Name : Names)
			{
				if (Name.IsEmpty())
				{
					continue;
				}
				const int32 Index = Helper.AddValue();
				NameProperty->SetPropertyValue(Helper.GetRawPtr(Index), FName(*Name));
			}
		}
	}
}

void AssignAdditionalRootBones(
	void* Container,
	UStruct* ContainerStruct,
	const TArray<FNteCharacterKawaiiAdditionalRootBonePlanItem>& RootBones)
{
	if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(FindProperty(ContainerStruct, TEXT("AdditionalRootBones"))))
	{
		if (FStructProperty* StructProperty = CastField<FStructProperty>(ArrayProperty->Inner))
		{
			FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Container));
			Helper.EmptyValues();
			for (const FNteCharacterKawaiiAdditionalRootBonePlanItem& RootBone : RootBones)
			{
				if (RootBone.RootBone.IsEmpty())
				{
					continue;
				}
				const int32 Index = Helper.AddValue();
				void* Element = Helper.GetRawPtr(Index);
				SetBoneReferenceProperty(Element, StructProperty->Struct, TEXT("RootBone"), RootBone.RootBone);
				SetBoolProperty(Element, StructProperty->Struct, TEXT("bUseOverrideExcludeBones"), RootBone.bUseOverrideExcludeBones);
				AssignBoneReferenceArray(Element, StructProperty->Struct, TEXT("OverrideExcludeBones"), RootBone.OverrideExcludeBones);
			}
		}
	}
}

void AssignPhysicsSettings(
	void* Container,
	UStruct* ContainerStruct,
	const FNteCharacterKawaiiPhysicsSettingsPlanItem& Settings)
{
	if (FStructProperty* Property = CastField<FStructProperty>(FindProperty(ContainerStruct, TEXT("PhysicsSettings"))))
	{
		void* SettingsContainer = Property->ContainerPtrToValuePtr<void>(Container);
		SetFloatProperty(SettingsContainer, Property->Struct, TEXT("Damping"), Settings.Damping);
		SetFloatProperty(SettingsContainer, Property->Struct, TEXT("Stiffness"), Settings.Stiffness);
		SetFloatProperty(SettingsContainer, Property->Struct, TEXT("WorldDampingLocation"), Settings.WorldDampingLocation);
		SetFloatProperty(SettingsContainer, Property->Struct, TEXT("WorldDampingRotation"), Settings.WorldDampingRotation);
		SetFloatProperty(SettingsContainer, Property->Struct, TEXT("Radius"), Settings.Radius);
		SetFloatProperty(SettingsContainer, Property->Struct, TEXT("LimitAngle"), Settings.LimitAngle);
		if (Settings.bHasForwardMoveOffset)
		{
			SetFloatProperty(SettingsContainer, Property->Struct, TEXT("ForwardMoveOffset"), Settings.ForwardMoveOffset);
		}
	}
}

void SetRuntimeFloatCurveExternalCurve(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName, UCurveFloat* Curve)
{
	if (!Curve)
	{
		return;
	}

	if (FStructProperty* Property = CastField<FStructProperty>(FindProperty(ContainerStruct, FieldName)))
	{
		SetObjectProperty(Property->ContainerPtrToValuePtr<void>(Container), Property->Struct, TEXT("ExternalCurve"), Curve);
	}
}

void SetCommonLimitFields(void* LimitContainer, UScriptStruct* LimitStruct, const FNteCharacterKawaiiLimitPlanItem& Limit)
{
	SetBoneReferenceProperty(LimitContainer, LimitStruct, TEXT("DrivingBone"), Limit.DrivingBone);
	SetVectorProperty(LimitContainer, LimitStruct, {TEXT("OffSetLocation"), TEXT("OffsetLocation")}, Limit.OffsetLocation);
	SetRotatorProperty(LimitContainer, LimitStruct, TEXT("OffsetRotation"), Limit.OffsetRotation);
	SetBoolProperty(LimitContainer, LimitStruct, TEXT("bEnable"), Limit.bEnable);
	SetEnumProperty(LimitContainer, LimitStruct, TEXT("SourceType"), TEXT("DataAsset"));
}

bool AssignLimitArray(
	UObject& DataAsset,
	const TCHAR* ArrayName,
	const TArray<const FNteCharacterKawaiiLimitPlanItem*>& Limits,
	FNteCharacterKawaiiAssetWriteResult& AssetResult)
{
	FArrayProperty* ArrayProperty = CastField<FArrayProperty>(FindProperty(DataAsset.GetClass(), ArrayName));
	if (!ArrayProperty)
	{
		AddWarning(AssetResult, FString::Printf(TEXT("Kawaii limits asset class has no array property %s."), ArrayName));
		return false;
	}

	FStructProperty* StructProperty = CastField<FStructProperty>(ArrayProperty->Inner);
	if (!StructProperty)
	{
		AddWarning(AssetResult, FString::Printf(TEXT("Kawaii limits asset property %s is not a struct array."), ArrayName));
		return false;
	}

	FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(&DataAsset));
	Helper.EmptyValues();
	for (const FNteCharacterKawaiiLimitPlanItem* Limit : Limits)
	{
		if (!Limit)
		{
			continue;
		}

		const int32 Index = Helper.AddValue();
		void* Element = Helper.GetRawPtr(Index);
		SetCommonLimitFields(Element, StructProperty->Struct, *Limit);
		SetFloatProperty(Element, StructProperty->Struct, TEXT("Radius"), Limit->Radius);
		SetFloatProperty(Element, StructProperty->Struct, TEXT("Length"), Limit->Length);
		SetFloatProperty(Element, StructProperty->Struct, TEXT("SphereRadius"), Limit->SphereRadius);
		SetVectorProperty(Element, StructProperty->Struct, {TEXT("Extent")}, Limit->Extent);
		SetPlaneProperty(Element, StructProperty->Struct, TEXT("Plane"), Limit->Plane);
		SetEnumProperty(Element, StructProperty->Struct, TEXT("LimitType"), Limit->LimitType);
	}

	AssetResult.Actions.Add(FString::Printf(TEXT("wrote %d %s entries"), Limits.Num(), ArrayName));
	return true;
}

void SplitLimitsByKind(
	const TArray<FNteCharacterKawaiiLimitPlanItem>& Limits,
	TArray<const FNteCharacterKawaiiLimitPlanItem*>& OutSpherical,
	TArray<const FNteCharacterKawaiiLimitPlanItem*>& OutCapsule,
	TArray<const FNteCharacterKawaiiLimitPlanItem*>& OutBox,
	TArray<const FNteCharacterKawaiiLimitPlanItem*>& OutPlanar)
{
	for (const FNteCharacterKawaiiLimitPlanItem& Limit : Limits)
	{
		if (Limit.LimitKind.Equals(TEXT("Spherical"), ESearchCase::IgnoreCase)
			|| Limit.LimitKind.Equals(TEXT("Sphere"), ESearchCase::IgnoreCase))
		{
			OutSpherical.Add(&Limit);
		}
		else if (Limit.LimitKind.Equals(TEXT("Capsule"), ESearchCase::IgnoreCase))
		{
			OutCapsule.Add(&Limit);
		}
		else if (Limit.LimitKind.Equals(TEXT("Box"), ESearchCase::IgnoreCase))
		{
			OutBox.Add(&Limit);
		}
		else if (Limit.LimitKind.Equals(TEXT("Planar"), ESearchCase::IgnoreCase)
			|| Limit.LimitKind.Equals(TEXT("Plane"), ESearchCase::IgnoreCase))
		{
			OutPlanar.Add(&Limit);
		}
	}
}

USkeleton* LoadTargetSkeleton(const FNteCharacterKawaiiPresetPlanItem& Preset, FNteCharacterKawaiiAssetWriteResult& AssetResult)
{
	if (Preset.TargetMeshPath.IsEmpty())
	{
		return nullptr;
	}

	USkeletalMesh* Mesh = NTEBuildTool::Editor::LoadAssetByPath<USkeletalMesh>(Preset.TargetMeshPath);
	if (!Mesh)
	{
		AddWarning(AssetResult, FString::Printf(TEXT("Target mesh could not be loaded for skeleton preview: %s"), *Preset.TargetMeshPath));
		return nullptr;
	}
	return Mesh->GetSkeleton();
}

USkeletalMesh* LoadTargetSkeletalMesh(const FNteCharacterKawaiiPresetPlanItem& Preset, FNteCharacterKawaiiAssetWriteResult& AssetResult)
{
	if (Preset.TargetMeshPath.IsEmpty())
	{
		AddError(AssetResult, FString::Printf(TEXT("Kawaii preset '%s' has no target mesh path."), *Preset.Id));
		return nullptr;
	}

	USkeletalMesh* Mesh = NTEBuildTool::Editor::LoadAssetByPath<USkeletalMesh>(Preset.TargetMeshPath);
	if (!Mesh)
	{
		AddError(AssetResult, FString::Printf(
			TEXT("Kawaii preset '%s' target mesh could not be loaded: %s"),
			*Preset.Id,
			*Preset.TargetMeshPath));
		return nullptr;
	}
	if (!Mesh->GetSkeleton())
	{
		AddError(AssetResult, FString::Printf(
			TEXT("Kawaii preset '%s' target mesh has no Skeleton: %s"),
			*Preset.Id,
			*Preset.TargetMeshPath));
		return nullptr;
	}
	return Mesh;
}

bool ApplyAnimBlueprintSkeleton(
	UAnimBlueprint& AnimBlueprint,
	USkeletalMesh& TargetMesh,
	FNteCharacterKawaiiAssetWriteResult& AssetResult)
{
	USkeleton* TargetSkeleton = TargetMesh.GetSkeleton();
	if (!TargetSkeleton)
	{
		AddError(AssetResult, FString::Printf(TEXT("Target SkeletalMesh has no Skeleton: %s"), *TargetMesh.GetPathName()));
		return false;
	}

	bool bChanged = false;
	if (AnimBlueprint.TargetSkeleton != TargetSkeleton)
	{
		AnimBlueprint.Modify();
		AnimBlueprint.TargetSkeleton = TargetSkeleton;
		bChanged = true;
		AssetResult.Actions.Add(FString::Printf(TEXT("set AnimBlueprint TargetSkeleton to %s"), *TargetSkeleton->GetPathName()));
	}
	if (AnimBlueprint.GetPreviewMesh() != &TargetMesh)
	{
		AnimBlueprint.Modify();
		AnimBlueprint.SetPreviewMesh(&TargetMesh);
		bChanged = true;
		AssetResult.Actions.Add(FString::Printf(TEXT("set AnimBlueprint PreviewSkeletalMesh to %s"), *TargetMesh.GetPathName()));
	}
	if (UAnimBlueprintGeneratedClass* GeneratedAnimClass = Cast<UAnimBlueprintGeneratedClass>(AnimBlueprint.GeneratedClass))
	{
		if (GeneratedAnimClass->TargetSkeleton != TargetSkeleton)
		{
			GeneratedAnimClass->Modify();
			GeneratedAnimClass->TargetSkeleton = TargetSkeleton;
			bChanged = true;
		}
	}
	if (UAnimBlueprintGeneratedClass* SkeletonGeneratedAnimClass = Cast<UAnimBlueprintGeneratedClass>(AnimBlueprint.SkeletonGeneratedClass))
	{
		if (SkeletonGeneratedAnimClass->TargetSkeleton != TargetSkeleton)
		{
			SkeletonGeneratedAnimClass->Modify();
			SkeletonGeneratedAnimClass->TargetSkeleton = TargetSkeleton;
			bChanged = true;
		}
	}
	if (bChanged)
	{
		AssetResult.bUpdated = true;
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&AnimBlueprint);
	}
	return true;
}

UAnimBlueprint* CreateOrLoadAnimBlueprint(
	const FString& BlueprintPath,
	USkeletalMesh& TargetMesh,
	const bool bUseAttachedMeshParent,
	FNteCharacterKawaiiAssetWriteResult& AssetResult)
{
	const FString NormalizedPath = NormalizePackagePath(BlueprintPath);
	if (!IsValidGamePackagePath(NormalizedPath))
	{
		AddError(AssetResult, FString::Printf(TEXT("AnimBlueprint path must be a /Game package path: %s"), *BlueprintPath));
		return nullptr;
	}

	const FString ObjectName = FPackageName::GetShortName(NormalizedPath);
	auto LoadExistingAnimBlueprint = [&AssetResult, &NormalizedPath, &TargetMesh, bUseAttachedMeshParent](UObject* ExistingAsset) -> UAnimBlueprint*
	{
		UAnimBlueprint* ExistingBlueprint = Cast<UAnimBlueprint>(ExistingAsset);
		if (!ExistingBlueprint)
		{
			AddError(AssetResult, FString::Printf(
				TEXT("Existing asset is %s, not an AnimBlueprint: %s"),
				ExistingAsset ? *ExistingAsset->GetClass()->GetPathName() : TEXT("<null>"),
				*NormalizedPath));
			return nullptr;
		}
		UClass* ExpectedParentClass = bUseAttachedMeshParent
			? NTEBuildTool::Editor::GetAttachedMeshAnimInstanceParentClass()
			: UAnimInstance::StaticClass();
		bool bParentChanged = false;
		FString ParentError;
		if (!ExpectedParentClass || !NTEBuildTool::Editor::EnsureBlueprintParentClass(
			*ExistingBlueprint,
			*ExpectedParentClass,
			bParentChanged,
			ParentError))
		{
			AddError(AssetResult, ParentError.IsEmpty()
				? TEXT("Attached mesh AnimBlueprint parent class is unavailable.")
				: ParentError);
			return nullptr;
		}
		if (bParentChanged)
		{
			AssetResult.Actions.Add(FString::Printf(
				TEXT("reparented attached AnimBlueprint to %s"),
				*ExpectedParentClass->GetPathName()));
		}
		AssetResult.bUpdated = true;
		AssetResult.Actions.Add(FString::Printf(TEXT("loaded existing AnimBlueprint %s"), *NormalizedPath));
		ApplyAnimBlueprintSkeleton(*ExistingBlueprint, TargetMesh, AssetResult);
		return ExistingBlueprint;
	};

	if (UPackage* ExistingPackage = FindPackage(nullptr, *NormalizedPath))
	{
		if (UObject* ExistingAsset = FindObject<UObject>(ExistingPackage, *ObjectName))
		{
			return LoadExistingAnimBlueprint(ExistingAsset);
		}
	}

	FString ExistingPackageFilename;
	if (FPackageName::DoesPackageExist(NormalizedPath, &ExistingPackageFilename))
	{
		if (UObject* ExistingAsset = NTEBuildTool::Editor::LoadAnyAssetByPath(NormalizedPath))
		{
			return LoadExistingAnimBlueprint(ExistingAsset);
		}
		AddError(AssetResult, FString::Printf(
			TEXT("Package exists but AnimBlueprint asset could not be loaded: %s (%s)"),
			*NormalizedPath,
			*ExistingPackageFilename));
		return nullptr;
	}

	UPackage* Package = CreatePackage(*NormalizedPath);
	if (!Package)
	{
		AddError(AssetResult, FString::Printf(TEXT("Could not create package: %s"), *NormalizedPath));
		return nullptr;
	}

	UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
	Factory->BlueprintType = BPTYPE_Normal;
	Factory->ParentClass = bUseAttachedMeshParent
		? NTEBuildTool::Editor::GetAttachedMeshAnimInstanceParentClass()
		: UAnimInstance::StaticClass();
	if (!Factory->ParentClass)
	{
		AddError(AssetResult, TEXT("Attached mesh AnimBlueprint parent class is unavailable."));
		return nullptr;
	}
	Factory->TargetSkeleton = TargetMesh.GetSkeleton();
	Factory->PreviewSkeletalMesh = &TargetMesh;
	Factory->bTemplate = false;

	UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(Factory->FactoryCreateNew(
		UAnimBlueprint::StaticClass(),
		Package,
		FName(*ObjectName),
		RF_Public | RF_Standalone,
		nullptr,
		GWarn));
	if (!Blueprint)
	{
		AddError(AssetResult, FString::Printf(TEXT("Could not create AnimBlueprint: %s"), *NormalizedPath));
		return nullptr;
	}

	Blueprint->SetFlags(RF_Public | RF_Standalone);
	ApplyAnimBlueprintSkeleton(*Blueprint, TargetMesh, AssetResult);
	FAssetRegistryModule::AssetCreated(Blueprint);
	Package->MarkPackageDirty();
	AssetResult.bCreated = true;
	AssetResult.bUpdated = true;
	AssetResult.Actions.Add(FString::Printf(TEXT("created AnimBlueprint %s for mesh %s"), *NormalizedPath, *TargetMesh.GetPathName()));
	return Blueprint;
}

UEdGraph* EnsureAnimGraph(UAnimBlueprint& AnimBlueprint, FNteCharacterKawaiiAssetWriteResult& AssetResult)
{
	for (UEdGraph* Graph : AnimBlueprint.FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == UEdGraphSchema_K2::GN_AnimGraph)
		{
			return Graph;
		}
	}

	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		&AnimBlueprint,
		UEdGraphSchema_K2::GN_AnimGraph,
		UAnimationGraph::StaticClass(),
		UAnimationGraphSchema::StaticClass());
	if (!NewGraph)
	{
		AddError(AssetResult, TEXT("Could not create AnimGraph."));
		return nullptr;
	}
	FBlueprintEditorUtils::AddDomainSpecificGraph(&AnimBlueprint, NewGraph);
	NewGraph->bAllowDeletion = false;
	AssetResult.Actions.Add(TEXT("created AnimGraph"));
	return NewGraph;
}

UAnimGraphNode_Base* FindAnimGraphRootNode(UEdGraph& Graph)
{
	UClass* RootClass = LoadObject<UClass>(nullptr, RootAnimGraphNodeClassPath);
	if (!RootClass)
	{
		return nullptr;
	}
	for (UEdGraphNode* Node : Graph.Nodes)
	{
		if (Node && Node->IsA(RootClass))
		{
			return Cast<UAnimGraphNode_Base>(Node);
		}
	}
	return nullptr;
}

void RemoveGeneratedKawaiiNodes(UEdGraph& Graph)
{
	TArray<UEdGraphNode*> NodesToRemove;
	for (UEdGraphNode* Node : Graph.Nodes)
	{
		if (Node && Node->NodeComment.StartsWith(GeneratedKawaiiNodeComment))
		{
			NodesToRemove.Add(Node);
		}
	}
	for (UEdGraphNode* Node : NodesToRemove)
	{
		Graph.RemoveNode(Node);
	}
}

UEdGraphPin* FindPosePin(UEdGraphNode& Node, const EEdGraphPinDirection Direction, const bool bComponentSpace)
{
	for (UEdGraphPin* Pin : Node.Pins)
	{
		if (!Pin || Pin->Direction != Direction)
		{
			continue;
		}
		const bool bMatches = bComponentSpace
			? UAnimationGraphSchema::IsComponentSpacePosePin(Pin->PinType)
			: UAnimationGraphSchema::IsLocalSpacePosePin(Pin->PinType);
		if (bMatches)
		{
			return Pin;
		}
	}
	return nullptr;
}

bool TryConnectPosePins(UEdGraphPin* FromPin, UEdGraphPin* ToPin, FNteCharacterKawaiiAssetWriteResult& AssetResult, const TCHAR* Description)
{
	if (!FromPin || !ToPin)
	{
		AddError(AssetResult, FString::Printf(TEXT("Could not find pose pins for %s."), Description));
		return false;
	}
	if (const UEdGraphSchema* Schema = FromPin->GetSchema())
	{
		if (Schema->TryCreateConnection(FromPin, ToPin))
		{
			return true;
		}
	}
	AddError(AssetResult, FString::Printf(TEXT("Could not connect pose pins for %s."), Description));
	return false;
}

bool AnyNodeOfClassLinksToClass(const UEdGraph& Graph, const UClass& SourceClass, const UClass& TargetClass)
{
	for (const UEdGraphNode* Node : Graph.Nodes)
	{
		if (!Node || !Node->IsA(&SourceClass))
		{
			continue;
		}
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin)
			{
				continue;
			}
			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (LinkedPin && LinkedPin->GetOwningNode() && LinkedPin->GetOwningNode()->IsA(&TargetClass))
				{
					return true;
				}
			}
		}
	}
	return false;
}

UEdGraphPin* FindLocalPoseInputPinAtIndex(UEdGraphNode& Node, const int32 InputIndex)
{
	int32 CurrentIndex = 0;
	for (UEdGraphPin* Pin : Node.Pins)
	{
		if (Pin
			&& Pin->Direction == EGPD_Input
			&& UAnimationGraphSchema::IsLocalSpacePosePin(Pin->PinType))
		{
			if (CurrentIndex++ == InputIndex)
			{
				return Pin;
			}
		}
	}
	return nullptr;
}

TArray<FName> CollectKawaiiChainRoots(const TArray<const FNteCharacterKawaiiPresetPlanItem*>& Presets)
{
	TArray<FName> Roots;
	for (const FNteCharacterKawaiiPresetPlanItem* Preset : Presets)
	{
		if (!Preset)
		{
			continue;
		}
		if (!Preset->RootBone.IsEmpty())
		{
			Roots.AddUnique(FName(*Preset->RootBone));
		}
		for (const FNteCharacterKawaiiAdditionalRootBonePlanItem& AdditionalRoot : Preset->AdditionalRootBones)
		{
			if (!AdditionalRoot.RootBone.IsEmpty())
			{
				Roots.AddUnique(FName(*AdditionalRoot.RootBone));
			}
		}
	}
	return Roots;
}

TArray<FName> ResolveReferencePoseBranchRoots(
	const USkeletalMesh& TargetMesh,
	const TArray<const FNteCharacterKawaiiPresetPlanItem*>& Presets,
	FNteCharacterKawaiiAssetWriteResult* AssetResult = nullptr)
{
	const FReferenceSkeleton& ReferenceSkeleton = TargetMesh.GetRefSkeleton();
	TArray<int32> DirectChildCounts;
	DirectChildCounts.Init(0, ReferenceSkeleton.GetNum());
	for (int32 BoneIndex = 0; BoneIndex < ReferenceSkeleton.GetNum(); ++BoneIndex)
	{
		const int32 ParentIndex = ReferenceSkeleton.GetParentIndex(BoneIndex);
		if (DirectChildCounts.IsValidIndex(ParentIndex))
		{
			++DirectChildCounts[ParentIndex];
		}
	}

	TArray<FName> BranchRoots;
	for (const FName SimulationRoot : CollectKawaiiChainRoots(Presets))
	{
		int32 BranchRootIndex = ReferenceSkeleton.FindBoneIndex(SimulationRoot);
		if (BranchRootIndex == INDEX_NONE)
		{
			if (AssetResult)
			{
				AddError(*AssetResult, FString::Printf(
					TEXT("Post Process reference-pose stabilizer could not resolve Kawaii root %s in %s."),
					*SimulationRoot.ToString(),
					*TargetMesh.GetPathName()));
			}
			continue;
		}

		for (;;)
		{
			const int32 ParentIndex = ReferenceSkeleton.GetParentIndex(BranchRootIndex);
			if (!DirectChildCounts.IsValidIndex(ParentIndex) || DirectChildCounts[ParentIndex] != 1)
			{
				break;
			}
			BranchRootIndex = ParentIndex;
		}

		BranchRoots.AddUnique(ReferenceSkeleton.GetBoneName(BranchRootIndex));
	}
	return BranchRoots;
}

bool ConfigureCustomChainReferencePoseBlend(
	UAnimGraphNode_Base& GraphNode,
	const TArray<FName>& BranchRoots,
	FNteCharacterKawaiiAssetWriteResult& AssetResult)
{
	UAnimGraphNode_LayeredBoneBlend* LayeredBlendNode = Cast<UAnimGraphNode_LayeredBoneBlend>(&GraphNode);
	if (!LayeredBlendNode)
	{
		AddError(AssetResult, TEXT("Reference-pose stabilizer is not a LayeredBoneBlend node."));
		return false;
	}
	if (BranchRoots.IsEmpty())
	{
		AddError(AssetResult, TEXT("Post Process Kawaii reference-pose stabilizer has no safe branch roots."));
		return false;
	}

	FAnimNode_LayeredBoneBlend& RuntimeNode = LayeredBlendNode->Node;
	RuntimeNode.BlendMode = ELayeredBoneBlendMode::BranchFilter;
	RuntimeNode.BlendPoses.SetNum(1);
	RuntimeNode.BlendWeights.SetNum(1);
	RuntimeNode.BlendWeights[0] = 1.0f;
	RuntimeNode.LayerSetup.SetNum(1);
	RuntimeNode.LayerSetup[0].BranchFilters.Reset();
	for (const FName BranchRoot : BranchRoots)
	{
		FBranchFilter& Filter = RuntimeNode.LayerSetup[0].BranchFilters.AddDefaulted_GetRef();
		Filter.BoneName = BranchRoot;
		Filter.BlendDepth = 0;
	}
	RuntimeNode.InvalidatePerBoneBlendWeights();
	LayeredBlendNode->ReconstructNode();
	AssetResult.Actions.Add(FString::Printf(
		TEXT("stabilized %d topology-safe Kawaii branch root(s) from target-mesh reference pose"),
		BranchRoots.Num()));
	return true;
}

UAnimGraphNode_Base* AddAnimGraphNodeByClass(
	UEdGraph& Graph,
	UClass* NodeClass,
	const int32 NodePosX,
	const int32 NodePosY,
	FNteCharacterKawaiiAssetWriteResult& AssetResult,
	const TCHAR* Label)
{
	if (!NodeClass)
	{
		AddError(AssetResult, FString::Printf(TEXT("AnimGraph node class is missing for %s."), Label));
		return nullptr;
	}
	if (!NodeClass->IsChildOf(UAnimGraphNode_Base::StaticClass()))
	{
		AddError(AssetResult, FString::Printf(TEXT("Class %s is not an AnimGraph node class."), *NodeClass->GetPathName()));
		return nullptr;
	}

	FGraphNodeCreator<UAnimGraphNode_Base> NodeCreator(Graph);
	UAnimGraphNode_Base* Node = NodeCreator.CreateNode(false, NodeClass);
	if (!Node)
	{
		AddError(AssetResult, FString::Printf(TEXT("Could not create AnimGraph node %s."), Label));
		return nullptr;
	}
	Node->NodePosX = NodePosX;
	Node->NodePosY = NodePosY;
	Node->NodeComment = GeneratedKawaiiNodeComment;
	NodeCreator.Finalize();
	return Node;
}

bool GetAnimNodeStructContainer(UObject& AnimGraphNode, void*& OutContainer, UScriptStruct*& OutStruct)
{
	if (FStructProperty* NodeProperty = CastField<FStructProperty>(FindProperty(AnimGraphNode.GetClass(), TEXT("Node"))))
	{
		OutContainer = NodeProperty->ContainerPtrToValuePtr<void>(&AnimGraphNode);
		OutStruct = NodeProperty->Struct;
		return OutContainer && OutStruct;
	}
	return false;
}

FString NormalizeBoneForwardAxis(FString Axis)
{
	Axis.TrimStartAndEndInline();
	if (Axis.Equals(TEXT("X"), ESearchCase::IgnoreCase) || Axis.Equals(TEXT("+X"), ESearchCase::IgnoreCase))
	{
		return TEXT("X_Positive");
	}
	if (Axis.Equals(TEXT("-X"), ESearchCase::IgnoreCase))
	{
		return TEXT("X_Negative");
	}
	if (Axis.Equals(TEXT("Y"), ESearchCase::IgnoreCase) || Axis.Equals(TEXT("+Y"), ESearchCase::IgnoreCase))
	{
		return TEXT("Y_Positive");
	}
	if (Axis.Equals(TEXT("-Y"), ESearchCase::IgnoreCase))
	{
		return TEXT("Y_Negative");
	}
	if (Axis.Equals(TEXT("Z"), ESearchCase::IgnoreCase) || Axis.Equals(TEXT("+Z"), ESearchCase::IgnoreCase))
	{
		return TEXT("Z_Positive");
	}
	if (Axis.Equals(TEXT("-Z"), ESearchCase::IgnoreCase))
	{
		return TEXT("Z_Negative");
	}
	return Axis;
}

const TCHAR* CurveFieldNameForKind(const FString& CurveKind)
{
	if (CurveKind.Equals(TEXT("Damping"), ESearchCase::IgnoreCase))
	{
		return TEXT("DampingCurveData");
	}
	if (CurveKind.Equals(TEXT("Stiffness"), ESearchCase::IgnoreCase))
	{
		return TEXT("StiffnessCurveData");
	}
	if (CurveKind.Equals(TEXT("WorldDampingLocation"), ESearchCase::IgnoreCase))
	{
		return TEXT("WorldDampingLocationCurveData");
	}
	if (CurveKind.Equals(TEXT("WorldDampingRotation"), ESearchCase::IgnoreCase))
	{
		return TEXT("WorldDampingRotationCurveData");
	}
	if (CurveKind.Equals(TEXT("Radius"), ESearchCase::IgnoreCase))
	{
		return TEXT("RadiusCurveData");
	}
	if (CurveKind.Equals(TEXT("LimitAngle"), ESearchCase::IgnoreCase))
	{
		return TEXT("LimitAngleCurveData");
	}
	return nullptr;
}

void ConfigureCopyPoseNode(UAnimGraphNode_Base& CopyPoseNode, FNteCharacterKawaiiAssetWriteResult& AssetResult)
{
	void* NodeContainer = nullptr;
	UScriptStruct* NodeStruct = nullptr;
	if (!GetAnimNodeStructContainer(CopyPoseNode, NodeContainer, NodeStruct))
	{
		AddError(AssetResult, TEXT("CopyPoseFromMesh graph node has no reflected Node struct."));
		return;
	}

	SetBoolProperty(NodeContainer, NodeStruct, TEXT("bUseAttachedParent"), true);
	SetBoolProperty(NodeContainer, NodeStruct, TEXT("bCopyCurves"), true);
	SetBoolProperty(NodeContainer, NodeStruct, TEXT("bCopyCustomAttributes"), true);
	SetBoolProperty(NodeContainer, NodeStruct, TEXT("bUseMeshPose"), false);
}

void ConfigureKawaiiNode(
	UAnimGraphNode_Base& KawaiiGraphNode,
	const FNteCharacterKawaiiPresetPlanItem& Preset,
	FNteCharacterKawaiiAssetWriteResult& AssetResult)
{
	void* NodeContainer = nullptr;
	UScriptStruct* NodeStruct = nullptr;
	if (!GetAnimNodeStructContainer(KawaiiGraphNode, NodeContainer, NodeStruct))
	{
		AddError(AssetResult, TEXT("Kawaii graph node has no reflected Node struct."));
		return;
	}

	SetBoneReferenceProperty(NodeContainer, NodeStruct, TEXT("RootBone"), Preset.RootBone);
	AssignBoneReferenceArray(NodeContainer, NodeStruct, TEXT("ExcludeBones"), Preset.ExcludeBones);
	AssignAdditionalRootBones(NodeContainer, NodeStruct, Preset.AdditionalRootBones);
	AssignPhysicsSettings(NodeContainer, NodeStruct, Preset.PhysicsSettings);
	SetFloatProperty(NodeContainer, NodeStruct, TEXT("DummyBoneLength"), Preset.DummyBoneLength);
	SetEnumProperty(NodeContainer, NodeStruct, TEXT("BoneForwardAxis"), NormalizeBoneForwardAxis(Preset.BoneForwardAxis));
	SetIntProperty(NodeContainer, NodeStruct, TEXT("TargetFrameRate"), Preset.TargetFramerate);
	SetBoolProperty(NodeContainer, NodeStruct, TEXT("OverrideTargetFramerate"), Preset.bOverrideTargetFramerate);
	SetIntProperty(NodeContainer, NodeStruct, TEXT("WarmUpFrames"), Preset.WarmUpFrames);
	SetBoolProperty(NodeContainer, NodeStruct, TEXT("bUseWarmUpWhenResetDynamics"), Preset.bUseWarmUpWhenResetDynamics);
	SetBoolProperty(NodeContainer, NodeStruct, TEXT("bNeedWarmUp"), Preset.bNeedWarmUp);
	SetFloatProperty(NodeContainer, NodeStruct, TEXT("TeleportDistanceThreshold"), Preset.TeleportDistanceThreshold);
	SetFloatProperty(NodeContainer, NodeStruct, TEXT("TeleportRotationThreshold"), Preset.TeleportRotationThreshold);
	SetEnumProperty(NodeContainer, NodeStruct, TEXT("PlanarConstraint"), Preset.PlanarConstraint);
	SetBoolProperty(NodeContainer, NodeStruct, TEXT("ResetBoneTransformWhenBoneNotFound"), Preset.bResetBoneTransformWhenBoneNotFound);
	SetEnumProperty(NodeContainer, NodeStruct, TEXT("BoneConstraintGlobalComplianceType"), Preset.BoneConstraintGlobalComplianceType);
	SetIntProperty(NodeContainer, NodeStruct, TEXT("BoneConstraintIterationCountBeforeCollision"), Preset.BoneConstraintIterationCountBeforeCollision);
	SetIntProperty(NodeContainer, NodeStruct, TEXT("BoneConstraintIterationCountAfterCollision"), Preset.BoneConstraintIterationCountAfterCollision);
	SetBoolProperty(NodeContainer, NodeStruct, TEXT("bAutoAddChildDummyBoneConstraint"), Preset.bAutoAddChildDummyBoneConstraint);
	SetVectorProperty(NodeContainer, NodeStruct, {TEXT("Gravity")}, Preset.Gravity);
	SetBoolProperty(NodeContainer, NodeStruct, TEXT("bEnableWind"), Preset.bEnableWind);
	SetFloatProperty(NodeContainer, NodeStruct, TEXT("WindScale"), Preset.WindScale);
	if (Preset.bHasUseRelativeMove)
	{
		SetBoolProperty(NodeContainer, NodeStruct, TEXT("bUseRelativeMove"), Preset.bUseRelativeMove);
		SetVectorProperty(NodeContainer, NodeStruct, {TEXT("MovementReferenceDisplacement")}, Preset.MovementReferenceDisplacement);
	}
	SetBoolProperty(NodeContainer, NodeStruct, TEXT("bAllowWorldCollision"), Preset.bAllowWorldCollision);
	SetBoolProperty(NodeContainer, NodeStruct, TEXT("bOverrideCollisionParams"), Preset.bOverrideCollisionParams);
	SetBoolProperty(NodeContainer, NodeStruct, TEXT("bIgnoreSelfComponent"), Preset.bIgnoreSelfComponent);
	AssignBoneReferenceArray(NodeContainer, NodeStruct, TEXT("IgnoreBones"), Preset.IgnoreBones);
	AssignNameArray(NodeContainer, NodeStruct, TEXT("IgnoreBoneNamePrefix"), Preset.IgnoreBoneNamePrefix);
	SetGameplayTagProperty(NodeContainer, NodeStruct, TEXT("KawaiiPhysicsTag"), Preset.KawaiiPhysicsTag);

	if (UObject* LimitsAsset = NTEBuildTool::Editor::LoadAnyAssetByPath(Preset.OutputLimitsDataAssetPath))
	{
		SetObjectProperty(NodeContainer, NodeStruct, TEXT("LimitsDataAsset"), LimitsAsset);
	}
	else if (!Preset.OutputLimitsDataAssetPath.IsEmpty())
	{
		AddWarning(AssetResult, FString::Printf(TEXT("Could not load generated Kawaii limits asset for node '%s': %s"), *Preset.Id, *Preset.OutputLimitsDataAssetPath));
	}
	if (UObject* PhysicsAsset = NTEBuildTool::Editor::LoadAnyAssetByPath(Preset.PhysicsAssetForLimitsPath))
	{
		SetObjectProperty(NodeContainer, NodeStruct, TEXT("PhysicsAssetForLimits"), PhysicsAsset);
	}
	else if (!Preset.PhysicsAssetForLimitsPath.IsEmpty())
	{
		AddWarning(AssetResult, FString::Printf(TEXT("Could not load PhysicsAssetForLimits for node '%s': %s"), *Preset.Id, *Preset.PhysicsAssetForLimitsPath));
	}
	if (UObject* ConstraintsAsset = NTEBuildTool::Editor::LoadAnyAssetByPath(Preset.OutputBoneConstraintsDataAssetPath))
	{
		SetObjectProperty(NodeContainer, NodeStruct, TEXT("BoneConstraintsDataAsset"), ConstraintsAsset);
	}
	else if (!Preset.OutputBoneConstraintsDataAssetPath.IsEmpty())
	{
		AddWarning(AssetResult, FString::Printf(TEXT("Could not load generated Kawaii bone constraints asset for node '%s': %s"), *Preset.Id, *Preset.OutputBoneConstraintsDataAssetPath));
	}

	for (const FNteCharacterKawaiiCurvePlanItem& Curve : Preset.Curves)
	{
		const TCHAR* CurveFieldName = CurveFieldNameForKind(Curve.CurveKind);
		if (!CurveFieldName)
		{
			AddWarning(AssetResult, FString::Printf(TEXT("Skipped unknown Kawaii curve kind '%s' for node '%s'."), *Curve.CurveKind, *Preset.Id));
			continue;
		}
		if (UCurveFloat* CurveAsset = NTEBuildTool::Editor::LoadAssetByPath<UCurveFloat>(Curve.OutputCurvePath))
		{
			SetRuntimeFloatCurveExternalCurve(NodeContainer, NodeStruct, CurveFieldName, CurveAsset);
		}
		else if (!Curve.OutputCurvePath.IsEmpty())
		{
			AddWarning(AssetResult, FString::Printf(TEXT("Could not load generated Kawaii curve '%s' for node '%s': %s"), *Curve.CurveKind, *Preset.Id, *Curve.OutputCurvePath));
		}
	}

	AssetResult.Actions.Add(FString::Printf(TEXT("configured Kawaii node %s root=%s"), *Preset.Id, *Preset.RootBone));
}

bool CompileAndSaveAnimBlueprint(
	UAnimBlueprint& AnimBlueprint,
	FNteCharacterKawaiiWriteResult& Result,
	FNteCharacterKawaiiAssetWriteResult& AssetResult)
{
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&AnimBlueprint);
	FKismetEditorUtilities::CompileBlueprint(&AnimBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	if (AnimBlueprint.Status == BS_Error)
	{
		const FString Error = FString::Printf(TEXT("AnimBlueprint compile failed: %s"), *AnimBlueprint.GetPathName());
		AddError(AssetResult, Error);
		AddError(Result, Error);
		return false;
	}
	return SaveAsset(AnimBlueprint, Result, AssetResult);
}

void WriteRuntimeAnimBlueprint(
	const FString& RuntimeAnimBlueprintPath,
	const TArray<const FNteCharacterKawaiiPresetPlanItem*>& Presets,
	FNteCharacterKawaiiWriteResult& Result)
{
	FNteCharacterKawaiiAssetWriteResult AssetResult;
	AssetResult.AssetPath = RuntimeAnimBlueprintPath;
	AssetResult.AssetKind = TEXT("KawaiiRuntimeAnimBlueprint");

	if (Presets.IsEmpty())
	{
		AddWarning(AssetResult, TEXT("Runtime AnimBlueprint group has no Kawaii presets."));
		Result.Warnings.Append(AssetResult.Warnings);
		Result.Assets.Add(MoveTemp(AssetResult));
		return;
	}

	const FNteCharacterKawaiiPresetPlanItem& FirstPreset = *Presets[0];
	const bool bMainPostProcess = FirstPreset.SourcePoseStrategy.Equals(TEXT("PostProcessInputPose"), ESearchCase::IgnoreCase);
	const bool bAttachedCopyPose = FirstPreset.SourcePoseStrategy.Equals(TEXT("AttachedParentCopyPose"), ESearchCase::IgnoreCase);
	if (!bMainPostProcess && !bAttachedCopyPose)
	{
		AddError(AssetResult, FString::Printf(
			TEXT("Kawaii preset '%s' has unsupported source pose strategy '%s' for target %s."),
			*FirstPreset.Id,
			*FirstPreset.SourcePoseStrategy,
			*FirstPreset.TargetKind));
		Result.Errors.Append(AssetResult.Errors);
		Result.Assets.Add(MoveTemp(AssetResult));
		return;
	}

	for (const FNteCharacterKawaiiPresetPlanItem* Preset : Presets)
	{
		if (!Preset)
		{
			continue;
		}
		if (Preset->TargetMeshPath != FirstPreset.TargetMeshPath)
		{
			AddError(AssetResult, FString::Printf(
				TEXT("Runtime AnimBlueprint path %s is shared by different target meshes: %s and %s."),
				*RuntimeAnimBlueprintPath,
				*FirstPreset.TargetMeshPath,
				*Preset->TargetMeshPath));
		}
		if (!Preset->SourcePoseStrategy.Equals(FirstPreset.SourcePoseStrategy, ESearchCase::IgnoreCase))
		{
			AddError(AssetResult, FString::Printf(
				TEXT("Runtime AnimBlueprint path %s mixes source pose strategies %s and %s."),
				*RuntimeAnimBlueprintPath,
				*FirstPreset.SourcePoseStrategy,
				*Preset->SourcePoseStrategy));
		}
	}
	if (!AssetResult.Errors.IsEmpty())
	{
		Result.Errors.Append(AssetResult.Errors);
		Result.Assets.Add(MoveTemp(AssetResult));
		return;
	}

	UClass* SourcePoseClass = LoadObject<UClass>(
		nullptr,
		bMainPostProcess ? LinkedInputPoseAnimGraphNodeClassPath : CopyPoseAnimGraphNodeClassPath);
	UClass* LocalRefPoseClass = bMainPostProcess ? LoadObject<UClass>(nullptr, LocalRefPoseAnimGraphNodeClassPath) : nullptr;
	UClass* LayeredBoneBlendClass = bMainPostProcess ? LoadObject<UClass>(nullptr, LayeredBoneBlendAnimGraphNodeClassPath) : nullptr;
	UClass* LocalToComponentClass = LoadObject<UClass>(nullptr, LocalToComponentAnimGraphNodeClassPath);
	UClass* KawaiiClass = LoadObject<UClass>(nullptr, KawaiiAnimGraphNodeClassPath);
	UClass* ComponentToLocalClass = LoadObject<UClass>(nullptr, ComponentToLocalAnimGraphNodeClassPath);
	if (!SourcePoseClass
		|| (bMainPostProcess && (!LocalRefPoseClass || !LayeredBoneBlendClass))
		|| !LocalToComponentClass
		|| !KawaiiClass
		|| !ComponentToLocalClass)
	{
		AddError(AssetResult, FString::Printf(
			TEXT("Required AnimGraph node classes are missing. SourcePose=%s LocalRefPose=%s LayeredBoneBlend=%s LocalToComponent=%s Kawaii=%s ComponentToLocal=%s"),
			SourcePoseClass ? TEXT("ok") : TEXT("missing"),
			!bMainPostProcess || LocalRefPoseClass ? TEXT("ok") : TEXT("missing"),
			!bMainPostProcess || LayeredBoneBlendClass ? TEXT("ok") : TEXT("missing"),
			LocalToComponentClass ? TEXT("ok") : TEXT("missing"),
			KawaiiClass ? TEXT("ok") : TEXT("missing"),
			ComponentToLocalClass ? TEXT("ok") : TEXT("missing")));
		Result.Errors.Append(AssetResult.Errors);
		Result.Assets.Add(MoveTemp(AssetResult));
		return;
	}

	USkeletalMesh* TargetMesh = LoadTargetSkeletalMesh(FirstPreset, AssetResult);
	UAnimBlueprint* AnimBlueprint = TargetMesh
		? CreateOrLoadAnimBlueprint(RuntimeAnimBlueprintPath, *TargetMesh, bAttachedCopyPose, AssetResult)
		: nullptr;
	UEdGraph* AnimGraph = AnimBlueprint ? EnsureAnimGraph(*AnimBlueprint, AssetResult) : nullptr;
	if (!AnimBlueprint || !AnimGraph)
	{
		Result.Errors.Append(AssetResult.Errors);
		Result.Warnings.Append(AssetResult.Warnings);
		Result.Assets.Add(MoveTemp(AssetResult));
		return;
	}

	RemoveGeneratedKawaiiNodes(*AnimGraph);
	UAnimGraphNode_Base* RootNode = FindAnimGraphRootNode(*AnimGraph);
	if (!RootNode)
	{
		if (const UEdGraphSchema* Schema = AnimGraph->GetSchema())
		{
			Schema->CreateDefaultNodesForGraph(*AnimGraph);
		}
		RootNode = FindAnimGraphRootNode(*AnimGraph);
	}
	if (!RootNode)
	{
		AddError(AssetResult, TEXT("AnimGraph has no root/output pose node."));
		Result.Errors.Append(AssetResult.Errors);
		Result.Assets.Add(MoveTemp(AssetResult));
		return;
	}
	RootNode->NodePosX = 460 + Presets.Num() * 280;
	RootNode->NodePosY = 0;
	if (UEdGraphPin* RootInputPin = FindPosePin(*RootNode, EGPD_Input, false))
	{
		RootInputPin->BreakAllPinLinks();
	}

	UAnimGraphNode_Base* SourcePoseNode = AddAnimGraphNodeByClass(
		*AnimGraph,
		SourcePoseClass,
		bMainPostProcess ? -1480 : -1120,
		0,
		AssetResult,
		bMainPostProcess ? TEXT("PostProcessInputPose") : TEXT("CopyPoseFromMesh"));
	UAnimGraphNode_Base* LocalRefPoseNode = bMainPostProcess
		? AddAnimGraphNodeByClass(*AnimGraph, LocalRefPoseClass, -1480, 260, AssetResult, TEXT("LocalRefPose"))
		: nullptr;
	UAnimGraphNode_Base* LayeredBoneBlendNode = bMainPostProcess
		? AddAnimGraphNodeByClass(*AnimGraph, LayeredBoneBlendClass, -1120, 0, AssetResult, TEXT("LayeredBoneBlend"))
		: nullptr;
	UAnimGraphNode_Base* LocalToComponentNode = AddAnimGraphNodeByClass(*AnimGraph, LocalToComponentClass, bMainPostProcess ? -760 : -820, 0, AssetResult, TEXT("LocalToComponentSpace"));
	UAnimGraphNode_Base* ComponentToLocalNode = AddAnimGraphNodeByClass(*AnimGraph, ComponentToLocalClass, 180 + Presets.Num() * 280, 0, AssetResult, TEXT("ComponentToLocalSpace"));
	if (!SourcePoseNode
		|| (bMainPostProcess && (!LocalRefPoseNode || !LayeredBoneBlendNode))
		|| !LocalToComponentNode
		|| !ComponentToLocalNode)
	{
		Result.Errors.Append(AssetResult.Errors);
		Result.Assets.Add(MoveTemp(AssetResult));
		return;
	}

	if (bAttachedCopyPose)
	{
		ConfigureCopyPoseNode(*SourcePoseNode, AssetResult);
	}
	if (bMainPostProcess)
	{
		const TArray<FName> ReferencePoseBranchRoots = ResolveReferencePoseBranchRoots(*TargetMesh, Presets, &AssetResult);
		ConfigureCustomChainReferencePoseBlend(*LayeredBoneBlendNode, ReferencePoseBranchRoots, AssetResult);
		TryConnectPosePins(
			FindPosePin(*SourcePoseNode, EGPD_Output, false),
			FindLocalPoseInputPinAtIndex(*LayeredBoneBlendNode, 0),
			AssetResult,
			TEXT("PostProcessInputPose -> LayeredBoneBlend.BasePose"));
		TryConnectPosePins(
			FindPosePin(*LocalRefPoseNode, EGPD_Output, false),
			FindLocalPoseInputPinAtIndex(*LayeredBoneBlendNode, 1),
			AssetResult,
			TEXT("LocalRefPose -> LayeredBoneBlend.BlendPose_0"));
		TryConnectPosePins(
			FindPosePin(*LayeredBoneBlendNode, EGPD_Output, false),
			FindPosePin(*LocalToComponentNode, EGPD_Input, false),
			AssetResult,
			TEXT("LayeredBoneBlend -> LocalToComponent"));
	}
	else
	{
		TryConnectPosePins(
			FindPosePin(*SourcePoseNode, EGPD_Output, false),
			FindPosePin(*LocalToComponentNode, EGPD_Input, false),
			AssetResult,
			TEXT("CopyPoseFromMesh -> LocalToComponent"));
	}

	UEdGraphPin* CurrentComponentPoseOut = FindPosePin(*LocalToComponentNode, EGPD_Output, true);
	for (int32 PresetIndex = 0; PresetIndex < Presets.Num(); ++PresetIndex)
	{
		const FNteCharacterKawaiiPresetPlanItem* Preset = Presets[PresetIndex];
		if (!Preset)
		{
			continue;
		}

		UAnimGraphNode_Base* KawaiiNode = AddAnimGraphNodeByClass(
			*AnimGraph,
			KawaiiClass,
			-520 + PresetIndex * 280,
			PresetIndex * 80,
			AssetResult,
			TEXT("KawaiiPhysics"));
		if (!KawaiiNode)
		{
			continue;
		}
		KawaiiNode->NodeComment = FString::Printf(TEXT("%s:%s"), GeneratedKawaiiNodeComment, *Preset->Id);
		ConfigureKawaiiNode(*KawaiiNode, *Preset, AssetResult);
		TryConnectPosePins(
			CurrentComponentPoseOut,
			FindPosePin(*KawaiiNode, EGPD_Input, true),
			AssetResult,
			TEXT("ComponentPose -> KawaiiPhysics"));
		CurrentComponentPoseOut = FindPosePin(*KawaiiNode, EGPD_Output, true);
	}

	TryConnectPosePins(
		CurrentComponentPoseOut,
		FindPosePin(*ComponentToLocalNode, EGPD_Input, true),
		AssetResult,
		TEXT("KawaiiPhysics -> ComponentToLocal"));
	TryConnectPosePins(
		FindPosePin(*ComponentToLocalNode, EGPD_Output, false),
		FindPosePin(*RootNode, EGPD_Input, false),
		AssetResult,
		TEXT("ComponentToLocal -> OutputPose"));

	if (AssetResult.Errors.IsEmpty())
	{
		AssetResult.Actions.Add(bMainPostProcess
			? FString::Printf(TEXT("rebuilt main-mesh Post Process AnimGraph with reference-pose-stabilized custom chains and %d Kawaii node(s)"), Presets.Num())
			: FString::Printf(TEXT("rebuilt attached-mesh Kawaii AnimGraph with %d Kawaii node(s)"), Presets.Num()));
		if (CompileAndSaveAnimBlueprint(*AnimBlueprint, Result, AssetResult) && bMainPostProcess)
		{
			FNteCharacterKawaiiAssetWriteResult MeshResult;
			MeshResult.AssetPath = FirstPreset.TargetMeshPath;
			MeshResult.AssetKind = TEXT("MainMeshPostProcessBinding");
			if (!AnimBlueprint->GeneratedClass)
			{
				AddError(MeshResult, TEXT("Compiled Post Process AnimBlueprint has no GeneratedClass."));
			}
			else
			{
				TargetMesh->Modify();
				TargetMesh->SetPostProcessAnimBlueprint(TSubclassOf<UAnimInstance>(AnimBlueprint->GeneratedClass));
				MeshResult.bUpdated = true;
				MeshResult.Actions.Add(FString::Printf(
					TEXT("set PostProcessAnimBlueprint to %s"),
					*AnimBlueprint->GeneratedClass->GetPathName()));
				SaveAsset(*TargetMesh, Result, MeshResult);
			}
			Result.Errors.Append(MeshResult.Errors);
			Result.Warnings.Append(MeshResult.Warnings);
			Result.Assets.Add(MoveTemp(MeshResult));
		}
	}

	Result.Errors.Append(AssetResult.Errors);
	Result.Warnings.Append(AssetResult.Warnings);
	Result.Assets.Add(MoveTemp(AssetResult));
}

void WriteCurveAsset(
	const FNteCharacterKawaiiCurvePlanItem& Curve,
	FNteCharacterKawaiiWriteResult& Result)
{
	FNteCharacterKawaiiAssetWriteResult AssetResult;
	AssetResult.AssetPath = Curve.OutputCurvePath;
	AssetResult.AssetKind = TEXT("CurveFloat");

	UCurveFloat* CurveAsset = Cast<UCurveFloat>(CreateOrLoadAssetOfClass(Curve.OutputCurvePath, UCurveFloat::StaticClass(), AssetResult));
	if (CurveAsset)
	{
		CurveAsset->Modify();
		UCurveFloat* SourceCurve = nullptr;
		if (!Curve.ExternalCurveObjectPath.IsEmpty())
		{
			SourceCurve = NTEBuildTool::Editor::LoadAssetByPath<UCurveFloat>(Curve.ExternalCurveObjectPath);
		}
		if (SourceCurve)
		{
			CurveAsset->FloatCurve = SourceCurve->FloatCurve;
			AssetResult.Actions.Add(FString::Printf(TEXT("copied keys from source curve %s"), *Curve.ExternalCurveObjectPath));
		}
		else if (!Curve.ExternalCurveObjectPath.IsEmpty())
		{
			AddWarning(AssetResult, FString::Printf(TEXT("Source curve could not be loaded; created editable empty CurveFloat: %s"), *Curve.ExternalCurveObjectPath));
		}
		else if (Curve.InlineKeyCount > 0)
		{
			AddWarning(AssetResult, FString::Printf(TEXT("Source JSON reports %d inline curve keys, but key values are not captured yet; created editable empty CurveFloat."), Curve.InlineKeyCount));
		}
		else
		{
			AddWarning(AssetResult, TEXT("Curve has no loadable source keys; created editable empty CurveFloat."));
		}
		SaveAsset(*CurveAsset, Result, AssetResult);
	}

	Result.Errors.Append(AssetResult.Errors);
	Result.Warnings.Append(AssetResult.Warnings);
	Result.Assets.Add(MoveTemp(AssetResult));
}

void WriteLimitsDataAsset(
	const FNteCharacterKawaiiPresetPlanItem& Preset,
	UClass* LimitsDataAssetClass,
	FNteCharacterKawaiiWriteResult& Result)
{
	FNteCharacterKawaiiAssetWriteResult AssetResult;
	AssetResult.AssetPath = Preset.OutputLimitsDataAssetPath;
	AssetResult.AssetKind = FString::Printf(TEXT("KawaiiLimitsDataAsset:%s"), *Preset.Id);

	UObject* DataAsset = CreateOrLoadAssetOfClass(Preset.OutputLimitsDataAssetPath, LimitsDataAssetClass, AssetResult);
	if (DataAsset)
	{
		DataAsset->Modify();
		if (USkeleton* Skeleton = LoadTargetSkeleton(Preset, AssetResult))
		{
			SetObjectProperty(DataAsset, DataAsset->GetClass(), TEXT("Skeleton"), Skeleton);
		}

		TArray<const FNteCharacterKawaiiLimitPlanItem*> SphericalLimits;
		TArray<const FNteCharacterKawaiiLimitPlanItem*> CapsuleLimits;
		TArray<const FNteCharacterKawaiiLimitPlanItem*> BoxLimits;
		TArray<const FNteCharacterKawaiiLimitPlanItem*> PlanarLimits;
		SplitLimitsByKind(Preset.CollisionLimits, SphericalLimits, CapsuleLimits, BoxLimits, PlanarLimits);
		AssignLimitArray(*DataAsset, TEXT("SphericalLimits"), SphericalLimits, AssetResult);
		AssignLimitArray(*DataAsset, TEXT("CapsuleLimits"), CapsuleLimits, AssetResult);
		AssignLimitArray(*DataAsset, TEXT("BoxLimits"), BoxLimits, AssetResult);
		AssignLimitArray(*DataAsset, TEXT("PlanarLimits"), PlanarLimits, AssetResult);

		if (Preset.CollisionLimits.IsEmpty()
			&& (Preset.SphericalLimitsDataCount + Preset.CapsuleLimitsDataCount + Preset.BoxLimitsDataCount + Preset.PlanarLimitsDataCount) > 0)
		{
			AddWarning(AssetResult, TEXT("Source JSON referenced limit data asset counts, but inline limit entries were not available; generated an empty editable limits asset."));
		}
		SaveAsset(*DataAsset, Result, AssetResult);
	}

	Result.Errors.Append(AssetResult.Errors);
	Result.Warnings.Append(AssetResult.Warnings);
	Result.Assets.Add(MoveTemp(AssetResult));
}

void WriteBoneConstraintsDataAsset(
	const FNteCharacterKawaiiPresetPlanItem& Preset,
	UClass* BoneConstraintsDataAssetClass,
	FNteCharacterKawaiiWriteResult& Result)
{
	FNteCharacterKawaiiAssetWriteResult AssetResult;
	AssetResult.AssetPath = Preset.OutputBoneConstraintsDataAssetPath;
	AssetResult.AssetKind = FString::Printf(TEXT("KawaiiBoneConstraintsDataAsset:%s"), *Preset.Id);

	UObject* DataAsset = CreateOrLoadAssetOfClass(Preset.OutputBoneConstraintsDataAssetPath, BoneConstraintsDataAssetClass, AssetResult);
	if (DataAsset)
	{
		DataAsset->Modify();
		if (USkeleton* Skeleton = LoadTargetSkeleton(Preset, AssetResult))
		{
			SetObjectProperty(DataAsset, DataAsset->GetClass(), TEXT("PreviewSkeleton"), Skeleton);
		}

		if (Preset.BoneConstraintCount > 0 || Preset.BoneConstraintsDataCount > 0)
		{
			AddWarning(AssetResult, FString::Printf(
				TEXT("Source reports BoneConstraintCount=%d and BoneConstraintsDataCount=%d, but bone-pair constraint entries are not imported yet; generated an empty editable constraints asset."),
				Preset.BoneConstraintCount,
				Preset.BoneConstraintsDataCount));
		}
		SaveAsset(*DataAsset, Result, AssetResult);
	}

	Result.Errors.Append(AssetResult.Errors);
	Result.Warnings.Append(AssetResult.Warnings);
	Result.Assets.Add(MoveTemp(AssetResult));
}

void AppendAssetResultJsonArray(
	const TSharedRef<FJsonObject>& Object,
	const TCHAR* FieldName,
	const TArray<FNteCharacterKawaiiAssetWriteResult>& Assets)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FNteCharacterKawaiiAssetWriteResult& Asset : Assets)
	{
		const TSharedRef<FJsonObject> AssetObject = MakeShared<FJsonObject>();
		AddStringIfNotEmpty(AssetObject, TEXT("AssetPath"), Asset.AssetPath);
		AddStringIfNotEmpty(AssetObject, TEXT("AssetKind"), Asset.AssetKind);
		AssetObject->SetBoolField(TEXT("Created"), Asset.bCreated);
		AssetObject->SetBoolField(TEXT("Updated"), Asset.bUpdated);
		AssetObject->SetBoolField(TEXT("Skipped"), Asset.bSkipped);
		AssetObject->SetArrayField(TEXT("Actions"), Json::StringArrayToJsonValues(Asset.Actions));
		AssetObject->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Asset.Warnings));
		AssetObject->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Asset.Errors));
		Values.Add(MakeShared<FJsonValueObject>(AssetObject));
	}
	Object->SetArrayField(FieldName, Values);
}
}

FNteCharacterKawaiiSchemaProbeResult ProbeNteKawaiiSchemaCompatibility()
{
	FNteCharacterKawaiiSchemaProbeResult Result;

	UClass* LimitsDataAssetClass = LoadObject<UClass>(nullptr, LimitsDataAssetClassPath);
	UClass* BoneConstraintsDataAssetClass = LoadObject<UClass>(nullptr, BoneConstraintsDataAssetClassPath);
	UClass* KawaiiAnimGraphNodeClass = LoadObject<UClass>(nullptr, KawaiiAnimGraphNodeClassPath);
	UScriptStruct* AnimNodeStruct = LoadScriptStruct(AnimNodeStructPath);
	UScriptStruct* PhysicsSettingsStruct = LoadScriptStruct(PhysicsSettingsStructPath);
	UScriptStruct* CapsuleLimitStruct = LoadScriptStruct(CapsuleLimitStructPath);
	UScriptStruct* CollisionLimitBaseStruct = LoadScriptStruct(CollisionLimitBaseStructPath);

	CheckType(LimitsDataAssetClass, LimitsDataAssetClassPath, Result);
	CheckType(BoneConstraintsDataAssetClass, BoneConstraintsDataAssetClassPath, Result);
	CheckType(KawaiiAnimGraphNodeClass, KawaiiAnimGraphNodeClassPath, Result);
	CheckType(AnimNodeStruct, AnimNodeStructPath, Result);
	CheckType(PhysicsSettingsStruct, PhysicsSettingsStructPath, Result);
	CheckType(CapsuleLimitStruct, CapsuleLimitStructPath, Result);
	CheckType(CollisionLimitBaseStruct, CollisionLimitBaseStructPath, Result);

	CheckField(PhysicsSettingsStruct, PhysicsSettingsStructPath, TEXT("ForwardMoveOffset"), Result);
	CheckField(AnimNodeStruct, AnimNodeStructPath, TEXT("bUseRelativeMove"), Result);
	CheckField(AnimNodeStruct, AnimNodeStructPath, TEXT("MovementReferenceDisplacement"), Result);
	CheckField(CapsuleLimitStruct, CapsuleLimitStructPath, TEXT("SphereRadius"), Result);
	CheckField(CollisionLimitBaseStruct, CollisionLimitBaseStructPath, TEXT("OffSetLocation"), Result);

	TrackPublicOnlyField(AnimNodeStruct, AnimNodeStructPath, TEXT("SimulationSpace"), Result);
	TrackPublicOnlyField(AnimNodeStruct, AnimNodeStructPath, TEXT("SimulationBaseBone"), Result);
	TrackPublicOnlyField(AnimNodeStruct, AnimNodeStructPath, TEXT("SkelCompMoveScale"), Result);
	TrackPublicOnlyField(AnimNodeStruct, AnimNodeStructPath, TEXT("bUpdatePhysicsSettingsInGame"), Result);
	TrackPublicOnlyField(CollisionLimitBaseStruct, CollisionLimitBaseStructPath, TEXT("OffsetLocation"), Result);

	Result.bKawaiiPhysicsModuleAvailable = LimitsDataAssetClass
		|| BoneConstraintsDataAssetClass
		|| AnimNodeStruct
		|| PhysicsSettingsStruct
		|| CapsuleLimitStruct
		|| CollisionLimitBaseStruct;
	Result.bKawaiiPhysicsEditorModuleAvailable = KawaiiAnimGraphNodeClass != nullptr;
	Result.bNteCompatible = Result.bKawaiiPhysicsModuleAvailable && Result.MissingTypes.IsEmpty() && Result.MissingFields.IsEmpty();
	if (!Result.PublicOnlyFields.IsEmpty())
	{
		Result.Warnings.Add(TEXT("KawaiiPhysics exposes public-plugin-only fields; this mirror plugin may not match the game schema."));
	}
	if (!Result.bNteCompatible)
	{
		Result.Errors.Add(TEXT("Mirror KawaiiPhysics schema is not NTE-compatible; Kawaii AnimBP/DataAsset writing is blocked."));
	}
	return Result;
}

FNteCharacterKawaiiAssetPreflightResult ValidateCharacterKawaiiAssetsForPackage(const FNteCharacterKawaiiPlan& Plan)
{
	FNteCharacterKawaiiAssetPreflightResult Result;
	TMap<FString, TArray<const FNteCharacterKawaiiPresetPlanItem*>> PresetsByAnimBlueprint;
	const auto RequireGeneratedAsset = [&Result](const FString& AssetPath, const FString& Description)
	{
		if (AssetPath.IsEmpty())
		{
			return;
		}
		if (!NTEBuildTool::Editor::LoadAssetByPath<UObject>(NormalizePackagePath(AssetPath)))
		{
			Result.Errors.Add(FString::Printf(TEXT("Missing generated %s asset: %s"), *Description, *AssetPath));
		}
	};

	for (const FNteCharacterKawaiiPresetPlanItem& Preset : Plan.Presets)
	{
		if (!Preset.MissingBones.IsEmpty())
		{
			Result.Errors.Add(FString::Printf(
				TEXT("Kawaii preset '%s' references bones missing from target mesh %s: %s"),
				*Preset.Id,
				*Preset.TargetMeshPath,
				*FString::Join(Preset.MissingBones, TEXT(", "))));
		}
		if (Preset.RuntimeAnimBlueprintPath.IsEmpty())
		{
			Result.Errors.Add(FString::Printf(TEXT("Kawaii preset '%s' has no RuntimeAnimBlueprintPath."), *Preset.Id));
		}
		else
		{
			PresetsByAnimBlueprint.FindOrAdd(NormalizePackagePath(Preset.RuntimeAnimBlueprintPath)).Add(&Preset);
		}
		RequireGeneratedAsset(Preset.OutputLimitsDataAssetPath, FString::Printf(TEXT("limits for preset '%s'"), *Preset.Id));
		RequireGeneratedAsset(Preset.OutputBoneConstraintsDataAssetPath, FString::Printf(TEXT("bone constraints for preset '%s'"), *Preset.Id));
		for (const FNteCharacterKawaiiCurvePlanItem& Curve : Preset.Curves)
		{
			RequireGeneratedAsset(Curve.OutputCurvePath, FString::Printf(TEXT("curve '%s' for preset '%s'"), *Curve.CurveKind, *Preset.Id));
		}
	}

	UClass* CopyPoseClass = LoadObject<UClass>(nullptr, CopyPoseAnimGraphNodeClassPath);
	UClass* LinkedInputPoseClass = LoadObject<UClass>(nullptr, LinkedInputPoseAnimGraphNodeClassPath);
	UClass* LocalRefPoseClass = LoadObject<UClass>(nullptr, LocalRefPoseAnimGraphNodeClassPath);
	UClass* LayeredBoneBlendClass = LoadObject<UClass>(nullptr, LayeredBoneBlendAnimGraphNodeClassPath);
	UClass* LocalToComponentClass = LoadObject<UClass>(nullptr, LocalToComponentAnimGraphNodeClassPath);
	UClass* KawaiiClass = LoadObject<UClass>(nullptr, KawaiiAnimGraphNodeClassPath);
	if (!CopyPoseClass || !LinkedInputPoseClass || !LocalRefPoseClass || !LayeredBoneBlendClass || !LocalToComponentClass || !KawaiiClass)
	{
		Result.Errors.Add(TEXT("Kawaii package preflight could not load required source-pose, reference-pose blend, or KawaiiPhysics graph node classes."));
		return Result;
	}

	for (const TPair<FString, TArray<const FNteCharacterKawaiiPresetPlanItem*>>& Pair : PresetsByAnimBlueprint)
	{
		UAnimBlueprint* AnimBlueprint = NTEBuildTool::Editor::LoadAssetByPath<UAnimBlueprint>(Pair.Key);
		if (!AnimBlueprint)
		{
			Result.Errors.Add(FString::Printf(TEXT("Missing generated Kawaii Runtime AnimBlueprint: %s"), *Pair.Key));
			continue;
		}
		Result.CheckedAnimBlueprints.AddUnique(Pair.Key);
		if (AnimBlueprint->Status == BS_Error)
		{
			Result.Errors.Add(FString::Printf(TEXT("Generated Kawaii Runtime AnimBlueprint has compile errors: %s"), *Pair.Key));
		}

		UEdGraph* AnimGraph = nullptr;
		for (UEdGraph* Graph : AnimBlueprint->FunctionGraphs)
		{
			if (Graph && Graph->GetFName() == UEdGraphSchema_K2::GN_AnimGraph)
			{
				AnimGraph = Graph;
				break;
			}
		}
		if (!AnimGraph)
		{
			Result.Errors.Add(FString::Printf(TEXT("Generated Kawaii Runtime AnimBlueprint has no AnimGraph: %s"), *Pair.Key));
			continue;
		}

		int32 CopyPoseCount = 0;
		int32 LinkedInputPoseCount = 0;
		int32 LocalRefPoseCount = 0;
		int32 LayeredBoneBlendCount = 0;
		int32 KawaiiCount = 0;
		UAnimGraphNode_LayeredBoneBlend* ReferencePoseBlendNode = nullptr;
		for (UEdGraphNode* Node : AnimGraph->Nodes)
		{
			if (!Node)
			{
				continue;
			}
			if (Node->IsA(CopyPoseClass))
			{
				++CopyPoseCount;
				void* NodeContainer = nullptr;
				UScriptStruct* NodeStruct = nullptr;
				if (!GetAnimNodeStructContainer(*Node, NodeContainer, NodeStruct))
				{
					Result.Errors.Add(FString::Printf(TEXT("CopyPoseFromMesh node has no reflected Node struct: %s"), *Pair.Key));
					continue;
				}
				const FBoolProperty* UseAttachedParentProperty = FindFProperty<FBoolProperty>(NodeStruct, TEXT("bUseAttachedParent"));
				if (!UseAttachedParentProperty || !UseAttachedParentProperty->GetPropertyValue_InContainer(NodeContainer))
				{
					Result.Errors.Add(FString::Printf(TEXT("CopyPoseFromMesh.bUseAttachedParent is not true: %s"), *Pair.Key));
				}
			}
			else if (Node->IsA(LinkedInputPoseClass))
			{
				++LinkedInputPoseCount;
			}
			else if (Node->IsA(LocalRefPoseClass))
			{
				++LocalRefPoseCount;
			}
			else if (Node->IsA(LayeredBoneBlendClass))
			{
				++LayeredBoneBlendCount;
				ReferencePoseBlendNode = Cast<UAnimGraphNode_LayeredBoneBlend>(Node);
			}
			else if (Node->IsA(KawaiiClass))
			{
				++KawaiiCount;
			}
		}

		const bool bMainPostProcess = Pair.Value[0]->SourcePoseStrategy.Equals(TEXT("PostProcessInputPose"), ESearchCase::IgnoreCase);
		const int32 ExpectedCopyPoseCount = bMainPostProcess ? 0 : 1;
		const int32 ExpectedLinkedInputPoseCount = bMainPostProcess ? 1 : 0;
		const int32 ExpectedReferencePoseNodeCount = bMainPostProcess ? 1 : 0;
		if (CopyPoseCount != ExpectedCopyPoseCount)
		{
			Result.Errors.Add(FString::Printf(TEXT("Expected %d CopyPoseFromMesh node(s) in %s, found %d."), ExpectedCopyPoseCount, *Pair.Key, CopyPoseCount));
		}
		if (LinkedInputPoseCount != ExpectedLinkedInputPoseCount)
		{
			Result.Errors.Add(FString::Printf(TEXT("Expected %d LinkedInputPose node(s) in %s, found %d."), ExpectedLinkedInputPoseCount, *Pair.Key, LinkedInputPoseCount));
		}
		if (LocalRefPoseCount != ExpectedReferencePoseNodeCount)
		{
			Result.Errors.Add(FString::Printf(TEXT("Expected %d LocalRefPose node(s) in %s, found %d."), ExpectedReferencePoseNodeCount, *Pair.Key, LocalRefPoseCount));
		}
		if (LayeredBoneBlendCount != ExpectedReferencePoseNodeCount)
		{
			Result.Errors.Add(FString::Printf(TEXT("Expected %d LayeredBoneBlend node(s) in %s, found %d."), ExpectedReferencePoseNodeCount, *Pair.Key, LayeredBoneBlendCount));
		}
		if (KawaiiCount != Pair.Value.Num())
		{
			Result.Errors.Add(FString::Printf(
				TEXT("Expected %d KawaiiPhysics node(s) in %s, found %d."),
				Pair.Value.Num(),
				*Pair.Key,
				KawaiiCount));
		}
		if (bMainPostProcess)
		{
			if (!AnyNodeOfClassLinksToClass(*AnimGraph, *LinkedInputPoseClass, *LayeredBoneBlendClass)
				|| !AnyNodeOfClassLinksToClass(*AnimGraph, *LocalRefPoseClass, *LayeredBoneBlendClass)
				|| !AnyNodeOfClassLinksToClass(*AnimGraph, *LayeredBoneBlendClass, *LocalToComponentClass))
			{
				Result.Errors.Add(FString::Printf(
					TEXT("Post Process reference-pose stabilizer is not fully connected in %s."),
					*Pair.Key));
			}
			USkeletalMesh* MainMesh = NTEBuildTool::Editor::LoadAssetByPath<USkeletalMesh>(Pair.Value[0]->TargetMeshPath);
			const TArray<FName> ExpectedBranchRoots = MainMesh
				? ResolveReferencePoseBranchRoots(*MainMesh, Pair.Value)
				: TArray<FName>();
			TArray<FName> ActualChainRoots;
			if (ReferencePoseBlendNode && ReferencePoseBlendNode->Node.LayerSetup.Num() == 1)
			{
				for (const FBranchFilter& Filter : ReferencePoseBlendNode->Node.LayerSetup[0].BranchFilters)
				{
					if (Filter.BlendDepth != 0)
					{
						Result.Errors.Add(FString::Printf(TEXT("Reference-pose branch %s in %s must use BlendDepth=0."), *Filter.BoneName.ToString(), *Pair.Key));
					}
					ActualChainRoots.AddUnique(Filter.BoneName);
				}
			}
			if (ActualChainRoots != ExpectedBranchRoots)
			{
				Result.Errors.Add(FString::Printf(
					TEXT("Reference-pose branch roots in %s do not match the topology-safe Kawaii anchors. Expected [%s], found [%s]."),
					*Pair.Key,
					*FString::JoinBy(ExpectedBranchRoots, TEXT(", "), [](const FName Name) { return Name.ToString(); }),
					*FString::JoinBy(ActualChainRoots, TEXT(", "), [](const FName Name) { return Name.ToString(); })));
			}
			if (!MainMesh || MainMesh->GetPostProcessAnimBlueprint() != AnimBlueprint->GeneratedClass)
			{
				Result.Errors.Add(FString::Printf(
					TEXT("Main mesh %s is not bound to generated Post Process AnimBlueprint %s."),
					*Pair.Value[0]->TargetMeshPath,
					*Pair.Key));
			}
		}
	}

	Result.CheckedAnimBlueprints.Sort();
	return Result;
}

FNteCharacterKawaiiWriteResult WriteAttachedMeshCopyPoseAnimBlueprint(
	const FString& RuntimeAnimBlueprintPath,
	const FString& TargetMeshPath)
{
	FNteCharacterKawaiiWriteResult Result;
	FNteCharacterKawaiiAssetWriteResult AssetResult;
	AssetResult.AssetPath = RuntimeAnimBlueprintPath;
	AssetResult.AssetKind = TEXT("AttachedMeshCopyPoseAnimBlueprint");

	USkeletalMesh* TargetMesh = NTEBuildTool::Editor::LoadAssetByPath<USkeletalMesh>(TargetMeshPath);
	if (!TargetMesh)
	{
		AddError(AssetResult, FString::Printf(TEXT("Attached mesh could not be loaded: %s"), *TargetMeshPath));
	}
	else if (!TargetMesh->GetSkeleton())
	{
		AddError(AssetResult, FString::Printf(TEXT("Attached mesh has no Skeleton: %s"), *TargetMeshPath));
	}

	UClass* CopyPoseClass = LoadObject<UClass>(nullptr, CopyPoseAnimGraphNodeClassPath);
	if (!CopyPoseClass)
	{
		AddError(AssetResult, TEXT("CopyPoseFromMesh AnimGraph node class is missing."));
	}

	UAnimBlueprint* AnimBlueprint = TargetMesh && AssetResult.Errors.IsEmpty()
		? CreateOrLoadAnimBlueprint(RuntimeAnimBlueprintPath, *TargetMesh, true, AssetResult)
		: nullptr;
	UEdGraph* AnimGraph = AnimBlueprint ? EnsureAnimGraph(*AnimBlueprint, AssetResult) : nullptr;
	if (AnimGraph)
	{
		UAnimGraphNode_Base* RootNode = FindAnimGraphRootNode(*AnimGraph);
		if (!RootNode)
		{
			if (const UEdGraphSchema* Schema = AnimGraph->GetSchema())
			{
				Schema->CreateDefaultNodesForGraph(*AnimGraph);
			}
			RootNode = FindAnimGraphRootNode(*AnimGraph);
		}

		if (!RootNode)
		{
			AddError(AssetResult, TEXT("AnimGraph has no root/output pose node."));
		}
		else
		{
			TArray<UEdGraphNode*> NodesToRemove;
			for (UEdGraphNode* Node : AnimGraph->Nodes)
			{
				if (Node && Node != RootNode)
				{
					NodesToRemove.Add(Node);
				}
			}
			for (UEdGraphNode* Node : NodesToRemove)
			{
				FBlueprintEditorUtils::RemoveNode(AnimBlueprint, Node, true);
			}

			RootNode->NodePosX = 220;
			RootNode->NodePosY = 0;
			if (UEdGraphPin* RootInputPin = FindPosePin(*RootNode, EGPD_Input, false))
			{
				RootInputPin->BreakAllPinLinks();
			}

			UAnimGraphNode_Base* CopyPoseNode = AddAnimGraphNodeByClass(
				*AnimGraph,
				CopyPoseClass,
				-120,
				0,
				AssetResult,
				TEXT("CopyPoseFromMesh"));
			if (CopyPoseNode)
			{
				ConfigureCopyPoseNode(*CopyPoseNode, AssetResult);
				TryConnectPosePins(
					FindPosePin(*CopyPoseNode, EGPD_Output, false),
					FindPosePin(*RootNode, EGPD_Input, false),
					AssetResult,
					TEXT("CopyPoseFromMesh -> OutputPose"));
			}
		}
	}

	if (AnimBlueprint && AssetResult.Errors.IsEmpty())
	{
		AssetResult.Actions.Add(TEXT("rebuilt attached-mesh AnimGraph with CopyPoseFromMesh only"));
		CompileAndSaveAnimBlueprint(*AnimBlueprint, Result, AssetResult);
	}

	Result.Errors.Append(AssetResult.Errors);
	Result.Warnings.Append(AssetResult.Warnings);
	Result.Assets.Add(MoveTemp(AssetResult));
	return Result;
}

FNteCharacterKawaiiWriteResult WriteCharacterKawaiiAssets(const FNteCharacterKawaiiPlan& Plan)
{
	FNteCharacterKawaiiWriteResult Result;
	Result.KawaiiAssetRootPath = Plan.KawaiiAssetRootPath;
	Result.SchemaProbe = ProbeNteKawaiiSchemaCompatibility();

	if (Plan.Presets.IsEmpty())
	{
		AddWarning(Result, TEXT("CharacterModSpec has no Kawaii presets; nothing to write."));
		return Result;
	}
	if (!Plan.Errors.IsEmpty())
	{
		Result.Errors.Append(Plan.Errors);
		return Result;
	}
	if (!Result.SchemaProbe.bNteCompatible)
	{
		Result.Errors.Append(Result.SchemaProbe.Errors);
		return Result;
	}

	for (const FNteCharacterKawaiiPresetPlanItem& Preset : Plan.Presets)
	{
		for (const FNteCharacterKawaiiCurvePlanItem& Curve : Preset.Curves)
		{
			WriteCurveAsset(Curve, Result);
		}
	}

	UClass* LimitsDataAssetClass = LoadObject<UClass>(nullptr, LimitsDataAssetClassPath);
	UClass* BoneConstraintsDataAssetClass = LoadObject<UClass>(nullptr, BoneConstraintsDataAssetClassPath);
	for (const FNteCharacterKawaiiPresetPlanItem& Preset : Plan.Presets)
	{
		WriteLimitsDataAsset(Preset, LimitsDataAssetClass, Result);
		WriteBoneConstraintsDataAsset(Preset, BoneConstraintsDataAssetClass, Result);
	}
	if (Result.HasErrors())
	{
		return Result;
	}

	TMap<FString, TArray<const FNteCharacterKawaiiPresetPlanItem*>> PresetsByRuntimeAnimBlueprint;
	for (const FNteCharacterKawaiiPresetPlanItem& Preset : Plan.Presets)
	{
		if (Preset.RuntimeAnimBlueprintPath.IsEmpty())
		{
			AddError(Result, FString::Printf(TEXT("Kawaii preset '%s' has no RuntimeAnimBlueprintPath."), *Preset.Id));
			continue;
		}
		PresetsByRuntimeAnimBlueprint.FindOrAdd(Preset.RuntimeAnimBlueprintPath).Add(&Preset);
	}
	for (const TPair<FString, TArray<const FNteCharacterKawaiiPresetPlanItem*>>& Pair : PresetsByRuntimeAnimBlueprint)
	{
		WriteRuntimeAnimBlueprint(Pair.Key, Pair.Value, Result);
	}
	return Result;
}

TSharedRef<FJsonObject> CharacterKawaiiSchemaProbeToJson(const FNteCharacterKawaiiSchemaProbeResult& Result)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetBoolField(TEXT("KawaiiPhysicsModuleAvailable"), Result.bKawaiiPhysicsModuleAvailable);
	Object->SetBoolField(TEXT("KawaiiPhysicsEditorModuleAvailable"), Result.bKawaiiPhysicsEditorModuleAvailable);
	Object->SetBoolField(TEXT("NteCompatible"), Result.bNteCompatible);
	Object->SetArrayField(TEXT("PresentTypes"), Json::StringArrayToJsonValues(Result.PresentTypes));
	Object->SetArrayField(TEXT("MissingTypes"), Json::StringArrayToJsonValues(Result.MissingTypes));
	Object->SetArrayField(TEXT("PresentFields"), Json::StringArrayToJsonValues(Result.PresentFields));
	Object->SetArrayField(TEXT("MissingFields"), Json::StringArrayToJsonValues(Result.MissingFields));
	Object->SetArrayField(TEXT("PublicOnlyFields"), Json::StringArrayToJsonValues(Result.PublicOnlyFields));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Result.Warnings));
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Result.Errors));
	return Object;
}

TSharedRef<FJsonObject> CharacterKawaiiWriteResultToJson(const FNteCharacterKawaiiWriteResult& Result)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	AddStringIfNotEmpty(Object, TEXT("KawaiiAssetRootPath"), Result.KawaiiAssetRootPath);
	Object->SetObjectField(TEXT("SchemaProbe"), CharacterKawaiiSchemaProbeToJson(Result.SchemaProbe));
	AppendAssetResultJsonArray(Object, TEXT("Assets"), Result.Assets);
	Object->SetNumberField(TEXT("AssetCount"), Result.Assets.Num());
	Object->SetArrayField(TEXT("SavedPackages"), Json::StringArrayToJsonValues(Result.SavedPackages));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Result.Warnings));
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Result.Errors));
	return Object;
}
}
