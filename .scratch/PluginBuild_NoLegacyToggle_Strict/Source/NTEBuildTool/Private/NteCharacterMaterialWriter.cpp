// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteCharacterMaterialWriter.h"

#include "NteCharacterMaterialPlan.h"
#include "NteJsonFileUtils.h"
#include "NteMaterialInstanceTool.h"

#include "Dom/JsonValue.h"

namespace NTEBuildTool::Character
{
namespace
{
TSharedRef<FJsonObject> StringMapToJson(const TMap<FString, FString>& Map)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Pair : Map)
	{
		Object->SetStringField(Pair.Key, Pair.Value);
	}
	return Object;
}

void AddStringIfNotEmpty(const TSharedRef<FJsonObject>& Object, const TCHAR* FieldName, const FString& Value)
{
	if (!Value.IsEmpty())
	{
		Object->SetStringField(FieldName, Value);
	}
}

void AddWarning(FNteCharacterMaterialWriteResult& Result, FNteCharacterMaterialOperationWriteResult& Operation, const FString& Warning)
{
	Operation.Warnings.Add(Warning);
	Result.Warnings.Add(Warning);
}

void AddError(FNteCharacterMaterialWriteResult& Result, FNteCharacterMaterialOperationWriteResult& Operation, const FString& Error)
{
	Operation.Errors.Add(Error);
	Result.Errors.Add(Error);
}

FNteCharacterMaterialOperationWriteResult MakeOperationWriteResultShell(const FNteCharacterMaterialOperationPlanItem& Operation)
{
	FNteCharacterMaterialOperationWriteResult Result;
	Result.Id = Operation.Id;
	Result.TargetMeshPath = Operation.TargetMeshPath;
	Result.SlotIndex = Operation.SlotIndex;
	Result.SourceMaterialJson = Operation.SourceMaterialJson;
	Result.ParentMaterialPath = Operation.ParentMaterialPath;
	Result.OutputMaterialPath = Operation.OutputMaterialPath;
	Result.Warnings.Append(Operation.Warnings);
	Result.Errors.Append(Operation.Errors);
	return Result;
}

void CopySourceTextureUsage(
	const TArray<NTEBuildTool::Material::FNteMaterialSourceTextureUsage>& Source,
	TArray<FNteCharacterMaterialSourceTextureUsageItem>& Target)
{
	for (const NTEBuildTool::Material::FNteMaterialSourceTextureUsage& SourceItem : Source)
	{
		FNteCharacterMaterialSourceTextureUsageItem& TargetItem = Target.AddDefaulted_GetRef();
		TargetItem.SourceTexturePath = SourceItem.SourceTexturePath;
		TargetItem.ParameterNames = SourceItem.ParameterNames;
	}
}

TSharedRef<FJsonObject> SourceTextureUsageToJson(const FNteCharacterMaterialSourceTextureUsageItem& Item)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(TEXT("SourceTexturePath"), Item.SourceTexturePath);
	Object->SetArrayField(TEXT("ParameterNames"), Json::StringArrayToJsonValues(Item.ParameterNames));
	return Object;
}

TSharedRef<FJsonObject> OperationWriteResultToJson(const FNteCharacterMaterialOperationWriteResult& Result)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	AddStringIfNotEmpty(Object, TEXT("Id"), Result.Id);
	AddStringIfNotEmpty(Object, TEXT("TargetMeshPath"), Result.TargetMeshPath);
	Object->SetNumberField(TEXT("SlotIndex"), Result.SlotIndex);
	AddStringIfNotEmpty(Object, TEXT("SourceMaterialJson"), Result.SourceMaterialJson);
	AddStringIfNotEmpty(Object, TEXT("ParentMaterialPath"), Result.ParentMaterialPath);
	AddStringIfNotEmpty(Object, TEXT("OutputMaterialPath"), Result.OutputMaterialPath);
	AddStringIfNotEmpty(Object, TEXT("AssignedMaterialPath"), Result.AssignedMaterialPath);
	AddStringIfNotEmpty(Object, TEXT("ReportFilename"), Result.ReportFilename);
	Object->SetNumberField(TEXT("TextureOverrides"), Result.TextureOverrides);
	Object->SetNumberField(TEXT("SourceTextureOverrideGroups"), Result.SourceTextureOverrideGroups);
	Object->SetNumberField(TEXT("ScalarOverrides"), Result.ScalarOverrides);
	Object->SetNumberField(TEXT("VectorOverrides"), Result.VectorOverrides);
	Object->SetNumberField(TEXT("StaticSwitchOverrides"), Result.StaticSwitchOverrides);
	Object->SetBoolField(TEXT("CreatedParentPlaceholder"), Result.bCreatedParentPlaceholder);
	Object->SetBoolField(TEXT("CreatedMaterialProxy"), Result.bCreatedMaterialProxy);
	Object->SetBoolField(TEXT("Applied"), Result.bApplied);
	Object->SetArrayField(TEXT("MissingTextures"), Json::StringArrayToJsonValues(Result.MissingTextures));
	Object->SetArrayField(TEXT("UnmatchedSourceTextureOverrides"), Json::StringArrayToJsonValues(Result.UnmatchedSourceTextureOverrides));

	TArray<TSharedPtr<FJsonValue>> SourceTextureUsageValues;
	for (const FNteCharacterMaterialSourceTextureUsageItem& Item : Result.SourceTextureUsage)
	{
		SourceTextureUsageValues.Add(MakeShared<FJsonValueObject>(SourceTextureUsageToJson(Item)));
	}
	Object->SetArrayField(TEXT("SourceTextureUsage"), SourceTextureUsageValues);
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Result.Errors));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Result.Warnings));
	return Object;
}
}

