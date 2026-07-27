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

struct FFModelPhysicsAssetImportOptions
{
	/** NAME_None imports the complete source asset. Any other bone imports that bone and its descendants only. */
	FName ChainRootBone = NAME_None;
};

class FFModelPhysicsAssetImporter
{
public:
	static UPhysicsAsset* ImportFromJsonFile(USkeletalMesh& SkeletalMesh, const FString& JsonFilePath, FFModelPhysicsAssetImportSummary& OutSummary, FString& OutError);
	static UPhysicsAsset* ImportFromJsonFile(USkeletalMesh& SkeletalMesh, const FString& JsonFilePath, const FFModelPhysicsAssetImportOptions& Options, FFModelPhysicsAssetImportSummary& OutSummary, FString& OutError);
};
}
