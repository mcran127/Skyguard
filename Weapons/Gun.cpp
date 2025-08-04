// Copyright Brigham Young University. All Rights Reserved.

#include "Gun.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "CineCameraComponent.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameplayCueManager.h"
#include "GunTracerData.h"
#include "TimerManager.h"
#include "Engine/DamageEvents.h"
#include "Utils/Gameplay/Cue.h"
#include "Y25/Enemies/BaseEnemy.h"
#include "Y25/Enemies/EnemySpawner/EnemySpawner.h"
#include "Y25/Gameplay/Attributes/AttributeSet_Gun.h"
#include "Y25/Gameplay/Cues.h"
#include "Y25/Gameplay/Tags.h"
#include "Y25/Player/MainCharacter.h"
#include "Y25/Player/MainPlayerController.h"
#include "AudioDevice.h"
#include "Y25/Game/Control/ControlHUD.h"
#include "Y25/Values/Collision.h"

DECLARE_LOG_CATEGORY_CLASS(LogGun, Log, All);

//Stats
namespace
{
	const FName ShotsFiredStat = TEXT("Shots Fired");
	const FName EnemyHitsStat = TEXT("Bot Hits");
	const FName FriendHitsStat = TEXT("Friend Hits");
	const FName CriticalHitsStat = TEXT("Critical Hits");
	const FName BotKillsStat = TEXT("Bot Kills");
}

AGun::AGun()
{
	PrimaryActorTick.bCanEverTick = true;

	//Create Meshes
	RootComponent = GunMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("GunMesh"));
	GunMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	MagMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MagMesh"));
	MagMesh->SetupAttachment(GunMesh);

	//Ability System Component Set
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);

	//GAS Attributes Set
	AttributeSet_Gun = CreateDefaultSubobject<UAttributeSet_Gun>(TEXT("AttributeSet.Gun"));
}

void AGun::BeginPlay()
{
	//Set Gun Type
	Super::BeginPlay();
	SetGunType(EGunType::Pistol);
	SetAmmoType(EAmmoType::Bullet);

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

}

void AGun::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bRecoiling)
	{
		if (const AMainCharacter* CharacterOwner = Cast<AMainCharacter>(GetOwner()))
		{
			FRotator CurrentRotation = CharacterOwner->GetController()->GetControlRotation();

			RecoilTimeElapsed += DeltaTime;
			
			const float DampingFactor = FMath::Exp(-RecoilDampen * RecoilTimeElapsed);

			const float BasePitch = RecoilCurveStrength * RecoilTimeElapsed;

			const float PitchNoise = FMath::PerlinNoise1D(RecoilTimeElapsed * RecoilNoiseFrequency);
			const float YawNoise = FMath::PerlinNoise1D((RecoilTimeElapsed + 1000.0f) * RecoilNoiseFrequency);

			const float PitchOffset = (BasePitch + PitchNoise * RecoilPitchIntensity) * DampingFactor;
			const float YawOffset = (YawNoise * RecoilYawIntensity) * (DampingFactor * 2);

			CurrentRotation.Pitch += (PitchOffset - LastRecoilPitch);
			CurrentRotation.Yaw += (YawOffset - LastRecoilYaw);

			LastRecoilPitch = PitchOffset;
			LastRecoilYaw = YawOffset;

			CharacterOwner->GetController()->SetControlRotation(CurrentRotation);

			if (RecoilTimeElapsed >= RecoilDuration)
			{
				bRecoiling = false;
				RecoilTimeElapsed = 0.0f;
				LastRecoilPitch = 0.0f;
				LastRecoilYaw = 0.0;
			}
		}
	}
	//Decrease recoil amount while not firing
	else
	{
		RecoilDuration = FMath::Max(RecoilDuration - DeltaTime / 6, 0);
	}

	//Aim gun towards the center of the screen, or object being aimed at
	if (!GunMesh->IsPlaying())
	{
		const UAttributeSet_Gun* GunAttributes = AbilitySystemComponent->GetSet<UAttributeSet_Gun>();

		const float CurrentRange = GunAttributes->GetRange();

		FMinimalViewInfo ViewInfo;
		if (GetOwner())
		{
			GetOwner()->CalcCamera(0, ViewInfo);
		}
		else
		{
			ViewInfo.Location = GetActorLocation();
			ViewInfo.Rotation = GetActorRotation();
		}

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		QueryParams.AddIgnoredActor(GetOwner());

		//Line trace to see where the gun is facing, check to see objects in the way
		FHitResult HitResult;
		GetWorld()->LineTraceSingleByChannel(
			HitResult,
			GetMuzzleTransform(),
			GetMuzzleTransform() + GetActorRightVector() * CurrentRange,
			Y25::Collision::Channels::Weapon,
			QueryParams);

		const float NotLaserRange = FVector::Dist(HitResult.Location, ViewInfo.Location) + 1;

		FVector ReturnLocation = ViewInfo.Location + ViewInfo.Rotation.Vector() * NotLaserRange;

		if (HitResult.IsValidBlockingHit())
		{
			ReturnLocation = HitResult.ImpactPoint;
		}

		//Calc Viewport
		if (const AActor* Actor = GetOwner())
		{
			if (const AMainPlayerController* MainPlayerController =
				Cast<AMainPlayerController>(Actor->GetInstigatorController()))
			{
				int32 ScreenX;
				int32 ScreenY;
				MainPlayerController->GetViewportSize(ScreenX, ScreenY);

				const float CenterScreenX = ScreenX / 2;
				const float CenterScreenY = ScreenY / 2;

				FVector2D ScreenLocation;

				MainPlayerController->ProjectWorldLocationToScreen(ReturnLocation, ScreenLocation, true);

				//If the gun is facing to far off center, adjust value
				if (ScreenLocation.X > CenterScreenX)
				{
					XOffset -= .02;
				}
				else
				{
					XOffset += .02;
				}
				if (ScreenLocation.Y > CenterScreenY)
				{
					YOffset -= .02;
				}
				else
				{
					YOffset += .02;
				}
			}
		}
	}
}

//Ammo trail struct
void AGun::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	TracerData = NewObject<UGunTracerData>(this);
}

