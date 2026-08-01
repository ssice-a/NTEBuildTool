// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteCurveFloatJsonImporter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Curves/CurveFloat.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace NTEBuildTool::Character
{
namespace
{
ERichCurveInterpMode ParseInterpMode(const FString& Value)
{
	if (Value.Contains(TEXT("Cubic"), ESearchCase::IgnoreCase))
	{
		return RCIM_Cubic;
	}
	if (Value.Contains(TEXT("Constant"), ESearchCase::IgnoreCase))
	{
		return RCIM_Constant;
	}
	return RCIM_Linear;
}

ERichCurveTangentMode ParseTangentMode(const FString& Value)
{
	if (Value.Contains(TEXT("Break"), ESearchCase::IgnoreCase))
	{
		return RCTM_Break;
	}
	if (Value.Contains(TEXT("User"), ESearchCase::IgnoreCase))
	{
		return RCTM_User;
	}
	return RCTM_Auto;
}

ERichCurveTangentWeightMode ParseTangentWeightMode(const FString& Value)
{
	if (Value.Contains(TEXT("WeightedBoth"), ESearchCase::IgnoreCase))
	{
		return RCTWM_WeightedBoth;
	}
	if (Value.Contains(TEXT("WeightedArrive"), ESearchCase::IgnoreCase))
	{
		return RCTWM_WeightedArrive;
	}
	if (Value.Contains(TEXT("WeightedLeave"), ESearchCase::IgnoreCase))
	{
		return RCTWM_WeightedLeave;
	}
	return RCTWM_WeightedNone;
}

float GetFloat(const FJsonObject& Object, const TCHAR* FieldName)
{
	double Value = 0.0;
	Object.TryGetNumberField(FieldName, Value);
	return static_cast<float>(Value);
}

bool LoadRootArray(const FString& Filename, TArray<TSharedPtr<FJsonValue>>& OutValues, FString& OutError)
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Filename))
	{
		OutError = FString::Printf(TEXT("Could not read CurveFloat JSON: %s"), *Filename);
		return false;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, OutValues))
	{
		OutError = FString::Printf(TEXT("CurveFloat source is not a JSON array: %s"), *Filename);
		return false;
	}
	return true;
}

bool SaveCurve(UCurveFloat& Curve, FNteCurveFloatJsonImportResult& Result)
{
	UPackage* Package = Curve.GetPackage();
	Package->MarkPackageDirty();
	Curve.MarkPackageDirty();

	const FString PackageName = Package->GetName();
	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, &Curve, *Filename, SaveArgs))
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to save CurveFloat package: %s"), *PackageName));
		return false;
	}

	Result.SavedPackages.AddUnique(PackageName);
	return true;
}

void ImportCurveObject(const FJsonObject& Object, const FString& SourceFilename, FNteCurveFloatJsonImportResult& Result)
{
	FString Type;
	Object.TryGetStringField(TEXT("Type"), Type);
	if (Type != TEXT("CurveFloat"))
	{
		return;
	}

	FString PackagePath;
	Object.TryGetStringField(TEXT("Package"), PackagePath);
	if (!FPackageName::IsValidLongPackageName(PackagePath) || !PackagePath.StartsWith(TEXT("/Game/")))
	{
		Result.Errors.Add(FString::Printf(TEXT("CurveFloat JSON has an invalid Package path in %s: %s"), *SourceFilename, *PackagePath));
		return;
	}

	const TSharedPtr<FJsonObject>* Properties = nullptr;
	const TSharedPtr<FJsonObject>* FloatCurveObject = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* KeyValues = nullptr;
	if (!Object.TryGetObjectField(TEXT("Properties"), Properties) || !Properties ||
		!(*Properties)->TryGetObjectField(TEXT("FloatCurve"), FloatCurveObject) || !FloatCurveObject ||
		!(*FloatCurveObject)->TryGetArrayField(TEXT("Keys"), KeyValues) || !KeyValues)
	{
		Result.Errors.Add(FString::Printf(TEXT("CurveFloat JSON has no Properties.FloatCurve.Keys array: %s"), *SourceFilename));
		return;
	}

	TArray<FRichCurveKey> Keys;
	for (const TSharedPtr<FJsonValue>& KeyValue : *KeyValues)
	{
		const TSharedPtr<FJsonObject> KeyObject = KeyValue.IsValid() ? KeyValue->AsObject() : nullptr;
		if (!KeyObject.IsValid())
		{
			continue;
		}

		FRichCurveKey Key(GetFloat(*KeyObject, TEXT("Time")), GetFloat(*KeyObject, TEXT("Value")));
		FString EnumValue;
		KeyObject->TryGetStringField(TEXT("InterpMode"), EnumValue);
		Key.InterpMode = ParseInterpMode(EnumValue);
		KeyObject->TryGetStringField(TEXT("TangentMode"), EnumValue);
		Key.TangentMode = ParseTangentMode(EnumValue);
		KeyObject->TryGetStringField(TEXT("TangentWeightMode"), EnumValue);
		Key.TangentWeightMode = ParseTangentWeightMode(EnumValue);
		Key.ArriveTangent = GetFloat(*KeyObject, TEXT("ArriveTangent"));
		Key.ArriveTangentWeight = GetFloat(*KeyObject, TEXT("ArriveTangentWeight"));
		Key.LeaveTangent = GetFloat(*KeyObject, TEXT("LeaveTangent"));
		Key.LeaveTangentWeight = GetFloat(*KeyObject, TEXT("LeaveTangentWeight"));
		Keys.Add(Key);
	}

	const FString ObjectName = FPackageName::GetShortName(PackagePath);
	const FString ObjectPath = PackagePath + TEXT(".") + ObjectName;
	UCurveFloat* Curve = LoadObject<UCurveFloat>(nullptr, *ObjectPath);
	bool bCreated = false;
	if (!Curve)
	{
		UPackage* Package = CreatePackage(*PackagePath);
		Curve = NewObject<UCurveFloat>(Package, *ObjectName, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Curve);
		bCreated = true;
	}

	Curve->Modify();
	Curve->FloatCurve.SetKeys(Keys);
	if (SaveCurve(*Curve, Result) && bCreated)
	{
		Result.Warnings.Add(FString::Printf(TEXT("Created mirror-only source CurveFloat for Kawaii import: %s"), *PackagePath));
	}
}
}

FNteCurveFloatJsonImportResult ImportCurveFloatAssetsFromFModelJsonFiles(const TArray<FString>& SourceJsonFiles)
{
	FNteCurveFloatJsonImportResult Result;
	for (FString Filename : SourceJsonFiles)
	{
		Filename.TrimStartAndEndInline();
		Filename.TrimQuotesInline();
		FPaths::NormalizeFilename(Filename);
		if (Filename.IsEmpty())
		{
			continue;
		}

		TArray<TSharedPtr<FJsonValue>> Values;
		FString Error;
		if (!LoadRootArray(Filename, Values, Error))
		{
			Result.Errors.Add(Error);
			continue;
		}

		const int32 PreviousSavedCount = Result.SavedPackages.Num();
		for (const TSharedPtr<FJsonValue>& Value : Values)
		{
			const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
			if (Object.IsValid())
			{
				ImportCurveObject(*Object, Filename, Result);
			}
		}
		if (Result.SavedPackages.Num() == PreviousSavedCount && !Result.HasErrors())
		{
			Result.Warnings.Add(FString::Printf(TEXT("No CurveFloat export was found in: %s"), *Filename));
		}
	}
	return Result;
}
}
