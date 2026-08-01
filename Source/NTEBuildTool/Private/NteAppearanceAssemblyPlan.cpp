// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteAppearanceAssemblyPlan.h"

#include "NteCharacterKawaiiPlan.h"
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
	AddStringIfNotEmpty(Object, TEXT("PresentationSocketName"), MeshData.PresentationSocketName);
	Object->SetArrayField(TEXT("MeshComponentOwnedTags"), Json::StringArrayToJsonValues(MeshData.MeshComponentOwnedTags));
	Object->SetArrayField(TEXT("PresentationTargetIds"), Json::StringArrayToJsonValues(MeshData.PresentationTargetIds));
	Object->SetObjectField(TEXT("RelativeLocation"), VectorToJson(MeshData.RelativeLocation));
	Object->SetObjectField(TEXT("RelativeRotation"), RotatorToJson(MeshData.RelativeRotation));
	Object->SetObjectField(TEXT("RelativeScale3D"), VectorToJson(MeshData.RelativeScale3D));
	Object->SetBoolField(TEXT("SyncToUIShow"), MeshData.bSyncToUIShow);
	Object->SetBoolField(TEXT("RuntimeActionsEnabled"), MeshData.bRuntimeActionsEnabled);
	return Object;
}

TSharedRef<FJsonObject> PresentationTargetPlanToJson(const FNteAppearancePresentationTargetPlan& Target)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	AddStringIfNotEmpty(Object, TEXT("Id"), Target.Id);
	AddStringIfNotEmpty(Object, TEXT("BlueprintClassPath"), Target.BlueprintClassPath);
	AddStringIfNotEmpty(Object, TEXT("ParentMeshComponentName"), Target.ParentMeshComponentName);
	AddStringIfNotEmpty(Object, TEXT("MainAnimInstancePath"), Target.MainAnimInstancePath);
	Object->SetBoolField(TEXT("ConfigureMainMesh"), Target.bConfigureMainMesh);
	return Object;
}

TSharedRef<FJsonObject> NPCAppearanceTargetPlanToJson(const FNteNPCAppearanceTargetPlan& Target)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	AddStringIfNotEmpty(Object, TEXT("AssetPath"), Target.AssetPath);
	AddStringIfNotEmpty(Object, TEXT("MainAnimInstancePath"), Target.MainAnimInstancePath);
	return Object;
}

FNteAppearanceMeshDataPlan MainMeshPlanFromSpec(const FNteCharacterModSpec& Spec)
{
	FNteAppearanceMeshDataPlan MeshData;
	MeshData.Id = TEXT("main");
	MeshData.Label = TEXT("Main");
	MeshData.CharacterMeshPath = Spec.MainMeshPath;
	MeshData.AnimInstancePath = Spec.MainAnimBlueprintPath;
	MeshData.UIAnimInstancePath = Spec.Appearance.MainUIAnimBlueprintPath;
	MeshData.bSyncToUIShow = false;
	return MeshData;
}

FString ResolveKawaiiRuntimeAnimBlueprintPath(
	const FNteCharacterAttachedMeshSpec& AttachedMesh,
	const FNteCharacterKawaiiPlan& KawaiiPlan)
{
	for (const FNteCharacterKawaiiPresetPlanItem& Preset : KawaiiPlan.Presets)
	{
		const bool bMatchesExplicitPreset = !AttachedMesh.KawaiiPresetId.IsEmpty() && Preset.Id == AttachedMesh.KawaiiPresetId;
		const bool bMatchesTargetMesh = AttachedMesh.KawaiiPresetId.IsEmpty() && Preset.TargetMeshId == AttachedMesh.Id;
		if ((bMatchesExplicitPreset || bMatchesTargetMesh) && !Preset.RuntimeAnimBlueprintPath.IsEmpty())
		{
			return Preset.RuntimeAnimBlueprintPath;
		}
	}
	return FString();
}