AControlHUD* AGun::GetPlayerHUD() const
{
	if (const AMainCharacter* MainCharacter = Cast<AMainCharacter>(GetOwner()))
	{
		if (const AMainPlayerController* MainPlayerController = Cast<AMainPlayerController>(MainCharacter->GetController()))
		{
			if (AControlHUD* MainPlayerHUD = Cast<AControlHUD>(MainPlayerController->GetHUD()))
			{
				return MainPlayerHUD;
			}
		}
	}
	return nullptr;
}

//return a player's stats based on name
int32& AGun::GetStat(const FName Name) const
{
	static int32 Default = 0;

	if (const AController* Instigator = GetInstigatorController())
	{
		if (AControlPlayerState* PlayerState = Instigator->GetPlayerState<AControlPlayerState>())
		{
			return PlayerState->PlayerEndStats.FindOrAdd(Name);
		}
	}

	// It might be better to return a pointer and let the callee handle the nullptr case
	// but this keeps it simple.
	Default = 0;
	return Default;
}

//Return a players stats based on name. These stats are what's displayed in game rather than for stat tracking purposes
float& AGun::GetTrueStat(const FName Name) const
{
	if (const AController* Instigator = GetInstigatorController())
	{
		if (AControlPlayerState* PlayerState = Instigator->GetPlayerState<AControlPlayerState>())
		{
			return PlayerState->PlayerTrueStats.FindOrAdd(Name);
		}
	}
	static float Default = 0;
	return Default;
}

//Update a player's accuracy stat
void AGun::UpdateAccuracy() const
{
	if (const AController* Instigator = GetInstigatorController())
	{
		if (AControlPlayerState* PlayerState = Instigator->GetPlayerState<AControlPlayerState>())
		{
			const float BotHits = PlayerState->PlayerEndStats.FindRef("Bot Hits");
			const float ShotsFired = PlayerState->PlayerEndStats.FindRef("Shots Fired");

			PlayerState->PlayerTrueStats.FindOrAdd("Accuracy") = BotHits / ShotsFired;
			//MVP calculation
			PlayerState->UpdateScore();
		}
	}
}

// Get and Set
#pragma region Getters/Setters

void AGun::SetGunType(const EGunType NewType)
{
	//If ability system component, remove and re-add the updated stats
	if (IsValid(*GunValues.Find(NewType)))
	{
		if (GunGameplayEffect.IsValid())
		{
			const bool bRemoved = AbilitySystemComponent->RemoveActiveGameplayEffect(GunGameplayEffect, -1);
			UE_LOG(LogGun, Warning, TEXT("Removing attributes = %d"), bRemoved);
		}
		const FGameplayEffectContextHandle GunContextHandle = AbilitySystemComponent->MakeEffectContext();
		GunGameplayEffect = AbilitySystemComponent->ApplyGameplayEffectToSelf(
			(*GunValues.Find(NewType))->GetDefaultObject<UGameplayEffect>(),
			1,
			GunContextHandle);

		const UAttributeSet_Gun* GunAttributes = AbilitySystemComponent->GetSet<UAttributeSet_Gun>();
		SetGunRange(GunAttributes->GetRange());
	}

	if (!SwapGunStats.IsEmpty())
	{
		if (GunModGameplayEffect.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(GunModGameplayEffect, 1);
		}
		//in this context we will add the effect
		const FGameplayEffectContextHandle GunModContextHandle = AbilitySystemComponent->MakeEffectContext();

		//make and set the effect stats
		// set blank for all other stats!!! or better yet move the tag check to here and just set them all each time
		const FGameplayEffectSpecHandle SwapGunSpecHandle = AbilitySystemComponent->MakeOutgoingSpec(GunModValues, 1.f, GunModContextHandle);
		if (SwapGunSpecHandle.IsValid())
		{
			for (
				const auto& KeyTag : SwapGunStats)
			{
				SwapGunSpecHandle.Data->SetSetByCallerMagnitude(KeyTag.Key, KeyTag.Value);
				UE_LOG(LogGun, Warning, TEXT("Itterating over my  set %f"), KeyTag.Value);
			}

			GunModGameplayEffect = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SwapGunSpecHandle.Data.Get());
		}
	}
	GunType = NewType;
	UpdateMagazineSize();
	OnGunSwap.Broadcast(NewType);

	//Mesh Update
	if (AMainCharacter* MainCharacter = Cast<AMainCharacter>(GetOwner()))
	{
		//Switch Meshes, also change camera on the sniper for correct zoom
		int8 Index;
		switch (NewType)
		{
		case EGunType::Pistol:
			Index = 0;
			MainCharacter->UpdateCameraEnd(false);
			break;
		case EGunType::Gatling:
			Index = 1;
			MainCharacter->UpdateCameraEnd(false);
			break;
		case EGunType::Shotgun:
			Index = 2;
			MainCharacter->UpdateCameraEnd(false);
			break;
		case EGunType::SniperRifle:
			Index = 3;
			MainCharacter->UpdateCameraEnd(true);
			break;
		default:
			Index = 0;
		}

		ChangeGunMesh(Index);
		OnReticleChange.Broadcast(Index);
		OnReticleSwapAnim.Broadcast(NewType);
	}
}

UAbilitySystemComponent* AGun::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AGun::SetMod(UShopData_Item* Mod)
{
	//NEED TO CHANGE THIS TO ALLOW FOR 3 AND REMOVE THE RIGHT ONE
	if (CurrentMods.Num() < 4)
	{
		CurrentMods.Add(Mod);

		if (PureModPlayEffect.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(PureModPlayEffect, -1);
		}
		//in this context we will add the effect
		const FGameplayEffectContextHandle GunModContextHandle = AbilitySystemComponent->MakeEffectContext();

		//make and set the effect stats
		// set blank for all other stats!!! or better yet move the tag check to here and just set them all each time
		const FGameplayEffectSpecHandle AddModSpecHandle = AbilitySystemComponent->MakeOutgoingSpec(GunModValues, 1.f, GunModContextHandle);
		if (AddModSpecHandle.IsValid())
		{
			for (
				const auto& KeyTag : SwapGunStats)
			{
				AddModSpecHandle.Data->SetSetByCallerMagnitude(KeyTag.Key, KeyTag.Value);
				UE_LOG(LogGun, Warning, TEXT("Itterating over my  set %f"), KeyTag.Value);
			}

			PureModPlayEffect = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*AddModSpecHandle.Data.Get());
		}
	}
	else { UE_LOG(LogGun, Log, TEXT("Mods are full")); }
}

TArray<UShopData_Item*> AGun::GetMods()
{
	return CurrentMods;
}

