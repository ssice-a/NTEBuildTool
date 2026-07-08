// Copyright (c) 2026 NTEBuildTool contributors.

#include "FModelKawaiiAnimLayerAnalysis.h"

#include "FModelJsonUtils.h"

namespace NTEBuildTool
{
namespace
{
using namespace FModelJson;

const TSet<FString>& GetSupportedNodeFields()
{
	static const TSet<FString> SupportedFields = {
		TEXT("RootBone"),
		TEXT("ExcludeBones"),
		TEXT("AdditionalRootBones"),
		TEXT("DummyBoneLength"),
		TEXT("BoneForwardAxis"),
		TEXT("PhysicsSettings"),
		TEXT("TargetFrameRate"),
		TEXT("TargetFramerate"),
		TEXT("OverrideTargetFramerate"),
		TEXT("WarmUpFrames"),
		TEXT("bUseWarmUpWhenResetDynamics"),
		TEXT("bNeedWarmUp"),
		TEXT("TeleportDistanceThreshold"),
		TEXT("TeleportRotationThreshold"),
		TEXT("PlanarConstraint"),
		TEXT("ResetBoneTransformWhenBoneNotFound"),
		TEXT("DampingCurveData"),
		TEXT("StiffnessCurveData"),
		TEXT("WorldDampingLocationCurveData"),
		TEXT("WorldDampingRotationCurveData"),
		TEXT("RadiusCurveData"),
		TEXT("LimitAngleCurveData"),
		TEXT("SphericalLimits"),
		TEXT("CapsuleLimits"),
		TEXT("BoxLimits"),
		TEXT("PlanarLimits"),
		TEXT("LimitsDataAsset"),
		TEXT("PhysicsAssetForLimits"),
		TEXT("SphericalLimitsData"),
		TEXT("CapsuleLimitsData"),
		TEXT("BoxLimitsData"),
		TEXT("PlanarLimitsData"),
		TEXT("BoneConstraintGlobalComplianceType"),
		TEXT("BoneConstraintIterationCountBeforeCollision"),
		TEXT("BoneConstraintIterationCountAfterCollision"),
		TEXT("bAutoAddChildDummyBoneConstraint"),
		TEXT("BoneConstraints"),
		TEXT("BoneConstraintsDataAsset"),
		TEXT("BoneConstraintsData"),
		TEXT("MergedBoneConstraints"),
		TEXT("Gravity"),
		TEXT("bEnableWind"),
		TEXT("WindScale"),
		TEXT("ExternalForces"),
		TEXT("CustomExternalForces"),
		TEXT("bUseRelativeMove"),
		TEXT("MovementReferenceDisplacement"),
		TEXT("bAllowWorldCollision"),
		TEXT("bOverrideCollisionParams"),
		TEXT("CollisionChannelSettings"),
		TEXT("bIgnoreSelfComponent"),
		TEXT("IgnoreBones"),
		TEXT("IgnoreBoneNamePrefix"),
		TEXT("KawaiiPhysicsTag"),
		TEXT("ModifyBones"),
		TEXT("DeltaTime"),
		TEXT("PreSkelCompTransform"),
		TEXT("bPhysicsSettingsInitialized"),
		TEXT("ComponentPose"),
		TEXT("LODThreshold"),
		TEXT("ActualAlpha"),
		TEXT("AlphaInputType"),
		TEXT("bAlphaBoolEnabled"),
		TEXT("Alpha"),
		TEXT("AlphaScaleBias"),
		TEXT("AlphaBoolBlend"),
		TEXT("AlphaCurveName"),
		TEXT("AlphaScaleBiasClamp")
	};

	return SupportedFields;
}

FName GetBoneName(const FJsonObject& Object, const TCHAR* FieldName)
{
	TSharedPtr<FJsonObject> BoneObject;
	return TryGetObject(Object, FieldName, BoneObject) ? GetOptionalName(*BoneObject, TEXT("BoneName")) : NAME_None;
}

TArray<FName> GetBoneNameArray(const FJsonObject& Object, const TCHAR* FieldName)
{
	TArray<FName> BoneNames;

	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!TryGetArray(Object, FieldName, Values))
	{
		return BoneNames;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			continue;
		}

		const TSharedPtr<FJsonObject> BoneObject = Value->AsObject();
		const FName BoneName = BoneObject.IsValid() ? GetOptionalName(*BoneObject, TEXT("BoneName")) : NAME_None;
		if (BoneName != NAME_None)
		{
			BoneNames.Add(BoneName);
		}
	}

