// Copyright (c) 2026 NTEBuildTool contributors.

#include "NteCharacterKawaiiAssetSync.h"

#include "NteEditorAssetUtils.h"

#include "Animation/AnimBlueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

namespace NTEBuildTool::Character
{
namespace
{
constexpr const TCHAR* GeneratedKawaiiNodeComment = TEXT("NTE Character Kawaii Generated");

void AddUpdated(FNteCharacterKawaiiAssetSyncResult& Result, const TCHAR* FieldName)
{
	Result.UpdatedFields.AddUnique(FieldName);
}

FProperty* FindProperty(UStruct* Struct, const TCHAR* FieldName)
{
	if (!Struct)
	{
		return nullptr;
	}

	for (TFieldIterator<FProperty> It(Struct, EFieldIterationFlags::IncludeSuper); It; ++It)
	{
		FProperty* Property = *It;
		if (Property && Property->GetAuthoredName() == FieldName)
		{
			return Property;
		}
	}
	return nullptr;
}

FProperty* FindPropertyByAnyName(UStruct* Struct, const TArray<FName>& FieldNames)
{
	for (const FName FieldName : FieldNames)
	{
		if (FProperty* Property = FindProperty(Struct, *FieldName.ToString()))
		{
			return Property;
		}
	}
	return nullptr;
}

bool TryGetKawaiiNodeStruct(UEdGraphNode& GraphNode, void*& OutContainer, UScriptStruct*& OutStruct)
{
	if (FStructProperty* NodeProperty = CastField<FStructProperty>(FindProperty(GraphNode.GetClass(), TEXT("Node"))))
	{
		OutContainer = NodeProperty->ContainerPtrToValuePtr<void>(&GraphNode);
		OutStruct = NodeProperty->Struct;
		return OutContainer && OutStruct;
	}
	return false;
}

UEdGraph* FindAnimGraph(UAnimBlueprint& AnimBlueprint)
{
	for (UEdGraph* Graph : AnimBlueprint.FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == FName(TEXT("AnimGraph")))
		{
			return Graph;
		}
	}
	return nullptr;
}

bool IsKawaiiGraphNode(const UEdGraphNode* Node)
{
	return Node && Node->GetClass() && Node->GetClass()->GetName() == TEXT("AnimGraphNode_KawaiiPhysics");
}

FString ReadBoneReferenceName(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName)
{
	FStructProperty* BoneProperty = CastField<FStructProperty>(FindProperty(ContainerStruct, FieldName));
	if (!BoneProperty || !BoneProperty->Struct)
	{
		return FString();
	}

	void* BoneContainer = BoneProperty->ContainerPtrToValuePtr<void>(Container);
	FNameProperty* BoneNameProperty = CastField<FNameProperty>(FindProperty(BoneProperty->Struct, TEXT("BoneName")));
	return BoneNameProperty ? BoneNameProperty->GetPropertyValue_InContainer(BoneContainer).ToString() : FString();
}

TArray<FString> ReadBoneReferenceArray(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName)
{
	TArray<FString> Result;
	FArrayProperty* ArrayProperty = CastField<FArrayProperty>(FindProperty(ContainerStruct, FieldName));
	FStructProperty* StructProperty = ArrayProperty ? CastField<FStructProperty>(ArrayProperty->Inner) : nullptr;
	if (!ArrayProperty || !StructProperty || !StructProperty->Struct)
	{
		return Result;
	}

	FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Container));
	for (int32 Index = 0; Index < Helper.Num(); ++Index)
	{
		void* Element = Helper.GetRawPtr(Index);
		FNameProperty* BoneNameProperty = CastField<FNameProperty>(FindProperty(StructProperty->Struct, TEXT("BoneName")));
		if (!BoneNameProperty)
		{
			continue;
		}

		const FString BoneName = BoneNameProperty->GetPropertyValue_InContainer(Element).ToString();
		if (!BoneName.IsEmpty() && BoneName != TEXT("None"))
		{
			Result.Add(BoneName);
		}
	}
	return Result;
}