EGunType AGun::GetGunType() const
{
	return GunType;
}

void AGun::SetAmmoType(const EAmmoType NewAmmo)
{
	//Update ammo values through GAS
	if (IsValid(*AmmoValues.Find(NewAmmo)))
	{
		if (AmmoGameplayEffect.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(AmmoGameplayEffect, -1);
		}
		const FGameplayEffectContextHandle AmmoContextHandle = AbilitySystemComponent->MakeEffectContext();

		AmmoGameplayEffect = AbilitySystemComponent->ApplyGameplayEffectToSelf(
			(*AmmoValues.Find(NewAmmo))->GetDefaultObject<UGameplayEffect>(),
			1,
			AmmoContextHandle);
	}

	AmmoType = NewAmmo;
	UpdateMagazineSize();

	//Update Ammo Mesh
	switch (NewAmmo)
	{
	case EAmmoType::Bullet:
		ChangeMagMesh(0);
		break;
	case EAmmoType::Piercing:
		ChangeMagMesh(1);
		break;
	case EAmmoType::Chain:
		ChangeMagMesh(2);
		break;
	case EAmmoType::Grenade:
		ChangeMagMesh(3);
		break;
	default:
		ChangeMagMesh(0);
	}
}

USkeletalMeshComponent* AGun::GetGunMesh()
{
	return GunMesh;
}

USkeletalMeshComponent* AGun::GetMagMesh()
{
	return MagMesh;
}

void AGun::SetReloadTime(const float NewTime)
{
	ReloadTime = NewTime;
}

float AGun::GetReloadTime() const
{
	return ReloadTime;
}

void AGun::SetCurrentClipSize(const int32 NewCurrSize)
{
	CurrentMagazineClipSize = NewCurrSize;
}

int32 AGun::GetCurrentClipSize() const
{
	return CurrentMagazineClipSize;
}

void AGun::SetCurrentNumBullets(const int32 NewNumBullets)
{
	CurrentMagazineNumBullets = NewNumBullets;
	OnReserveChange.Broadcast(GetCurrentNumBullets(), GetCurrentReserves());
}

int32 AGun::GetCurrentNumBullets() const
{
	return CurrentMagazineNumBullets;
}

void AGun::SetCurrentReserves(const float NewCurrReserves)
{
	CurrentMagazineCurrentReserves = NewCurrReserves;
	OnReserveChange.Broadcast(GetCurrentNumBullets(), GetCurrentReserves());
}

float AGun::GetCurrentReserves() const
{
	return CurrentMagazineCurrentReserves;
}

void AGun::SetMaxReserves(const float NewMaxReserves)
{
	CurrentMagazineMaxReserves = NewMaxReserves;
}

float AGun::GetMaxReserves() const
{
	return CurrentMagazineMaxReserves;
}

void AGun::SetGunRange(const float NewGunRange)
{
	GunRange = NewGunRange;
}

float AGun::GetGunRange() const
{
	return GunRange;
}

void AGun::SetChainBounceRange(const float NewChainBounceRange)
{
	ChainBounceRange = NewChainBounceRange;
}

float AGun::GetChainBounceRange() const
{
	return ChainBounceRange;
}

void AGun::SetCritDamageMultiplier(const float NewCritDamageMultiplier)
{
	CritDamageMultiplier = NewCritDamageMultiplier;
}

float AGun::GetCritDamageMultiplier() const
{
	return CritDamageMultiplier;
}

void AGun::SetCriticalDistance(const float NewCriticalDistance)
{
	CriticalDistance = NewCriticalDistance;
}

float AGun::GetCriticalDistance() const
{
	return CriticalDistance;
}

void AGun::AddAmmoToReserve(const int32 AmountToAdd)
{
	const int32 Current = GetCurrentReserves();
	const int32 Max = GetCurrentMagazineMaxReserves();
	if (Current + AmountToAdd < Max)
	{
		SetCurrentReserves(Current + AmountToAdd);
	}
	else
	{
		SetCurrentReserves(Max);
	}
}

int32 AGun::GetCurrentMagazineClipSize() const
{
	return CurrentMagazineClipSize;
}

void AGun::SetCurrentMagazineClipSize(const int32 NewCurrentMagazineClipSize)
{
	CurrentMagazineClipSize = NewCurrentMagazineClipSize;
}

int32 AGun::GetCurrentMagazineNumBullets() const
{
	return CurrentMagazineNumBullets;
}

void AGun::SetCurrentMagazineNumBullets(const int32 NewCurrentMagazineNumBullets)
{
	CurrentMagazineNumBullets = NewCurrentMagazineNumBullets;
}

int32 AGun::GetCurrentMagazineCurrentReserves() const
{
	return CurrentMagazineCurrentReserves;
}

void AGun::SetCurrentMagazineCurrentReserves(const int32 NewCurrentMagazineCurrentReserves)
{
	CurrentMagazineCurrentReserves = NewCurrentMagazineCurrentReserves;
}

int32 AGun::GetCurrentMagazineMaxReserves() const
{
	return CurrentMagazineMaxReserves;
}

void AGun::SetCurrentMagazineMaxReserves(const int32 NewCurrentMagazineMaxReserves)
{
	CurrentMagazineMaxReserves = NewCurrentMagazineMaxReserves;
}

bool AGun::CanFire() const
{
	return bCanFire;
}

void AGun::SetCanFire(const bool CanFire)
{
	bCanFire = CanFire;
}

bool AGun::GetReloading() const
{
	return bReloading;
}

void AGun::SetReloading(const bool Reloading)
{
	bReloading = Reloading;
}

bool AGun::GetAiming() const
{
	return bAiming;
}

void AGun::SetAiming(const bool Aiming)
{
	bAiming = Aiming;
}
#pragma endregion

//Gun Visual Function
void AGun::ChangeGunMesh(const int8 Index)
{
	if (GunMeshes.IsValidIndex(Index))
	{
		GunMesh->SetSkeletalMesh(GunMeshes[Index]);
	}
}

void AGun::ChangeMagMesh(const int8 Index)
{
	if (MagMeshes.IsValidIndex(Index))
	{
		MagMesh->SetSkeletalMesh(MagMeshes[Index]);
	}
}

//Gun Functions

