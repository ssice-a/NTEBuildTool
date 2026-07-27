// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteCharacterKawaiiPlan.h"

#include "NteCharacterModSpec.h"
#include "NteEditorAssetUtils.h"
#include "NteJsonFileUtils.h"

#include "Animation/Skeleton.h"
#include "Dom/JsonValue.h"
#include "Engine/SkeletalMesh.h"
#include "GameplayTagsManager.h"
#include "Misc/PackageName.h"

namespace NTEBuildTool::Character
{
namespace
{
constexpr const TCHAR* MainMeshId = TEXT("main");

bool IsMainMeshId(const FString& MeshId)
{
	return MeshId.IsEmpty() || MeshId.Equals(MainMeshId, ESearchCase::IgnoreCase);
}

const FNteCharacterAttachedMeshSpec* FindAttachedMeshById(const FNteCharacterModSpec& Spec, const FString& Id)
{
	for (const FNteCharacterAttachedMeshSpec& AttachedMesh : Spec.AttachedMeshes)
	{
		if (AttachedMesh.Id == Id)
		{
			return &AttachedMesh;
		}
	}
	return nullptr;
}

void AddStringIfNotEmpty(const TSharedRef<FJsonObject>& Object, const TCHAR* FieldName, const FString& Value)
{
	if (!Value.IsEmpty())
	{
		Object->SetStringField(FieldName, Value);
	}
}

TSharedRef<FJsonObject> VectorToJson(const FVector& Value)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("X"), Value.X);
	Object->SetNumberField(TEXT("Y"), Value.Y);
	Object->SetNumberField(TEXT("Z"), Value.Z);
	return Object;
}

FString NormalizeGamePackagePath(FString Path)
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
	while (Path.EndsWith(TEXT("/")))
	{
		Path.LeftChopInline(1);
	}
	return Path;
}

bool IsGamePackagePath(const FString& Path)
{
	return Path.StartsWith(TEXT("/Game/")) && !Path.Contains(TEXT("."));
}

void AddPackageSeed(TArray<FString>& Seeds, const FString& Path)
{
	const FString NormalizedPath = NormalizeGamePackagePath(Path);
	if (IsGamePackagePath(NormalizedPath))
	{
		Seeds.AddUnique(NormalizedPath);
	}
}

FString SanitizeAssetNamePart(FString RawName)
{
	RawName.TrimStartAndEndInline();
	FString Result;
	Result.Reserve(RawName.Len());
	bool bPreviousWasSeparator = false;
	for (int32 Index = 0; Index < RawName.Len(); ++Index)
	{
		const TCHAR Character = RawName[Index];
		if (FChar::IsAlnum(Character))
		{
			Result.AppendChar(Character);
			bPreviousWasSeparator = false;
		}
		else if (!bPreviousWasSeparator && !Result.IsEmpty())
		{
			Result.AppendChar(TEXT('_'));
			bPreviousWasSeparator = true;
		}
	}
	Result.RemoveFromEnd(TEXT("_"));
	return Result.IsEmpty() ? TEXT("Kawaii") : Result;
}

FString GetAssetDirectory(FString AssetPath)
{
	AssetPath = NormalizeGamePackagePath(AssetPath);
	if (AssetPath.IsEmpty())
	{
		return FString();
	}
	return NormalizeGamePackagePath(FPackageName::GetLongPackagePath(AssetPath));
}

FString GetPackageParentDirectory(FString PackageDirectory)
{
	PackageDirectory = NormalizeGamePackagePath(PackageDirectory);
	if (PackageDirectory.IsEmpty() || PackageDirectory == TEXT("/Game"))
	{
		return FString();
	}
	int32 SlashIndex = INDEX_NONE;
	if (!PackageDirectory.FindLastChar(TEXT('/'), SlashIndex) || SlashIndex <= FString(TEXT("/Game")).Len())
	{
		return TEXT("/Game");
	}
	return PackageDirectory.Left(SlashIndex);
}

FString DeriveModKawaiiRootFromAssetPath(FString AssetPath)
{
	AssetPath = NormalizeGamePackagePath(AssetPath);
	if (AssetPath.IsEmpty())
	{
		return FString();
	}

	const FString ModMarker = TEXT("/mod/");
	const int32 ModMarkerIndex = AssetPath.Find(ModMarker, ESearchCase::IgnoreCase, ESearchDir::FromStart);
	if (ModMarkerIndex != INDEX_NONE)
	{
		return NormalizeGamePackagePath(AssetPath.Left(ModMarkerIndex) / TEXT("mod/Kawaii"));
	}

	const FString AssetDirectory = GetAssetDirectory(AssetPath);
	return AssetDirectory.IsEmpty() ? FString() : NormalizeGamePackagePath(AssetDirectory / TEXT("mod/Kawaii"));
}

