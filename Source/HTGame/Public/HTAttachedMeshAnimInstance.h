// Copyright (c) 2026 NTEBuildTool contributors.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"

#include "HTAttachedMeshAnimInstance.generated.h"

// Editor/cook schema bridge for /Script/HTGame.HTAttachedMeshAnimInstance.
// The game supplies the real runtime class; this stub module is not shipped
// as a mod DLL.
UCLASS(Transient, Blueprintable)
class HTGAME_API UHTAttachedMeshAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Attached Mesh")
	float EnablePhysicsWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Attached Mesh")
	float PhysicsCurveInterpSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Attached Mesh")
	bool bUseParentPhysicsWeight = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Attached Mesh")
	int32 ParentDepth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Attached Mesh")
	bool bOverrideByCurveWhenUseParentWeight = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Attached Mesh")
	bool bPhysicsWeightBlendMontage = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Attached Mesh")
	float SpeedBlendSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Attached Mesh")
	FVector MovementReferenceDisplacement = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Attached Mesh")
	FName AnimationToPlay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Attached Mesh")
	float MovementSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Attached Mesh")
	bool bIsSitting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Attached Mesh")
	float SleepWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HT|Attached Mesh")
	TObjectPtr<UObject> OwnerCharacter = nullptr;
};