FVector AGun::GetMuzzleTransform() const
{
	//Get location to shoot from via socket
	FName SocketName;

	switch (GetGunType())
	{
	case EGunType::Pistol:
		SocketName = "barrelEndPistol";
		break;
	case EGunType::Shotgun:
		SocketName = "barrelEndShotgun";
		break;
	case EGunType::Gatling:
		SocketName = "barrelEndGatling";
		break;
	case EGunType::SniperRifle:
		SocketName = "barrelEndSniper";
		break;
	default:
		SocketName = "barrelEndPistol";
		break;
	}

	return GunMesh->GetSocketLocation(SocketName);
}

void AGun::UpdateMagazineSize()
{
	const UAttributeSet_Gun* MyAttributes = AbilitySystemComponent->GetSet<UAttributeSet_Gun>();

	//Update values based on GAS
	SetCurrentClipSize(MyAttributes->GetMagSize());
	SetCurrentReserves(MyAttributes->GetReservesSize());
	SetMaxReserves(MyAttributes->GetReservesSize());
	SetReloadTime(MyAttributes->GetReloadSpeed());
	SetCurrentNumBullets(GetCurrentClipSize());

	if (AMainCharacter* MainCharacter = Cast<AMainCharacter>(GetOwner()))
	{
		MainCharacter->ActivateAmmoOutline(false);
	}

	//Update HUD bullet amount
	OnReserveChange.Broadcast(GetCurrentNumBullets(), GetCurrentReserves());
	OnGunChange.Broadcast(GetCurrentNumBullets(), GetCurrentReserves());
}

void AGun::ReloadGun()
{
	//Get the possible maximum amount of bullets you should grab
	const int32 MaxToGrab = GetCurrentClipSize() - GetCurrentNumBullets();
	const int32 BulletsGrabbed = FMath::Min(static_cast<int32>(GetCurrentReserves()), MaxToGrab);
	SetCurrentReserves(GetCurrentReserves() - BulletsGrabbed);

	//Announce low ammo and reload
	if (AMainCharacter* MainCharacter = Cast<AMainCharacter>(GetOwner()))
	{
		if (GetCurrentReserves() < GetCurrentClipSize())
		{
			GetPlayerHUD()->AddPlayerNotification(INVTEXT("Low ammo look for a station"));
			MainCharacter->ActivateAmmoOutline(true);
			MainCharacter->PlayVO(Y25::Cues::Player_Ammo_Low);
		}
		if (GetCurrentReserves() == 0)
		{
			MainCharacter->PlayVO(Y25::Cues::Player_Ammo_LastReload);
		}
	}
	else if (bPromptReload)
	{
		bPromptReload = false;
		GetPlayerHUD()->RemovePlayerNotification();
	}

	bShouldPlayReloadVoice = true;
	SetCurrentNumBullets(GetCurrentNumBullets() + BulletsGrabbed);

	//Update HUD bullets
	OnReserveChange.Broadcast(GetCurrentNumBullets(), GetCurrentReserves());
}

void AGun::ShootGun()
{
	//Can Shoot
	if (GetReloading() || !CanFire())
	{
		return;
	}

	if (AMainCharacter* MainCharacter = Cast<AMainCharacter>(GetOwner()))
	{
		// send notification to player no ammo
		if (CurrentMagazineNumBullets + CurrentMagazineCurrentReserves == 0)
		{
			GetPlayerHUD()->AddPlayerNotification(INVTEXT("Out of ammo find a station"));
			MainCharacter->PlayVO(Y25::Cues::Player_Ammo_Depleted);
		}

		if (MainCharacter->GetCurrentMontage() == MainCharacter->GetEmoteMontage())
		{
			MainCharacter->StopAnimMontage(MainCharacter->GetCurrentMontage());
		}
	}
	
	else if (bPromptReload)
	{
		GetPlayerHUD()->AddPlayerNotification(INVTEXT("Press X to reload"));
	}

	//Needs to reload/out of ammo
	if (GetCurrentNumBullets() <= 0)
	{
		if (const AMainCharacter* MainCharacter = Cast<AMainCharacter>(GetOwner()))
		{
			if (const AMainPlayerController* MainPlayerController = Cast<AMainPlayerController>(MainCharacter->GetController()))
			{
				if (UAbilitySystemComponent* AbilitySystemComponent = MainPlayerController->GetAbilitySystemComponent())
				{
					//Stop aiming and reload
					AbilitySystemComponent->AbilityLocalInputReleased(4);
					AbilitySystemComponent->AbilityLocalInputPressed(5);
				}
			}
		}
		return;
	}

	const UAttributeSet_Gun* MyAttributes = AbilitySystemComponent->GetSet<UAttributeSet_Gun>();

	// recoil
	if (const AMainCharacter* CharacterOwner = Cast<AMainCharacter>(GetOwner()))
	{
		if (!bRecoiling)
		{
			OriginalPitch = CharacterOwner->GetControlRotation().Pitch;
			RecoilTimeElapsed = 0.0f;
			LastRecoilPitch = 0.0f;
			LastRecoilYaw = 0.0f;
		}
		bRecoiling = true;
		RecoilDuration = FMath::Min(RecoilStepDuration + RecoilDuration, RecoilMaxDuration);

		//Controller rumble
		if (AMainPlayerController* MainPlayerController = Cast<AMainPlayerController>(CharacterOwner->GetController()))
		{
			MainPlayerController->PlayRumbleEffect(WeaponFireRumbleEffect);
		}
	}

	OnShootAnim.Broadcast();
	SetCanFire(false);
	SetReloading(true);

	//Fire Delay
	FTimerHandle FireDelayTimerHandle;
	TWeakObjectPtr WeakThis(this);
	
	GetWorldTimerManager().SetTimer(
		FireDelayTimerHandle,
		[WeakThis]
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			
			if (const AMainCharacter* MainCharacter = Cast<AMainCharacter>(WeakThis->GetOwner());
				MainCharacter && !MainCharacter->IsGliding() && !MainCharacter->GetHasLaunched() &&
				!MainCharacter->GetIsDead())
			{
				WeakThis->SetCanFire(true);
			}
			WeakThis->SetReloading(false);
		},
		MyAttributes->GetFireDelay(),
		false);

	// count shots fired
	GetStat(ShotsFiredStat)++;
	UpdateAccuracy();

	// set need to reload for prompt
	if (GetCurrentNumBullets() <= GetCurrentMagazineClipSize() * .3)
	{
		bPromptReload = true;
	}

	const int32 NumFired = MyAttributes->GetBulletsPerShot();
	SetCurrentNumBullets(GetCurrentNumBullets() - 1);

	OnFire.Broadcast(GetCurrentNumBullets());

	FVector TraceStart = GetMuzzleTransform();

	for (int i = 0; i < NumFired; i++)
	{
		//Line Trace or Grenade
		switch (GetAmmoType())
		{
		case EAmmoType::Bullet:
		case EAmmoType::Piercing:
		case EAmmoType::Chain:
			LineTrace(TraceStart);
			break;
		case EAmmoType::Grenade:
			SpawnGrenade(TraceStart);
			break;
		default:
			break;
		}
	}
}

