// Copyright Brigham Young University. All Rights Reserved.

#pragma once

#include "Gun.h"
#include "UObject/Object.h"

#include "GunTracerData.generated.h"

UCLASS(BlueprintType)
class Y25_API UGunTracerData final : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	FVector MuzzlePosition;

	UPROPERTY(BlueprintReadOnly)
	TArray<FVector> ImpactPositions;

	UPROPERTY(BlueprintReadOnly)
	EAmmoType AmmoType;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<USkeletalMeshComponent> GunMesh;
};
