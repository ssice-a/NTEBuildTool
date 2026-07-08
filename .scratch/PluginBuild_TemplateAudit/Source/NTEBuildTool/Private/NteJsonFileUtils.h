// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace NTEBuildTool::Json
{
bool LoadJsonObjectFromFile(const FString& Filename, TSharedPtr<FJsonObject>& OutObject, FString& OutError);
bool SaveJsonObjectToFile(const TSharedRef<FJsonObject>& Object, const FString& Filename, FString& OutError);

FString GetStringAny(const FJsonObject& Object, const TCHAR* FirstName, const TCHAR* SecondName = nullptr, const TCHAR* ThirdName = nullptr);
bool GetIntAny(const FJsonObject& Object, int32& OutValue, const TCHAR* FirstName, const TCHAR* SecondName = nullptr);
bool GetBoolAny(const FJsonObject& Object, bool& OutValue, const TCHAR* FirstName, const TCHAR* SecondName = nullptr);
bool TryGetObjectAny(const FJsonObject& Object, const TSharedPtr<FJsonObject>*& OutObject, const TCHAR* FirstName, const TCHAR* SecondName = nullptr, const TCHAR* ThirdName = nullptr);
TArray<FString> GetStringArrayAny(const FJsonObject& Object, const TCHAR* FirstName, const TCHAR* SecondName = nullptr);
TArray<TSharedPtr<FJsonValue>> StringArrayToJsonValues(const TArray<FString>& Values);
}
