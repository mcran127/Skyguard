// Copyright Brigham Young University. All Rights Reserved.

#include "GrenadeProjectile.h"

#include "AbilitySystemGlobals.h"
#include "GameFramework/DamageType.h"
#include "Math/Vector.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameplayCueManager.h"
#include "Y25/Enemies/BaseEnemy.h"
#include "Y25/Gameplay/Cues.h"
#include "Y25/Gameplay/Tags.h"
#include "Y25/Player/MainCharacter.h"

AGrenadeProjectile::AGrenadeProjectile()
{
	//Set up components
	SetRootComponent(HitboxCollision);

	GrenadeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GrenadeMesh"));
	GrenadeMesh->SetupAttachment(HitboxCollision);
	GrenadeMesh->SetVisibility(true);
}

void AGrenadeProjectile::BeginPlay()
{
	Super::BeginPlay();

	//Grenade should only last 5 seconds max
	AGrenadeProjectile::SetLifeSpan(5.0f);

	//On hit event
	if (HitboxCollision)
	{
		HitboxCollision->OnComponentHit.AddDynamic(this, &AGrenadeProjectile::OnHit);
	}

	// hit tracking
	EnemyHits = InstigatorVar->GetPlayerState<AControlPlayerState>()->PlayerEndStats.Find(TEXT("Bot Hits"));
	FriendHits = InstigatorVar->GetPlayerState<AControlPlayerState>()->PlayerEndStats.Find(TEXT("Friend Hits"));
	BotKills = InstigatorVar->GetPlayerState<AControlPlayerState>()->PlayerEndStats.Find(TEXT("Bot Kills"));

	FriendTrueHits = InstigatorVar->GetPlayerState<AControlPlayerState>()->PlayerTrueStats.Find(TEXT("Friend Hits"));
	BotTrueKills = InstigatorVar->GetPlayerState<AControlPlayerState>()->PlayerTrueStats.Find(TEXT("Bot Kills"));
}

void AGrenadeProjectile::UpdateAccuracy()
{
	if (AControlPlayerState* PlayerState = InstigatorVar->GetPlayerState<AControlPlayerState>())
	{
		const float BotHits = PlayerState->PlayerEndStats.FindRef("Bot Hits");
		const float ShotsFired = PlayerState->PlayerEndStats.FindRef("Shots Fired");

		PlayerState->PlayerTrueStats.FindOrAdd("Accuracy") = BotHits / ShotsFired;
	}
}

void AGrenadeProjectile::NewInitialize(
	const float Damage,
	const float MaxSpeed,
	const float StartSpeed,
	AMainPlayerController* InstigatingActor,
	const float Knockback)
{
	Super::Initialize(Damage, MaxSpeed, StartSpeed);

	//Set up base values
	DamageAmount = Damage;

	ProjectileMovementComponent->InitialSpeed = StartSpeed;
	ProjectileMovementComponent->MaxSpeed = MaxSpeed;

	const FVector LaunchVelocity = GetActorForwardVector() * StartSpeed;

	GrenadeKnockback = Knockback;

	ProjectileMovementComponent->Velocity = LaunchVelocity;
	ProjectileMovementComponent->Activate(true);

	InstigatorVar = InstigatingActor;

	//Set up ammo trail
	FGameplayCueParameters CueParam;
	CueParam.Instigator = GetOwner();
	CueParam.SourceObject = this;
	CueParam.Location = GetActorLocation();

	UAbilitySystemGlobals::Get().GetGameplayCueManager()
		->ExecuteGameplayCue_NonReplicated(this, Y25::Cues::Gun_AmmoTrail_Grenade, CueParam);
}

void AGrenadeProjectile::OnHit(
	UPrimitiveComponent* HitComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& HitResult)
{
	if (OtherActor && OtherActor != this && !OtherActor->IsA(StaticClass()))
	{
		// Ignore self
		if (OtherActor == Cast<AActor>(InstigatorVar->GetPawn()))
		{
			return;
		}
		//If enemy
		if (OtherActor->IsA(ABaseEnemy::StaticClass()))
		{
			(*EnemyHits)++;
			UpdateAccuracy();
		}
		//If player
		else if (OtherActor->IsA(AMainCharacter::StaticClass()))
		{
			(*FriendHits)++;
			(*FriendTrueHits)++;
		}
		Explode();
	}
}