FString DeriveKawaiiRootPath(const FNteCharacterModSpec& Spec)
{
	for (const FNteCharacterKawaiiPresetSpec& Preset : Spec.KawaiiPresets)
	{
		const FString PresetRuntimeDirectory = GetAssetDirectory(Preset.RuntimeAnimBlueprintPath);
		if (!PresetRuntimeDirectory.IsEmpty())
		{
			const FString ParentDirectory = GetPackageParentDirectory(PresetRuntimeDirectory);
			return ParentDirectory.IsEmpty() ? PresetRuntimeDirectory : ParentDirectory;
		}
		const FString PresetLimitsDirectory = GetAssetDirectory(Preset.OutputLimitsDataAssetPath);
		if (!PresetLimitsDirectory.IsEmpty())
		{
			const FString ParentDirectory = GetPackageParentDirectory(PresetLimitsDirectory);
			return ParentDirectory.IsEmpty() ? PresetLimitsDirectory : ParentDirectory;
		}
	}

	for (const FNteCharacterAttachedMeshSpec& AttachedMesh : Spec.AttachedMeshes)
	{
		const FString AttachedRoot = DeriveModKawaiiRootFromAssetPath(AttachedMesh.MeshPath);
		if (!AttachedRoot.IsEmpty())
		{
			return AttachedRoot;
		}
	}

	const FString AppearanceRoot = DeriveModKawaiiRootFromAssetPath(Spec.Appearance.PlayerAppearanceAssetPath);
	if (!AppearanceRoot.IsEmpty())
	{
		return AppearanceRoot;
	}

	const FString MainRoot = DeriveModKawaiiRootFromAssetPath(Spec.MainMeshPath);
	if (!MainRoot.IsEmpty())
	{
		return MainRoot;
	}
	return FString();
}

FString DeriveTargetKawaiiRootPath(const FNteCharacterKawaiiPresetPlanItem& Item, const FNteCharacterModSpec& Spec)
{
	const FString TargetRoot = DeriveModKawaiiRootFromAssetPath(Item.TargetMeshPath);
	if (!TargetRoot.IsEmpty())
	{
		return TargetRoot;
	}
	return DeriveKawaiiRootPath(Spec);
}

FString JoinAssetPath(const FString& Folder, const FString& AssetName)
{
	if (Folder.IsEmpty() || AssetName.IsEmpty())
	{
		return FString();
	}
	return NTEBuildTool::Editor::JoinAssetPath(NormalizeGamePackagePath(Folder), AssetName);
}

FString MakePresetAssetSuffix(const FNteCharacterKawaiiPresetPlanItem& Item)
{
	if (!Item.Id.IsEmpty())
	{
		return SanitizeAssetNamePart(Item.Id);
	}
	if (!Item.SourceNodeName.IsEmpty())
	{
		return SanitizeAssetNamePart(Item.SourceNodeName);
	}
	return SanitizeAssetNamePart(Item.TargetMeshId);
}

FString MakeHostAnimBlueprintName(const FNteCharacterKawaiiPresetPlanItem& Item)
{
	const FString TargetMeshId = Item.TargetMeshId.IsEmpty() ? FString(MainMeshId) : Item.TargetMeshId;
	return FString::Printf(TEXT("ABP_NTE_%s_Kawaii"), *SanitizeAssetNamePart(TargetMeshId));
}

FString MakeLimitsAssetName(const FNteCharacterKawaiiPresetPlanItem& Item)
{
	return FString::Printf(TEXT("DA_NTE_%s_KawaiiLimits"), *MakePresetAssetSuffix(Item));
}

FString MakeBoneConstraintsAssetName(const FNteCharacterKawaiiPresetPlanItem& Item)
{
	return FString::Printf(TEXT("DA_NTE_%s_KawaiiConstraints"), *MakePresetAssetSuffix(Item));
}

FString MakeCurveAssetName(const FNteCharacterKawaiiPresetPlanItem& Item, const FNteCharacterKawaiiCurvePlanItem& Curve)
{
	return FString::Printf(
		TEXT("CF_NTE_%s_%s"),
		*MakePresetAssetSuffix(Item),
		*SanitizeAssetNamePart(Curve.CurveKind));
}

FString ResolveAttachedRuntimeAnimBlueprintPath(const FNteCharacterAttachedMeshSpec& AttachedMesh)
{
	if (!AttachedMesh.RuntimeAnimBlueprintPath.IsEmpty())
	{
		return NTEBuildTool::Editor::NormalizeAssetPathForText(AttachedMesh.RuntimeAnimBlueprintPath);
	}
	return FString();
}

FNteCharacterKawaiiAdditionalRootBonePlanItem BuildAdditionalRootBonePlanItem(const FNteCharacterKawaiiAdditionalRootBoneSpec& RootBone)
{
	FNteCharacterKawaiiAdditionalRootBonePlanItem Item;
	Item.RootBone = RootBone.RootBone;
	Item.OverrideExcludeBones = RootBone.OverrideExcludeBones;
	Item.bUseOverrideExcludeBones = RootBone.bUseOverrideExcludeBones;
	return Item;
}

FNteCharacterKawaiiPhysicsSettingsPlanItem BuildPhysicsSettingsPlanItem(const FNteCharacterKawaiiPhysicsSettingsSpec& Settings)
{
	FNteCharacterKawaiiPhysicsSettingsPlanItem Item;
	Item.Damping = Settings.Damping;
	Item.Stiffness = Settings.Stiffness;
	Item.WorldDampingLocation = Settings.WorldDampingLocation;
	Item.WorldDampingRotation = Settings.WorldDampingRotation;
	Item.Radius = Settings.Radius;
	Item.LimitAngle = Settings.LimitAngle;
	Item.ForwardMoveOffset = Settings.ForwardMoveOffset;
	Item.bHasForwardMoveOffset = Settings.bHasForwardMoveOffset;
	return Item;
}

