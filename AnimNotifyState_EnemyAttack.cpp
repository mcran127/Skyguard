// Copyright Brigham Young University. All Rights Reserved.

#include "AnimNotifyState_EnemyAttack.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/OverlapResult.h"
#include "Misc/DataValidation.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Y25/Enemies/BaseEnemy.h"

UAnimNotifyState_EnemyAttack::UAnimNotifyState_EnemyAttack()
{
#if WITH_EDITORONLY_DATA
	bShouldFireInEditor = true;
#endif
}

void UAnimNotifyState_EnemyAttack::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	//Setup who's getting hit
	BodyInstance = nullptr;
	HitInstances.Empty();
	HitCharacters.Empty();

	//Make sure you have the mesh and physics body
	if (!MeshComp)
	{
		return;
	}

	if (PhysicsBody.IsNone())
	{
		return;
	}

	//Make body instance
	BodyInstance = MeshComp->GetBodyInstance(PhysicsBody);
}

void UAnimNotifyState_EnemyAttack::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	//Check mesh and bodyinstance
	if (!MeshComp || !BodyInstance)
	{
		return;
	}

	//Check overlap
	const FTransform Transform = MeshComp->GetSocketTransform(PhysicsBody, RTS_World);

	TArray<FOverlapResult> Results;
	BodyInstance->OverlapMulti(
		Results,
		MeshComp->GetWorld(),
		nullptr,
		Transform.GetLocation(),
		Transform.GetRotation(),
		ECC_Pawn,
		FComponentQueryParams::DefaultComponentQueryParams,
		FCollisionResponseParams::DefaultResponseParam);

	//Get all overlaps
	for (const FOverlapResult& Result : Results)
	{
		//Ignore if already in
		if (HitInstances.Contains(Result.OverlapObjectHandle))
		{
			continue;
		}
		
		if (AActor* Actor = Result.GetActor())
		{
			if (HitCharacters.Contains(Actor))
			{
				continue;
			}
			HitCharacters.Add(Actor);

			//Call damage in enemy
			if (ABaseEnemy* BaseEnemy = Cast<ABaseEnemy>(MeshComp->GetOwner()))
			{
				BaseEnemy->Attacking(Actor);
			}
		}
	}
}

void UAnimNotifyState_EnemyAttack::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	//clear body instance and sets
	BodyInstance = nullptr;
	HitInstances.Empty();
	HitCharacters.Empty();
}

#if WITH_EDITOR
TArray<FName> UAnimNotifyState_EnemyAttack::GetPhysicsBodyOptions() const
{
	TArray<FName> Names;

	if (PhysicsAsset)
	{
		for (const USkeletalBodySetup* SkeletalBodySetup : PhysicsAsset->SkeletalBodySetups)
		{
			if (SkeletalBodySetup)
			{
				Names.Add(SkeletalBodySetup->BoneName);
			}
		}
	}

	return Names;
}

EDataValidationResult UAnimNotifyState_EnemyAttack::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (PhysicsBody.IsNone())
	{
		Context.AddError(INVTEXT("Physics body is not set."));
		Result = EDataValidationResult::Invalid;
	}
	else
	{
		if (!GetPhysicsBodyOptions().Contains(PhysicsBody))
		{
			Context.AddError(INVTEXT("Physics body could not be found in the physics asset."));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif
