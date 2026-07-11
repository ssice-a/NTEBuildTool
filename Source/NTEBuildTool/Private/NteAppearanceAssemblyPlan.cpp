// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteAppearanceAssemblyPlan.h"

#include "NteCharacterModSpec.h"
#include "NteJsonFileUtils.h"

#include "Dom/JsonValue.h"

namespace NTEBuildTool::Character
{
namespace
{
TSharedRef<FJsonObject> VectorToJson(const FVector& Value)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("X"), Value.X);
	Object->SetNumberField(TEXT("Y"), Value.Y);
	Object->SetNumberField(TEXT("Z"), Value.Z);
	return Object;
}

TSharedRef<FJsonObject> RotatorToJson(const FRotator& Value)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("Pitch"), Value.Pitch);
	Object->SetNumberField(TEXT("Yaw"), Value.Yaw);
	Object->SetNumberField(TEXT("Roll"), Value.Roll);
	return Object;
}

void AddStringIfNotEmpty(const TSharedRef<FJsonObject>& Object, const TCHAR* FieldName, const FString& Value)
{
	if (!Value.IsEmpty())
	{
		Object->SetStringField(FieldName, Value);
	}
}

TSharedRef<FJsonObject> MeshDataPlanToJson(const FNteAppearanceMeshDataPlan& MeshData)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	AddStringIfNotEmpty(Object, TEXT("Id"), MeshData.Id);
	AddStringIfNotEmpty(Object, TEXT("Label"), MeshData.Label);
	AddStringIfNotEmpty(Object, TEXT("CharacterMeshPath"), MeshData.CharacterMeshPath);
	AddStringIfNotEmpty(Object, TEXT("AnimInstancePath"), MeshData.AnimInstancePath);
	AddStringIfNotEmpty(Object, TEXT("MobileAnimInstancePath"), MeshData.MobileAnimInstancePath);
	AddStringIfNotEmpty(Object, TEXT("UIAnimInstancePath"), MeshData.UIAnimInstancePath);
	AddStringIfNotEmpty(Object, TEXT("SocketName"), MeshData.SocketName);
	Object->SetObjectField(TEXT("RelativeLocation"), VectorToJson(MeshData.RelativeLocation));
	Object->SetObjectField(TEXT("RelativeRotation"), RotatorToJson(MeshData.RelativeRotation));
	Object->SetObjectField(TEXT("RelativeScale3D"), VectorToJson(MeshData.RelativeScale3D));
	Object->SetBoolField(TEXT("SyncToUIShow"), MeshData.bSyncToUIShow);
	Object->SetBoolField(TEXT("RuntimeActionsEnabled"), MeshData.bRuntimeActionsEnabled);
	return Object;
}

FNteAppearanceMeshDataPlan MainMeshPlanFromSpec(const FNteCharacterModSpec& Spec)
{
	FNteAppearanceMeshDataPlan MeshData;
	MeshData.Id = TEXT("main");
	MeshData.Label = TEXT("Main");
	MeshData.CharacterMeshPath = Spec.MainMeshPath;
	MeshData.AnimInstancePath = Spec.MainAnimBlueprintPath;
	MeshData.bSyncToUIShow = false;
	return MeshData;
}

FNteAppearanceMeshDataPlan AttachedMeshPlanFromSpec(const FNteCharacterAttachedMeshSpec& AttachedMesh)
{
	FNteAppearanceMeshDataPlan MeshData;
	MeshData.Id = AttachedMesh.Id;
	MeshData.Label = AttachedMesh.Label;
	MeshData.CharacterMeshPath = AttachedMesh.MeshPath;
	MeshData.AnimInstancePath = !AttachedMesh.RuntimeAnimBlueprintPath.IsEmpty()
		? AttachedMesh.RuntimeAnimBlueprintPath
		: AttachedMesh.AnimBlueprintPath;
	MeshData.MobileAnimInstancePath = AttachedMesh.MobileAnimBlueprintPath;
	MeshData.UIAnimInstancePath = AttachedMesh.UIAnimBlueprintPath;
	MeshData.SocketName = AttachedMesh.SocketName;
	MeshData.RelativeLocation = AttachedMesh.RelativeLocation;
	MeshData.RelativeRotation = AttachedMesh.RelativeRotation;
	MeshData.RelativeScale3D = AttachedMesh.RelativeScale;
	MeshData.bSyncToUIShow = AttachedMesh.bSyncToUIShow;
	MeshData.bRuntimeActionsEnabled = AttachedMesh.bEnableRuntimeActions;
	return MeshData;
}
}