FNteCharacterKawaiiCurvePlanItem BuildCurvePlanItem(
	const FNteCharacterKawaiiPresetPlanItem& PresetItem,
	const FNteCharacterKawaiiCurveSpec& Curve)
{
	FNteCharacterKawaiiCurvePlanItem Item;
	Item.CurveKind = Curve.CurveKind;
	Item.ExternalCurveObjectName = Curve.ExternalCurveObjectName;
	Item.ExternalCurveObjectPath = Curve.ExternalCurveObjectPath;
	Item.InlineKeyCount = Curve.InlineKeyCount;
	Item.OutputCurvePath = NormalizeGamePackagePath(Curve.OutputCurvePath);

	if (Item.CurveKind.IsEmpty())
	{
		Item.Errors.Add(TEXT("Curve has no CurveKind."));
	}
	if (Item.OutputCurvePath.IsEmpty())
	{
		Item.OutputCurvePath = JoinAssetPath(PresetItem.KawaiiAssetRootPath / TEXT("Curves"), MakeCurveAssetName(PresetItem, Item));
		Item.bDerivedOutputCurvePath = !Item.OutputCurvePath.IsEmpty();
	}
	if (Item.OutputCurvePath.IsEmpty())
	{
		Item.Errors.Add(TEXT("Could not derive OutputCurvePath."));
	}
	else if (!IsGamePackagePath(Item.OutputCurvePath))
	{
		Item.Errors.Add(FString::Printf(TEXT("OutputCurvePath must be a /Game package path: %s"), *Item.OutputCurvePath));
	}
	return Item;
}

FNteCharacterKawaiiLimitPlanItem BuildLimitPlanItem(const FNteCharacterKawaiiLimitSpec& Limit)
{
	FNteCharacterKawaiiLimitPlanItem Item;
	Item.LimitKind = Limit.LimitKind;
	Item.DrivingBone = Limit.DrivingBone;
	Item.OffsetLocation = Limit.OffsetLocation;
	Item.OffsetRotation = Limit.OffsetRotation;
	Item.Radius = Limit.Radius;
	Item.Length = Limit.Length;
	Item.SphereRadius = Limit.SphereRadius;
	Item.Extent = Limit.Extent;
	Item.Plane = Limit.Plane;
	Item.LimitType = Limit.LimitType;
	Item.SourceType = Limit.SourceType;
	Item.bEnable = Limit.bEnable;
	return Item;
}

bool IsMeaningfulBoneName(const FString& BoneName)
{
	return !BoneName.IsEmpty() && !BoneName.Equals(TEXT("None"), ESearchCase::IgnoreCase);
}

void AddReferencedBone(TArray<FString>& Bones, const FString& BoneName)
{
	if (IsMeaningfulBoneName(BoneName))
	{
		Bones.AddUnique(BoneName);
	}
}

void AddReferencedBones(TArray<FString>& Bones, const TArray<FString>& BoneNames)
{
	for (const FString& BoneName : BoneNames)
	{
		AddReferencedBone(Bones, BoneName);
	}
}

void DiagnoseKawaiiPresetBonesAndTag(FNteCharacterKawaiiPresetPlanItem& Item)
{
	AddReferencedBone(Item.ReferencedBones, Item.RootBone);
	AddReferencedBones(Item.ReferencedBones, Item.ExcludeBones);
	for (const FNteCharacterKawaiiAdditionalRootBonePlanItem& AdditionalRootBone : Item.AdditionalRootBones)
	{
		AddReferencedBone(Item.ReferencedBones, AdditionalRootBone.RootBone);
		AddReferencedBones(Item.ReferencedBones, AdditionalRootBone.OverrideExcludeBones);
	}
	for (const FNteCharacterKawaiiLimitPlanItem& Limit : Item.CollisionLimits)
	{
		AddReferencedBone(Item.ReferencedBones, Limit.DrivingBone);
	}
	AddReferencedBones(Item.ReferencedBones, Item.IgnoreBones);
	Item.ReferencedBones.Sort();

	if (!Item.TargetMeshPath.IsEmpty())
	{
		if (USkeletalMesh* TargetMesh = NTEBuildTool::Editor::LoadAssetByPath<USkeletalMesh>(Item.TargetMeshPath))
		{
			Item.bTargetMeshLoaded = true;
			if (const USkeleton* Skeleton = TargetMesh->GetSkeleton())
			{
				Item.TargetSkeletonPath = Skeleton->GetPackage()->GetName();
			}

			const FReferenceSkeleton& ReferenceSkeleton = TargetMesh->GetRefSkeleton();
			for (const FString& BoneName : Item.ReferencedBones)
			{
				if (ReferenceSkeleton.FindBoneIndex(FName(*BoneName)) == INDEX_NONE)
				{
					Item.MissingBones.Add(BoneName);
				}
			}
			Item.MissingBones.Sort();
			for (const FString& MissingBone : Item.MissingBones)
			{
				Item.Warnings.Add(FString::Printf(
					TEXT("Kawaii preset references bone '%s' that is not present on target mesh %s."),
					*MissingBone,
					*Item.TargetMeshPath));
			}
		}
		else
		{
			Item.Warnings.Add(FString::Printf(
				TEXT("Target mesh could not be loaded for non-blocking Kawaii bone diagnostics: %s"),
				*Item.TargetMeshPath));
		}
	}

	if (!Item.KawaiiPhysicsTag.IsEmpty())
	{
		Item.bKawaiiPhysicsTagChecked = true;
		const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*Item.KawaiiPhysicsTag), false);
		Item.bKawaiiPhysicsTagValid = Tag.IsValid();
		if (!Item.bKawaiiPhysicsTagValid)
		{
			Item.Warnings.Add(FString::Printf(
				TEXT("KawaiiPhysicsTag '%s' is not registered in GameplayTags. The cooked asset can still be generated, but UE/game logs may warn until the tag exists."),
				*Item.KawaiiPhysicsTag));
		}
	}
}

