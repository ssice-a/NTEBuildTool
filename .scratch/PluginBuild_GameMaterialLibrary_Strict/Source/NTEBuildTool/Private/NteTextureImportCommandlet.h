// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "NteTextureImportCommandlet.generated.h"

UCLASS()
class UNteTextureImportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UNteTextureImportCommandlet();

	virtual int32 Main(const FString& Params) override;
};
