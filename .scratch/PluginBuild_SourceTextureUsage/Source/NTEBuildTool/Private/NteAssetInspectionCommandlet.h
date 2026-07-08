// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "NteAssetInspectionCommandlet.generated.h"

UCLASS()
class UNteAssetInspectionCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UNteAssetInspectionCommandlet();

	virtual int32 Main(const FString& Params) override;
};
