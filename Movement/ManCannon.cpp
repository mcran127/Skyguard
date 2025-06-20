// Copyright Brigham Young University. All Rights Reserved.

#include "ManCannon.h"

#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Components/ArrowComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "ProfilingDebugging/StallDetector.h"
#include "Y25/Enemies/EnemySpawner/MatchManagerComponent.h"
#include "Y25/Game/Y25_GameMode.h"
#include "Y25/Gameplay/Cues.h"
#include "Y25/Player/MainCharacter.h"
#include "Y25/Values/Collision.h"

AManCannon::AManCannon()
{
	PrimaryActorTick.bCanEverTick = false;

	//Setup 
	RootComponent = MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CannonMesh"));
	MeshComponent->bAllowConcurrentTick = true;
	MeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	CollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("JumpCollision"));
	CollisionComponent->SetupAttachment(MeshComponent);
	CollisionComponent->SetCollisionProfileName(Y25::Collision::Profiles::Trigger);
	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnOverlapBegin);

	LaunchDirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("LaunchDirection"));
	LaunchDirectionArrow->SetupAttachment(MeshComponent);
	LaunchDirectionArrow->SetHiddenInGame(true);
	LaunchDirectionArrow->ArrowSize = 3.0f;
}

void AManCannon::SetCanLaunch(const bool bEnabled)
{
	bCanLaunch = bEnabled;

	//Re fire man cannon if people are overlapping when ready to fire again
	if (CanLaunch())
	{
		TArray<AActor*> OverlappingActors;
		CollisionComponent->GetOverlappingActors(OverlappingActors);

		if (OverlappingActors.Num() > 0)
		{
			for (AActor* OverlappingActor : OverlappingActors)
			{
				if (OverlappingActor->IsA<AMainCharacter>())
				{
					OnOverlapBegin(
						CollisionComponent,
						OverlappingActor,
						nullptr,
						0,
						false,
						FHitResult());
					break;
				}
			}
		}
	}
}

bool AManCannon::CanLaunch() const
{
	return bCanLaunch;
}

void AManCannon::OnOverlapBegin(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& Hit)
{

	if (OtherActor->IsA(AMainCharacter::StaticClass()))
	{
		SetCanLaunch(false);

		//Set the person who originally overlapped
		FirstToLaunch = Cast<AMainCharacter>(OtherActor);

		MeshComponent->PlayAnimation(LaunchMontage, false);

		//Edit player movement/montages
		const AMainCharacter* Character = Cast<AMainCharacter>(OtherActor);
		Character->GetMovementComponent()->ClearAccumulatedForces();
		Character->GetMovementComponent()->SquatStop();
		Character->GetMesh()->GetAnimInstance()->StopAllMontages(0);

		//Reset man cannon after launch
		FTimerHandle TimerHandle;
		GetWorldTimerManager().SetTimer(
			TimerHandle,
			[this]
			{
				if (!IsValid(this)) {return;}
				SetCanLaunch(true);
				if (MeshComponent && DormantMontage)
				{
					MeshComponent->PlayAnimation(DormantMontage, true);
				}
			},
			LaunchMontage->GetPlayLength(),
			false);
	}
}

//Called from anim notify in animation
void AManCannon::FireCannon()
{
	//Launch the first person set before the animation
	if (FirstToLaunch)
	{
		LaunchActor(FirstToLaunch);
	}

	//Launch everyone else on the cannon
	TArray<AActor*> Result;
	CollisionComponent->GetOverlappingActors(Result);
	if (!Result.IsEmpty())
	{
		for (int i = 0; i < Result.Num(); i++)
		{
			if (AMainCharacter* CurrResult = Cast<AMainCharacter>(Result[i]))
			{
				if (CurrResult != FirstToLaunch)
				{
					LaunchActor(CurrResult);
				}
			}
		}
	}
}

void AManCannon::LaunchActor(AMainCharacter* MainCharacter)
{
	//Set movement
	MainCharacter->SetHasLaunched(true);
	MainCharacter->GetMovementComponent()->SetMovementMode(MOVE_Falling);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (const AMainPlayerController* MainPlayerController = Cast<AMainPlayerController>(MainCharacter->GetController()))
	{
		if (UAbilitySystemComponent* AbilitySystemComponent = MainPlayerController->GetAbilitySystemComponent())
		{
			//Stop and disable shooting/aiming
			AbilitySystemComponent->AbilityLocalInputReleased(3);
			AbilitySystemComponent->AbilityLocalInputReleased(4);
			MainCharacter->GetGun()->SetCanFire(false);
		}
		MainCharacter->GetGun()->SetCanFire(false);
	}

	//Launch character
	FVector LaunchDirection = LaunchDirectionArrow->GetForwardVector();
	LaunchDirection.Normalize();

	MainCharacter->GetMovementComponent()->Velocity = LaunchDirection * Force;


	TWeakObjectPtr<ACharacter> WeakCharacter = MainCharacter;
	FTimerHandle* LaunchTimer = new FTimerHandle();

	//Move player into glide mode
	GetWorldTimerManager().SetTimer(
		*LaunchTimer,
		[this, WeakCharacter, LaunchTimer] ()mutable 
		{
			if (WeakCharacter.IsValid())
			{
				AMainCharacter* Character = Cast<AMainCharacter>(WeakCharacter.Get());

				if (Character->GetVelocity().Z <= 0) {
					Character->SetHasLanded(false);
					Character->SetHasLaunched(false);
					Character->SetGlideMode(true);
					GetWorldTimerManager().ClearTimer(*LaunchTimer);
					delete LaunchTimer;
				}
				
			}
		},
		0.1f,
		true);

	//Reset mesh collision
	FTimerHandle CollisionTimer;
	GetWorldTimerManager().SetTimer(
		CollisionTimer,
		[this]
		{
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		},
		0.1f,
		false);
}

void AManCannon::BeginPlay()
{
	Super::BeginPlay();

	//Loop dormant montage
	FGameplayCueParameters CueParam;
	CueParam.SourceObject = this;
	CueParam.Location = GetActorLocation();

	UAbilitySystemGlobals::Get().GetGameplayCueManager()->ExecuteGameplayCue_NonReplicated(
		this,
		Y25::Cues::ManCannon_Dormant,
		CueParam);

	if (MeshComponent && DormantMontage)
	{
		MeshComponent->PlayAnimation(DormantMontage, true);
	}
}

void AManCannon::Destroyed()
{
	//Clean up timers
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	Super::Destroyed();
}
