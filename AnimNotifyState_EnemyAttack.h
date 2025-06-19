// Copyright Brigham Young University. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Engine/ActorInstanceHandle.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Y25/Player/MainCharacter.h"

#include "AnimNotifyState_EnemyAttack.generated.h"

UCLASS()
class Y25_API UAnimNotifyState_EnemyAttack : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_EnemyAttack();

	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyTick(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

protected:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Config")
	TObjectPtr<UPhysicsAsset> PhysicsAsset;
#endif

	UPROPERTY(EditAnywhere, Category = "Config", meta = (GetOptions = GetPhysicsBodyOptions))
	FName PhysicsBody;

private:
	FBodyInstance* BodyInstance = nullptr;

	UPROPERTY(Transient)
	TSet<FActorInstanceHandle> HitInstances;

	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> HitCharacters;

#if WITH_EDITOR
	UFUNCTION()
	TArray<FName> GetPhysicsBodyOptions() const;

	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