FVector AGun::GetSpreadPoint() const
{
	const UAttributeSet_Gun* GunAttributes = AbilitySystemComponent->GetSet<UAttributeSet_Gun>();

	//Bullet spread and range
	const float CurrentSpreadAngle = FMath::DegreesToRadians(GunAttributes->GetSpreadAngle());
	const float CurrentRange = GunAttributes->GetRange();

	//Get Camera view
	FMinimalViewInfo ViewInfo;
	if (GetOwner())
	{
		GetOwner()->CalcCamera(0, ViewInfo);
	}
	else
	{
		ViewInfo.Location = GetActorLocation();
		ViewInfo.Rotation = GetActorRotation();
	}

	//Prepare line trace
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(GetOwner());

	FHitResult HitResult;
	GetWorld()->LineTraceSingleByChannel(
		HitResult,
		ViewInfo.Location + ViewInfo.Rotation.Vector() * 25,
		ViewInfo.Location + ViewInfo.Rotation.Vector() * CurrentRange,
		Y25::Collision::Channels::Weapon,
		QueryParams);

	//If it hit a blocking object
	if (HitResult.IsValidBlockingHit())
	{
		//Randomize spread
		const FVector SpreadDirection = FMath::VRandCone(
			HitResult.ImpactPoint - ViewInfo.Location,
			CurrentSpreadAngle);

		const float NewRange = FVector::Dist(HitResult.Location, ViewInfo.Location) + 5;

		return ViewInfo.Location + SpreadDirection * NewRange;
	}
	const FVector SpreadDirection = FMath::VRandCone(ViewInfo.Rotation.Vector(), CurrentSpreadAngle);
	return GetActorLocation() + SpreadDirection * CurrentRange;
}

void AGun::LineTrace(const FVector& TraceStart)
{
	FHitResult Hit;

	//Get the start and end locations
	const FVector TraceEnd = GetSpreadPoint();
	FVector LaunchDirection = TraceEnd - TraceStart;
	LaunchDirection.Normalize();

	//Ignore gun and player
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(GetOwner());
	TArray<FHitResult> Hits;

	FCollisionResponseParams ResponseParams;

	switch (GetAmmoType())
	{
	case EAmmoType::Piercing:
		//Piercing ammo should overlap enemies so it can hit multiple
		ResponseParams.CollisionResponse.SetResponse(Y25::Collision::Channels::Pawn, ECR_Overlap);

		FVector NewEnd = TraceStart + LaunchDirection * GetGunRange();

		GetWorld()->LineTraceMultiByChannel(
			Hits,
			TraceStart,
			NewEnd,
			Y25::Collision::Channels::Weapon,
			QueryParams,
			ResponseParams);

		LaserLineTraceEffect(LaunchDirection, Hits);
		break;

	//Bullet and chain are a single line trace with pawns blocking
	case EAmmoType::Bullet:
	case EAmmoType::Chain:
		ResponseParams.CollisionResponse.SetResponse(Y25::Collision::Channels::Pawn, ECR_Block);

		GetWorld()->LineTraceSingleByChannel(
			Hit,
			TraceStart,
			TraceEnd,
			Y25::Collision::Channels::Weapon,
			QueryParams,
			ResponseParams);

		BulletChainLineTraceEffect(LaunchDirection, Hit);
		break;

	//You shouldn't be here
	case EAmmoType::Grenade:
	default:
		break;
	}
}