FNteCharacterKawaiiPresetPlanItem BuildPresetPlanItem(
	const FNteCharacterModSpec& Spec,
	const FNteCharacterKawaiiPresetSpec& Preset)
{
	FNteCharacterKawaiiPresetPlanItem Item;
	Item.Id = Preset.Id;
	Item.Label = Preset.Label;
	Item.TargetMeshId = IsMainMeshId(Preset.TargetMeshId) ? MainMeshId : Preset.TargetMeshId;
	Item.SourceKind = Preset.SourceKind.IsEmpty() ? TEXT("Manual") : Preset.SourceKind;
	Item.SourceAnimBlueprintJson = Preset.SourceAnimBlueprintJson;
	Item.SourceGeneratedClassName = Preset.SourceGeneratedClassName;
	Item.SourceClassDefaultObjectName = Preset.SourceClassDefaultObjectName;
	Item.SourceNodeName = Preset.SourceNodeName;
	Item.ReferencedPresetId = Preset.ReferencedPresetId;
	Item.TemplateKind = Preset.TemplateKind;
	Item.SchemaStatus = Preset.SchemaStatus;
	Item.RootBone = Preset.RootBone;
	Item.ExcludeBones = Preset.ExcludeBones;
	Item.PhysicsSettings = BuildPhysicsSettingsPlanItem(Preset.PhysicsSettings);
	Item.DummyBoneLength = Preset.DummyBoneLength;
	Item.BoneForwardAxis = Preset.BoneForwardAxis;
	Item.TargetFramerate = Preset.TargetFramerate;
	Item.bOverrideTargetFramerate = Preset.bOverrideTargetFramerate;
	Item.WarmUpFrames = Preset.WarmUpFrames;
	Item.bUseWarmUpWhenResetDynamics = Preset.bUseWarmUpWhenResetDynamics;
	Item.bNeedWarmUp = Preset.bNeedWarmUp;
	Item.TeleportDistanceThreshold = Preset.TeleportDistanceThreshold;
	Item.TeleportRotationThreshold = Preset.TeleportRotationThreshold;
	Item.PlanarConstraint = Preset.PlanarConstraint;
	Item.bResetBoneTransformWhenBoneNotFound = Preset.bResetBoneTransformWhenBoneNotFound;
	Item.AdditionalRootBoneCount = Preset.AdditionalRootBones.Num();
	Item.CollisionLimitCount = Preset.CollisionLimits.Num();
	Item.CurveCount = Preset.Curves.Num();
	Item.SphericalLimitsDataCount = Preset.SphericalLimitsDataCount;
	Item.CapsuleLimitsDataCount = Preset.CapsuleLimitsDataCount;
	Item.BoxLimitsDataCount = Preset.BoxLimitsDataCount;
	Item.PlanarLimitsDataCount = Preset.PlanarLimitsDataCount;
	Item.BoneConstraintCount = Preset.BoneConstraintCount;
	Item.BoneConstraintsDataCount = Preset.BoneConstraintsDataCount;
	Item.LimitsDataAssetPath = NormalizeGamePackagePath(Preset.LimitsDataAssetPath);
	Item.PhysicsAssetForLimitsPath = NormalizeGamePackagePath(Preset.PhysicsAssetForLimitsPath);
	Item.OutputLimitsDataAssetPath = NormalizeGamePackagePath(Preset.OutputLimitsDataAssetPath);
	Item.BoneConstraintsDataAssetPath = NormalizeGamePackagePath(Preset.BoneConstraintsDataAssetPath);
	Item.OutputBoneConstraintsDataAssetPath = NormalizeGamePackagePath(Preset.OutputBoneConstraintsDataAssetPath);
	Item.BoneConstraintGlobalComplianceType = Preset.BoneConstraintGlobalComplianceType;
	Item.BoneConstraintIterationCountBeforeCollision = Preset.BoneConstraintIterationCountBeforeCollision;
	Item.BoneConstraintIterationCountAfterCollision = Preset.BoneConstraintIterationCountAfterCollision;
	Item.bAutoAddChildDummyBoneConstraint = Preset.bAutoAddChildDummyBoneConstraint;
	Item.Gravity = Preset.Gravity;
	Item.bEnableWind = Preset.bEnableWind;
	Item.WindScale = Preset.WindScale;
	Item.bHasUseRelativeMove = Preset.bHasUseRelativeMove;
	Item.bUseRelativeMove = Preset.bUseRelativeMove;
	Item.MovementReferenceDisplacement = Preset.MovementReferenceDisplacement;
	Item.bAllowWorldCollision = Preset.bAllowWorldCollision;
	Item.bOverrideCollisionParams = Preset.bOverrideCollisionParams;
	Item.bIgnoreSelfComponent = Preset.bIgnoreSelfComponent;
	Item.IgnoreBones = Preset.IgnoreBones;
	Item.IgnoreBoneNamePrefix = Preset.IgnoreBoneNamePrefix;
	Item.KawaiiPhysicsTag = Preset.KawaiiPhysicsTag;

	if (Item.TargetMeshId == MainMeshId)
	{
		Item.TargetKind = TEXT("MainMesh");
		Item.TargetMeshPath = NormalizeGamePackagePath(Spec.MainMeshPath);
	}
	else if (const FNteCharacterAttachedMeshSpec* AttachedMesh = FindAttachedMeshById(Spec, Item.TargetMeshId))
	{
		Item.TargetKind = TEXT("AttachedMesh");
		Item.TargetMeshPath = NormalizeGamePackagePath(AttachedMesh->MeshPath);
	}
	else
	{
		Item.TargetKind = TEXT("Unknown");
		Item.Errors.Add(FString::Printf(TEXT("Kawaii preset targets unknown mesh id '%s'."), *Preset.TargetMeshId));
	}

	Item.KawaiiAssetRootPath = DeriveTargetKawaiiRootPath(Item, Spec);
	if (Item.KawaiiAssetRootPath.IsEmpty())
	{
		Item.Errors.Add(TEXT("Could not derive Kawaii asset root path from target mesh, appearance, or main mesh."));
	}

	Item.RuntimeAnimBlueprintPath = NormalizeGamePackagePath(Preset.RuntimeAnimBlueprintPath);
	if (Item.RuntimeAnimBlueprintPath.IsEmpty() && Item.TargetMeshId != MainMeshId)
	{
		if (const FNteCharacterAttachedMeshSpec* AttachedMesh = FindAttachedMeshById(Spec, Item.TargetMeshId))
		{
			Item.RuntimeAnimBlueprintPath = ResolveAttachedRuntimeAnimBlueprintPath(*AttachedMesh);
		}
	}
	if (Item.RuntimeAnimBlueprintPath.IsEmpty())
	{
		Item.RuntimeAnimBlueprintPath = JoinAssetPath(Item.KawaiiAssetRootPath / TEXT("Anim"), MakeHostAnimBlueprintName(Item));
		Item.bDerivedRuntimeAnimBlueprintPath = !Item.RuntimeAnimBlueprintPath.IsEmpty();
	}

	if (Item.OutputLimitsDataAssetPath.IsEmpty())
	{
		Item.OutputLimitsDataAssetPath = JoinAssetPath(Item.KawaiiAssetRootPath / TEXT("Data"), MakeLimitsAssetName(Item));
		Item.bDerivedOutputLimitsDataAssetPath = !Item.OutputLimitsDataAssetPath.IsEmpty();
	}
	if (Item.OutputBoneConstraintsDataAssetPath.IsEmpty())
	{
		Item.OutputBoneConstraintsDataAssetPath = JoinAssetPath(Item.KawaiiAssetRootPath / TEXT("Data"), MakeBoneConstraintsAssetName(Item));
		Item.bDerivedOutputBoneConstraintsDataAssetPath = !Item.OutputBoneConstraintsDataAssetPath.IsEmpty();
	}

	if (Item.RuntimeAnimBlueprintPath.IsEmpty())
	{
		Item.Errors.Add(TEXT("Could not resolve RuntimeAnimBlueprintPath."));
	}
	else if (!IsGamePackagePath(Item.RuntimeAnimBlueprintPath))
	{
		Item.Errors.Add(FString::Printf(TEXT("RuntimeAnimBlueprintPath must be a /Game package path: %s"), *Item.RuntimeAnimBlueprintPath));
	}

	if (Item.OutputLimitsDataAssetPath.IsEmpty())
	{
		Item.Errors.Add(TEXT("Could not resolve OutputLimitsDataAssetPath."));
	}
	else if (!IsGamePackagePath(Item.OutputLimitsDataAssetPath))
	{
		Item.Errors.Add(FString::Printf(TEXT("OutputLimitsDataAssetPath must be a /Game package path: %s"), *Item.OutputLimitsDataAssetPath));
	}

	if (Item.OutputBoneConstraintsDataAssetPath.IsEmpty())
	{
		Item.Errors.Add(TEXT("Could not resolve OutputBoneConstraintsDataAssetPath."));
	}
	else if (!IsGamePackagePath(Item.OutputBoneConstraintsDataAssetPath))
	{
		Item.Errors.Add(FString::Printf(TEXT("OutputBoneConstraintsDataAssetPath must be a /Game package path: %s"), *Item.OutputBoneConstraintsDataAssetPath));
	}

	if (Item.RootBone.IsEmpty())
	{
		Item.Warnings.Add(TEXT("Kawaii preset has no RootBone."));
	}
	if (!Item.SchemaStatus.IsEmpty()
		&& !Item.SchemaStatus.Equals(TEXT("Compatible"), ESearchCase::IgnoreCase)
		&& !Item.SchemaStatus.Equals(TEXT("Unknown"), ESearchCase::IgnoreCase))
	{
		Item.Warnings.Add(FString::Printf(TEXT("Kawaii schema status is '%s'; AnimBP/DataAsset writer must not cook final Kawaii assets until schema compatibility is resolved."), *Item.SchemaStatus));
	}
	if (Item.SourceKind == TEXT("ImportedJson") && Item.SourceNodeName.IsEmpty())
	{
		Item.Warnings.Add(TEXT("Imported Kawaii preset has no SourceNodeName."));
	}
	if (Item.CollisionLimitCount == 0 && Item.LimitsDataAssetPath.IsEmpty() && Item.OutputLimitsDataAssetPath.IsEmpty())
	{
		Item.Warnings.Add(TEXT("Kawaii preset has no inline collision limits and no limits data asset path."));
	}

	for (const FNteCharacterKawaiiCurveSpec& Curve : Preset.Curves)
	{
		FNteCharacterKawaiiCurvePlanItem CurveItem = BuildCurvePlanItem(Item, Curve);
		Item.Errors.Append(CurveItem.Errors);
		Item.Warnings.Append(CurveItem.Warnings);
		Item.Curves.Add(MoveTemp(CurveItem));
	}
	for (const FNteCharacterKawaiiAdditionalRootBoneSpec& AdditionalRootBone : Preset.AdditionalRootBones)
	{
		Item.AdditionalRootBones.Add(BuildAdditionalRootBonePlanItem(AdditionalRootBone));
	}
	for (const FNteCharacterKawaiiLimitSpec& Limit : Preset.CollisionLimits)
	{
		Item.CollisionLimits.Add(BuildLimitPlanItem(Limit));
	}

	DiagnoseKawaiiPresetBonesAndTag(Item);

	AddPackageSeed(Item.PackageSeeds, Item.RuntimeAnimBlueprintPath);
	AddPackageSeed(Item.PackageSeeds, Item.OutputLimitsDataAssetPath);
	AddPackageSeed(Item.PackageSeeds, Item.PhysicsAssetForLimitsPath);
	AddPackageSeed(Item.PackageSeeds, Item.OutputBoneConstraintsDataAssetPath);
	for (const FNteCharacterKawaiiCurvePlanItem& Curve : Item.Curves)
	{
		AddPackageSeed(Item.PackageSeeds, Curve.OutputCurvePath);
	}
	Item.PackageSeeds.Sort();

	return Item;
}

