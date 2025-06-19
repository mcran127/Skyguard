// Copyright Brigham Young University. All Rights Reserved.

#pragma once

#include "NiagaraSystem.h"
#include "Y25/Weapons/BaseProjectile.h"
#include "Components/StaticMeshComponent.h"
#include "Y25/Player/MainPlayerController.h"

#include "GrenadeProjectile.generated.h"

UCLASS()
class Y25_API AGrenadeProjectile : public ABaseProjectile
{
	GENERATED_BODY()

public:
	AGrenadeProjectile();

	virtual void NewInitialize(
		float Damage,
		float MaxSpeed,
		float StartSpeed,
		AMainPlayerController* InstigatingActor,
		float Knockback);

	UFUNCTION()
	void OnHit(
		UPrimitiveComponent* HitComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& HitResult);

	UFUNCTION()
	void Explode();

	UFUNCTION()
	virtual void LifeSpanExpired() override;

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditDefaultsOnly, Category="Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bullet", meta=(AllowPrivateAccess=true))
	TObjectPtr<UStaticMeshComponent> GrenadeMesh;

	//Grenade stats
	UPROPERTY(EditAnywhere, Category = "Bullet")
	float DamageRadius = 500;
	UPROPERTY(EditAnywhere, Category = "Bullet")
	float GrenadeKnockback = 100;

	UPROPERTY(EditAnywhere, Category = "Bullet")
	TObjectPtr<AMainPlayerController> InstigatorVar;

	void UpdateAccuracy();

	int32* EnemyHits;
	int32* FriendHits;
	int32* BotKills;
	float* FriendTrueHits;
	float* BotTrueKills;
};
