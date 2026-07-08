// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "NteMeshToggleCommandlet.generated.h"

UCLASS()
class UNteMeshToggleCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UNteMeshToggleCommandlet();

	virtual int32 Main(const FString& Params) override;
};