	return BoneNames;
}

FVector GetVectorField(const FJsonObject& Object, const TCHAR* FieldName)
{
	TSharedPtr<FJsonObject> VectorObject;
	return TryGetObject(Object, FieldName, VectorObject) ? GetVector(*VectorObject) : FVector::ZeroVector;
}

FRotator GetRotatorField(const FJsonObject& Object, const TCHAR* FieldName)
{
	TSharedPtr<FJsonObject> RotatorObject;
	return TryGetObject(Object, FieldName, RotatorObject) ? GetRotator(*RotatorObject) : FRotator::ZeroRotator;
}

FString GetReferencedObjectPath(const FJsonObject& Object, const TCHAR* FieldName)
{
	const TSharedPtr<FJsonValue>* Value = Object.Values.Find(FString(FieldName));
	if (!Value || !Value->IsValid() || (*Value)->Type != EJson::Object)
	{
		return FString();
	}

	const TSharedPtr<FJsonObject> RefObject = (*Value)->AsObject();
	return RefObject.IsValid() ? GetString(*RefObject, TEXT("ObjectPath")) : FString();
}

TArray<FName> GetNameArray(const FJsonObject& Object, const TCHAR* FieldName)
{
	TArray<FName> Names;

	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!TryGetArray(Object, FieldName, Values))
	{
		return Names;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (!Value.IsValid())
		{
			continue;
		}

		if (Value->Type == EJson::String)
		{
			const FString Name = Value->AsString();
			if (!Name.IsEmpty() && Name != TEXT("None"))
			{
				Names.Add(FName(*Name));
			}
		}
		else if (Value->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject> ObjectValue = Value->AsObject();
			const FName BoneName = ObjectValue.IsValid() ? GetOptionalName(*ObjectValue, TEXT("BoneName")) : NAME_None;
			if (BoneName != NAME_None)
			{
				Names.Add(BoneName);
			}
		}
	}

	return Names;
}

int32 CountObjectArray(const FJsonObject& Object, const TCHAR* FieldName)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	return TryGetArray(Object, FieldName, Values) ? Values->Num() : 0;
}

FFModelKawaiiCurveReference AnalyzeCurve(const FJsonObject& Object, const TCHAR* FieldName)
{
	FFModelKawaiiCurveReference Result;

	TSharedPtr<FJsonObject> CurveObject;
	if (!TryGetObject(Object, FieldName, CurveObject))
	{
		return Result;
	}

	TSharedPtr<FJsonObject> EditorCurveData;
	if (TryGetObject(*CurveObject, TEXT("EditorCurveData"), EditorCurveData))
	{
		const TArray<TSharedPtr<FJsonValue>>* Keys = nullptr;
		Result.InlineKeyCount = TryGetArray(*EditorCurveData, TEXT("Keys"), Keys) ? Keys->Num() : 0;
	}

	TSharedPtr<FJsonObject> ExternalCurve;
	if (TryGetObject(*CurveObject, TEXT("ExternalCurve"), ExternalCurve))
	{
		Result.ExternalCurveObjectName = GetString(*ExternalCurve, TEXT("ObjectName"));
		Result.ExternalCurveObjectPath = GetString(*ExternalCurve, TEXT("ObjectPath"));
	}

	return Result;
}

FFModelKawaiiPhysicsSettings AnalyzePhysicsSettings(const FJsonObject& NodeObject)
{
	FFModelKawaiiPhysicsSettings Result;

	TSharedPtr<FJsonObject> Settings;
	if (!TryGetObject(NodeObject, TEXT("PhysicsSettings"), Settings))
	{
		return Result;
	}

	Result.Damping = GetFloat(*Settings, TEXT("Damping"));
	Result.Stiffness = GetFloat(*Settings, TEXT("Stiffness"));
	Result.WorldDampingLocation = GetFloat(*Settings, TEXT("WorldDampingLocation"));
	Result.WorldDampingRotation = GetFloat(*Settings, TEXT("WorldDampingRotation"));
	Result.Radius = GetFloat(*Settings, TEXT("Radius"));
	Result.LimitAngle = GetFloat(*Settings, TEXT("LimitAngle"));
	Result.bHasForwardMoveOffset = Settings->HasField(TEXT("ForwardMoveOffset"));
	Result.ForwardMoveOffset = GetFloat(*Settings, TEXT("ForwardMoveOffset"));
	return Result;
}

