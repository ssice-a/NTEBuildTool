// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "NteMaterialConfigCommandlet.generated.h"

UCLASS()
class UNteMaterialConfigCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UNteMaterialConfigCommandlet();

	virtual int32 Main(const FString& Params) override;
};