TSharedRef<FJsonObject> CurvePlanItemToJson(const FNteCharacterKawaiiCurvePlanItem& Item)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	AddStringIfNotEmpty(Object, TEXT("CurveKind"), Item.CurveKind);
	AddStringIfNotEmpty(Object, TEXT("ExternalCurveObjectName"), Item.ExternalCurveObjectName);
	AddStringIfNotEmpty(Object, TEXT("ExternalCurveObjectPath"), Item.ExternalCurveObjectPath);
	Object->SetNumberField(TEXT("InlineKeyCount"), Item.InlineKeyCount);
	AddStringIfNotEmpty(Object, TEXT("OutputCurvePath"), Item.OutputCurvePath);
	Object->SetBoolField(TEXT("DerivedOutputCurvePath"), Item.bDerivedOutputCurvePath);
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Item.Errors));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Item.Warnings));
	return Object;
}

TSharedRef<FJsonObject> AdditionalRootBonePlanItemToJson(const FNteCharacterKawaiiAdditionalRootBonePlanItem& Item)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	AddStringIfNotEmpty(Object, TEXT("RootBone"), Item.RootBone);
	Object->SetArrayField(TEXT("OverrideExcludeBones"), Json::StringArrayToJsonValues(Item.OverrideExcludeBones));
	Object->SetBoolField(TEXT("UseOverrideExcludeBones"), Item.bUseOverrideExcludeBones);
	return Object;
}