void AnalyzeAdditionalRootBones(const FJsonObject& NodeObject, TArray<FFModelKawaiiRootBoneSetting>& OutRootBones)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!TryGetArray(NodeObject, TEXT("AdditionalRootBones"), Values))
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			continue;
		}

		const TSharedPtr<FJsonObject> RootObject = Value->AsObject();
		if (!RootObject.IsValid())
		{
			continue;
		}

		FFModelKawaiiRootBoneSetting RootBone;
		RootBone.RootBone = GetBoneName(*RootObject, TEXT("RootBone"));
		RootBone.OverrideExcludeBones = GetBoneNameArray(*RootObject, TEXT("OverrideExcludeBones"));
		RootBone.bUseOverrideExcludeBones = GetBool(*RootObject, TEXT("bUseOverrideExcludeBones"));
		OutRootBones.Add(RootBone);
	}
}

FVector GetLimitOffsetLocation(const FJsonObject& LimitObject)
{
	TSharedPtr<FJsonObject> OffsetObject;
	if (TryGetObject(LimitObject, TEXT("OffSetLocation"), OffsetObject) || TryGetObject(LimitObject, TEXT("OffsetLocation"), OffsetObject))
	{
		return GetVector(*OffsetObject);
	}

	return FVector::ZeroVector;
}

FFModelKawaiiCollisionLimit AnalyzeLimit(const FJsonObject& LimitObject, const FString& LimitKind)
{
	FFModelKawaiiCollisionLimit Result;
	Result.LimitKind = LimitKind;
	Result.DrivingBone = GetBoneName(LimitObject, TEXT("DrivingBone"));
	Result.OffsetLocation = GetLimitOffsetLocation(LimitObject);
	Result.OffsetRotation = GetRotatorField(LimitObject, TEXT("OffsetRotation"));
	Result.Radius = GetFloat(LimitObject, TEXT("Radius"));
	Result.Length = GetFloat(LimitObject, TEXT("Length"));
	Result.SphereRadius = GetFloat(LimitObject, TEXT("SphereRadius"));
	Result.LimitType = GetString(LimitObject, TEXT("LimitType"));
	Result.SourceType = GetString(LimitObject, TEXT("SourceType"));
	Result.bEnable = GetBool(LimitObject, TEXT("bEnable"), true);
	return Result;
}

void AnalyzeLimits(const FJsonObject& NodeObject, const TCHAR* FieldName, const FString& LimitKind, TArray<FFModelKawaiiCollisionLimit>& OutLimits)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!TryGetArray(NodeObject, FieldName, Values))
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			continue;
		}

		const TSharedPtr<FJsonObject> LimitObject = Value->AsObject();
		if (LimitObject.IsValid())
		{
			OutLimits.Add(AnalyzeLimit(*LimitObject, LimitKind));
		}
	}
}

