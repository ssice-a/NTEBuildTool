// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Engine/DataAsset.h"
#include "Engine/SkeletalMesh.h"

#include "HTPlayerAppearance.generated.h"

USTRUCT(BlueprintType)
struct HTGAME_API FHTFashionMeshData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TObjectPtr<USkeletalMesh> CharacterMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TSubclassOf<UAnimInstance> AnimInstance;
};

USTRUCT(BlueprintType)
struct HTGAME_API FHTFashionAttachedMeshData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TObjectPtr<USkeletalMesh> CharacterMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TSubclassOf<UAnimInstance> AnimInstance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TSubclassOf<UAnimInstance> MobileAnimInstance;

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

UCLASS(BlueprintType)
class HTGAME_API UHTPlayerAppearance : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FHTFashionMeshData FashionMeshData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	float CapsuleHalfHeight = 76.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	float CapsuleRadius = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	FVector RelativeLocation = FVector(0.0, 0.0, -78.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	float FPSCameraCapsuleTopOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TArray<FHTFashionAttachedMeshData> ArrayFashionAttachedMeshData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Appearance")
	TSubclassOf<UObject> UltraSkillSequence;
};
