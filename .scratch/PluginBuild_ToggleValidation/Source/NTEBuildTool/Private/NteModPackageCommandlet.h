// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "NteModPackageCommandlet.generated.h"

UCLASS()
class UNteModPackageCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UNteModPackageCommandlet();

	virtual int32 Main(const FString& Params) override;
};
