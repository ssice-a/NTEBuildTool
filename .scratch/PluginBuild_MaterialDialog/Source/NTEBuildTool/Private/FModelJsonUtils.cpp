// Copyright (c) 2026 NTEBuildTool contributors.

#include "FModelJsonUtils.h"

#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace NTEBuildTool::FModelJson
{
bool LoadJsonArrayFromFile(const FString& FilePath, TArray<TSharedPtr<FJsonValue>>& OutRootArray, FString& OutError)
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
	{
		OutError = FString::Printf(TEXT("Could not read file: %s"), *FilePath);
		return false;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, OutRootArray) || OutRootArray.IsEmpty())
	{
		OutError = TEXT("The selected file is not a valid FModel JSON array.");
		return false;
	}

	return true;
}

bool TryGetObject(const FJsonObject& Object, const TCHAR* FieldName, TSharedPtr<FJsonObject>& OutObject)
{
	const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;
	if (Object.TryGetObjectField(FieldName, ObjectPtr) && ObjectPtr && ObjectPtr->IsValid())
	{
		OutObject = *ObjectPtr;
		return true;
	}

	return false;
}

bool TryGetArray(const FJsonObject& Object, const TCHAR* FieldName, const TArray<TSharedPtr<FJsonValue>>*& OutArray)
{
	return Object.TryGetArrayField(FieldName, OutArray) && OutArray;
}

float GetFloat(const FJsonObject& Object, const TCHAR* FieldName, float DefaultValue)
{
	double Number = DefaultValue;
	Object.TryGetNumberField(FieldName, Number);
	return static_cast<float>(Number);
}

bool GetBool(const FJsonObject& Object, const TCHAR* FieldName, bool DefaultValue)
{
	bool Value = DefaultValue;
	Object.TryGetBoolField(FieldName, Value);
	return Value;
}

FString GetString(const FJsonObject& Object, const TCHAR* FieldName, const FString& DefaultValue)
{
	FString Value;
	return Object.TryGetStringField(FieldName, Value) ? Value : DefaultValue;
}

FVector GetVector(const FJsonObject& Object)
{
	return FVector(
		GetFloat(Object, TEXT("X")),
		GetFloat(Object, TEXT("Y")),
		GetFloat(Object, TEXT("Z")));
}

FRotator GetRotator(const FJsonObject& Object)
{
	return FRotator(
		GetFloat(Object, TEXT("Pitch")),
		GetFloat(Object, TEXT("Yaw")),
		GetFloat(Object, TEXT("Roll")));
}

FName GetOptionalName(const FJsonObject& Object, const TCHAR* FieldName, FName DefaultValue)
{
	const FString Value = GetString(Object, FieldName);
	if (Value.IsEmpty() || Value == TEXT("None"))
	{
		return DefaultValue;
	}

	return FName(*Value);
}

FString ExtractReferencedObjectName(const TSharedPtr<FJsonValue>& RefValue)
{
	if (!RefValue.IsValid() || RefValue->Type != EJson::Object)
	{
		return FString();
	}

	const TSharedPtr<FJsonObject> RefObject = RefValue->AsObject();
	if (!RefObject.IsValid())
	{
		return FString();
	}

	FString ObjectName = GetString(*RefObject, TEXT("ObjectName"));
	int32 ColonIndex = INDEX_NONE;
	int32 QuoteIndex = INDEX_NONE;
	if (ObjectName.FindChar(TEXT(':'), ColonIndex) && ObjectName.FindLastChar(TEXT('\''), QuoteIndex) && QuoteIndex > ColonIndex)
	{
		return ObjectName.Mid(ColonIndex + 1, QuoteIndex - ColonIndex - 1);
	}

	ObjectName.Split(TEXT("'"), nullptr, &ObjectName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	return ObjectName;
}

void FExportObjectIndex::Build(const TArray<TSharedPtr<FJsonValue>>& RootArray)
{
	ObjectsByType.Reset();
	ObjectsByTypeAndName.Reset();

	for (const TSharedPtr<FJsonValue>& Value : RootArray)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			continue;
		}

		const TSharedPtr<FJsonObject> Object = Value->AsObject();
		if (!Object.IsValid())
		{
			continue;
		}

		const FString Type = GetString(*Object, TEXT("Type"));
		const FString Name = GetString(*Object, TEXT("Name"));
		if (Type.IsEmpty())
		{
			continue;
		}

		ObjectsByType.FindOrAdd(Type).Add(Object);
		if (!Name.IsEmpty())
		{
			ObjectsByTypeAndName.FindOrAdd(Type).Add(Name, Object);
		}
	}
}

TSharedPtr<FJsonObject> FExportObjectIndex::FindFirstByType(const FString& Type) const
{
	const TArray<TSharedPtr<FJsonObject>>* Objects = ObjectsByType.Find(Type);
	return Objects && !Objects->IsEmpty() ? (*Objects)[0] : nullptr;
}

TSharedPtr<FJsonObject> FExportObjectIndex::FindByTypeAndName(const FString& Type, const FString& Name) const
{
	const TMap<FString, TSharedPtr<FJsonObject>>* Objects = ObjectsByTypeAndName.Find(Type);
	if (!Objects)
	{
		return nullptr;
	}

	const TSharedPtr<FJsonObject>* Object = Objects->Find(Name);
	return Object ? *Object : nullptr;
}
}
