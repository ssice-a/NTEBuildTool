// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "NteCharacterModSpecCommandlet.generated.h"

UCLASS()
class UNteCharacterModSpecCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UNteCharacterModSpecCommandlet();

	virtual int32 Main(const FString& Params) override;
};
