// Copyright (c) 2026 NTEBuildTool contributors.

#include "NTEBuildTool.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "DesktopPlatformModule.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "IContentBrowserSingleton.h"
#include "IDesktopPlatform.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/RigidBodyIndexPair.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/SphylElem.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "NTEBuildTool"

DEFINE_LOG_CATEGORY_STATIC(LogNTEBuildTool, Log, All);

namespace
{
void ShowError(const FText& Message)
{
	UE_LOG(LogNTEBuildTool, Error, TEXT("%s"), *Message.ToString());
	FMessageDialog::Open(EAppMsgType::Ok, Message, LOCTEXT("NTEBuildToolErrorTitle", "NTE Build Tool"));
}

void ShowInfo(const FText& Message)
{
	UE_LOG(LogNTEBuildTool, Display, TEXT("%s"), *Message.ToString());
	FMessageDialog::Open(EAppMsgType::Ok, Message, LOCTEXT("NTEBuildToolInfoTitle", "NTE Build Tool"));
}

bool TryGetObject(const FJsonObject& Object, const TCHAR* FieldName, TSharedPtr<FJsonObject>& OutObject)
{
	const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;
	if (Object.TryGetObjectField(FieldName, ObjectPtr) && ObjectPtr && ObjectPtr->IsValid())
	{
		OutObject = *ObjectPtr;
		return true;
	}

	return false;
}

bool TryGetArray(const FJsonObject& Object, const TCHAR* FieldName, const TArray<TSharedPtr<FJsonValue>>*& OutArray)
{
	return Object.TryGetArrayField(FieldName, OutArray) && OutArray;
}

float GetFloat(const FJsonObject& Object, const TCHAR* FieldName, float DefaultValue = 0.0f)
{
	double Number = DefaultValue;
	Object.TryGetNumberField(FieldName, Number);
	return static_cast<float>(Number);
}

bool GetBool(const FJsonObject& Object, const TCHAR* FieldName, bool DefaultValue = false)
{
	bool Value = DefaultValue;
	Object.TryGetBoolField(FieldName, Value);
	return Value;
}

FString GetString(const FJsonObject& Object, const TCHAR* FieldName, const FString& DefaultValue = FString())
{
	FString Value;
	return Object.TryGetStringField(FieldName, Value) ? Value : DefaultValue;
}

FVector GetVector(const FJsonObject& Object)
{
	return FVector(
		GetFloat(Object, TEXT("X")),
		GetFloat(Object, TEXT("Y")),
		GetFloat(Object, TEXT("Z")));
}

FRotator GetRotator(const FJsonObject& Object)
{
	return FRotator(
		GetFloat(Object, TEXT("Pitch")),
		GetFloat(Object, TEXT("Yaw")),
		GetFloat(Object, TEXT("Roll")));
}

FName GetOptionalName(const FJsonObject& Object, const TCHAR* FieldName, FName DefaultValue = NAME_None)
{
	const FString Value = GetString(Object, FieldName);
	if (Value.IsEmpty() || Value == TEXT("None"))
	{
		return DefaultValue;
	}

	return FName(*Value);
}

FString ExtractReferencedObjectName(const TSharedPtr<FJsonValue>& RefValue)
{
	if (!RefValue.IsValid() || RefValue->Type != EJson::Object)
	{
		return FString();
	}

	const TSharedPtr<FJsonObject> RefObject = RefValue->AsObject();
	if (!RefObject.IsValid())
	{
		return FString();
	}

	FString ObjectName = GetString(*RefObject, TEXT("ObjectName"));
	int32 ColonIndex = INDEX_NONE;
	int32 QuoteIndex = INDEX_NONE;
	if (ObjectName.FindChar(TEXT(':'), ColonIndex) && ObjectName.FindLastChar(TEXT('\''), QuoteIndex) && QuoteIndex > ColonIndex)
	{
		return ObjectName.Mid(ColonIndex + 1, QuoteIndex - ColonIndex - 1);
	}

	ObjectName.Split(TEXT("'"), nullptr, &ObjectName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	return ObjectName;
}

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

UPhysicsAsset* CreatePhysicsAssetFromFModelJson(USkeletalMesh& SkeletalMesh, const FString& JsonFilePath, FString& OutError)
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *JsonFilePath))
	{
		OutError = FString::Printf(TEXT("Could not read file: %s"), *JsonFilePath);
		return nullptr;
	}

	TArray<TSharedPtr<FJsonValue>> RootArray;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootArray) || RootArray.IsEmpty())
	{
		OutError = TEXT("The selected file is not a valid FModel JSON array.");
		return nullptr;
	}

	TSharedPtr<FJsonObject> PhysicsAssetJson;
	TMap<FString, TSharedPtr<FJsonObject>> BodiesByName;
	TMap<FString, TSharedPtr<FJsonObject>> ConstraintsByName;

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
		if (Type == TEXT("PhysicsAsset"))
		{
			PhysicsAssetJson = Object;
		}
		else if (Type == TEXT("SkeletalBodySetup"))
		{
			BodiesByName.Add(Name, Object);
		}
		else if (Type == TEXT("PhysicsConstraintTemplate"))
		{
			ConstraintsByName.Add(Name, Object);
		}
	}

	if (!PhysicsAssetJson.IsValid())
	{
		OutError = TEXT("No PhysicsAsset object was found in this JSON.");
		return nullptr;
	}

	TSharedPtr<FJsonObject> PhysicsAssetProperties;
	if (!TryGetObject(*PhysicsAssetJson, TEXT("Properties"), PhysicsAssetProperties))
	{
		OutError = TEXT("PhysicsAsset object has no Properties object.");
		return nullptr;
	}

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

	const TArray<TSharedPtr<FJsonValue>>* BodyRefs = nullptr;
	if (!TryGetArray(*PhysicsAssetProperties, TEXT("SkeletalBodySetups"), BodyRefs))
	{
		OutError = TEXT("PhysicsAsset has no SkeletalBodySetups array.");
		return nullptr;
	}

	for (const TSharedPtr<FJsonValue>& BodyRef : *BodyRefs)
	{
		const FString BodyName = ExtractReferencedObjectName(BodyRef);
		const TSharedPtr<FJsonObject>* BodyJson = BodiesByName.Find(BodyName);
		if (!BodyJson || !BodyJson->IsValid())
		{
			OutError = FString::Printf(TEXT("Could not resolve body reference '%s'."), *BodyName);
			return nullptr;
		}

		if (!BuildBodyFromJson(*PhysicsAsset, SkeletalMesh, **BodyJson, OutError))
		{
			return nullptr;
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* ConstraintRefs = nullptr;
	if (TryGetArray(*PhysicsAssetProperties, TEXT("ConstraintSetup"), ConstraintRefs))
	{
		for (const TSharedPtr<FJsonValue>& ConstraintRef : *ConstraintRefs)
		{
			const FString ConstraintName = ExtractReferencedObjectName(ConstraintRef);
			const TSharedPtr<FJsonObject>* ConstraintJson = ConstraintsByName.Find(ConstraintName);
			if (!ConstraintJson || !ConstraintJson->IsValid())
			{
				OutError = FString::Printf(TEXT("Could not resolve constraint reference '%s'."), *ConstraintName);
				return nullptr;
			}

			if (!BuildConstraintFromJson(*PhysicsAsset, SkeletalMesh, **ConstraintJson, OutError))
			{
				return nullptr;
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* BoundsBodyValues = nullptr;
	if (TryGetArray(*PhysicsAssetProperties, TEXT("BoundsBodies"), BoundsBodyValues))
	{
		for (const TSharedPtr<FJsonValue>& Value : *BoundsBodyValues)
		{
			PhysicsAsset->BoundsBodies.Add(static_cast<int32>(Value->AsNumber()));
		}
	}
	else
	{
		PhysicsAsset->UpdateBoundsBodiesArray();
	}

	const TArray<TSharedPtr<FJsonValue>>* CollisionDisableValues = nullptr;
	if (TryGetArray(*PhysicsAssetJson, TEXT("CollisionDisableTable"), CollisionDisableValues))
	{
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
				PhysicsAsset->CollisionDisableTable.Add(FRigidBodyIndexPair(IndexA, IndexB), false);
			}
		}
	}

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

void FNTEBuildToolModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FNTEBuildToolModule::RegisterMenus));
}

void FNTEBuildToolModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

IMPLEMENT_MODULE(FNTEBuildToolModule, NTEBuildTool)

void FNTEBuildToolModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
	FToolMenuSection& Section = Menu->AddSection(TEXT("NTEBuildTool"), LOCTEXT("NTEBuildToolSection", "NTE Build Tool"));
	Section.AddEntry(FToolMenuEntry::InitMenuEntry(
		TEXT("NTEBuildTool_ImportFModelPhysicsAsset"),
		LOCTEXT("ImportFModelPhysicsAssetLabel", "Import FModel PhysicsAsset JSON"),
		LOCTEXT("ImportFModelPhysicsAssetTooltip", "Create a PhysicsAsset from FModel Save Properties JSON and assign it to the selected SkeletalMesh."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FNTEBuildToolModule::ImportFModelPhysicsAssetJson))));
}

void FNTEBuildToolModule::ImportFModelPhysicsAssetJson()
{
	TArray<FAssetData> SelectedAssets;
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	USkeletalMesh* SelectedSkeletalMesh = nullptr;
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(AssetData.GetAsset()))
		{
			if (SelectedSkeletalMesh)
			{
				ShowError(LOCTEXT("SelectOneSkeletalMesh", "Please select exactly one SkeletalMesh in the Content Browser."));
				return;
			}

			SelectedSkeletalMesh = SkeletalMesh;
		}
	}

	if (!SelectedSkeletalMesh)
	{
		ShowError(LOCTEXT("NoSkeletalMeshSelected", "Select one SkeletalMesh in the Content Browser before importing."));
		return;
	}

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		ShowError(LOCTEXT("NoDesktopPlatform", "Desktop file dialog is unavailable."));
		return;
	}

	TArray<FString> OpenFilenames;
	const void* ParentWindowHandle = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)
		: nullptr;

	const bool bOpened = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		LOCTEXT("ChoosePhysicsAssetJson", "Choose FModel PhysicsAsset JSON").ToString(),
		FPaths::ProjectDir(),
		TEXT("player_051_female_skin_PhysicsAsset.json"),
		TEXT("JSON files (*.json)|*.json"),
		EFileDialogFlags::None,
		OpenFilenames);

	if (!bOpened || OpenFilenames.IsEmpty())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("ImportFModelPhysicsAssetTransaction", "Import FModel PhysicsAsset JSON"));
	SelectedSkeletalMesh->Modify();

	FString Error;
	UPhysicsAsset* PhysicsAsset = CreatePhysicsAssetFromFModelJson(*SelectedSkeletalMesh, OpenFilenames[0], Error);
	if (!PhysicsAsset)
	{
		ShowError(FText::FromString(Error));
		return;
	}

	SelectedSkeletalMesh->SetPhysicsAsset(PhysicsAsset);
	SelectedSkeletalMesh->PostEditChange();
	SelectedSkeletalMesh->MarkPackageDirty();

	TArray<UObject*> ObjectsToSync;
	ObjectsToSync.Add(PhysicsAsset);
	ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync);

	ShowInfo(FText::Format(
		LOCTEXT("ImportedPhysicsAsset", "Imported {0} bodies and {1} constraints into {2}, and assigned it to {3}. Save the dirty assets after checking it."),
		FText::AsNumber(PhysicsAsset->SkeletalBodySetups.Num()),
		FText::AsNumber(PhysicsAsset->ConstraintSetup.Num()),
		FText::FromString(PhysicsAsset->GetName()),
		FText::FromString(SelectedSkeletalMesh->GetName())));
}

#undef LOCTEXT_NAMESPACE