TSharedRef<FJsonObject> PhysicsSettingsPlanItemToJson(const FNteCharacterKawaiiPhysicsSettingsPlanItem& Item)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("Damping"), Item.Damping);
	Object->SetNumberField(TEXT("Stiffness"), Item.Stiffness);
	Object->SetNumberField(TEXT("WorldDampingLocation"), Item.WorldDampingLocation);
	Object->SetNumberField(TEXT("WorldDampingRotation"), Item.WorldDampingRotation);
	Object->SetNumberField(TEXT("Radius"), Item.Radius);
	Object->SetNumberField(TEXT("LimitAngle"), Item.LimitAngle);
	Object->SetNumberField(TEXT("ForwardMoveOffset"), Item.ForwardMoveOffset);
	Object->SetBoolField(TEXT("HasForwardMoveOffset"), Item.bHasForwardMoveOffset);
	return Object;
}

TSharedRef<FJsonObject> PresetPlanItemToJson(const FNteCharacterKawaiiPresetPlanItem& Item)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	AddStringIfNotEmpty(Object, TEXT("Id"), Item.Id);
	AddStringIfNotEmpty(Object, TEXT("Label"), Item.Label);
	AddStringIfNotEmpty(Object, TEXT("TargetMeshId"), Item.TargetMeshId);
	AddStringIfNotEmpty(Object, TEXT("TargetKind"), Item.TargetKind);
	AddStringIfNotEmpty(Object, TEXT("TargetMeshPath"), Item.TargetMeshPath);
	Object->SetBoolField(TEXT("TargetMeshLoaded"), Item.bTargetMeshLoaded);
	AddStringIfNotEmpty(Object, TEXT("TargetSkeletonPath"), Item.TargetSkeletonPath);
	Object->SetArrayField(TEXT("ReferencedBones"), Json::StringArrayToJsonValues(Item.ReferencedBones));
	Object->SetArrayField(TEXT("MissingBones"), Json::StringArrayToJsonValues(Item.MissingBones));
	AddStringIfNotEmpty(Object, TEXT("SourceKind"), Item.SourceKind);
	AddStringIfNotEmpty(Object, TEXT("SourceAnimBlueprintJson"), Item.SourceAnimBlueprintJson);
	AddStringIfNotEmpty(Object, TEXT("SourceGeneratedClassName"), Item.SourceGeneratedClassName);
	AddStringIfNotEmpty(Object, TEXT("SourceClassDefaultObjectName"), Item.SourceClassDefaultObjectName);
	AddStringIfNotEmpty(Object, TEXT("SourceNodeName"), Item.SourceNodeName);
	AddStringIfNotEmpty(Object, TEXT("ReferencedPresetId"), Item.ReferencedPresetId);
	AddStringIfNotEmpty(Object, TEXT("TemplateKind"), Item.TemplateKind);
	AddStringIfNotEmpty(Object, TEXT("SchemaStatus"), Item.SchemaStatus);
	AddStringIfNotEmpty(Object, TEXT("RootBone"), Item.RootBone);
	Object->SetArrayField(TEXT("ExcludeBones"), Json::StringArrayToJsonValues(Item.ExcludeBones));
	Object->SetObjectField(TEXT("PhysicsSettings"), PhysicsSettingsPlanItemToJson(Item.PhysicsSettings));
	Object->SetNumberField(TEXT("DummyBoneLength"), Item.DummyBoneLength);
	AddStringIfNotEmpty(Object, TEXT("BoneForwardAxis"), Item.BoneForwardAxis);
	Object->SetNumberField(TEXT("TargetFramerate"), Item.TargetFramerate);
	Object->SetBoolField(TEXT("OverrideTargetFramerate"), Item.bOverrideTargetFramerate);
	Object->SetNumberField(TEXT("WarmUpFrames"), Item.WarmUpFrames);
	Object->SetBoolField(TEXT("UseWarmUpWhenResetDynamics"), Item.bUseWarmUpWhenResetDynamics);
	Object->SetBoolField(TEXT("NeedWarmUp"), Item.bNeedWarmUp);
	Object->SetNumberField(TEXT("TeleportDistanceThreshold"), Item.TeleportDistanceThreshold);
	Object->SetNumberField(TEXT("TeleportRotationThreshold"), Item.TeleportRotationThreshold);
	AddStringIfNotEmpty(Object, TEXT("PlanarConstraint"), Item.PlanarConstraint);
	Object->SetBoolField(TEXT("ResetBoneTransformWhenBoneNotFound"), Item.bResetBoneTransformWhenBoneNotFound);
	Object->SetNumberField(TEXT("AdditionalRootBoneCount"), Item.AdditionalRootBoneCount);
	Object->SetNumberField(TEXT("CollisionLimitCount"), Item.CollisionLimitCount);
	Object->SetNumberField(TEXT("CurveCount"), Item.CurveCount);
	Object->SetNumberField(TEXT("SphericalLimitsDataCount"), Item.SphericalLimitsDataCount);
	Object->SetNumberField(TEXT("CapsuleLimitsDataCount"), Item.CapsuleLimitsDataCount);
	Object->SetNumberField(TEXT("BoxLimitsDataCount"), Item.BoxLimitsDataCount);
	Object->SetNumberField(TEXT("PlanarLimitsDataCount"), Item.PlanarLimitsDataCount);
	Object->SetNumberField(TEXT("BoneConstraintCount"), Item.BoneConstraintCount);
	Object->SetNumberField(TEXT("BoneConstraintsDataCount"), Item.BoneConstraintsDataCount);
	AddStringIfNotEmpty(Object, TEXT("KawaiiAssetRootPath"), Item.KawaiiAssetRootPath);
	AddStringIfNotEmpty(Object, TEXT("RuntimeAnimBlueprintPath"), Item.RuntimeAnimBlueprintPath);
	Object->SetBoolField(TEXT("DerivedRuntimeAnimBlueprintPath"), Item.bDerivedRuntimeAnimBlueprintPath);
	AddStringIfNotEmpty(Object, TEXT("LimitsDataAssetPath"), Item.LimitsDataAssetPath);
	AddStringIfNotEmpty(Object, TEXT("PhysicsAssetForLimitsPath"), Item.PhysicsAssetForLimitsPath);
	AddStringIfNotEmpty(Object, TEXT("OutputLimitsDataAssetPath"), Item.OutputLimitsDataAssetPath);
	Object->SetBoolField(TEXT("DerivedOutputLimitsDataAssetPath"), Item.bDerivedOutputLimitsDataAssetPath);
	AddStringIfNotEmpty(Object, TEXT("BoneConstraintsDataAssetPath"), Item.BoneConstraintsDataAssetPath);
	AddStringIfNotEmpty(Object, TEXT("OutputBoneConstraintsDataAssetPath"), Item.OutputBoneConstraintsDataAssetPath);
	Object->SetBoolField(TEXT("DerivedOutputBoneConstraintsDataAssetPath"), Item.bDerivedOutputBoneConstraintsDataAssetPath);

	TArray<TSharedPtr<FJsonValue>> Curves;
	for (const FNteCharacterKawaiiCurvePlanItem& Curve : Item.Curves)
	{
		Curves.Add(MakeShared<FJsonValueObject>(CurvePlanItemToJson(Curve)));
	}
	Object->SetArrayField(TEXT("Curves"), Curves);

	TArray<TSharedPtr<FJsonValue>> AdditionalRootBones;
	for (const FNteCharacterKawaiiAdditionalRootBonePlanItem& RootBone : Item.AdditionalRootBones)
	{
		AdditionalRootBones.Add(MakeShared<FJsonValueObject>(AdditionalRootBonePlanItemToJson(RootBone)));
	}
	Object->SetArrayField(TEXT("AdditionalRootBones"), AdditionalRootBones);

	AddStringIfNotEmpty(Object, TEXT("BoneConstraintGlobalComplianceType"), Item.BoneConstraintGlobalComplianceType);
	Object->SetNumberField(TEXT("BoneConstraintIterationCountBeforeCollision"), Item.BoneConstraintIterationCountBeforeCollision);
	Object->SetNumberField(TEXT("BoneConstraintIterationCountAfterCollision"), Item.BoneConstraintIterationCountAfterCollision);
	Object->SetBoolField(TEXT("AutoAddChildDummyBoneConstraint"), Item.bAutoAddChildDummyBoneConstraint);
	Object->SetObjectField(TEXT("Gravity"), VectorToJson(Item.Gravity));
	Object->SetBoolField(TEXT("EnableWind"), Item.bEnableWind);
	Object->SetNumberField(TEXT("WindScale"), Item.WindScale);
	Object->SetBoolField(TEXT("HasUseRelativeMove"), Item.bHasUseRelativeMove);
	Object->SetBoolField(TEXT("UseRelativeMove"), Item.bUseRelativeMove);
	Object->SetObjectField(TEXT("MovementReferenceDisplacement"), VectorToJson(Item.MovementReferenceDisplacement));
	Object->SetBoolField(TEXT("AllowWorldCollision"), Item.bAllowWorldCollision);
	Object->SetBoolField(TEXT("OverrideCollisionParams"), Item.bOverrideCollisionParams);
	Object->SetBoolField(TEXT("IgnoreSelfComponent"), Item.bIgnoreSelfComponent);
	Object->SetArrayField(TEXT("IgnoreBones"), Json::StringArrayToJsonValues(Item.IgnoreBones));
	Object->SetArrayField(TEXT("IgnoreBoneNamePrefix"), Json::StringArrayToJsonValues(Item.IgnoreBoneNamePrefix));
	AddStringIfNotEmpty(Object, TEXT("KawaiiPhysicsTag"), Item.KawaiiPhysicsTag);
	Object->SetBoolField(TEXT("KawaiiPhysicsTagChecked"), Item.bKawaiiPhysicsTagChecked);
	Object->SetBoolField(TEXT("KawaiiPhysicsTagValid"), Item.bKawaiiPhysicsTagValid);
	Object->SetArrayField(TEXT("PackageSeeds"), Json::StringArrayToJsonValues(Item.PackageSeeds));
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Item.Errors));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Item.Warnings));
	return Object;
}
}