FNteAppearanceAssemblyPlan BuildAppearanceAssemblyPlanFromSpec(const FNteCharacterModSpec& Spec)
{
	FNteAppearanceAssemblyPlan Plan;
	Plan.PlayerAppearanceAssetPath = Spec.Appearance.PlayerAppearanceAssetPath;
	Plan.UIActorClassPath = Spec.Appearance.UIActorClassPath;
	Plan.MainMesh = MainMeshPlanFromSpec(Spec);

	if (Plan.PlayerAppearanceAssetPath.IsEmpty())
	{
		Plan.Errors.Add(TEXT("PlayerAppearanceAssetPath is required for MeshAsset assembly."));
	}
	if (Plan.MainMesh.CharacterMeshPath.IsEmpty())
	{
		Plan.Errors.Add(TEXT("MainMeshPath is required for FashionMeshData."));
	}
	if (Plan.UIActorClassPath.IsEmpty())
	{
		Plan.Warnings.Add(TEXT("UIActorClassPath is empty; runtime MeshAsset can be planned, but PlayerUIShow preview sync cannot be planned."));
	}

	for (const FNteCharacterAttachedMeshSpec& AttachedMesh : Spec.AttachedMeshes)
	{
		FNteAppearanceMeshDataPlan AttachedPlan = AttachedMeshPlanFromSpec(AttachedMesh);
		if (AttachedPlan.Id.IsEmpty())
		{
			Plan.Errors.Add(TEXT("Attached mesh plan has an empty Id."));
		}
		if (AttachedPlan.CharacterMeshPath.IsEmpty())
		{
			Plan.Errors.Add(FString::Printf(TEXT("Attached mesh '%s' has no CharacterMeshPath."), *AttachedPlan.Id));
		}
		if (AttachedPlan.AnimInstancePath.IsEmpty())
		{
			Plan.Warnings.Add(FString::Printf(TEXT("Attached mesh '%s' has no AnimInstancePath; it will rely on component defaults unless an asset writer rejects it."), *AttachedPlan.Id));
		}
		if (AttachedPlan.SocketName.IsEmpty())
		{
			Plan.Warnings.Add(FString::Printf(TEXT("Attached mesh '%s' has no SocketName."), *AttachedPlan.Id));
		}
		if (AttachedPlan.bSyncToUIShow && Plan.UIActorClassPath.IsEmpty())
		{
			Plan.Warnings.Add(FString::Printf(TEXT("Attached mesh '%s' requests UIShow sync but UIActorClassPath is empty."), *AttachedPlan.Id));
		}
		Plan.AttachedMeshes.Add(MoveTemp(AttachedPlan));
	}

	return Plan;
}

TSharedRef<FJsonObject> AppearanceAssemblyPlanToJson(const FNteAppearanceAssemblyPlan& Plan)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	AddStringIfNotEmpty(Object, TEXT("PlayerAppearanceAssetPath"), Plan.PlayerAppearanceAssetPath);
	AddStringIfNotEmpty(Object, TEXT("UIActorClassPath"), Plan.UIActorClassPath);
	Object->SetObjectField(TEXT("MainMesh"), MeshDataPlanToJson(Plan.MainMesh));

	TArray<TSharedPtr<FJsonValue>> AttachedMeshes;
	for (const FNteAppearanceMeshDataPlan& AttachedMesh : Plan.AttachedMeshes)
	{
		AttachedMeshes.Add(MakeShared<FJsonValueObject>(MeshDataPlanToJson(AttachedMesh)));
	}
	Object->SetArrayField(TEXT("AttachedMeshes"), AttachedMeshes);
	Object->SetArrayField(TEXT("Errors"), Json::StringArrayToJsonValues(Plan.Errors));
	Object->SetArrayField(TEXT("Warnings"), Json::StringArrayToJsonValues(Plan.Warnings));
	return Object;
}
}