TArray<FString> ReadNameArray(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName)
{
	TArray<FString> Result;
	FArrayProperty* ArrayProperty = CastField<FArrayProperty>(FindProperty(ContainerStruct, FieldName));
	FNameProperty* NameProperty = ArrayProperty ? CastField<FNameProperty>(ArrayProperty->Inner) : nullptr;
	if (!ArrayProperty || !NameProperty)
	{
		return Result;
	}

	FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Container));
	for (int32 Index = 0; Index < Helper.Num(); ++Index)
	{
		const FString Name = NameProperty->GetPropertyValue(Helper.GetRawPtr(Index)).ToString();
		if (!Name.IsEmpty() && Name != TEXT("None"))
		{
			Result.Add(Name);
		}
	}
	return Result;
}

bool ReadBool(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName, bool& OutValue)
{
	if (FBoolProperty* Property = CastField<FBoolProperty>(FindProperty(ContainerStruct, FieldName)))
	{
		OutValue = Property->GetPropertyValue_InContainer(Container);
		return true;
	}
	return false;
}

bool ReadFloat(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName, float& OutValue)
{
	if (FNumericProperty* Property = CastField<FNumericProperty>(FindProperty(ContainerStruct, FieldName)))
	{
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
		if (Property->IsFloatingPoint())
		{
			OutValue = static_cast<float>(Property->GetFloatingPointPropertyValue(ValuePtr));
			return true;
		}
		if (Property->IsInteger())
		{
			OutValue = static_cast<float>(Property->GetSignedIntPropertyValue(ValuePtr));
			return true;
		}
	}
	return false;
}

bool ReadInt(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName, int32& OutValue)
{
	if (FNumericProperty* Property = CastField<FNumericProperty>(FindProperty(ContainerStruct, FieldName)))
	{
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
		if (Property->IsInteger())
		{
			OutValue = static_cast<int32>(Property->GetSignedIntPropertyValue(ValuePtr));
			return true;
		}
		if (Property->IsFloatingPoint())
		{
			OutValue = static_cast<int32>(Property->GetFloatingPointPropertyValue(ValuePtr));
			return true;
		}
	}
	return false;
}

bool ReadVectorByAnyName(void* Container, UStruct* ContainerStruct, const TArray<FName>& FieldNames, FVector& OutValue)
{
	if (FStructProperty* Property = CastField<FStructProperty>(FindPropertyByAnyName(ContainerStruct, FieldNames)))
	{
		if (Property->Struct == TBaseStructure<FVector>::Get())
		{
			OutValue = *Property->ContainerPtrToValuePtr<FVector>(Container);
			return true;
		}
	}
	return false;
}

bool ReadRotator(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName, FRotator& OutValue)
{
	if (FStructProperty* Property = CastField<FStructProperty>(FindProperty(ContainerStruct, FieldName)))
	{
		if (Property->Struct == TBaseStructure<FRotator>::Get())
		{
			OutValue = *Property->ContainerPtrToValuePtr<FRotator>(Container);
			return true;
		}
	}
	return false;
}

bool ReadPlane(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName, FPlane& OutValue)
{
	if (FStructProperty* Property = CastField<FStructProperty>(FindProperty(ContainerStruct, FieldName)))
	{
		if (Property->Struct == TBaseStructure<FPlane>::Get())
		{
			OutValue = *Property->ContainerPtrToValuePtr<FPlane>(Container);
			return true;
		}
	}
	return false;
}

FString ReadEnum(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName)
{
	FProperty* Property = FindProperty(ContainerStruct, FieldName);
	UEnum* Enum = nullptr;
	int64 Value = INDEX_NONE;
	if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		Enum = EnumProperty->GetEnum();
		Value = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(
			EnumProperty->ContainerPtrToValuePtr<void>(Container));
	}
	else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		Enum = ByteProperty->Enum;
		Value = ByteProperty->GetSignedIntPropertyValue(ByteProperty->ContainerPtrToValuePtr<void>(Container));
	}

	if (!Enum || Value == INDEX_NONE)
	{
		return FString();
	}

	FString Name = Enum->GetNameStringByValue(Value);
	Name.Split(TEXT("::"), nullptr, &Name, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	return Name;
}

