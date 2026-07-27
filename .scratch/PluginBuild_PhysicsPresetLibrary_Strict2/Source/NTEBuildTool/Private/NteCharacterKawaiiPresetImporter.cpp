// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteCharacterKawaiiPresetImporter.h"

#include "FModelJsonUtils.h"
#include "FModelKawaiiAnimLayerAnalysis.h"

#include "Dom/JsonValue.h"
#include "Misc/Paths.h"

namespace NTEBuildTool::Character
{
namespace
{
TArray<TSharedPtr<FJsonValue>> StringArrayToJsonValues(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	for (const FString& Value : Values)
	{
		Result.Add(MakeShared<FJsonValueString>(Value));
	}
	return Result;
}

FString NameToString(const FName Name)
{
	return Name.IsNone() ? FString() : Name.ToString();
}

TArray<FString> NameArrayToStringArray(const TArray<FName>& Names)
{
	TArray<FString> Result;
	for (const FName Name : Names)
	{
		if (!Name.IsNone())
		{
			Result.Add(Name.ToString());
		}
	}
	return Result;
}

FString SanitizeIdentifier(const FString& Value)
{
	FString Result;
	Result.Reserve(Value.Len());
	bool bPreviousWasSeparator = false;
	for (int32 Index = 0; Index < Value.Len(); ++Index)
	{
		const TCHAR Character = Value[Index];
		if (FChar::IsAlnum(Character))
		{
			Result.AppendChar(FChar::ToLower(Character));
			bPreviousWasSeparator = false;
		}
		else if (!bPreviousWasSeparator && !Result.IsEmpty())
		{
			Result.AppendChar(TEXT('_'));
			bPreviousWasSeparator = true;
		}
	}
	Result.RemoveFromEnd(TEXT("_"));
	return Result.IsEmpty() ? TEXT("kawaii") : Result;
}

FString MakeNodeSlug(const FString& GraphNodeName)
{
	FString Suffix;
	if (GraphNodeName == TEXT("AnimGraphNode_KawaiiPhysics"))
	{
		return TEXT("kawaii");
	}
	FString Prefix;
	if (GraphNodeName.Split(TEXT("AnimGraphNode_KawaiiPhysics_"), &Prefix, &Suffix) && !Suffix.IsEmpty())
	{
		return FString::Printf(TEXT("kawaii_%s"), *SanitizeIdentifier(Suffix));
	}
	return SanitizeIdentifier(GraphNodeName);
}

FString MakePresetId(const FNteCharacterKawaiiImportOptions& Options, const FString& GraphNodeName)
{
	const FString Prefix = Options.PresetIdPrefix.IsEmpty() ? Options.TargetMeshId : Options.PresetIdPrefix;
	return FString::Printf(TEXT("%s_%s"), *SanitizeIdentifier(Prefix.IsEmpty() ? TEXT("main") : Prefix), *MakeNodeSlug(GraphNodeName));
}

FNteCharacterKawaiiPhysicsSettingsSpec ConvertPhysicsSettings(const FFModelKawaiiPhysicsSettings& Source)
{
	FNteCharacterKawaiiPhysicsSettingsSpec Target;
	Target.Damping = Source.Damping;
	Target.Stiffness = Source.Stiffness;
	Target.WorldDampingLocation = Source.WorldDampingLocation;
	Target.WorldDampingRotation = Source.WorldDampingRotation;
	Target.Radius = Source.Radius;
	Target.LimitAngle = Source.LimitAngle;
	Target.ForwardMoveOffset = Source.ForwardMoveOffset;
	Target.bHasForwardMoveOffset = Source.bHasForwardMoveOffset;
	return Target;
}

FNteCharacterKawaiiAdditionalRootBoneSpec ConvertAdditionalRootBone(const FFModelKawaiiRootBoneSetting& Source)
{
	FNteCharacterKawaiiAdditionalRootBoneSpec Target;
	Target.RootBone = NameToString(Source.RootBone);
	Target.OverrideExcludeBones = NameArrayToStringArray(Source.OverrideExcludeBones);
	Target.bUseOverrideExcludeBones = Source.bUseOverrideExcludeBones;
	return Target;
}

FNteCharacterKawaiiLimitSpec ConvertLimit(const FFModelKawaiiCollisionLimit& Source)
{
	FNteCharacterKawaiiLimitSpec Target;
	Target.LimitKind = Source.LimitKind;
	Target.DrivingBone = NameToString(Source.DrivingBone);
	Target.OffsetLocation = Source.OffsetLocation;
	Target.OffsetRotation = Source.OffsetRotation;
	Target.Radius = Source.Radius;
	Target.Length = Source.Length;
	Target.SphereRadius = Source.SphereRadius;
	Target.Extent = Source.Extent;
	Target.Plane = Source.Plane;
	Target.LimitType = Source.LimitType;
	Target.SourceType = Source.SourceType;
	Target.bEnable = Source.bEnable;
	return Target;
}

void AppendLimits(const TArray<FFModelKawaiiCollisionLimit>& Source, TArray<FNteCharacterKawaiiLimitSpec>& Target)
{
	for (const FFModelKawaiiCollisionLimit& Limit : Source)
	{
		Target.Add(ConvertLimit(Limit));
	}
}

void AddCurveIfPresent(
	TArray<FNteCharacterKawaiiCurveSpec>& Curves,
	const FString& CurveKind,
	const FFModelKawaiiCurveReference& Source)
{
	if (Source.ExternalCurveObjectName.IsEmpty()
		&& Source.ExternalCurveObjectPath.IsEmpty()
		&& Source.InlineKeyCount == 0)
	{
		return;
	}

	FNteCharacterKawaiiCurveSpec Curve;
	Curve.CurveKind = CurveKind;
	Curve.ExternalCurveObjectName = Source.ExternalCurveObjectName;
	Curve.ExternalCurveObjectPath = Source.ExternalCurveObjectPath;
	Curve.InlineKeyCount = Source.InlineKeyCount;
	Curves.Add(Curve);
}

FNteCharacterKawaiiPresetSpec ConvertNode(
	const FFModelKawaiiAnimLayerAnalysis& Analysis,
	const FFModelKawaiiNodeAnalysis& Node,
	const FNteCharacterKawaiiImportOptions& Options,
	const FString& PresetId)
{
	FNteCharacterKawaiiPresetSpec Preset;
	Preset.Id = PresetId;
	Preset.Label = Analysis.GeneratedClassName.IsEmpty()
		? Node.GraphNodeName
		: FString::Printf(TEXT("%s / %s"), *Analysis.GeneratedClassName, *Node.GraphNodeName);
	Preset.TargetMeshId = Options.TargetMeshId.IsEmpty() ? TEXT("main") : Options.TargetMeshId;
	Preset.SourceKind = TEXT("ImportedJson");
	Preset.SourceAnimBlueprintJson = Options.SourceJsonPath;
	Preset.SourceGeneratedClassName = Analysis.GeneratedClassName;
	Preset.SourceClassDefaultObjectName = Analysis.ClassDefaultObjectName;
	Preset.SourceNodeName = Node.GraphNodeName;
	Preset.SchemaStatus = TEXT("NeedsNteSchemaCheck");

	Preset.RootBone = NameToString(Node.RootBone);
	Preset.ExcludeBones = NameArrayToStringArray(Node.ExcludeBones);
	for (const FFModelKawaiiRootBoneSetting& AdditionalRootBone : Node.AdditionalRootBones)
	{
		Preset.AdditionalRootBones.Add(ConvertAdditionalRootBone(AdditionalRootBone));
	}

	Preset.PhysicsSettings = ConvertPhysicsSettings(Node.PhysicsSettings);
	Preset.DummyBoneLength = Node.DummyBoneLength;
	Preset.BoneForwardAxis = Node.BoneForwardAxis;
	Preset.TargetFramerate = Node.TargetFramerate;
	Preset.bOverrideTargetFramerate = Node.bOverrideTargetFramerate;
	Preset.WarmUpFrames = Node.WarmUpFrames;
	Preset.bUseWarmUpWhenResetDynamics = Node.bUseWarmUpWhenResetDynamics;
	Preset.bNeedWarmUp = Node.bNeedWarmUp;
	Preset.TeleportDistanceThreshold = Node.TeleportDistanceThreshold;
	Preset.TeleportRotationThreshold = Node.TeleportRotationThreshold;
	Preset.PlanarConstraint = Node.PlanarConstraint;
	Preset.bResetBoneTransformWhenBoneNotFound = Node.bResetBoneTransformWhenBoneNotFound;

	AddCurveIfPresent(Preset.Curves, TEXT("Damping"), Node.DampingCurve);
	AddCurveIfPresent(Preset.Curves, TEXT("Stiffness"), Node.StiffnessCurve);
	AddCurveIfPresent(Preset.Curves, TEXT("WorldDampingLocation"), Node.WorldDampingLocationCurve);
	AddCurveIfPresent(Preset.Curves, TEXT("WorldDampingRotation"), Node.WorldDampingRotationCurve);
	AddCurveIfPresent(Preset.Curves, TEXT("Radius"), Node.RadiusCurve);
	AddCurveIfPresent(Preset.Curves, TEXT("LimitAngle"), Node.LimitAngleCurve);

	Preset.LimitsDataAssetPath = Node.LimitsDataAssetPath;
	Preset.PhysicsAssetForLimitsPath = Node.PhysicsAssetForLimitsPath;
	Preset.BoneConstraintsDataAssetPath = Node.BoneConstraintsDataAssetPath;
	Preset.SphericalLimitsDataCount = Node.SphericalLimitsDataCount;
	Preset.CapsuleLimitsDataCount = Node.CapsuleLimitsDataCount;
	Preset.BoxLimitsDataCount = Node.BoxLimitsDataCount;
	Preset.PlanarLimitsDataCount = Node.PlanarLimitsDataCount;
	AppendLimits(Node.SphericalLimits, Preset.CollisionLimits);
	AppendLimits(Node.CapsuleLimits, Preset.CollisionLimits);
	AppendLimits(Node.BoxLimits, Preset.CollisionLimits);
	AppendLimits(Node.PlanarLimits, Preset.CollisionLimits);

	Preset.BoneConstraintGlobalComplianceType = Node.BoneConstraintGlobalComplianceType;
	Preset.BoneConstraintIterationCountBeforeCollision = Node.BoneConstraintIterationCountBeforeCollision;
	Preset.BoneConstraintIterationCountAfterCollision = Node.BoneConstraintIterationCountAfterCollision;
	Preset.bAutoAddChildDummyBoneConstraint = Node.bAutoAddChildDummyBoneConstraint;
	Preset.BoneConstraintCount = Node.BoneConstraintCount;
	Preset.BoneConstraintsDataCount = Node.BoneConstraintsDataCount;

	Preset.Gravity = Node.Gravity;
	Preset.bEnableWind = Node.bEnableWind;
	Preset.WindScale = Node.WindScale;
	Preset.bHasUseRelativeMove = Node.bHasUseRelativeMove;
	Preset.bUseRelativeMove = Node.bUseRelativeMove;
	Preset.MovementReferenceDisplacement = Node.MovementReferenceDisplacement;
	Preset.bAllowWorldCollision = Node.bAllowWorldCollision;
	Preset.bOverrideCollisionParams = Node.bOverrideCollisionParams;
	Preset.bIgnoreSelfComponent = Node.bIgnoreSelfComponent;
	Preset.IgnoreBones = NameArrayToStringArray(Node.IgnoreBones);
	Preset.IgnoreBoneNamePrefix = NameArrayToStringArray(Node.IgnoreBoneNamePrefix);
	Preset.KawaiiPhysicsTag = Node.KawaiiPhysicsTag;
	Preset.UnsupportedSourceFields = Node.UnsupportedFields;
	return Preset;
}

TSharedRef<FJsonObject> PresetPreviewToJson(const FNteCharacterKawaiiPresetSpec& Preset)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("Id"), Preset.Id);
	Object->SetStringField(TEXT("Label"), Preset.Label);
	Object->SetStringField(TEXT("TargetMeshId"), Preset.TargetMeshId);
	Object->SetStringField(TEXT("SourceKind"), Preset.SourceKind);
	Object->SetStringField(TEXT("SourceNodeName"), Preset.SourceNodeName);
	Object->SetStringField(TEXT("RootBone"), Preset.RootBone);
	Object->SetNumberField(TEXT("AdditionalRootBoneCount"), Preset.AdditionalRootBones.Num());
	Object->SetNumberField(TEXT("CollisionLimitCount"), Preset.CollisionLimits.Num());
	Object->SetNumberField(TEXT("CurveCount"), Preset.Curves.Num());
	Object->SetNumberField(TEXT("UnsupportedSourceFieldCount"), Preset.UnsupportedSourceFields.Num());
	Object->SetStringField(TEXT("RuntimeAnimBlueprintPath"), Preset.RuntimeAnimBlueprintPath);
	Object->SetStringField(TEXT("LimitsDataAssetPath"), Preset.LimitsDataAssetPath);
	Object->SetStringField(TEXT("OutputLimitsDataAssetPath"), Preset.OutputLimitsDataAssetPath);
	Object->SetStringField(TEXT("BoneConstraintsDataAssetPath"), Preset.BoneConstraintsDataAssetPath);
	Object->SetStringField(TEXT("OutputBoneConstraintsDataAssetPath"), Preset.OutputBoneConstraintsDataAssetPath);
	Object->SetStringField(TEXT("SchemaStatus"), Preset.SchemaStatus);

