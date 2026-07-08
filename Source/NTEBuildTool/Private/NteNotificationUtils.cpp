// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteNotificationUtils.h"

#include "NTEBuildTool.h"

#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IDesktopPlatform.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "NTEBuildTool"

namespace NTEBuildTool::Editor
{
void ShowError(const FText& Message)
{
	UE_LOG(LogNTEBuildTool, Error, TEXT("%s"), *Message.ToString());
	FMessageDialog::Open(EAppMsgType::Ok, Message, LOCTEXT("NTEBuildToolErrorTitle", "NTE Build Tool"));
}

void ShowInfo(const FText& Message)
{
	UE_LOG(LogNTEBuildTool, Display, TEXT("%s"), *Message.ToString());
	FMessageDialog::Open(EAppMsgType::Ok, Message, LOCTEXT("NTEBuildToolInfoTitle", "NTE Build Tool"));
}

void ShowSuccessNotification(const FText& Message)
{
	UE_LOG(LogNTEBuildTool, Display, TEXT("%s"), *Message.ToString());

	FNotificationInfo Info(Message);
	Info.ExpireDuration = 5.0f;
	Info.FadeOutDuration = 0.4f;
	Info.bUseSuccessFailIcons = true;

	const TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Success);
	}
}

bool ChooseJsonFileWithTitle(const FText& Title, const FString& DefaultFilename, FString& OutFilename)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		ShowError(LOCTEXT("NoDesktopPlatform", "Desktop file dialog is unavailable."));
		return false;
	}

	TArray<FString> OpenFilenames;
	const void* ParentWindowHandle = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)
		: nullptr;

	const bool bOpened = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		Title.ToString(),
		FPaths::ProjectDir(),
		DefaultFilename,
		TEXT("JSON files (*.json)|*.json"),
		EFileDialogFlags::None,
		OpenFilenames);

	if (!bOpened || OpenFilenames.IsEmpty())
	{
		return false;
	}

	OutFilename = OpenFilenames[0];
	return true;
}

bool ChooseSaveJsonFileWithTitle(const FText& Title, const FString& DefaultFilename, FString& OutFilename)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		ShowError(LOCTEXT("NoDesktopPlatformForSave", "Desktop file dialog is unavailable."));
		return false;
	}

	TArray<FString> SaveFilenames;
	const void* ParentWindowHandle = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)
		: nullptr;

	const bool bSaved = DesktopPlatform->SaveFileDialog(
		ParentWindowHandle,
		Title.ToString(),
		FPaths::ProjectSavedDir() / TEXT("NTEBuildTool"),
		DefaultFilename,
		TEXT("JSON files (*.json)|*.json"),
		EFileDialogFlags::None,
		SaveFilenames);

	if (!bSaved || SaveFilenames.IsEmpty())
	{
		return false;
	}

	OutFilename = SaveFilenames[0];
	if (!OutFilename.EndsWith(TEXT(".json"), ESearchCase::IgnoreCase))
	{
		OutFilename += TEXT(".json");
	}
	return true;
}

bool ChooseAssetFileWithTitle(const FText& Title, const FString& DefaultFilename, const FString& FileTypes, FString& OutFilename)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		ShowError(LOCTEXT("NoDesktopPlatformForAsset", "Desktop file dialog is unavailable."));
		return false;
	}

	TArray<FString> OpenFilenames;
	const void* ParentWindowHandle = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)
		: nullptr;

	const bool bOpened = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		Title.ToString(),
		FPaths::ProjectContentDir(),
		DefaultFilename,
		FileTypes,
		EFileDialogFlags::None,
		OpenFilenames);

	if (!bOpened || OpenFilenames.IsEmpty())
	{
		return false;
	}

	OutFilename = OpenFilenames[0];
	return true;
}

bool ChooseDirectoryWithTitle(const FText& Title, const FString& DefaultDirectory, FString& OutDirectory)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		ShowError(LOCTEXT("NoDesktopPlatformForDirectory", "Desktop directory dialog is unavailable."));
		return false;
	}

	const void* ParentWindowHandle = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)
		: nullptr;

	return DesktopPlatform->OpenDirectoryDialog(
		ParentWindowHandle,
		Title.ToString(),
		DefaultDirectory,
		OutDirectory);
}
}

#undef LOCTEXT_NAMESPACE
