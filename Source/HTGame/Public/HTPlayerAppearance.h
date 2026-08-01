// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Engine/DataAsset.h"
#include "Engine/SkeletalMesh.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "HTPlayerAppearance.generated.h"

// These reflected names, property kinds, and declaration order mirror the
// target game's usmap. They are a cook-time schema bridge for /Script/HTGame.
USTRUCT(BlueprintType)
struct HTGAME_API FAppearanceAudioData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TSoftObjectPtr<UObject> SoftAudioEvent;
};

USTRUCT(BlueprintType)
struct HTGAME_API FAppearanceParticleData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TObjectPtr<UObject> ParticleSystem = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	bool bUseSoftLoad = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TSoftObjectPtr<UObject> SoftParticleSystem;
};

USTRUCT(BlueprintType)
struct HTGAME_API FCharacterMeshData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TObjectPtr<USkeletalMesh> CharacterMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TObjectPtr<UObject> AnimInstance = nullptr;
};

USTRUCT(BlueprintType)
struct HTGAME_API FAttachedMeshData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TObjectPtr<USkeletalMesh> CharacterMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TObjectPtr<UObject> AnimInstance = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TObjectPtr<UObject> MobileAnimInstance = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TArray<FName> MeshComponentOwnedTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FRotator RelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FVector RelativeScale3D = FVector::OneVector;
};

USTRUCT(BlueprintType)
struct HTGAME_API FExtraAttachedMeshData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FName TagName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TArray<FAttachedMeshData> ArrayExtraAttachedMeshData;
};

USTRUCT(BlueprintType)
struct HTGAME_API FPlayerAppearanceMeshAttachEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FName MeshComponentTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FName AttachSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TSoftObjectPtr<UObject> Effect;
};

USTRUCT(BlueprintType)
struct HTGAME_API FPlayerAppearanceTagAudioReplacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TSoftObjectPtr<UObject> NewAudioEvent;
};

USTRUCT(BlueprintType)
struct HTGAME_API FPlayerAppearanceTagAudioReplacementArray
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TArray<FPlayerAppearanceTagAudioReplacement> Replacements;
};

USTRUCT(BlueprintType)
struct HTGAME_API FSpawnParticleEffectParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	bool bUseSoftObject = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TSoftObjectPtr<UObject> SoftPSTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	bool bCheckAppearance = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TObjectPtr<UObject> PSTemplate = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TArray<FName> AddTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FVector Scale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	bool Attached = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	bool IgnoreHitEffectOpt = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	bool UseParentBoundBox = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	bool bAcceptCustomDilation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	bool bAttachRootComponent = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	bool AdaptiveScale = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TArray<FInstancedStruct> UserParameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FInstancedStruct DirectionForce;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TObjectPtr<UObject> SavedParticleSystem = nullptr;
};

USTRUCT(BlueprintType)
struct HTGAME_API FSpawnParticleEffectParamsArray
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TArray<FSpawnParticleEffectParams> ParticleEffectParamsArray;
};

UCLASS(BlueprintType)
class HTGAME_API UHTAppearanceDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TMap<FName, FAppearanceAudioData> AudioEventMap;
};

UCLASS(BlueprintType)
class HTGAME_API UHTPlayerAppearance : public UHTAppearanceDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FCharacterMeshData FashionMeshData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	float CapsuleHalfHeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	float CapsuleRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	bool SortTriangles = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	float FPSCameraCapsuleTopOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	float VinesIKFootOffsetAdditive = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TArray<FAttachedMeshData> ArrayFashionAttachedMeshData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TArray<FExtraAttachedMeshData> ArrayDynamicAttachedMeshData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TMap<FName, FCharacterMeshData> ChildMeshDataMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TArray<FPlayerAppearanceMeshAttachEffect> FashionMeshAttachEffects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TObjectPtr<UObject> AppearanceWeapon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TObjectPtr<UObject> UltraSkillSequence = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TMap<FName, FAppearanceParticleData> ParticleMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TMap<FName, TObjectPtr<UObject>> ForceLoadParticleMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TMap<FName, FPlayerAppearanceTagAudioReplacementArray> GameplayTagAudioEventMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TMap<FName, TSoftObjectPtr<UObject>> SpawnActorMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TMap<FName, FSpawnParticleEffectParamsArray> ParticleMapWithParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TMap<FName, TObjectPtr<UObject>> SequenceMap;
};

UCLASS(BlueprintType)
class HTGAME_API UHTPlayerNPCAppearance : public UHTAppearanceDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FCharacterMeshData FashionMeshData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TArray<FAttachedMeshData> ArrayFashionAttachedMeshData;
};
