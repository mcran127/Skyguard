// Copyright Brigham Young University. All Rights Reserved.

#pragma once

#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "Y25/Player/MainCharacter.h"

#include "ManCannon.generated.h"

UCLASS(Abstract)
class Y25_API AManCannon final : public AActor
{
	GENERATED_BODY()

public:
	AManCannon();

	void SetCanLaunch(bool bEnabled);

	bool CanLaunch() const;

	void FireCannon();

	void LaunchActor(AMainCharacter* MainCharacter);

private:
	UFUNCTION()
	void OnOverlapBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& Hit);

	virtual void BeginPlay() override;

	bool bCanStartRound = false;

	virtual void Destroyed() override;

protected:
	//Building the man cannon
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> CollisionComponent;

	UPROPERTY(EditAnywhere, Category = "Cannon|Launch")
	float Force = 50000.0f;

	UPROPERTY(EditAnywhere, Category = "Cannon|Launch")
	TObjectPtr<UArrowComponent> LaunchDirectionArrow;

	UPROPERTY(EditAnywhere, Category = "Cannon|Launch")
	bool bCanLaunch = true;

	//Animations
	UPROPERTY(EditDefaultsOnly, Category = "Cannon|Animation")
	TObjectPtr<UAnimMontage> LaunchMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Cannon|Animation")
	TObjectPtr<UAnimMontage> DormantMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Cannon")
	TObjectPtr<AMainCharacter> FirstToLaunch;
};
