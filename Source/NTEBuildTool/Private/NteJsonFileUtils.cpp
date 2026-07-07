// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteJsonFileUtils.h"

#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace NTEBuildTool::Json
{
bool LoadJsonObjectFromFile(const FString& Filename, TSharedPtr<FJsonObject>& OutObject, FString& OutError)
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Filename))
	{
		OutError = FString::Printf(TEXT("Could not read file: %s"), *Filename);
		return false;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
	{
		OutError = FString::Printf(TEXT("File is not a valid JSON object: %s"), *Filename);
		return false;
	}

	return true;
}

bool SaveJsonObjectToFile(const TSharedRef<FJsonObject>& Object, const FString& Filename, FString& OutError)
{
	FString JsonText;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(Object, Writer))
	{
		OutError = TEXT("Could not serialize JSON.");
		return false;
	}

	if (!FFileHelper::SaveStringToFile(JsonText, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("Could not write file: %s"), *Filename);
		return false;
	}

	return true;
}

FString GetStringAny(const FJsonObject& Object, const TCHAR* FirstName, const TCHAR* SecondName, const TCHAR* ThirdName)
{
	FString Value;
	if (FirstName && Object.TryGetStringField(FirstName, Value))
	{
		return Value;
	}
	if (SecondName && Object.TryGetStringField(SecondName, Value))
	{
		return Value;
	}
	if (ThirdName && Object.TryGetStringField(ThirdName, Value))
	{
		return Value;
	}

	return FString();
}

bool GetIntAny(const FJsonObject& Object, int32& OutValue, const TCHAR* FirstName, const TCHAR* SecondName)
{
	double Number = 0.0;
	if (FirstName && Object.TryGetNumberField(FirstName, Number))
	{
		OutValue = static_cast<int32>(Number);
		return true;
	}
	if (SecondName && Object.TryGetNumberField(SecondName, Number))
	{
		OutValue = static_cast<int32>(Number);
		return true;
	}

	return false;
}

bool GetBoolAny(const FJsonObject& Object, bool& OutValue, const TCHAR* FirstName, const TCHAR* SecondName)
{
	bool Value = false;
	if (FirstName && Object.TryGetBoolField(FirstName, Value))
	{
		OutValue = Value;
		return true;
	}
	if (SecondName && Object.TryGetBoolField(SecondName, Value))
	{
		OutValue = Value;
		return true;
	}

	return false;
}

bool TryGetObjectAny(const FJsonObject& Object, const TSharedPtr<FJsonObject>*& OutObject, const TCHAR* FirstName, const TCHAR* SecondName, const TCHAR* ThirdName)
{
	if (FirstName && Object.TryGetObjectField(FirstName, OutObject) && OutObject)
	{
		return true;
	}
	if (SecondName && Object.TryGetObjectField(SecondName, OutObject) && OutObject)
	{
		return true;
	}
	if (ThirdName && Object.TryGetObjectField(ThirdName, OutObject) && OutObject)
	{
		return true;
	}

	return false;
}

TArray<FString> GetStringArrayAny(const FJsonObject& Object, const TCHAR* FirstName, const TCHAR* SecondName)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!((FirstName && Object.TryGetArrayField(FirstName, Values)) || (SecondName && Object.TryGetArrayField(SecondName, Values))) || !Values)
	{
		return {};
	}

	TArray<FString> Result;
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (!Value.IsValid())
		{
			continue;
		}

		FString StringValue = Value->AsString();
		StringValue.TrimStartAndEndInline();
		if (!StringValue.IsEmpty())
		{
			Result.Add(StringValue);
		}
	}
	return Result;
}

TArray<TSharedPtr<FJsonValue>> StringArrayToJsonValues(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> JsonValues;
	for (const FString& Value : Values)
	{
		JsonValues.Add(MakeShared<FJsonValueString>(Value));
	}
	return JsonValues;
}
}