void AGun::BulletChainLineTraceEffect(FVector& LaunchDirection, const FHitResult& Hit)
{
	const UAttributeSet_Gun* MyAttributes = AbilitySystemComponent->GetSet<UAttributeSet_Gun>();

	{
		// Create tracer effect
		TracerData->MuzzlePosition = GetMuzzleTransform();
		TracerData->AmmoType = GetAmmoType();
		TracerData->GunMesh = GunMesh;
		if (Hit.bBlockingHit)
		{
			TracerData->ImpactPositions = {Hit.ImpactPoint};
		}
		else
		{
			TracerData->ImpactPositions = {GetMuzzleTransform() + LaunchDirection * MyAttributes->GetRange()};
		}
		Gameplay::Cue(Y25::Cues::Gun_Tracer)
			.Instigator(GetInstigator())
			.SourceObject(TracerData)
			.Execute(this);
	}

	//Did it hit someone
	if (!IsValid(Hit.GetActor()))
	{
		return;
	}

	//Different for enemies/allies/other
	ABaseEnemy* HitEnemy = Cast<ABaseEnemy>(Hit.GetActor());
	AMainCharacter* HitAlly = Cast<AMainCharacter>(Hit.GetActor());
	AEnemySpawner* HitSpawner = Cast<AEnemySpawner>(Hit.GetActor());

	FGameplayCueParameters CueParam;
	CueParam.Instigator = GetOwner();

	//Play an on hit cue
	if (!HitEnemy && !HitAlly && !HitSpawner)
	{
		CueParam.Location = Hit.ImpactPoint;
		UAbilitySystemGlobals::Get().GetGameplayCueManager()->ExecuteGameplayCue_NonReplicated(
			this,
			Y25::Cues::Gun_AmmoHit_Other,
			CueParam);
		return;
	}

	//Check if chain ammo
	FGameplayTag EffectTag;
	if (GetAmmoType() == EAmmoType::Bullet)
	{
		EffectTag = Y25::Cues::Gun_AmmoHit_Bullet;
	}
	else
	{
		EffectTag = Y25::Cues::Gun_AmmoHit_Chain;
	}

	//If hit an enemy
	if (!HitAlly && !HitSpawner && HitEnemy && !HitEnemy->IsDead())
	{
		//On hit effect
		CueParam.SourceObject = HitEnemy;
		CueParam.Location = Hit.ImpactPoint;
		UAbilitySystemGlobals::Get().GetGameplayCueManager()->ExecuteGameplayCue_NonReplicated(
			HitEnemy,
			EffectTag,
			CueParam);

		//Bounce to multiple enemies if chain ammo
		if (GetAmmoType() == EAmmoType::Chain)
		{
			ChainBounce(HitEnemy, LaunchDirection);
		}

		float Damage = MyAttributes->GetBulletDamage();
		if (const USkeletalMeshComponent* EnemyMesh = HitEnemy->GetMesh())
		{
			//Check for hit close enough to the crit point on an enemy
			if (const float CritDistance = FVector::Dist(Hit.ImpactPoint, EnemyMesh->GetSocketLocation("Critical"));
				CritDistance <= GetCriticalDistance())
			{
				//Deal crit damage + activate crit effects
				UAbilitySystemGlobals::Get().GetGameplayCueManager()->ExecuteGameplayCue_NonReplicated(
					HitEnemy,
					Y25::Cues::Gun_AmmoHit_Crit,
					CueParam);

				Damage *= GetCritDamageMultiplier();
				GetStat(CriticalHitsStat)++;
				GetTrueStat(TEXT("Critical Hits"))++;

				if (const AMainCharacter* MainCharacter = Cast<AMainCharacter>(GetOwner()))
				{
					if (AControlPlayerState* ControlPlayerState =
						Cast<AControlPlayerState>(MainCharacter->GetPlayerState()))
					{
						ControlPlayerState->UpdateScore();
					}
				}
			}
		}

		DealDamage(Damage, LaunchDirection, HitEnemy);

		// add to bots killed
		if (HitEnemy->Health->GetHealth() <= 0)
		{
			GetStat(BotKillsStat)++;
			GetTrueStat(TEXT("Bot Kills"))++;

			if (const AMainCharacter* MainCharacter = Cast<AMainCharacter>(GetOwner()))
			{
				if (AControlPlayerState* ControlPlayerState =
					Cast<AControlPlayerState>(MainCharacter->GetPlayerState()))
				{
					ControlPlayerState->UpdateScore();
				}
			}
		}

		GetStat(EnemyHitsStat)++;
		UpdateAccuracy();
	}
	//Else if attacking an ally
	else if (!HitSpawner && HitAlly && !HitAlly->GetIsDead())
	{
		//On hit effect
		CueParam.SourceObject = HitAlly;
		CueParam.Location = HitAlly->GetActorLocation();
		UAbilitySystemGlobals::Get().GetGameplayCueManager()->ExecuteGameplayCue_NonReplicated(
			HitAlly,
			EffectTag,
			CueParam);

		//Bounce to multiple enemies if chain ammo
		if (GetAmmoType() == EAmmoType::Chain)
		{
			ChainBounce(HitAlly, LaunchDirection);
		}
		
		DealAllyDamage(MyAttributes->GetBulletDamage(), LaunchDirection, HitAlly);
		GetStat(FriendHitsStat)++;
		GetTrueStat(TEXT("Friend Hits"))++;
	}
	//If they shot the enemy drop-ship
	else
	{
		CueParam.SourceObject = HitSpawner;
		CueParam.Location = Hit.Location;
		UAbilitySystemGlobals::Get().GetGameplayCueManager()->ExecuteGameplayCue_NonReplicated(
			HitSpawner,
			Y25::Cues::Gun_AmmoHit_EnemySpawner,
			CueParam);
	}
}

void AGun::ChainBounce(APawn* HitEnemy, FVector& LaunchDirection)
{
	const UAttributeSet_Gun* MyAttributes = AbilitySystemComponent->GetSet<UAttributeSet_Gun>();

	//How many bounces and who has been hit?
	float RemainingBounces = MyAttributes->GetNumBounces();
	TSet<APawn*> CollidedTargets = TSet<APawn*>();
	CollidedTargets.Add(HitEnemy);

	FTimerHandle ChainDelayTimer;
	TWeakObjectPtr WeakThis = this;

	//Small timer for non-near instant bounce
	GetWorldTimerManager().SetTimer(
		ChainDelayTimer,
		[WeakThis, CollidedTargets, HitEnemy, RemainingBounces, LaunchDirection]()
		{
			if (!WeakThis.IsValid()) {return;}
			
			WeakThis->ChainBounceHelper(CollidedTargets, HitEnemy, RemainingBounces, LaunchDirection);
		},
		0.2f,
		false);
}

void AGun::ChainBounceHelper(TSet<APawn*> CollidedTargets, APawn* HitEnemy, float RemainingBounces, FVector LaunchDirection)
{
	if (IsPendingKillPending() || !IsValid(this) || !IsValid(AbilitySystemComponent)) {return;}
	const UAttributeSet_Gun* MyAttributes = AbilitySystemComponent->GetSet<UAttributeSet_Gun>();

	//check hit an enemy and still have bounces left
	if (!HitEnemy || !HitEnemy->IsA(ABaseEnemy::StaticClass()) || RemainingBounces <= 0)
	{
		return;
	}

	//Get new target
	ABaseEnemy* BounceTarget = FindNearestPawn(
		GetChainBounceRange(),
		HitEnemy->GetActorLocation(),
		CollidedTargets);

	//If valid
	if (BounceTarget)
	{
		//Update bounces left and who's been hit
		RemainingBounces--;
		CollidedTargets.Add(BounceTarget);

		{
			// Create tracer effect
			TracerData->MuzzlePosition = HitEnemy->GetActorLocation();
			TracerData->AmmoType = GetAmmoType();
			TracerData->GunMesh = GunMesh;
			TracerData->ImpactPositions = {BounceTarget->GetActorLocation()};

			Gameplay::Cue(Y25::Cues::Gun_Tracer)
				.Instigator(GetInstigator())
				.SourceObject(TracerData)
				.Execute(BounceTarget);
		}

		FGameplayCueParameters CueParam;
		CueParam.Instigator = GetOwner();
		CueParam.SourceObject = HitEnemy;
		CueParam.Location = BounceTarget->GetActorLocation();
		UAbilitySystemGlobals::Get().GetGameplayCueManager()->ExecuteGameplayCue_NonReplicated(
			BounceTarget,
			Y25::GameplayCues::Gun_AmmoHit_Chain,
			CueParam);

		//Less Damage based on number of bounces
		DealDamage(
			MyAttributes->GetBulletDamage() / pow(2, 5 - RemainingBounces),
			LaunchDirection,
			BounceTarget);
	}
	//If invalid target
	else
	{
		return;
	}

	//Repeat if targets available and bounces left
	FTimerHandle ChainDelayHelperTimer;
	TWeakObjectPtr WeakThis = this;

	GetWorldTimerManager().SetTimer(
		ChainDelayHelperTimer,
		[WeakThis, CollidedTargets, BounceTarget, RemainingBounces, LaunchDirection]
		{
			if (!WeakThis.IsValid()) {return;}
			
			WeakThis->ChainBounceHelper(CollidedTargets, BounceTarget, RemainingBounces, LaunchDirection);
		},
		.2f,
		false);
}