FNteAppearanceMeshDataPlan AttachedMeshPlanFromSpec(
	const FNteCharacterAttachedMeshSpec& AttachedMesh,
	const FNteCharacterKawaiiPlan& KawaiiPlan)
{
	FNteAppearanceMeshDataPlan MeshData;
	MeshData.Id = AttachedMesh.Id;
	MeshData.Label = AttachedMesh.Label;
	MeshData.CharacterMeshPath = AttachedMesh.MeshPath;
	const FString KawaiiRuntimeAnimBlueprintPath = ResolveKawaiiRuntimeAnimBlueprintPath(AttachedMesh, KawaiiPlan);
	MeshData.AnimInstancePath = !AttachedMesh.RuntimeAnimBlueprintPath.IsEmpty()
		? AttachedMesh.RuntimeAnimBlueprintPath
		: (!KawaiiRuntimeAnimBlueprintPath.IsEmpty() ? KawaiiRuntimeAnimBlueprintPath : AttachedMesh.AnimBlueprintPath);
	MeshData.MobileAnimInstancePath = AttachedMesh.MobileAnimBlueprintPath;
	MeshData.UIAnimInstancePath = AttachedMesh.UIAnimBlueprintPath;
	MeshData.SocketName = AttachedMesh.SocketName;
	MeshData.PresentationSocketName = AttachedMesh.PresentationSocketName;
	MeshData.MeshComponentOwnedTags = AttachedMesh.MeshComponentOwnedTags;
	MeshData.PresentationTargetIds = AttachedMesh.PresentationTargetIds;
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
	const FNteCharacterKawaiiPlan KawaiiPlan = BuildCharacterKawaiiPlanFromSpec(Spec);
	Plan.PlayerAppearanceAssetPath = Spec.Appearance.PlayerAppearanceAssetPath;
	for (const FNteCharacterNPCAppearanceTargetSpec& SpecTarget : Spec.Appearance.NPCAppearanceTargets)
	{
		FNteNPCAppearanceTargetPlan Target;
		Target.AssetPath = SpecTarget.AssetPath;
		Target.MainAnimInstancePath = SpecTarget.MainAnimBlueprintPath;
		Plan.NPCAppearanceTargets.Add(MoveTemp(Target));
	}
	Plan.CapsuleHalfHeight = Spec.Appearance.CapsuleHalfHeight;
	Plan.CapsuleRadius = Spec.Appearance.CapsuleRadius;
	Plan.RelativeLocation = Spec.Appearance.RelativeLocation;
	Plan.MainMesh = MainMeshPlanFromSpec(Spec);
	for (const FNteCharacterPresentationTargetSpec& SpecTarget : Spec.Appearance.PresentationTargets)
	{
		FNteAppearancePresentationTargetPlan Target;
		Target.Id = SpecTarget.Id;
		Target.BlueprintClassPath = SpecTarget.BlueprintClassPath;
		Target.ParentMeshComponentName = SpecTarget.ParentMeshComponentName;
		Target.MainAnimInstancePath = SpecTarget.MainAnimBlueprintPath;
		Target.bConfigureMainMesh = SpecTarget.bConfigureMainMesh;
		Plan.PresentationTargets.Add(MoveTemp(Target));
	}
	if (Plan.PresentationTargets.IsEmpty() && !Spec.Appearance.UIActorClassPath.IsEmpty())
	{
		FNteAppearancePresentationTargetPlan LegacyUiTarget;
		LegacyUiTarget.Id = TEXT("ui");
		LegacyUiTarget.BlueprintClassPath = Spec.Appearance.UIActorClassPath;
		LegacyUiTarget.ParentMeshComponentName = TEXT("Mesh");
		LegacyUiTarget.MainAnimInstancePath = Spec.Appearance.MainUIAnimBlueprintPath;
		Plan.PresentationTargets.Add(MoveTemp(LegacyUiTarget));
	}

	if (Plan.PlayerAppearanceAssetPath.IsEmpty())
	{
		Plan.Errors.Add(TEXT("PlayerAppearanceAssetPath is required for MeshAsset assembly."));
	}
	if (Plan.MainMesh.CharacterMeshPath.IsEmpty())
	{
		Plan.Errors.Add(TEXT("MainMeshPath is required for FashionMeshData."));
	}
	for (const FNteNPCAppearanceTargetPlan& Target : Plan.NPCAppearanceTargets)
	{
		if (Target.AssetPath.IsEmpty())
		{
			Plan.Errors.Add(TEXT("NPC appearance target has an empty AssetPath."));
		}
		if (Target.MainAnimInstancePath.IsEmpty())
		{
			Plan.Errors.Add(FString::Printf(TEXT("NPC appearance target '%s' has no main AnimInstancePath."), *Target.AssetPath));
		}
	}
	if (Plan.PresentationTargets.IsEmpty())
	{
		Plan.Warnings.Add(TEXT("No presentation targets are configured; only the runtime MeshAsset can be written."));
	}

	for (const FNteCharacterAttachedMeshSpec& AttachedMesh : Spec.AttachedMeshes)
	{
		FNteAppearanceMeshDataPlan AttachedPlan = AttachedMeshPlanFromSpec(AttachedMesh, KawaiiPlan);
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
		AttachedPlan.SocketName.TrimStartAndEndInline();
		if (AttachedPlan.SocketName.IsEmpty() || FName(*AttachedPlan.SocketName).IsNone())
		{
			Plan.Errors.Add(FString::Printf(TEXT("Attached mesh '%s' has no valid SocketName; native ArrayFashionAttachedMeshData entries require an explicit mount bone or socket, and NAME_None is not valid."), *AttachedPlan.Id));
		}
		Plan.AttachedMeshes.Add(MoveTemp(AttachedPlan));
	}

	return Plan;
}

TSharedRef<FJsonObject> AppearanceAssemblyPlanToJson(const FNteAppearanceAssemblyPlan& Plan)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	AddStringIfNotEmpty(Object, TEXT("PlayerAppearanceAssetPath"), Plan.PlayerAppearanceAssetPath);
	TArray<TSharedPtr<FJsonValue>> NPCAppearanceTargets;
	for (const FNteNPCAppearanceTargetPlan& Target : Plan.NPCAppearanceTargets)
	{
		NPCAppearanceTargets.Add(MakeShared<FJsonValueObject>(NPCAppearanceTargetPlanToJson(Target)));
	}
	Object->SetArrayField(TEXT("NPCAppearanceTargets"), NPCAppearanceTargets);
	TArray<TSharedPtr<FJsonValue>> PresentationTargets;
	for (const FNteAppearancePresentationTargetPlan& Target : Plan.PresentationTargets)
	{
		PresentationTargets.Add(MakeShared<FJsonValueObject>(PresentationTargetPlanToJson(Target)));
	}
	Object->SetArrayField(TEXT("PresentationTargets"), PresentationTargets);
	if (Plan.CapsuleHalfHeight.IsSet())
	{
		Object->SetNumberField(TEXT("CapsuleHalfHeight"), Plan.CapsuleHalfHeight.GetValue());
	}
	if (Plan.CapsuleRadius.IsSet())
	{
		Object->SetNumberField(TEXT("CapsuleRadius"), Plan.CapsuleRadius.GetValue());
	}
	if (Plan.RelativeLocation.IsSet())
	{
		Object->SetObjectField(TEXT("RelativeLocation"), VectorToJson(Plan.RelativeLocation.GetValue()));
	}
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