FString ReadObjectPackagePath(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName)
{
	FProperty* Property = FindProperty(ContainerStruct, FieldName);
	if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
	{
		UObject* Object = ObjectProperty->GetObjectPropertyValue_InContainer(Container);
		return NTEBuildTool::Editor::GetAssetPackagePath(Object);
	}
	if (FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
	{
		const FSoftObjectPtr SoftObjectPtr = SoftObjectProperty->GetPropertyValue_InContainer(Container);
		return SoftObjectPtr.ToSoftObjectPath().GetLongPackageName();
	}
	return FString();
}

FString ReadGameplayTagName(void* Container, UStruct* ContainerStruct, const TCHAR* FieldName)
{
	FStructProperty* TagProperty = CastField<FStructProperty>(FindProperty(ContainerStruct, FieldName));
	if (!TagProperty || !TagProperty->Struct)
	{
		return FString();
	}

	void* TagContainer = TagProperty->ContainerPtrToValuePtr<void>(Container);
	FNameProperty* TagNameProperty = CastField<FNameProperty>(FindProperty(TagProperty->Struct, TEXT("TagName")));
	const FString TagName = TagNameProperty ? TagNameProperty->GetPropertyValue_InContainer(TagContainer).ToString() : FString();
	return TagName == TEXT("None") ? FString() : TagName;
}

TArray<FNteCharacterKawaiiAdditionalRootBoneSpec> ReadAdditionalRootBones(void* Container, UStruct* ContainerStruct)
{
	TArray<FNteCharacterKawaiiAdditionalRootBoneSpec> Result;
	FArrayProperty* ArrayProperty = CastField<FArrayProperty>(FindProperty(ContainerStruct, TEXT("AdditionalRootBones")));
	FStructProperty* StructProperty = ArrayProperty ? CastField<FStructProperty>(ArrayProperty->Inner) : nullptr;
	if (!ArrayProperty || !StructProperty || !StructProperty->Struct)
	{
		return Result;
	}

	FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Container));
	for (int32 Index = 0; Index < Helper.Num(); ++Index)
	{
		void* Element = Helper.GetRawPtr(Index);
		FNteCharacterKawaiiAdditionalRootBoneSpec Root;
		Root.RootBone = ReadBoneReferenceName(Element, StructProperty->Struct, TEXT("RootBone"));
		ReadBool(Element, StructProperty->Struct, TEXT("bUseOverrideExcludeBones"), Root.bUseOverrideExcludeBones);
		Root.OverrideExcludeBones = ReadBoneReferenceArray(Element, StructProperty->Struct, TEXT("OverrideExcludeBones"));
		if (!Root.RootBone.IsEmpty() && Root.RootBone != TEXT("None"))
		{
			Result.Add(MoveTemp(Root));
		}
	}
	return Result;
}

void ReadPhysicsSettings(
	void* NodeContainer,
	UStruct* NodeStruct,
	FNteCharacterKawaiiPhysicsSettingsSpec& OutSettings,
	FNteCharacterKawaiiAssetSyncResult& Result)
{
	FStructProperty* SettingsProperty = CastField<FStructProperty>(FindProperty(NodeStruct, TEXT("PhysicsSettings")));
	if (!SettingsProperty || !SettingsProperty->Struct)
	{
		Result.Warnings.Add(TEXT("Kawaii node has no PhysicsSettings struct."));
		return;
	}

	void* SettingsContainer = SettingsProperty->ContainerPtrToValuePtr<void>(NodeContainer);
	ReadFloat(SettingsContainer, SettingsProperty->Struct, TEXT("Damping"), OutSettings.Damping);
	ReadFloat(SettingsContainer, SettingsProperty->Struct, TEXT("Stiffness"), OutSettings.Stiffness);
	ReadFloat(SettingsContainer, SettingsProperty->Struct, TEXT("WorldDampingLocation"), OutSettings.WorldDampingLocation);
	ReadFloat(SettingsContainer, SettingsProperty->Struct, TEXT("WorldDampingRotation"), OutSettings.WorldDampingRotation);
	ReadFloat(SettingsContainer, SettingsProperty->Struct, TEXT("Radius"), OutSettings.Radius);
	ReadFloat(SettingsContainer, SettingsProperty->Struct, TEXT("LimitAngle"), OutSettings.LimitAngle);
	if (ReadFloat(SettingsContainer, SettingsProperty->Struct, TEXT("ForwardMoveOffset"), OutSettings.ForwardMoveOffset))
	{
		OutSettings.bHasForwardMoveOffset = true;
	}
}