ABaseEnemy* AGun::FindNearestPawn(
	const float MaxDistance,
	const FVector& HitLocation,
	const TSet<APawn*> PreviousTargets) const
{
	//Ignore all previous targets
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(this);
	for (APawn* PreviousTarget : PreviousTargets)
	{
		CollisionParams.AddIgnoredActor(PreviousTarget);
	}

	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Block);

	//Sphere trace
	TArray<FHitResult> HitResults;
	const bool bFoundTarget = GetWorld()->SweepMultiByChannel(
		HitResults,
		HitLocation,
		HitLocation,
		FQuat::Identity,
		Y25::Collision::Channels::Weapon,
		FCollisionShape::MakeSphere(MaxDistance),
		CollisionParams,
		ResponseParams);

	if (!bFoundTarget)
	{
		return nullptr;
	}

	//Get closest pawn
	HitResults.Sort(
		[](const FHitResult& A, const FHitResult& B)
		{
			return A.Distance < B.Distance;
		});

	//return nearest enemy or nothing
	for (FHitResult HitResult : HitResults)
	{
		if (HitResult.GetActor()->IsA(ABaseEnemy::StaticClass()))
		{
			if (ABaseEnemy* BaseEnemy = Cast<ABaseEnemy>(HitResult.GetActor()))
			{
				if (BaseEnemy->IsDead())
				{
					continue;
				}
				return BaseEnemy;
			}
		}
	}
	return nullptr;
}

void AGun::LaserLineTraceEffect(FVector& LaunchDirection, const TArray<FHitResult>& Hits)
{
	const UAttributeSet_Gun* MyAttributes = AbilitySystemComponent->GetSet<UAttributeSet_Gun>();

	{
		// Check for blocking hit
		const FHitResult* BlockHit = nullptr;
		for (const FHitResult& Hit : Hits)
		{
			if (Hit.bBlockingHit)
			{
				BlockHit = &Hit;
				break;
			}
		}

		// Create tracer effect
		TracerData->MuzzlePosition = GetMuzzleTransform();
		TracerData->AmmoType = GetAmmoType();
		TracerData->GunMesh = GunMesh;
		if (BlockHit)
		{
			TracerData->ImpactPositions = {BlockHit->Location};
		}
		else
		{
			TracerData->ImpactPositions = {GetMuzzleTransform() + LaunchDirection * MyAttributes->GetRange()};
		}
		Gameplay::Cue(Y25::Cues::Gun_Tracer)
			.Instigator(GetInstigator())
			.SourceObject(TracerData)
			.Execute(this);
	}

	//If no hits
	if (Hits.Num() <= 0)
	{
		return;
	}
	int32 PierceCounter = 0;

	bool bHitCounted = true;

	//For each hit
	for (const FHitResult& HitResult : Hits)
	{
		if (!IsValid(HitResult.GetActor()))
		{
			continue;
		}

		//Enemies vs allies
		ABaseEnemy* HitEnemy = Cast<ABaseEnemy>(HitResult.GetActor());
		AMainCharacter* HitAlly = Cast<AMainCharacter>(HitResult.GetActor());
		AEnemySpawner* HitSpawner = Cast<AEnemySpawner>(HitResult.GetActor());

		FGameplayCueParameters CueParam;
		CueParam.Instigator = GetOwner();
		CueParam.Instigator = GetOwner();
		CueParam.SourceObject = this;

		//Checking what was hit
		if (!HitEnemy && !HitAlly && !HitSpawner)
		{
			CueParam.Location = HitResult.ImpactPoint;
			UAbilitySystemGlobals::Get().GetGameplayCueManager()->ExecuteGameplayCue_NonReplicated(
				this,
				Y25::Cues::Gun_AmmoHit_Other,
				CueParam);
			return;
		}

		if (!HitAlly && !HitSpawner && HitEnemy && !HitEnemy->IsDead())
		{
			//Ammo hit and damage to enemies
			CueParam.SourceObject = HitEnemy;
			CueParam.Location = HitResult.ImpactPoint;
			UAbilitySystemGlobals::Get().GetGameplayCueManager()->ExecuteGameplayCue_NonReplicated(
				HitEnemy,
				Y25::Cues::Gun_AmmoHit_Laser,
				CueParam);

			float Damage = MyAttributes->GetBulletDamage();
			if (const USkeletalMeshComponent* EnemyMesh = HitEnemy->GetMesh())
			{
				//check for critical
				if (const float CritDistance = FVector::Dist(HitResult.ImpactPoint,
					EnemyMesh->GetSocketLocation("Critical")); CritDistance <= GetCriticalDistance())
				{
					UAbilitySystemGlobals::Get().GetGameplayCueManager()->ExecuteGameplayCue_NonReplicated(
						HitEnemy,
						Y25::Cues::Gun_AmmoHit_Crit,
						CueParam);
					Damage *= GetCritDamageMultiplier();
					GetStat(CriticalHitsStat)++;
					GetTrueStat(TEXT("Critical Hits"))++;

					if (const AMainCharacter* MainCharacter = Cast<AMainCharacter>(GetOwner()))
					{
						if (AControlPlayerState* ControlPlayerState =
							Cast<AControlPlayerState>(MainCharacter->GetPlayerState()))
						{
							ControlPlayerState->UpdateScore();
						}
					}
				}
			}

			DealDamage(Damage, LaunchDirection, HitEnemy);

			// add to bots killed
			if (HitEnemy->Health->GetHealth() <= 0)
			{
				GetStat(BotKillsStat)++;
				GetTrueStat(TEXT("Bot Kills"))++;
				if (const AMainCharacter* MainCharacter = Cast<AMainCharacter>(GetOwner()))
				{
					if (AControlPlayerState* ControlPlayerState =
						Cast<AControlPlayerState>(MainCharacter->GetPlayerState()))
					{
						ControlPlayerState->UpdateScore();
					}
				}
			}

			if (bHitCounted)
			{
				GetStat(EnemyHitsStat)++;
				UpdateAccuracy();
				bHitCounted = false;
			}
		}
		//If hit an ally
		else if (!HitSpawner && HitAlly && !HitAlly->GetIsDead())
		{
			//Ammo hit and damage to allies
			CueParam.SourceObject = HitAlly;
			CueParam.Location = HitAlly->GetActorLocation();
			UAbilitySystemGlobals::Get().GetGameplayCueManager()->ExecuteGameplayCue_NonReplicated(
				HitAlly,
				Y25::Cues::Gun_AmmoHit_Laser,
				CueParam);

			DealAllyDamage(MyAttributes->GetBulletDamage(), LaunchDirection, HitAlly);
			if (bHitCounted)
			{
				GetStat(FriendHitsStat)++;
				GetTrueStat(TEXT("Friend Hits"))++;
				bHitCounted = false;
			}
		}
		else
		{
			CueParam.SourceObject = HitSpawner;
			CueParam.Location = HitResult.Location;
			UAbilitySystemGlobals::Get().GetGameplayCueManager()->ExecuteGameplayCue_NonReplicated(
				HitSpawner,
				Y25::Cues::Gun_AmmoHit_EnemySpawner,
				CueParam);
		}

		//increment and check if done with pierces
		PierceCounter++;
		if (PierceCounter >= MyAttributes->GetNumPierces() + 1)
		{
			break;
		}
	}
}