	const TSharedRef<FJsonObject> ImportedData = MakeShared<FJsonObject>();
	ImportedData->SetNumberField(TEXT("SphericalLimitsDataCount"), Preset.SphericalLimitsDataCount);
	ImportedData->SetNumberField(TEXT("CapsuleLimitsDataCount"), Preset.CapsuleLimitsDataCount);
	ImportedData->SetNumberField(TEXT("BoxLimitsDataCount"), Preset.BoxLimitsDataCount);
	ImportedData->SetNumberField(TEXT("PlanarLimitsDataCount"), Preset.PlanarLimitsDataCount);
	ImportedData->SetNumberField(TEXT("BoneConstraintCount"), Preset.BoneConstraintCount);
	ImportedData->SetNumberField(TEXT("BoneConstraintsDataCount"), Preset.BoneConstraintsDataCount);
	Object->SetObjectField(TEXT("ImportedData"), ImportedData);
	return Object;
}
}

FNteCharacterKawaiiImportResult ImportKawaiiPresetsFromFModelJson(const FNteCharacterKawaiiImportOptions& Options)
{
	FNteCharacterKawaiiImportResult Result;
	Result.SourceJsonPath = Options.SourceJsonPath;
	Result.TargetMeshId = Options.TargetMeshId.IsEmpty() ? TEXT("main") : Options.TargetMeshId;

	if (Options.SourceJsonPath.IsEmpty())
	{
		Result.Errors.Add(TEXT("ImportKawaiiJson path is empty."));
		return Result;
	}

	FNteCharacterKawaiiImportOptions NormalizedOptions = Options;
	NormalizedOptions.TargetMeshId = Result.TargetMeshId;
	FPaths::NormalizeFilename(NormalizedOptions.SourceJsonPath);
	Result.SourceJsonPath = NormalizedOptions.SourceJsonPath;

	TArray<TSharedPtr<FJsonValue>> RootArray;
	FString Error;
	if (!FModelJson::LoadJsonArrayFromFile(NormalizedOptions.SourceJsonPath, RootArray, Error))
	{
		Result.Errors.Add(Error);
		return Result;
	}

	FFModelKawaiiAnimLayerAnalysis Analysis;
	if (!AnalyzeFModelKawaiiAnimLayerJson(RootArray, Analysis, Error))
	{
		Result.Errors.Add(Error);
		return Result;
	}

	Result.SourceGeneratedClassName = Analysis.GeneratedClassName;
	Result.SourceClassDefaultObjectName = Analysis.ClassDefaultObjectName;

	TSet<FString> SeenIds;
	for (const FFModelKawaiiNodeAnalysis& Node : Analysis.KawaiiNodes)
	{
		if (!NormalizedOptions.SourceNodeNames.IsEmpty() && !NormalizedOptions.SourceNodeNames.Contains(Node.GraphNodeName))
		{
			continue;
		}

		FString PresetId = MakePresetId(NormalizedOptions, Node.GraphNodeName);
		if (SeenIds.Contains(PresetId))
		{
			int32 Suffix = 2;
			FString CandidateId;
			do
			{
				CandidateId = FString::Printf(TEXT("%s_%d"), *PresetId, Suffix++);
			}
			while (SeenIds.Contains(CandidateId));
			PresetId = CandidateId;
		}
		SeenIds.Add(PresetId);
		Result.ImportedPresets.Add(ConvertNode(Analysis, Node, NormalizedOptions, PresetId));
	}

	if (Result.ImportedPresets.IsEmpty())
	{
		Result.Errors.Add(NormalizedOptions.SourceNodeNames.IsEmpty()
			? TEXT("No Kawaii presets were imported from the source JSON.")
			: TEXT("The selected Kawaii node was not found in the source JSON."));
	}
	return Result;
}

