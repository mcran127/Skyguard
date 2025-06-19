// Copyright Brigham Young University. All Rights Reserved.

#include "JumpPad.h"

#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"
#include "TimerManager.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Y25/Gameplay/Cues.h"

AJumpPad::AJumpPad()
{
	PrimaryActorTick.bCanEverTick = true;

	//Setup jump pad itself
	JumpMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("JumpMesh"));
	CollisionCylinder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CollisionCylinder"));

	RootComponent = JumpMesh;
	CollisionCylinder->SetupAttachment(JumpMesh);

	CollisionCylinder->OnComponentBeginOverlap.AddDynamic(this, &AJumpPad::OnOverlapBegin);
}

void AJumpPad::BeginPlay()
{
	Super::BeginPlay();

	//PLay dormant cue
	FGameplayCueParameters CueParam;
	CueParam.SourceObject = this;
	CueParam.Location = GetActorLocation();

	UAbilitySystemGlobals::Get().GetGameplayCueManager()->ExecuteGameplayCue_NonReplicated(
		this,
		Y25::Cues::JumpPad_Dormant,
		CueParam);
}

//Get/Set
void AJumpPad::SetLaunchForce(const float Value)
{
	JumpForce = Value;
}

float AJumpPad::GetLaunchForce() const
{
	return JumpForce;
}

void AJumpPad::SetCanLaunch(const bool bValue)
{
	bCanLaunch = bValue;
}

bool AJumpPad::GetCanLaunch() const
{
	return bCanLaunch;
}

void AJumpPad::OnOverlapBegin(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& Hit)
{
	//Check if can launch
	if (!GetCanLaunch())
	{
		return;
	}

	if (AMainCharacter* Character = Cast<AMainCharacter>(OtherActor))
	{
		SetCanLaunch(false);

		const FVector LaunchDirection = FVector(0, 0, 1);

		//Play launch animation
		JumpMesh->PlayAnimation(LaunchMontage, false);

		//Control player movement
		Character->GetMovementComponent()->SlideStop();
		Character->GetMovementComponent()->SetJumpAllowed(false);

		//Launch player
		Character->LaunchCharacter(LaunchDirection * JumpForce, false, true);

		//Reset jump pad and player
		GetWorld()->GetTimerManager().SetTimer(
			LaunchResetTimerHandle,
			[this, Character]
			{
				SetCanLaunch(true);
				Character->GetMovementComponent()->SetJumpAllowed(true);
			},
			LaunchMontage->GetPlayLength(),
			false);
	}
}
