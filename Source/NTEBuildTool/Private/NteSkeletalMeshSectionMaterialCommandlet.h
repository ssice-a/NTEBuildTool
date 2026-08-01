// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "NteSkeletalMeshSectionMaterialCommandlet.generated.h"

UCLASS()
class UNteSkeletalMeshSectionMaterialCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UNteSkeletalMeshSectionMaterialCommandlet();

	virtual int32 Main(const FString& Params) override;
};
