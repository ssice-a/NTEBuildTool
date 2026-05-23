// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "Modules/ModuleManager.h"

class FNTEBuildToolModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void ImportFModelPhysicsAssetJson();
};