int32 KawaiiNodeSortKey(const FString& NodeName)
{
	if (NodeName == TEXT("AnimGraphNode_KawaiiPhysics"))
	{
		return -1;
	}

	FString Suffix;
	if (NodeName.Split(TEXT("_"), nullptr, &Suffix, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
	{
		int32 Parsed = 0;
		if (LexTryParseString(Parsed, *Suffix))
		{
			return Parsed;
		}
	}

	return -1;
}

bool IsKawaiiNodeField(const FString& FieldName)
{
	return FieldName == TEXT("AnimGraphNode_KawaiiPhysics") || FieldName.StartsWith(TEXT("AnimGraphNode_KawaiiPhysics_"));
}

void TrackUnsupportedFields(const FJsonObject& NodeObject, TArray<FString>& OutUnsupportedFields)
{
	const TSet<FString>& SupportedFields = GetSupportedNodeFields();
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : NodeObject.Values)
	{
		if (!SupportedFields.Contains(Field.Key))
		{
			OutUnsupportedFields.Add(Field.Key);
		}
	}
}

FFModelKawaiiNodeAnalysis AnalyzeNode(const FString& GraphNodeName, const FJsonObject& NodeObject)
{
	FFModelKawaiiNodeAnalysis Result;
	Result.GraphNodeName = GraphNodeName;
	Result.RootBone = GetBoneName(NodeObject, TEXT("RootBone"));
	Result.ExcludeBones = GetBoneNameArray(NodeObject, TEXT("ExcludeBones"));
	AnalyzeAdditionalRootBones(NodeObject, Result.AdditionalRootBones);

	Result.DummyBoneLength = GetFloat(NodeObject, TEXT("DummyBoneLength"));
	Result.BoneForwardAxis = GetString(NodeObject, TEXT("BoneForwardAxis"));
	Result.PhysicsSettings = AnalyzePhysicsSettings(NodeObject);
	Result.TargetFramerate = static_cast<int32>(GetFloat(NodeObject, TEXT("TargetFramerate"), GetFloat(NodeObject, TEXT("TargetFrameRate"), 60.0f)));
	Result.bOverrideTargetFramerate = GetBool(NodeObject, TEXT("OverrideTargetFramerate"));
	Result.WarmUpFrames = static_cast<int32>(GetFloat(NodeObject, TEXT("WarmUpFrames")));
	Result.bUseWarmUpWhenResetDynamics = GetBool(NodeObject, TEXT("bUseWarmUpWhenResetDynamics"), true);
	Result.bNeedWarmUp = GetBool(NodeObject, TEXT("bNeedWarmUp"));
	Result.TeleportDistanceThreshold = GetFloat(NodeObject, TEXT("TeleportDistanceThreshold"));
	Result.TeleportRotationThreshold = GetFloat(NodeObject, TEXT("TeleportRotationThreshold"));
	Result.PlanarConstraint = GetString(NodeObject, TEXT("PlanarConstraint"));
	Result.bResetBoneTransformWhenBoneNotFound = GetBool(NodeObject, TEXT("ResetBoneTransformWhenBoneNotFound"));

	Result.DampingCurve = AnalyzeCurve(NodeObject, TEXT("DampingCurveData"));
	Result.StiffnessCurve = AnalyzeCurve(NodeObject, TEXT("StiffnessCurveData"));
	Result.WorldDampingLocationCurve = AnalyzeCurve(NodeObject, TEXT("WorldDampingLocationCurveData"));
	Result.WorldDampingRotationCurve = AnalyzeCurve(NodeObject, TEXT("WorldDampingRotationCurveData"));
	Result.RadiusCurve = AnalyzeCurve(NodeObject, TEXT("RadiusCurveData"));
	Result.LimitAngleCurve = AnalyzeCurve(NodeObject, TEXT("LimitAngleCurveData"));

	AnalyzeLimits(NodeObject, TEXT("SphericalLimits"), TEXT("Spherical"), Result.SphericalLimits);
	AnalyzeLimits(NodeObject, TEXT("CapsuleLimits"), TEXT("Capsule"), Result.CapsuleLimits);
	AnalyzeLimits(NodeObject, TEXT("BoxLimits"), TEXT("Box"), Result.BoxLimits);
	AnalyzeLimits(NodeObject, TEXT("PlanarLimits"), TEXT("Planar"), Result.PlanarLimits);

	Result.LimitsDataAssetPath = GetReferencedObjectPath(NodeObject, TEXT("LimitsDataAsset"));
	Result.PhysicsAssetForLimitsPath = GetReferencedObjectPath(NodeObject, TEXT("PhysicsAssetForLimits"));
	Result.SphericalLimitsDataCount = CountObjectArray(NodeObject, TEXT("SphericalLimitsData"));
	Result.CapsuleLimitsDataCount = CountObjectArray(NodeObject, TEXT("CapsuleLimitsData"));
	Result.BoxLimitsDataCount = CountObjectArray(NodeObject, TEXT("BoxLimitsData"));
	Result.PlanarLimitsDataCount = CountObjectArray(NodeObject, TEXT("PlanarLimitsData"));

	Result.BoneConstraintGlobalComplianceType = GetString(NodeObject, TEXT("BoneConstraintGlobalComplianceType"));
	Result.BoneConstraintIterationCountBeforeCollision = static_cast<int32>(GetFloat(NodeObject, TEXT("BoneConstraintIterationCountBeforeCollision")));
	Result.BoneConstraintIterationCountAfterCollision = static_cast<int32>(GetFloat(NodeObject, TEXT("BoneConstraintIterationCountAfterCollision")));
	Result.bAutoAddChildDummyBoneConstraint = GetBool(NodeObject, TEXT("bAutoAddChildDummyBoneConstraint"), true);
	Result.BoneConstraintCount = CountObjectArray(NodeObject, TEXT("BoneConstraints"));
	Result.BoneConstraintsDataAssetPath = GetReferencedObjectPath(NodeObject, TEXT("BoneConstraintsDataAsset"));
	Result.BoneConstraintsDataCount = CountObjectArray(NodeObject, TEXT("BoneConstraintsData"));

	Result.Gravity = GetVectorField(NodeObject, TEXT("Gravity"));
	Result.bEnableWind = GetBool(NodeObject, TEXT("bEnableWind"));
	Result.WindScale = GetFloat(NodeObject, TEXT("WindScale"), 1.0f);
	Result.bHasUseRelativeMove = NodeObject.HasField(TEXT("bUseRelativeMove"));
	Result.bUseRelativeMove = GetBool(NodeObject, TEXT("bUseRelativeMove"));
	Result.MovementReferenceDisplacement = GetVectorField(NodeObject, TEXT("MovementReferenceDisplacement"));
	Result.bAllowWorldCollision = GetBool(NodeObject, TEXT("bAllowWorldCollision"));
	Result.bOverrideCollisionParams = GetBool(NodeObject, TEXT("bOverrideCollisionParams"));
	Result.bIgnoreSelfComponent = GetBool(NodeObject, TEXT("bIgnoreSelfComponent"), true);
	Result.IgnoreBones = GetBoneNameArray(NodeObject, TEXT("IgnoreBones"));
	Result.IgnoreBoneNamePrefix = GetNameArray(NodeObject, TEXT("IgnoreBoneNamePrefix"));

	TSharedPtr<FJsonObject> TagObject;
	if (TryGetObject(NodeObject, TEXT("KawaiiPhysicsTag"), TagObject))
	{
		Result.KawaiiPhysicsTag = GetString(*TagObject, TEXT("TagName"));
	}

	TrackUnsupportedFields(NodeObject, Result.UnsupportedFields);
	return Result;
}
}