void AGun::DealDamage(const float DamageToDeal, FVector& LaunchDirection, ABaseEnemy* Target)
{
	//Can't launch enemies upwards
	if (LaunchDirection.Z > 0)
	{
		LaunchDirection.Z = 0;
	}
	LaunchDirection.Normalize();

	if (IsPowerStationShipAlive)
	{
		PowerStationAbility(Target);
	}

	//Deal damage to enemies
	AController* InstigatingActor = Cast<AMainCharacter>(GetOwner())->GetController();

	const TSubclassOf<UDamageType> ValidDamageTypeClass = UDamageType::StaticClass();
	const FDamageEvent DamageEvent(ValidDamageTypeClass);

	Target->TakeDamage(
		DamageToDeal,
		DamageEvent,
		InstigatingActor,
		this);
}

	

void AGun::DealAllyDamage(const float DamageToDeal, FVector& LaunchDirection, AMainCharacter* Target)
{
	//Can't launch upwards
	if (LaunchDirection.Z > 0)
	{
		LaunchDirection.Z = 0;
	}
	LaunchDirection.Normalize();

	const UAttributeSet_Gun* MyAttributes = AbilitySystemComponent->GetSet<UAttributeSet_Gun>();

	//Friendly fire voice
	Target->PlayVO(Y25::Cues::Player_FriendlyFire);

	//Gameplay spec to deal damage through ability system component
	if (UAbilitySystemComponent* AbilitySystemComponent = Target->GetAbilitySystemComponent())
	{
		FGameplayEffectContextHandle EffectContextHandle = AbilitySystemComponent->MakeEffectContext();
		EffectContextHandle.AddSourceObject(this);

		const FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->
			MakeOutgoingSpec(DamageEffect, 1.f, EffectContextHandle);

		if (SpecHandle.IsValid())
		{
			SpecHandle.Data->SetSetByCallerMagnitude(Y25::Tags::GameplayEffect_Health_Damaged, DamageToDeal);
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}

	//Launch allies, less launch in air
	float LaunchForce = MyAttributes->GetKnockBackForce();
	if (!Target->GetMovementComponent()->IsMovingOnGround())
	{
		LaunchForce = LaunchForce / 1000;
	}

	Target->LaunchCharacter(
		LaunchDirection * LaunchForce,
		true,
		false);
}

void AGun::SpawnGrenade(FVector& SpawnLocation) const
{
	const UAttributeSet_Gun* MyAttributes = AbilitySystemComponent->GetSet<UAttributeSet_Gun>();

	// Randomize where the grenade will launch
	AMainCharacter* MainCharacter = Cast<AMainCharacter>(GetOwner());

	const FVector AimVector = GetSpreadPoint();

	const FRotator SpawnRotation = (AimVector - GetMuzzleTransform()).Rotation();

	//Set up transform
	const FTransform SpawnTransform(SpawnRotation, SpawnLocation);

	//Get self as instigator for enemies to aggro to
	AMainPlayerController* InstigatingActor = Cast<AMainPlayerController>(
		Cast<AMainCharacter>(GetOwner())->GetController());

	//Delay spawn
	if (AGrenadeProjectile* Projectile = GetWorld()->SpawnActorDeferred<AGrenadeProjectile>(
		GrenadeClass,
		FTransform::Identity,
		Cast<AActor>(MainCharacter),
		InstigatingActor->GetPawn(),
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
	{
		//Initialize the rest of the needed values
		const float Speed = MyAttributes->GetBulletSpeed();
		Projectile->NewInitialize(
			MyAttributes->GetBulletDamage(),
			Speed,
			Speed,
			InstigatingActor,
			MyAttributes->GetKnockBackForce());

		//Finish spawn
		if (Projectile)
		{
			Projectile->FinishSpawning(
				SpawnTransform,
				false,
				nullptr,
				ESpawnActorScaleMethod::MultiplyWithRoot);
		}
	}
}

void AGun::HandleOnPowerStationDeath()
{
	IsPowerStationShipAlive = false;
	UE_LOG(LogGun, Log, TEXT("shipdeath event triggered"));
}

void AGun::PowerStationAbility(AActor* Target)
{
	const AActor* CurrentTarget = Cast<AActor>(Target);

	if (UAbilitySystemComponent* Asc = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CurrentTarget))
	{
		const FGameplayEffectContextHandle EffectContext = Asc->MakeEffectContext();
		const FGameplayEffectSpecHandle SpecHandle = Asc->MakeOutgoingSpec(PowerStationShipEffect, 1.0f, EffectContext);
		if (SpecHandle.IsValid())
		{
			Asc->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
	else
	{
		UE_LOG(LogGun, Log, TEXT("No ASC in Enemies"));
	}
}