FString ReadRuntimeFloatCurveExternalPath(void* NodeContainer, UStruct* NodeStruct, const TCHAR* FieldName)
{
	FStructProperty* CurveProperty = CastField<FStructProperty>(FindProperty(NodeStruct, FieldName));
	if (!CurveProperty || !CurveProperty->Struct)
	{
		return FString();
	}

	void* CurveContainer = CurveProperty->ContainerPtrToValuePtr<void>(NodeContainer);
	return ReadObjectPackagePath(CurveContainer, CurveProperty->Struct, TEXT("ExternalCurve"));
}

void UpsertCurvePath(TArray<FNteCharacterKawaiiCurveSpec>& Curves, const FString& CurveKind, const FString& CurvePath)
{
	if (CurvePath.IsEmpty())
	{
		return;
	}

	for (FNteCharacterKawaiiCurveSpec& Curve : Curves)
	{
		if (Curve.CurveKind.Equals(CurveKind, ESearchCase::IgnoreCase))
		{
			Curve.ExternalCurveObjectName = FPackageName::GetShortName(CurvePath);
			Curve.ExternalCurveObjectPath = CurvePath;
			Curve.OutputCurvePath = CurvePath;
			return;
		}
	}

	FNteCharacterKawaiiCurveSpec Curve;
	Curve.CurveKind = CurveKind;
	Curve.ExternalCurveObjectName = FPackageName::GetShortName(CurvePath);
	Curve.ExternalCurveObjectPath = CurvePath;
	Curve.OutputCurvePath = CurvePath;
	Curves.Add(MoveTemp(Curve));
}

void ReadCurvePaths(void* NodeContainer, UStruct* NodeStruct, FNteCharacterKawaiiPresetSpec& Preset)
{
	UpsertCurvePath(Preset.Curves, TEXT("Damping"), ReadRuntimeFloatCurveExternalPath(NodeContainer, NodeStruct, TEXT("DampingCurveData")));
	UpsertCurvePath(Preset.Curves, TEXT("Stiffness"), ReadRuntimeFloatCurveExternalPath(NodeContainer, NodeStruct, TEXT("StiffnessCurveData")));
	UpsertCurvePath(Preset.Curves, TEXT("WorldDampingLocation"), ReadRuntimeFloatCurveExternalPath(NodeContainer, NodeStruct, TEXT("WorldDampingLocationCurveData")));
	UpsertCurvePath(Preset.Curves, TEXT("WorldDampingRotation"), ReadRuntimeFloatCurveExternalPath(NodeContainer, NodeStruct, TEXT("WorldDampingRotationCurveData")));
	UpsertCurvePath(Preset.Curves, TEXT("Radius"), ReadRuntimeFloatCurveExternalPath(NodeContainer, NodeStruct, TEXT("RadiusCurveData")));
	UpsertCurvePath(Preset.Curves, TEXT("LimitAngle"), ReadRuntimeFloatCurveExternalPath(NodeContainer, NodeStruct, TEXT("LimitAngleCurveData")));
}

UEdGraphNode* FindPresetKawaiiNode(UAnimBlueprint& AnimBlueprint, const FNteCharacterKawaiiPresetPlanItem& PlanItem)
{
	UEdGraph* AnimGraph = FindAnimGraph(AnimBlueprint);
	if (!AnimGraph)
	{
		return nullptr;
	}

	const FString ExpectedComment = FString::Printf(TEXT("%s:%s"), GeneratedKawaiiNodeComment, *PlanItem.Id);
	UEdGraphNode* RootFallback = nullptr;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (!IsKawaiiGraphNode(Node))
		{
			continue;
		}
		if (Node->NodeComment.Equals(ExpectedComment, ESearchCase::IgnoreCase))
		{
			return Node;
		}

		void* NodeContainer = nullptr;
		UScriptStruct* NodeStruct = nullptr;
		if (!RootFallback && TryGetKawaiiNodeStruct(*Node, NodeContainer, NodeStruct))
		{
			const FString RootBone = ReadBoneReferenceName(NodeContainer, NodeStruct, TEXT("RootBone"));
			if (!RootBone.IsEmpty() && RootBone == PlanItem.RootBone)
			{
				RootFallback = Node;
			}
		}
	}
	return RootFallback;
}

