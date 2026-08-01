// Copyright (c) 2026 NTEBuildTool contributors.

#include "NTEBuildTool.h"

#include "NtePakmodProjectEditor.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "NTEBuildTool"

DEFINE_LOG_CATEGORY(LogNTEBuildTool);

void FNTEBuildToolModule::StartupModule()
{
	NTEBuildTool::ProjectEditor::RegisterPakmodProjectEditorTab();
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FNTEBuildToolModule::RegisterMenus));
}

void FNTEBuildToolModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	NTEBuildTool::ProjectEditor::UnregisterPakmodProjectEditorTab();
}

IMPLEMENT_MODULE(FNTEBuildToolModule, NTEBuildTool)

void FNTEBuildToolModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
	FToolMenuSection& Section = Menu->AddSection(TEXT("NTEBuildTool"), LOCTEXT("NTEBuildToolSection", "NTE Build Tool"));
	Section.AddEntry(FToolMenuEntry::InitMenuEntry(TEXT("NTEBuildTool_OpenPakmodProject"), LOCTEXT("OpenPakmodProjectLabel", "Open Pakmod Project"), LOCTEXT("OpenPakmodProjectTooltip", "Open the persistent package-first editor for assets, authoring recipes, manifest, and build."), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(this, &FNTEBuildToolModule::OpenPakmodProject))));
}

void FNTEBuildToolModule::OpenPakmodProject()
{
	NTEBuildTool::ProjectEditor::OpenPakmodProjectEditorTab();
}

#undef LOCTEXT_NAMESPACE
