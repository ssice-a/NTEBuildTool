// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace NTEBuildTool
{
struct FFModelPhysicsAssetAnalysis
{
	TSharedPtr<FJsonObject> PhysicsAssetObject;
	TSharedPtr<FJsonObject> PhysicsAssetProperties;
	TArray<TSharedPtr<FJsonObject>> OrderedBodies;
	TArray<TSharedPtr<FJsonObject>> OrderedConstraints;
	int32 DisabledCollisionPairCount = 0;
};

bool AnalyzeFModelPhysicsAssetJson(const TArray<TSharedPtr<FJsonValue>>& RootArray, FFModelPhysicsAssetAnalysis& OutAnalysis, FString& OutError);
}