void ReadCommonLimitFields(
	void* LimitContainer,
	UStruct* LimitStruct,
	FNteCharacterKawaiiLimitSpec& OutLimit)
{
	OutLimit.DrivingBone = ReadBoneReferenceName(LimitContainer, LimitStruct, TEXT("DrivingBone"));
	ReadVectorByAnyName(LimitContainer, LimitStruct, {TEXT("OffSetLocation"), TEXT("OffsetLocation")}, OutLimit.OffsetLocation);
	ReadRotator(LimitContainer, LimitStruct, TEXT("OffsetRotation"), OutLimit.OffsetRotation);
	ReadBool(LimitContainer, LimitStruct, TEXT("bEnable"), OutLimit.bEnable);
	OutLimit.SourceType = ReadEnum(LimitContainer, LimitStruct, TEXT("SourceType"));
}

void ReadLimitArray(UObject& DataAsset, const TCHAR* ArrayName, const FString& LimitKind, TArray<FNteCharacterKawaiiLimitSpec>& OutLimits)
{
	FArrayProperty* ArrayProperty = CastField<FArrayProperty>(FindProperty(DataAsset.GetClass(), ArrayName));
	FStructProperty* StructProperty = ArrayProperty ? CastField<FStructProperty>(ArrayProperty->Inner) : nullptr;
	if (!ArrayProperty || !StructProperty || !StructProperty->Struct)
	{
		return;
	}

	FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(&DataAsset));
	for (int32 Index = 0; Index < Helper.Num(); ++Index)
	{
		void* Element = Helper.GetRawPtr(Index);
		FNteCharacterKawaiiLimitSpec Limit;
		Limit.LimitKind = LimitKind;
		ReadCommonLimitFields(Element, StructProperty->Struct, Limit);
		ReadFloat(Element, StructProperty->Struct, TEXT("Radius"), Limit.Radius);
		ReadFloat(Element, StructProperty->Struct, TEXT("Length"), Limit.Length);
		ReadFloat(Element, StructProperty->Struct, TEXT("SphereRadius"), Limit.SphereRadius);
		ReadVectorByAnyName(Element, StructProperty->Struct, {TEXT("Extent")}, Limit.Extent);
		ReadPlane(Element, StructProperty->Struct, TEXT("Plane"), Limit.Plane);
		Limit.LimitType = ReadEnum(Element, StructProperty->Struct, TEXT("LimitType"));
		OutLimits.Add(MoveTemp(Limit));
	}
}

int32 GetArrayLength(UObject& Object, const TCHAR* ArrayName)
{
	FArrayProperty* ArrayProperty = CastField<FArrayProperty>(FindProperty(Object.GetClass(), ArrayName));
	if (!ArrayProperty)
	{
		return 0;
	}
	FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(&Object));
	return Helper.Num();
}

FString RestoreSourceBoneName(const FString& BoneName, const TMap<FString, FString>& TargetToSource)
{
	if (const FString* SourceBone = TargetToSource.Find(BoneName))
	{
		return *SourceBone;
	}
	return BoneName;
}

void RestoreSourceBoneNames(TArray<FString>& BoneNames, const TMap<FString, FString>& TargetToSource)
{
	for (FString& BoneName : BoneNames)
	{
		BoneName = RestoreSourceBoneName(BoneName, TargetToSource);
	}
}