bool AnalyzeFModelKawaiiAnimLayerJson(const TArray<TSharedPtr<FJsonValue>>& RootArray, FFModelKawaiiAnimLayerAnalysis& OutAnalysis, FString& OutError)
{
	OutAnalysis = FFModelKawaiiAnimLayerAnalysis();

	TSharedPtr<FJsonObject> ClassDefaultObject;
	for (const TSharedPtr<FJsonValue>& Value : RootArray)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			continue;
		}

		const TSharedPtr<FJsonObject> Object = Value->AsObject();
		if (!Object.IsValid())
		{
			continue;
		}

		const FString Type = GetString(*Object, TEXT("Type"));
		const FString Name = GetString(*Object, TEXT("Name"));
		if (Type == TEXT("AnimBlueprintGeneratedClass"))
		{
			OutAnalysis.GeneratedClassName = Name;
		}
		else if (Name.StartsWith(TEXT("Default__")))
		{
			OutAnalysis.ClassDefaultObjectName = Name;
			ClassDefaultObject = Object;
		}
	}

	if (!ClassDefaultObject.IsValid())
	{
		OutError = TEXT("No class default object was found in this AnimLayer JSON.");
		return false;
	}

	TSharedPtr<FJsonObject> Properties;
	if (!TryGetObject(*ClassDefaultObject, TEXT("Properties"), Properties))
	{
		OutError = TEXT("Class default object has no Properties object.");
		return false;
	}

	TArray<TPair<FString, TSharedPtr<FJsonObject>>> NodeObjects;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Properties->Values)
	{
		if (!IsKawaiiNodeField(Field.Key) || !Field.Value.IsValid() || Field.Value->Type != EJson::Object)
		{
			continue;
		}

		NodeObjects.Add(TPair<FString, TSharedPtr<FJsonObject>>(Field.Key, Field.Value->AsObject()));
	}

	NodeObjects.Sort([](const TPair<FString, TSharedPtr<FJsonObject>>& Left, const TPair<FString, TSharedPtr<FJsonObject>>& Right)
	{
		return KawaiiNodeSortKey(Left.Key) > KawaiiNodeSortKey(Right.Key);
	});

	for (const TPair<FString, TSharedPtr<FJsonObject>>& NodeObject : NodeObjects)
	{
		if (NodeObject.Value.IsValid())
		{
			OutAnalysis.KawaiiNodes.Add(AnalyzeNode(NodeObject.Key, *NodeObject.Value));
		}
	}

	if (OutAnalysis.KawaiiNodes.IsEmpty())
	{
		OutError = TEXT("No AnimGraphNode_KawaiiPhysics properties were found in this AnimLayer JSON.");
		return false;
	}

	return true;
}
}
