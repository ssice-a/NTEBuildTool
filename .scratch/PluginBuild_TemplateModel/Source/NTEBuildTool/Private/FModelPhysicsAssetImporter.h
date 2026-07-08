// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"

class UPhysicsAsset;
class USkeletalMesh;

namespace NTEBuildTool
{
struct FFModelPhysicsAssetImportSummary
{
	int32 BodyCount = 0;
	int32 ConstraintCount = 0;
	int32 DisabledCollisionPairCount = 0;
};

class FFModelPhysicsAssetImporter
{
public:
	static UPhysicsAsset* ImportFromJsonFile(USkeletalMesh& SkeletalMesh, const FString& JsonFilePath, FFModelPhysicsAssetImportSummary& OutSummary, FString& OutError);
};
}