void RestoreSourceBoneReferences(FNteCharacterKawaiiPresetSpec& Preset)
{
	TMap<FString, FString> TargetToSource;
	for (const FNteCharacterKawaiiBoneRemapSpec& Remap : Preset.BoneRemaps)
	{
		if (!Remap.SourceBone.IsEmpty() && !Remap.TargetBone.IsEmpty() && !TargetToSource.Contains(Remap.TargetBone))
		{
			TargetToSource.Add(Remap.TargetBone, Remap.SourceBone);
		}
	}

	Preset.RootBone = RestoreSourceBoneName(Preset.RootBone, TargetToSource);
	RestoreSourceBoneNames(Preset.ExcludeBones, TargetToSource);
	for (FNteCharacterKawaiiAdditionalRootBoneSpec& AdditionalRootBone : Preset.AdditionalRootBones)
	{
		AdditionalRootBone.RootBone = RestoreSourceBoneName(AdditionalRootBone.RootBone, TargetToSource);
		RestoreSourceBoneNames(AdditionalRootBone.OverrideExcludeBones, TargetToSource);
	}
	for (FNteCharacterKawaiiLimitSpec& Limit : Preset.CollisionLimits)
	{
		Limit.DrivingBone = RestoreSourceBoneName(Limit.DrivingBone, TargetToSource);
	}
	RestoreSourceBoneNames(Preset.IgnoreBones, TargetToSource);
}

void ReadLimitsDataAsset(
	const FString& LimitsDataAssetPath,
	FNteCharacterKawaiiPresetSpec& Preset,
	FNteCharacterKawaiiAssetSyncResult& Result)
{
	if (LimitsDataAssetPath.IsEmpty())
	{
		return;
	}

	UObject* DataAsset = NTEBuildTool::Editor::LoadAnyAssetByPath(LimitsDataAssetPath);
	if (!DataAsset)
	{
		Result.Warnings.Add(FString::Printf(TEXT("Could not load Kawaii limits data asset: %s"), *LimitsDataAssetPath));
		return;
	}

	TArray<FNteCharacterKawaiiLimitSpec> Limits;
	ReadLimitArray(*DataAsset, TEXT("SphericalLimits"), TEXT("Spherical"), Limits);
	ReadLimitArray(*DataAsset, TEXT("CapsuleLimits"), TEXT("Capsule"), Limits);
	ReadLimitArray(*DataAsset, TEXT("BoxLimits"), TEXT("Box"), Limits);
	ReadLimitArray(*DataAsset, TEXT("PlanarLimits"), TEXT("Planar"), Limits);

	Preset.CollisionLimits = MoveTemp(Limits);
	Preset.SphericalLimitsDataCount = GetArrayLength(*DataAsset, TEXT("SphericalLimits"));
	Preset.CapsuleLimitsDataCount = GetArrayLength(*DataAsset, TEXT("CapsuleLimits"));
	Preset.BoxLimitsDataCount = GetArrayLength(*DataAsset, TEXT("BoxLimits"));
	Preset.PlanarLimitsDataCount = GetArrayLength(*DataAsset, TEXT("PlanarLimits"));
	AddUpdated(Result, TEXT("CollisionLimits"));
}

void ReadBoneConstraintsDataAsset(
	const FString& BoneConstraintsDataAssetPath,
	FNteCharacterKawaiiPresetSpec& Preset,
	FNteCharacterKawaiiAssetSyncResult& Result)
{
	if (BoneConstraintsDataAssetPath.IsEmpty())
	{
		return;
	}

	UObject* DataAsset = NTEBuildTool::Editor::LoadAnyAssetByPath(BoneConstraintsDataAssetPath);
	if (!DataAsset)
	{
		Result.Warnings.Add(FString::Printf(TEXT("Could not load Kawaii bone constraints data asset: %s"), *BoneConstraintsDataAssetPath));
		return;
	}

	Preset.BoneConstraintsDataCount = GetArrayLength(*DataAsset, TEXT("BoneConstraintsData"));
	AddUpdated(Result, TEXT("BoneConstraintsDataCount"));
}
}

