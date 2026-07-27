// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace NTEBuildTool::FModelJson
{
bool LoadJsonArrayFromFile(const FString& FilePath, TArray<TSharedPtr<FJsonValue>>& OutRootArray, FString& OutError);

bool TryGetObject(const FJsonObject& Object, const TCHAR* FieldName, TSharedPtr<FJsonObject>& OutObject);
bool TryGetArray(const FJsonObject& Object, const TCHAR* FieldName, const TArray<TSharedPtr<FJsonValue>>*& OutArray);

float GetFloat(const FJsonObject& Object, const TCHAR* FieldName, float DefaultValue = 0.0f);
bool GetBool(const FJsonObject& Object, const TCHAR* FieldName, bool DefaultValue = false);
FString GetString(const FJsonObject& Object, const TCHAR* FieldName, const FString& DefaultValue = FString());
FVector GetVector(const FJsonObject& Object);
FRotator GetRotator(const FJsonObject& Object);
FName GetOptionalName(const FJsonObject& Object, const TCHAR* FieldName, FName DefaultValue = NAME_None);
FString ExtractReferencedObjectName(const TSharedPtr<FJsonValue>& RefValue);

class FExportObjectIndex
{
public:
	void Build(const TArray<TSharedPtr<FJsonValue>>& RootArray);

	TSharedPtr<FJsonObject> FindFirstByType(const FString& Type) const;
	TSharedPtr<FJsonObject> FindByTypeAndName(const FString& Type, const FString& Name) const;

private:
	TMap<FString, TArray<TSharedPtr<FJsonObject>>> ObjectsByType;
	TMap<FString, TMap<FString, TSharedPtr<FJsonObject>>> ObjectsByTypeAndName;
};
}
