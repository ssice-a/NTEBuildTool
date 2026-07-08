// Copyright (c) 2026 NTEBuildTool contributors.

#include "FModelPhysicsAssetImporter.h"

#include "FModelJsonUtils.h"
#include "FModelPhysicsAssetAnalysis.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/RigidBodyIndexPair.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/SphylElem.h"

namespace NTEBuildTool
{
namespace
{
using namespace FModelJson;

EPhysicsType ParsePhysicsType(const FString& Value)
{
	if (Value.Contains(TEXT("PhysType_Kinematic")))
	{
		return EPhysicsType::PhysType_Kinematic;
	}
	if (Value.Contains(TEXT("PhysType_Simulated")))
	{
		return EPhysicsType::PhysType_Simulated;
	}

	return EPhysicsType::PhysType_Default;
}

ECollisionTraceFlag ParseCollisionTraceFlag(const FString& Value)
{
	if (Value.Contains(TEXT("CTF_UseSimpleAndComplex")))
	{
		return ECollisionTraceFlag::CTF_UseSimpleAndComplex;
	}
	if (Value.Contains(TEXT("CTF_UseSimpleAsComplex")))
	{
		return ECollisionTraceFlag::CTF_UseSimpleAsComplex;
	}
	if (Value.Contains(TEXT("CTF_UseComplexAsSimple")))
	{
		return ECollisionTraceFlag::CTF_UseComplexAsSimple;
	}

	return ECollisionTraceFlag::CTF_UseDefault;
}

ECollisionEnabled::Type ParseCollisionEnabled(const FString& Value)
{
	if (Value.Contains(TEXT("NoCollision")))
	{
		return ECollisionEnabled::NoCollision;
	}
	if (Value.Contains(TEXT("QueryOnly")))
	{
		return ECollisionEnabled::QueryOnly;
	}
	if (Value.Contains(TEXT("PhysicsOnly")))
	{
		return ECollisionEnabled::PhysicsOnly;
	}

	return ECollisionEnabled::QueryAndPhysics;
}

EAngularConstraintMotion ParseAngularMotion(const FString& Value)
{
	if (Value.Contains(TEXT("ACM_Free")))
	{
		return EAngularConstraintMotion::ACM_Free;
	}
	if (Value.Contains(TEXT("ACM_Locked")))
	{
		return EAngularConstraintMotion::ACM_Locked;
	}

	return EAngularConstraintMotion::ACM_Limited;
}

ELinearConstraintMotion ParseLinearMotion(const FString& Value)
{
	if (Value.Contains(TEXT("LCM_Free")))
	{
		return ELinearConstraintMotion::LCM_Free;
	}
	if (Value.Contains(TEXT("LCM_Limited")))
	{
		return ELinearConstraintMotion::LCM_Limited;
	}

	return ELinearConstraintMotion::LCM_Locked;
}

void ApplyConstraintBaseParams(const FJsonObject& Json, FConstraintBaseParams& Params)
{
	if (Json.HasField(TEXT("Stiffness")))
	{
		Params.Stiffness = GetFloat(Json, TEXT("Stiffness"));
	}
	if (Json.HasField(TEXT("Damping")))
	{
		Params.Damping = GetFloat(Json, TEXT("Damping"));
	}
	if (Json.HasField(TEXT("Restitution")))
	{
		Params.Restitution = GetFloat(Json, TEXT("Restitution"));
	}
	if (Json.HasField(TEXT("ContactDistance")))
	{
		Params.ContactDistance = GetFloat(Json, TEXT("ContactDistance"));
	}
	if (Json.HasField(TEXT("bSoftConstraint")))
	{
		Params.bSoftConstraint = GetBool(Json, TEXT("bSoftConstraint"));
	}
}

void ApplyLinearConstraint(const FJsonObject& Json, FLinearConstraint& LinearLimit)
{
	ApplyConstraintBaseParams(Json, LinearLimit);
	if (Json.HasField(TEXT("Limit")))
	{
		LinearLimit.Limit = GetFloat(Json, TEXT("Limit"));
	}
	if (Json.HasField(TEXT("XMotion")))
	{
		LinearLimit.XMotion = ParseLinearMotion(GetString(Json, TEXT("XMotion")));
	}
	if (Json.HasField(TEXT("YMotion")))
	{
		LinearLimit.YMotion = ParseLinearMotion(GetString(Json, TEXT("YMotion")));
	}
	if (Json.HasField(TEXT("ZMotion")))
	{
		LinearLimit.ZMotion = ParseLinearMotion(GetString(Json, TEXT("ZMotion")));
	}
}

void ApplyConeConstraint(const FJsonObject& Json, FConeConstraint& ConeLimit)
{
	ApplyConstraintBaseParams(Json, ConeLimit);
	if (Json.HasField(TEXT("Swing1LimitDegrees")))
	{
		ConeLimit.Swing1LimitDegrees = GetFloat(Json, TEXT("Swing1LimitDegrees"));
	}
	if (Json.HasField(TEXT("Swing2LimitDegrees")))
	{
		ConeLimit.Swing2LimitDegrees = GetFloat(Json, TEXT("Swing2LimitDegrees"));
	}
	if (Json.HasField(TEXT("Swing1Motion")))
	{
		ConeLimit.Swing1Motion = ParseAngularMotion(GetString(Json, TEXT("Swing1Motion")));
	}
	if (Json.HasField(TEXT("Swing2Motion")))
	{
		ConeLimit.Swing2Motion = ParseAngularMotion(GetString(Json, TEXT("Swing2Motion")));
	}
}

void ApplyTwistConstraint(const FJsonObject& Json, FTwistConstraint& TwistLimit)
{
	ApplyConstraintBaseParams(Json, TwistLimit);
	if (Json.HasField(TEXT("TwistLimitDegrees")))
	{
		TwistLimit.TwistLimitDegrees = GetFloat(Json, TEXT("TwistLimitDegrees"));
	}
	if (Json.HasField(TEXT("TwistMotion")))
	{
		TwistLimit.TwistMotion = ParseAngularMotion(GetString(Json, TEXT("TwistMotion")));
	}
}

void ApplyConstraintProfile(const FJsonObject& Json, FConstraintProfileProperties& Profile)
{
	TSharedPtr<FJsonObject> LinearLimitJson;
	if (TryGetObject(Json, TEXT("LinearLimit"), LinearLimitJson))
	{
		ApplyLinearConstraint(*LinearLimitJson, Profile.LinearLimit);
	}

	TSharedPtr<FJsonObject> ConeLimitJson;
	if (TryGetObject(Json, TEXT("ConeLimit"), ConeLimitJson))
	{
		ApplyConeConstraint(*ConeLimitJson, Profile.ConeLimit);
	}

	TSharedPtr<FJsonObject> TwistLimitJson;
	if (TryGetObject(Json, TEXT("TwistLimit"), TwistLimitJson))
	{
		ApplyTwistConstraint(*TwistLimitJson, Profile.TwistLimit);
	}

	if (Json.HasField(TEXT("bDisableCollision")))
	{
		Profile.bDisableCollision = GetBool(Json, TEXT("bDisableCollision"));
	}
	if (Json.HasField(TEXT("bParentDominates")))
	{
		Profile.bParentDominates = GetBool(Json, TEXT("bParentDominates"));
	}
	if (Json.HasField(TEXT("bEnableProjection")))
	{
		Profile.bEnableProjection = GetBool(Json, TEXT("bEnableProjection"));
	}
	if (Json.HasField(TEXT("bEnableMassConditioning")))
	{
		Profile.bEnableMassConditioning = GetBool(Json, TEXT("bEnableMassConditioning"));
	}
	if (Json.HasField(TEXT("ProjectionLinearTolerance")))
	{
		Profile.ProjectionLinearTolerance = GetFloat(Json, TEXT("ProjectionLinearTolerance"));
	}
	if (Json.HasField(TEXT("ProjectionAngularTolerance")))
	{
		Profile.ProjectionAngularTolerance = GetFloat(Json, TEXT("ProjectionAngularTolerance"));
	}
}

bool BoneExists(const USkeletalMesh& SkeletalMesh, const FName BoneName)
{
	return BoneName == NAME_None || SkeletalMesh.GetRefSkeleton().FindBoneIndex(BoneName) != INDEX_NONE;
}

bool BuildBodyFromJson(UPhysicsAsset& PhysicsAsset, const USkeletalMesh& SkeletalMesh, const FJsonObject& BodyJson, FString& OutError)
{
	TSharedPtr<FJsonObject> Properties;
	if (!TryGetObject(BodyJson, TEXT("Properties"), Properties))
	{
		OutError = FString::Printf(TEXT("%s has no Properties object."), *GetString(BodyJson, TEXT("Name")));
		return false;
	}

	const FName BoneName = GetOptionalName(*Properties, TEXT("BoneName"));
	if (!BoneExists(SkeletalMesh, BoneName))
	{
		OutError = FString::Printf(TEXT("Bone '%s' is not in selected SkeletalMesh '%s'."), *BoneName.ToString(), *SkeletalMesh.GetName());
		return false;
	}

	USkeletalBodySetup* BodySetup = NewObject<USkeletalBodySetup>(&PhysicsAsset, NAME_None, RF_Transactional);
	BodySetup->BoneName = BoneName;
	BodySetup->PhysicsType = ParsePhysicsType(GetString(*Properties, TEXT("PhysicsType")));
	BodySetup->CollisionTraceFlag = ParseCollisionTraceFlag(GetString(*Properties, TEXT("CollisionTraceFlag"), TEXT("ECollisionTraceFlag::CTF_UseSimpleAsComplex")));
	BodySetup->bConsiderForBounds = true;
	BodySetup->AggGeom.EmptyElements();

	TSharedPtr<FJsonObject> AggGeom;
	if (TryGetObject(*Properties, TEXT("AggGeom"), AggGeom))
	{
		const TArray<TSharedPtr<FJsonValue>>* SphylElems = nullptr;
		if (TryGetArray(*AggGeom, TEXT("SphylElems"), SphylElems))
		{
			for (const TSharedPtr<FJsonValue>& SphylValue : *SphylElems)
			{
				if (!SphylValue.IsValid() || SphylValue->Type != EJson::Object)
				{
					continue;
				}

				const TSharedPtr<FJsonObject> SphylJson = SphylValue->AsObject();
				if (!SphylJson.IsValid())
				{
					continue;
				}

				FKSphylElem SphylElem;
				SphylElem.Radius = GetFloat(*SphylJson, TEXT("Radius"), 1.0f);
				SphylElem.Length = GetFloat(*SphylJson, TEXT("Length"), 1.0f);
				SphylElem.RestOffset = GetFloat(*SphylJson, TEXT("RestOffset"));
				SphylElem.SetName(GetOptionalName(*SphylJson, TEXT("Name")));
				SphylElem.SetContributeToMass(GetBool(*SphylJson, TEXT("bContributeToMass"), true));
				SphylElem.SetCollisionEnabled(ParseCollisionEnabled(GetString(*SphylJson, TEXT("CollisionEnabled"))));

				TSharedPtr<FJsonObject> CenterJson;
				if (TryGetObject(*SphylJson, TEXT("Center"), CenterJson))
				{
					SphylElem.Center = GetVector(*CenterJson);
				}

				TSharedPtr<FJsonObject> RotationJson;
				if (TryGetObject(*SphylJson, TEXT("Rotation"), RotationJson))
				{
					SphylElem.Rotation = GetRotator(*RotationJson);
				}

				BodySetup->AggGeom.SphylElems.Add(SphylElem);
			}
		}
	}

	if (BodySetup->AggGeom.GetElementCount() == 0)
	{
		OutError = FString::Printf(TEXT("Body '%s' has no supported collision shape."), *BoneName.ToString());
		return false;
	}

	PhysicsAsset.SkeletalBodySetups.Add(BodySetup);
	return true;
}

bool BuildConstraintFromJson(UPhysicsAsset& PhysicsAsset, const USkeletalMesh& SkeletalMesh, const FJsonObject& ConstraintJson, FString& OutError)
{
	TSharedPtr<FJsonObject> Properties;
	TSharedPtr<FJsonObject> InstanceJson;
	if (!TryGetObject(ConstraintJson, TEXT("Properties"), Properties) || !TryGetObject(*Properties, TEXT("DefaultInstance"), InstanceJson))
	{
		OutError = FString::Printf(TEXT("%s has no DefaultInstance object."), *GetString(ConstraintJson, TEXT("Name")));
		return false;
	}

	UPhysicsConstraintTemplate* ConstraintTemplate = NewObject<UPhysicsConstraintTemplate>(&PhysicsAsset, NAME_None, RF_Transactional);
	FConstraintInstance& Instance = ConstraintTemplate->DefaultInstance;
	Instance.JointName = GetOptionalName(*InstanceJson, TEXT("JointName"));
	Instance.ConstraintBone1 = GetOptionalName(*InstanceJson, TEXT("ConstraintBone1"));
	Instance.ConstraintBone2 = GetOptionalName(*InstanceJson, TEXT("ConstraintBone2"));

	if (!BoneExists(SkeletalMesh, Instance.ConstraintBone1) || !BoneExists(SkeletalMesh, Instance.ConstraintBone2))
	{
		OutError = FString::Printf(
			TEXT("Constraint '%s' references missing bones '%s' -> '%s'."),
			*GetString(ConstraintJson, TEXT("Name")),
			*Instance.ConstraintBone1.ToString(),
			*Instance.ConstraintBone2.ToString());
		return false;
	}

	TSharedPtr<FJsonObject> VectorJson;
	if (TryGetObject(*InstanceJson, TEXT("Pos1"), VectorJson))
	{
		Instance.Pos1 = GetVector(*VectorJson);
	}
	if (TryGetObject(*InstanceJson, TEXT("PriAxis1"), VectorJson))
	{
		Instance.PriAxis1 = GetVector(*VectorJson);
	}
	if (TryGetObject(*InstanceJson, TEXT("SecAxis1"), VectorJson))
	{
		Instance.SecAxis1 = GetVector(*VectorJson);
	}
	if (TryGetObject(*InstanceJson, TEXT("Pos2"), VectorJson))
	{
		Instance.Pos2 = GetVector(*VectorJson);
	}
	if (TryGetObject(*InstanceJson, TEXT("PriAxis2"), VectorJson))
	{
		Instance.PriAxis2 = GetVector(*VectorJson);
	}
	if (TryGetObject(*InstanceJson, TEXT("SecAxis2"), VectorJson))
	{
		Instance.SecAxis2 = GetVector(*VectorJson);
	}
	if (TryGetObject(*InstanceJson, TEXT("AngularRotationOffset"), VectorJson))
	{
		Instance.AngularRotationOffset = GetRotator(*VectorJson);
	}

	TSharedPtr<FJsonObject> ProfileJson;
	if (TryGetObject(*InstanceJson, TEXT("ProfileInstance"), ProfileJson))
	{
		ApplyConstraintProfile(*ProfileJson, Instance.ProfileInstance);
	}

	ConstraintTemplate->SetDefaultProfile(Instance);
	PhysicsAsset.ConstraintSetup.Add(ConstraintTemplate);
	return true;
}

void ApplyBoundsBodies(UPhysicsAsset& PhysicsAsset, const FFModelPhysicsAssetAnalysis& Analysis)
{
	const TArray<TSharedPtr<FJsonValue>>* BoundsBodyValues = nullptr;
	if (TryGetArray(*Analysis.PhysicsAssetProperties, TEXT("BoundsBodies"), BoundsBodyValues))
	{
		for (const TSharedPtr<FJsonValue>& Value : *BoundsBodyValues)
		{
			PhysicsAsset.BoundsBodies.Add(static_cast<int32>(Value->AsNumber()));
		}
	}
	else
	{
		PhysicsAsset.UpdateBoundsBodiesArray();
	}
}

void ApplyCollisionDisableTable(UPhysicsAsset& PhysicsAsset, const FFModelPhysicsAssetAnalysis& Analysis)
{
	const TArray<TSharedPtr<FJsonValue>>* CollisionDisableValues = nullptr;
	if (!TryGetArray(*Analysis.PhysicsAssetObject, TEXT("CollisionDisableTable"), CollisionDisableValues))
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& EntryValue : *CollisionDisableValues)
	{
		if (!EntryValue.IsValid() || EntryValue->Type != EJson::Object)
		{
			continue;
		}

		const TSharedPtr<FJsonObject> EntryObject = EntryValue->AsObject();
		TSharedPtr<FJsonObject> KeyObject;
		const TArray<TSharedPtr<FJsonValue>>* IndexValues = nullptr;
		if (EntryObject.IsValid() && TryGetObject(*EntryObject, TEXT("Key"), KeyObject) && TryGetArray(*KeyObject, TEXT("Indices"), IndexValues) && IndexValues->Num() == 2)
		{
			const int32 IndexA = static_cast<int32>((*IndexValues)[0]->AsNumber());
			const int32 IndexB = static_cast<int32>((*IndexValues)[1]->AsNumber());
			PhysicsAsset.CollisionDisableTable.Add(FRigidBodyIndexPair(IndexA, IndexB), false);
		}
	}
}

UPhysicsAsset* CreatePhysicsAsset(USkeletalMesh& SkeletalMesh, const FFModelPhysicsAssetAnalysis& Analysis, FString& OutError)
{
	const FString MeshPackageName = SkeletalMesh.GetOutermost()->GetName();
	const FString MeshPackagePath = FPackageName::GetLongPackagePath(MeshPackageName);
	const FString BasePackageName = MeshPackagePath / (SkeletalMesh.GetName() + TEXT("_FModel_PhysicsAsset"));

	FString NewPackageName;
	FString NewAssetName;
	FAssetToolsModule::GetModule().Get().CreateUniqueAssetName(BasePackageName, TEXT(""), NewPackageName, NewAssetName);

	UPackage* Package = CreatePackage(*NewPackageName);
	UPhysicsAsset* PhysicsAsset = NewObject<UPhysicsAsset>(Package, UPhysicsAsset::StaticClass(), *NewAssetName, RF_Public | RF_Standalone | RF_Transactional);
	if (!PhysicsAsset)
	{
		OutError = FString::Printf(TEXT("Could not create PhysicsAsset package: %s"), *NewPackageName);
		return nullptr;
	}

	PhysicsAsset->Modify();
	PhysicsAsset->SetPreviewMesh(&SkeletalMesh, false);
	PhysicsAsset->SkeletalBodySetups.Empty();
	PhysicsAsset->ConstraintSetup.Empty();
	PhysicsAsset->BoundsBodies.Empty();
	PhysicsAsset->CollisionDisableTable.Empty();

	for (const TSharedPtr<FJsonObject>& Body : Analysis.OrderedBodies)
	{
		if (!Body.IsValid() || !BuildBodyFromJson(*PhysicsAsset, SkeletalMesh, *Body, OutError))
		{
			return nullptr;
		}
	}

	for (const TSharedPtr<FJsonObject>& Constraint : Analysis.OrderedConstraints)
	{
		if (!Constraint.IsValid() || !BuildConstraintFromJson(*PhysicsAsset, SkeletalMesh, *Constraint, OutError))
		{
			return nullptr;
		}
	}

	ApplyBoundsBodies(*PhysicsAsset, Analysis);
	ApplyCollisionDisableTable(*PhysicsAsset, Analysis);

	for (USkeletalBodySetup* BodySetup : PhysicsAsset->SkeletalBodySetups)
	{
		if (BodySetup)
		{
			BodySetup->InvalidatePhysicsData();
			BodySetup->CreatePhysicsMeshes();
		}
	}

	PhysicsAsset->UpdateBodySetupIndexMap();
	PhysicsAsset->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(PhysicsAsset);
	return PhysicsAsset;
}
}

UPhysicsAsset* FFModelPhysicsAssetImporter::ImportFromJsonFile(USkeletalMesh& SkeletalMesh, const FString& JsonFilePath, FFModelPhysicsAssetImportSummary& OutSummary, FString& OutError)
{
	OutSummary = FFModelPhysicsAssetImportSummary();

	TArray<TSharedPtr<FJsonValue>> RootArray;
	if (!LoadJsonArrayFromFile(JsonFilePath, RootArray, OutError))
	{
		return nullptr;
	}

	FFModelPhysicsAssetAnalysis Analysis;
	if (!AnalyzeFModelPhysicsAssetJson(RootArray, Analysis, OutError))
	{
		return nullptr;
	}

	UPhysicsAsset* PhysicsAsset = CreatePhysicsAsset(SkeletalMesh, Analysis, OutError);
	if (!PhysicsAsset)
	{
		return nullptr;
	}

	OutSummary.BodyCount = PhysicsAsset->SkeletalBodySetups.Num();
	OutSummary.ConstraintCount = PhysicsAsset->ConstraintSetup.Num();
	OutSummary.DisabledCollisionPairCount = Analysis.DisabledCollisionPairCount;
	return PhysicsAsset;
}
}