FNteCharacterMaterialWriteResult WriteCharacterMaterials(const FNteCharacterMaterialPlan& Plan)
{
	FNteCharacterMaterialWriteResult Result;
	Result.Errors.Append(Plan.Errors);
	Result.Warnings.Append(Plan.Warnings);
	if (!Plan.Errors.IsEmpty())
	{
		return Result;
	}

	for (const FNteCharacterMaterialOperationPlanItem& Operation : Plan.Operations)
	{
		FNteCharacterMaterialOperationWriteResult OperationResult = MakeOperationWriteResultShell(Operation);
		if (!OperationResult.Errors.IsEmpty())
		{
			Result.Errors.Append(OperationResult.Errors);
			Result.Warnings.Append(OperationResult.Warnings);
			Result.Operations.Add(MoveTemp(OperationResult));
			continue;
		}

		NTEBuildTool::Material::FNteMaterialInstanceOptions Options;
		Options.SourceMaterialJson = Operation.SourceMaterialJson;
		Options.ParentMaterialPath = Operation.ParentMaterialPath;
		Options.OutputMaterialPath = Operation.OutputMaterialPath;
		Options.MeshPath = Operation.TargetMeshPath;
		Options.SlotIndex = Operation.SlotIndex;
		Options.bAssignToMeshSlot = Operation.bAssignToSlot;
		Options.bCopySourceParameters = false;
		Options.bCopySourceTextures = false;
		Options.bResetForPakTextureOnly = true;
		Options.bAllowStaticSwitchOverrides = false;
		Options.bEnsureParentPlaceholder = true;
		Options.bReplaceWrongParentPlaceholder = true;

		TSharedPtr<FJsonObject> SourceTextureOverrides;
		if (!Operation.SourceTextureOverrides.IsEmpty())
		{
			SourceTextureOverrides = StringMapToJson(Operation.SourceTextureOverrides);
		}

		NTEBuildTool::Material::FNteMaterialConfigApplyResult ApplyResult;
		FString Error;
		if (!NTEBuildTool::Material::ApplyModMaterialConfig(
			Options,
			SourceTextureOverrides.Get(),
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			ApplyResult,
			Error))
		{
			AddError(Result, OperationResult, FString::Printf(TEXT("Material operation '%s' failed: %s"), *Operation.Id, *Error));
			Result.Operations.Add(MoveTemp(OperationResult));
			continue;
		}

		OperationResult.bApplied = true;
		OperationResult.SourceMaterialJson = ApplyResult.SourceMaterialJson;
		OperationResult.ParentMaterialPath = ApplyResult.ParentMaterialPath;
		OperationResult.OutputMaterialPath = ApplyResult.OutputMaterialPath;
		OperationResult.AssignedMaterialPath = ApplyResult.CreateResult.AssignedMaterialPath;
		OperationResult.ReportFilename = ApplyResult.ReportFilename;
		OperationResult.TextureOverrides = ApplyResult.CreateResult.ApplySummary.TextureOverrides;
		OperationResult.SourceTextureOverrideGroups = ApplyResult.CreateResult.ApplySummary.SourceTextureOverrideGroups;
		OperationResult.ScalarOverrides = ApplyResult.CreateResult.ApplySummary.ScalarOverrides;
		OperationResult.VectorOverrides = ApplyResult.CreateResult.ApplySummary.VectorOverrides;
		OperationResult.StaticSwitchOverrides = ApplyResult.CreateResult.ApplySummary.StaticSwitchOverrides;
		OperationResult.bCreatedParentPlaceholder = ApplyResult.CreateResult.bCreatedParentPlaceholder;
		OperationResult.bCreatedMaterialProxy = ApplyResult.CreateResult.bCreatedMaterialProxy;
		OperationResult.MissingTextures = ApplyResult.CreateResult.ApplySummary.MissingTextures;
		OperationResult.UnmatchedSourceTextureOverrides = ApplyResult.CreateResult.ApplySummary.UnmatchedSourceTextureOverrides;
		CopySourceTextureUsage(ApplyResult.SourceTextureUsage, OperationResult.SourceTextureUsage);

		for (const FString& MissingTexture : OperationResult.MissingTextures)
		{
			AddWarning(
				Result,
				OperationResult,
				FString::Printf(TEXT("Material operation '%s' references missing texture asset: %s"), *Operation.Id, *MissingTexture));
		}
		for (const FString& UnmatchedSourceTexture : OperationResult.UnmatchedSourceTextureOverrides)
		{
			AddWarning(
				Result,
				OperationResult,
				FString::Printf(TEXT("Material operation '%s' did not find source texture in material JSON: %s"), *Operation.Id, *UnmatchedSourceTexture));
		}

		Result.Operations.Add(MoveTemp(OperationResult));
	}

	return Result;
}

TSharedRef<FJsonObject> CharacterMaterialWriteResultToJson(const FNteCharacterMaterialWriteResult& Result)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Operations;
	int32 AppliedCount = 0;
	for (const FNteCharacterMaterialOperationWriteResult& Operation : Result.Operations)
	{
		if (Operation.bApplied)
		{
			++AppliedCount;
		}
		Operations.Add(MakeShared<FJsonValueObject>(OperationWriteResultToJson(Operation)));
	}
	Object->SetNumberField(TEXT("OperationCount"), Result.Operations.Num());
	Object->SetNumberField(TEXT("AppliedCount"), AppliedCount);
	Object->SetNumberField(TEXT("ErrorCount"), Result.Errors.Num());
	Object->SetNumberField(TEXT("WarningCount"), Result.Warnings.Num());
	Object->SetArrayField(TEXT("Operations"), Operations);
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Result.Errors));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Result.Warnings));
	return Object;
}
}
