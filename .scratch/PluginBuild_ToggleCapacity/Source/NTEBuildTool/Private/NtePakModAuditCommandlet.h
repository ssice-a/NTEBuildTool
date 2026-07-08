// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "NtePakModAuditCommandlet.generated.h"

UCLASS()
class UNtePakModAuditCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UNtePakModAuditCommandlet();

	virtual int32 Main(const FString& Params) override;
};