FNteCharacterKawaiiAssetSyncResult SyncKawaiiPresetSpecFromGeneratedAssets(
	const FNteCharacterKawaiiPresetPlanItem& PlanItem,
	FNteCharacterKawaiiPresetSpec& InOutPreset)
{
	FNteCharacterKawaiiAssetSyncResult Result;
	Result.PresetId = PlanItem.Id;
	Result.RuntimeAnimBlueprintPath = PlanItem.RuntimeAnimBlueprintPath;

	UAnimBlueprint* AnimBlueprint = NTEBuildTool::Editor::LoadAssetByPath<UAnimBlueprint>(PlanItem.RuntimeAnimBlueprintPath);
	if (!AnimBlueprint)
	{
		Result.Errors.Add(FString::Printf(TEXT("Could not load Kawaii Runtime AnimBlueprint: %s"), *PlanItem.RuntimeAnimBlueprintPath));
		return Result;
	}

	UEdGraphNode* KawaiiNode = FindPresetKawaiiNode(*AnimBlueprint, PlanItem);
	if (!KawaiiNode)
	{
		Result.Errors.Add(FString::Printf(TEXT("Could not find KawaiiPhysics node for preset '%s' in %s."), *PlanItem.Id, *PlanItem.RuntimeAnimBlueprintPath));
		return Result;
	}

	void* NodeContainer = nullptr;
	UScriptStruct* NodeStruct = nullptr;
	if (!TryGetKawaiiNodeStruct(*KawaiiNode, NodeContainer, NodeStruct))
	{
		Result.Errors.Add(FString::Printf(TEXT("KawaiiPhysics node for preset '%s' has no reflected Node struct."), *PlanItem.Id));
		return Result;
	}

	InOutPreset.RuntimeAnimBlueprintPath = PlanItem.RuntimeAnimBlueprintPath;
	InOutPreset.RootBone = ReadBoneReferenceName(NodeContainer, NodeStruct, TEXT("RootBone"));
	InOutPreset.ExcludeBones = ReadBoneReferenceArray(NodeContainer, NodeStruct, TEXT("ExcludeBones"));
	InOutPreset.AdditionalRootBones = ReadAdditionalRootBones(NodeContainer, NodeStruct);
	ReadPhysicsSettings(NodeContainer, NodeStruct, InOutPreset.PhysicsSettings, Result);
	ReadFloat(NodeContainer, NodeStruct, TEXT("DummyBoneLength"), InOutPreset.DummyBoneLength);
	InOutPreset.BoneForwardAxis = ReadEnum(NodeContainer, NodeStruct, TEXT("BoneForwardAxis"));
	ReadInt(NodeContainer, NodeStruct, TEXT("TargetFrameRate"), InOutPreset.TargetFramerate);
	ReadBool(NodeContainer, NodeStruct, TEXT("OverrideTargetFramerate"), InOutPreset.bOverrideTargetFramerate);
	ReadInt(NodeContainer, NodeStruct, TEXT("WarmUpFrames"), InOutPreset.WarmUpFrames);
	ReadBool(NodeContainer, NodeStruct, TEXT("bUseWarmUpWhenResetDynamics"), InOutPreset.bUseWarmUpWhenResetDynamics);
	ReadBool(NodeContainer, NodeStruct, TEXT("bNeedWarmUp"), InOutPreset.bNeedWarmUp);
	ReadFloat(NodeContainer, NodeStruct, TEXT("TeleportDistanceThreshold"), InOutPreset.TeleportDistanceThreshold);
	ReadFloat(NodeContainer, NodeStruct, TEXT("TeleportRotationThreshold"), InOutPreset.TeleportRotationThreshold);
	InOutPreset.PlanarConstraint = ReadEnum(NodeContainer, NodeStruct, TEXT("PlanarConstraint"));
	ReadBool(NodeContainer, NodeStruct, TEXT("ResetBoneTransformWhenBoneNotFound"), InOutPreset.bResetBoneTransformWhenBoneNotFound);
	ReadCurvePaths(NodeContainer, NodeStruct, InOutPreset);
	InOutPreset.OutputLimitsDataAssetPath = ReadObjectPackagePath(NodeContainer, NodeStruct, TEXT("LimitsDataAsset"));
	InOutPreset.PhysicsAssetForLimitsPath = ReadObjectPackagePath(NodeContainer, NodeStruct, TEXT("PhysicsAssetForLimits"));
	InOutPreset.OutputBoneConstraintsDataAssetPath = ReadObjectPackagePath(NodeContainer, NodeStruct, TEXT("BoneConstraintsDataAsset"));
	InOutPreset.BoneConstraintGlobalComplianceType = ReadEnum(NodeContainer, NodeStruct, TEXT("BoneConstraintGlobalComplianceType"));
	ReadInt(NodeContainer, NodeStruct, TEXT("BoneConstraintIterationCountBeforeCollision"), InOutPreset.BoneConstraintIterationCountBeforeCollision);
	ReadInt(NodeContainer, NodeStruct, TEXT("BoneConstraintIterationCountAfterCollision"), InOutPreset.BoneConstraintIterationCountAfterCollision);
	ReadBool(NodeContainer, NodeStruct, TEXT("bAutoAddChildDummyBoneConstraint"), InOutPreset.bAutoAddChildDummyBoneConstraint);
	InOutPreset.BoneConstraintCount = 0;
	if (FArrayProperty* BoneConstraintsArray = CastField<FArrayProperty>(FindProperty(NodeStruct, TEXT("BoneConstraints"))))
	{
		FScriptArrayHelper Helper(BoneConstraintsArray, BoneConstraintsArray->ContainerPtrToValuePtr<void>(NodeContainer));
		InOutPreset.BoneConstraintCount = Helper.Num();
	}
	ReadVectorByAnyName(NodeContainer, NodeStruct, {TEXT("Gravity")}, InOutPreset.Gravity);
	ReadBool(NodeContainer, NodeStruct, TEXT("bEnableWind"), InOutPreset.bEnableWind);
	ReadFloat(NodeContainer, NodeStruct, TEXT("WindScale"), InOutPreset.WindScale);
	if (ReadBool(NodeContainer, NodeStruct, TEXT("bUseRelativeMove"), InOutPreset.bUseRelativeMove))
	{
		InOutPreset.bHasUseRelativeMove = true;
	}
	ReadVectorByAnyName(NodeContainer, NodeStruct, {TEXT("MovementReferenceDisplacement")}, InOutPreset.MovementReferenceDisplacement);
	ReadBool(NodeContainer, NodeStruct, TEXT("bAllowWorldCollision"), InOutPreset.bAllowWorldCollision);
	ReadBool(NodeContainer, NodeStruct, TEXT("bOverrideCollisionParams"), InOutPreset.bOverrideCollisionParams);
	ReadBool(NodeContainer, NodeStruct, TEXT("bIgnoreSelfComponent"), InOutPreset.bIgnoreSelfComponent);
	InOutPreset.IgnoreBones = ReadBoneReferenceArray(NodeContainer, NodeStruct, TEXT("IgnoreBones"));
	InOutPreset.IgnoreBoneNamePrefix = ReadNameArray(NodeContainer, NodeStruct, TEXT("IgnoreBoneNamePrefix"));
	InOutPreset.KawaiiPhysicsTag = ReadGameplayTagName(NodeContainer, NodeStruct, TEXT("KawaiiPhysicsTag"));

	AddUpdated(Result, TEXT("RuntimeAnimBlueprintPath"));
	AddUpdated(Result, TEXT("RootBone"));
	AddUpdated(Result, TEXT("ExcludeBones"));
	AddUpdated(Result, TEXT("AdditionalRootBones"));
	AddUpdated(Result, TEXT("PhysicsSettings"));
	AddUpdated(Result, TEXT("NodeSettings"));
	AddUpdated(Result, TEXT("DataAssetReferences"));
	AddUpdated(Result, TEXT("ForcesAndCollision"));

	ReadLimitsDataAsset(InOutPreset.OutputLimitsDataAssetPath, InOutPreset, Result);
	ReadBoneConstraintsDataAsset(InOutPreset.OutputBoneConstraintsDataAssetPath, InOutPreset, Result);
	RestoreSourceBoneReferences(InOutPreset);
	if (!InOutPreset.BoneRemaps.IsEmpty())
	{
		AddUpdated(Result, TEXT("SourceBoneReferencesRestoredFromBoneRemaps"));
	}

	if (InOutPreset.SourceKind.IsEmpty())
	{
		InOutPreset.SourceKind = TEXT("Manual");
	}

	return Result;
}
}
