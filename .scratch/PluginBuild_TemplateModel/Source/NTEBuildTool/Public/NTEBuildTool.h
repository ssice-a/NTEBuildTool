// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNTEBuildTool, Log, All);

class FNTEBuildToolModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void ImportFModelPhysicsAssetJson();
	void CreateModMaterialInstanceFromSourceJson();
	void CreateModMaterialInstanceFromConfig();
	void CreateMeshToggleUiSetup();
	void BuildSelectedAssetsModPackage();
	void BuildModPackageFromJobJson();
};