void AGrenadeProjectile::Explode()
{
	//Explosion effect
	FGameplayCueParameters CueParam;

	CueParam.Instigator = InstigatorVar;
	CueParam.SourceObject = this;
	CueParam.Location = GetActorLocation();

	UAbilitySystemGlobals::Get().GetGameplayCueManager()->ExecuteGameplayCue_NonReplicated(
		InstigatorVar,
		Y25::Cues::Gun_AmmoHit_Grenade,
		CueParam);

	//Deal damage to all in radius
	TArray<AActor*> DamagedActors;

	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(this);

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(this);

	UKismetSystemLibrary::SphereOverlapActors(
		this,
		GetActorLocation(),
		DamageRadius,
		TArray<TEnumAsByte<EObjectTypeQuery>>(),
		nullptr,
		ActorsToIgnore,
		DamagedActors);

	for (AActor* DamagedActor : DamagedActors)
	{
		//If player or enemy
		if (DamagedActor && (DamagedActor->IsA(ABaseEnemy::StaticClass()) ||
			DamagedActor->IsA(AMainCharacter::StaticClass())))
		{
			//Get direction to launch
			FVector EnemyLaunch = DamagedActor->GetActorLocation() - GetActorLocation();
			if (EnemyLaunch.Z > 0)
			{
				EnemyLaunch.Z = 0;
			}
			EnemyLaunch.Normalize();

			//deal damage to enemy
			if (DamagedActor->IsA(ABaseEnemy::StaticClass()))
			{
				ABaseEnemy* DamagedEnemy = Cast<ABaseEnemy>(DamagedActor);

				if (!DamagedEnemy->IsDead())
				{
					UGameplayStatics::ApplyDamage(
					DamagedEnemy,
					DamageAmount,
					InstigatorVar,
					this,
					UDamageType::StaticClass());

					// add to bots killed
					if (DamagedEnemy->Health->GetHealth() <= 0)
					{
						(*BotKills)++;
						(*BotTrueKills)++;

						if (AControlPlayerState* ControlPlayerState = InstigatorVar->GetPlayerState<AControlPlayerState>())
						{
							ControlPlayerState->UpdateScore();
						}
					}

					//Set up launch, decrease to airborne enemies
					float LaunchForce = GrenadeKnockback;
					if (!DamagedEnemy->GetMovementComponent()->IsMovingOnGround())
					{
						LaunchForce = LaunchForce / 1000;
					}

					DamagedEnemy->LaunchCharacter(EnemyLaunch * LaunchForce, true, false);
				}
			}

			//deal damage to ally
			else
			{
				// Damage effect
				AMainCharacter* DamagedChar = Cast<AMainCharacter>(DamagedActor);
				UY25_AbilitySystemComponent* AbilitySystemComponent = DamagedChar->GetAbilitySystemComponent();

				if (!DamagedChar->GetIsDead())
				{
					FGameplayEffectContextHandle EffectContextHandle = AbilitySystemComponent->MakeEffectContext();
					EffectContextHandle.AddSourceObject(this);

					FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->
						MakeOutgoingSpec(DamageEffect, 1.f, EffectContextHandle);

					if (SpecHandle.IsValid())
					{
						SpecHandle.Data->SetSetByCallerMagnitude(Y25::Tags::GameplayEffect_Health_Damaged, DamageAmount);
						AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
					}
				}
				
				// knock back, less to airborne enemies
				float LaunchForce = GrenadeKnockback;
				if (!DamagedChar->GetMovementComponent()->IsMovingOnGround())
				{
					LaunchForce = LaunchForce / 1000;
				}

				DamagedChar->LaunchCharacter(EnemyLaunch * LaunchForce, true, false);
			}
		}
	}
	Destroy();
}

void AGrenadeProjectile::LifeSpanExpired()
{
	//Explode on death
	Explode();
}