void UpsertKawaiiPresets(
	FNteCharacterModSpec& Spec,
	const TArray<FNteCharacterKawaiiPresetSpec>& Presets,
	const bool bReplaceExistingById,
	FNteCharacterKawaiiImportResult& InOutResult)
{
	for (const FNteCharacterKawaiiPresetSpec& Preset : Presets)
	{
		if (Preset.Id.IsEmpty())
		{
			InOutResult.Errors.Add(TEXT("Imported Kawaii preset has no Id; cannot upsert."));
			continue;
		}

		int32 ExistingIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Spec.KawaiiPresets.Num(); ++Index)
		{
			if (Spec.KawaiiPresets[Index].Id == Preset.Id)
			{
				ExistingIndex = Index;
				break;
			}
		}

		if (ExistingIndex != INDEX_NONE)
		{
			if (!bReplaceExistingById)
			{
				InOutResult.Warnings.Add(FString::Printf(TEXT("Kawaii preset '%s' already exists and replace-existing is disabled; imported preset was skipped."), *Preset.Id));
				continue;
			}
			Spec.KawaiiPresets[ExistingIndex] = Preset;
			++InOutResult.ReplacedPresetCount;
		}
		else
		{
			Spec.KawaiiPresets.Add(Preset);
			++InOutResult.AddedPresetCount;
		}
	}
}

