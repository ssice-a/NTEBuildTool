// Copyright (c) 2026 NTEBuildTool contributors.

#include "FModelPhysicsAssetAnalysis.h"

#include "FModelJsonUtils.h"

namespace NTEBuildTool
{
namespace
{
int32 CountDisabledCollisionPairs(const FJsonObject& PhysicsAssetJson)
{
	const TArray<TSharedPtr<FJsonValue>>* CollisionDisableValues = nullptr;
	if (!FModelJson::TryGetArray(PhysicsAssetJson, TEXT("CollisionDisableTable"), CollisionDisableValues))
	{
		return 0;
	}

	int32 Count = 0;
	for (const TSharedPtr<FJsonValue>& EntryValue : *CollisionDisableValues)
	{
		if (!EntryValue.IsValid() || EntryValue->Type != EJson::Object)
		{
			continue;
		}

		const TSharedPtr<FJsonObject> EntryObject = EntryValue->AsObject();
		TSharedPtr<FJsonObject> KeyObject;
		const TArray<TSharedPtr<FJsonValue>>* IndexValues = nullptr;
		if (EntryObject.IsValid() && FModelJson::TryGetObject(*EntryObject, TEXT("Key"), KeyObject) && FModelJson::TryGetArray(*KeyObject, TEXT("Indices"), IndexValues) && IndexValues->Num() == 2)
		{
			++Count;
		}
	}

	return Count;
}

bool ResolveOrderedReferences(
	const FModelJson::FExportObjectIndex& Index,
	const FJsonObject& Properties,
	const TCHAR* FieldName,
	const FString& ReferencedType,
	TArray<TSharedPtr<FJsonObject>>& OutObjects,
	FString& OutError)
{
	const TArray<TSharedPtr<FJsonValue>>* References = nullptr;
	if (!FModelJson::TryGetArray(Properties, FieldName, References))
	{
		OutError = FString::Printf(TEXT("PhysicsAsset has no %s array."), FieldName);
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Reference : *References)
	{
		const FString ObjectName = FModelJson::ExtractReferencedObjectName(Reference);
		const TSharedPtr<FJsonObject> Object = Index.FindByTypeAndName(ReferencedType, ObjectName);
		if (!Object.IsValid())
		{
			OutError = FString::Printf(TEXT("Could not resolve %s reference '%s'."), FieldName, *ObjectName);
			return false;
		}

		OutObjects.Add(Object);
	}

	return true;
}
}

bool AnalyzeFModelPhysicsAssetJson(const TArray<TSharedPtr<FJsonValue>>& RootArray, FFModelPhysicsAssetAnalysis& OutAnalysis, FString& OutError)
{
	OutAnalysis = FFModelPhysicsAssetAnalysis();

	FModelJson::FExportObjectIndex Index;
	Index.Build(RootArray);

	OutAnalysis.PhysicsAssetObject = Index.FindFirstByType(TEXT("PhysicsAsset"));
	if (!OutAnalysis.PhysicsAssetObject.IsValid())
	{
		OutError = TEXT("No PhysicsAsset object was found in this JSON.");
		return false;
	}

	if (!FModelJson::TryGetObject(*OutAnalysis.PhysicsAssetObject, TEXT("Properties"), OutAnalysis.PhysicsAssetProperties))
	{
		OutError = TEXT("PhysicsAsset object has no Properties object.");
		return false;
	}

	if (!ResolveOrderedReferences(Index, *OutAnalysis.PhysicsAssetProperties, TEXT("SkeletalBodySetups"), TEXT("SkeletalBodySetup"), OutAnalysis.OrderedBodies, OutError))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ConstraintRefs = nullptr;
	if (FModelJson::TryGetArray(*OutAnalysis.PhysicsAssetProperties, TEXT("ConstraintSetup"), ConstraintRefs))
	{
		for (const TSharedPtr<FJsonValue>& ConstraintRef : *ConstraintRefs)
		{
			const FString ConstraintName = FModelJson::ExtractReferencedObjectName(ConstraintRef);
			const TSharedPtr<FJsonObject> ConstraintObject = Index.FindByTypeAndName(TEXT("PhysicsConstraintTemplate"), ConstraintName);
			if (!ConstraintObject.IsValid())
			{
				OutError = FString::Printf(TEXT("Could not resolve ConstraintSetup reference '%s'."), *ConstraintName);
				return false;
			}

			OutAnalysis.OrderedConstraints.Add(ConstraintObject);
		}
	}

	OutAnalysis.DisabledCollisionPairCount = CountDisabledCollisionPairs(*OutAnalysis.PhysicsAssetObject);
	return true;
}
}