FNteCharacterKawaiiPlan BuildCharacterKawaiiPlanFromSpec(const FNteCharacterModSpec& Spec)
{
	FNteCharacterKawaiiPlan Plan;
	Plan.KawaiiAssetRootPath = DeriveKawaiiRootPath(Spec);
	if (Spec.KawaiiPresets.IsEmpty())
	{
		return Plan;
	}
	if (Plan.KawaiiAssetRootPath.IsEmpty())
	{
		Plan.Errors.Add(TEXT("Could not derive a /Game Kawaii asset root path from CharacterModSpec."));
	}

	for (const FNteCharacterKawaiiPresetSpec& Preset : Spec.KawaiiPresets)
	{
		FNteCharacterKawaiiPresetPlanItem Item = BuildPresetPlanItem(Spec, Preset);
		Plan.Errors.Append(Item.Errors);
		Plan.Warnings.Append(Item.Warnings);
		for (const FString& Seed : Item.PackageSeeds)
		{
			Plan.PackageSeeds.AddUnique(Seed);
		}
		Plan.Presets.Add(MoveTemp(Item));
	}
	Plan.PackageSeeds.Sort();
	return Plan;
}

TSharedRef<FJsonObject> CharacterKawaiiPlanToJson(const FNteCharacterKawaiiPlan& Plan)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	AddStringIfNotEmpty(Object, TEXT("KawaiiAssetRootPath"), Plan.KawaiiAssetRootPath);
	Object->SetNumberField(TEXT("PresetCount"), Plan.Presets.Num());
	TArray<TSharedPtr<FJsonValue>> Presets;
	for (const FNteCharacterKawaiiPresetPlanItem& Preset : Plan.Presets)
	{
		Presets.Add(MakeShared<FJsonValueObject>(PresetPlanItemToJson(Preset)));
	}
	Object->SetArrayField(TEXT("Presets"), Presets);
	Object->SetArrayField(TEXT("PackageSeeds"), Json::StringArrayToJsonValues(Plan.PackageSeeds));
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Plan.Errors));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Plan.Warnings));
	return Object;
}

TArray<FString> CollectCharacterKawaiiPlanPackageSeeds(const FNteCharacterKawaiiPlan& Plan)
{
	TArray<FString> Seeds = Plan.PackageSeeds;
	Seeds.Sort();
	return Seeds;
}
}