TSharedRef<FJsonObject> CharacterKawaiiImportResultToJson(const FNteCharacterKawaiiImportResult& Result)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("SourceJsonPath"), Result.SourceJsonPath);
	Object->SetStringField(TEXT("TargetMeshId"), Result.TargetMeshId);
	Object->SetStringField(TEXT("SourceGeneratedClassName"), Result.SourceGeneratedClassName);
	Object->SetStringField(TEXT("SourceClassDefaultObjectName"), Result.SourceClassDefaultObjectName);
	Object->SetNumberField(TEXT("ImportedPresetCount"), Result.ImportedPresets.Num());
	Object->SetNumberField(TEXT("AddedPresetCount"), Result.AddedPresetCount);
	Object->SetNumberField(TEXT("ReplacedPresetCount"), Result.ReplacedPresetCount);
	Object->SetArrayField(TEXT("Errors"), StringArrayToJsonValues(Result.Errors));
	Object->SetArrayField(TEXT("Warnings"), StringArrayToJsonValues(Result.Warnings));

	TArray<TSharedPtr<FJsonValue>> Presets;
	for (const FNteCharacterKawaiiPresetSpec& Preset : Result.ImportedPresets)
	{
		Presets.Add(MakeShared<FJsonValueObject>(PresetPreviewToJson(Preset)));
	}
	Object->SetArrayField(TEXT("Presets"), Presets);
	return Object;
}

TSharedRef<FJsonObject> CharacterKawaiiPresetPlanToJson(const FNteCharacterModSpec& Spec)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("PresetCount"), Spec.KawaiiPresets.Num());

	TArray<TSharedPtr<FJsonValue>> Presets;
	for (const FNteCharacterKawaiiPresetSpec& Preset : Spec.KawaiiPresets)
	{
		Presets.Add(MakeShared<FJsonValueObject>(PresetPreviewToJson(Preset)));
	}
	Object->SetArrayField(TEXT("Presets"), Presets);
	return Object;
}
}
