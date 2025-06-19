// Copyright Brigham Young University. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Y25/Player/MainCharacter.h"

#include "JumpPad.generated.h"

UCLASS()
class Y25_API AJumpPad : public AActor
{
	GENERATED_BODY()

public:
	AJumpPad();

	//Get/Set
	UFUNCTION(Category="GetSet")
	void SetLaunchForce(float Value);

	UFUNCTION(Category="GetSet")
	float GetLaunchForce() const;

	UFUNCTION(Category="GetSet")
	void SetCanLaunch(bool bValue);

	UFUNCTION(Category="GetSet")
	bool GetCanLaunch() const;

	UFUNCTION()
	void OnOverlapBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& Hit);

	FTimerHandle LaunchResetTimerHandle;

private:
	//
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="JumpMesh", meta=(AllowPrivateAccess=true))
	TObjectPtr<USkeletalMeshComponent> JumpMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="JumpMesh", meta=(AllowPrivateAccess=true))
	TObjectPtr<UStaticMeshComponent> CollisionCylinder;

	//Animation
	UPROPERTY(EditDefaultsOnly, Category = "Animations")
	TObjectPtr<UAnimMontage> LaunchMontage;

	//Stat
	UPROPERTY(EditAnywhere, Category="JumpForce")
	float JumpForce = 5000.0f;

	UPROPERTY(EditAnywhere, Category="Launch")
	bool bCanLaunch = true;

protected:
	virtual void BeginPlay() override;

};
