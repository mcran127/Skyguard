// Copyright Brigham Young University. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "Delegates/DelegateCombinations.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Y25/Enemies/BaseEnemy.h"
#include "Y25/Enemies/EnemySpawner/EnemySpawner.h"
#include "Y25/Weapons/GrenadeProjectile.h"
#include "Y25/Game/Control/ControlHUD.h"

#include "Gun.generated.h"

class AMainCharacter;
class UShopData_Item;
class UGunTracerData;
class AGunVisualEffects;
class UAttributeSet_Gun;

//Enums
#pragma region Enums

class USkeletalMesh;

UENUM(BlueprintType)
enum class EModType : uint8
{
	Barrel,
	Ammo,
	Stat,
};

UENUM(BlueprintType)
enum class EGunType : uint8
{
	Pistol,
	Gatling,
	Shotgun,
	SniperRifle,
};

UENUM(BlueprintType)
enum class EAmmoType : uint8
{
	Bullet,
	Piercing,
	Grenade,
	Chain,
};

#pragma endregion

DECLARE_MULTICAST_DELEGATE_OneParam(FOnFire, int32 currBullets);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnReserveChange, int32 currBullets, float currReserves);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGunChange, int32 currBullets, float currReserves);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnReticleChange, int32 newVal);

DECLARE_MULTICAST_DELEGATE(FOnReloadAnim);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnReticleSwapAnim, EGunType gunType);

DECLARE_MULTICAST_DELEGATE(FOnShootAnim);

UCLASS(Abstract, Config=Game)
class Y25_API AGun : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AGun();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	//To broadcast
	FOnFire OnFire;

	FOnReserveChange OnReserveChange;

	FOnReticleChange OnReticleChange;

	FOnGunChange OnGunChange;

	FOnShootAnim OnShootAnim;

	FOnReticleSwapAnim OnReticleSwapAnim;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGunSwap, EGunType, GunType);

	UPROPERTY(BlueprintAssignable)
	FOnGunSwap OnGunSwap;

	//Gameplay effects
	TSubclassOf<UGameplayEffect> InitialAttributesEffect;

	//Gun and Ammo types
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Gun Values")
	TMap<EGunType, TSubclassOf<UGameplayEffect>> GunValues;

	UPROPERTY(EditDefaultsOnly, Category="Gun Values")
	TMap<EAmmoType, TSubclassOf<UGameplayEffect>> AmmoValues;

	//gameplay effect for setting the gun mods
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Gun Values")
	TSubclassOf<UGameplayEffect> GunModValues;

	UPROPERTY(BlueprintReadWrite)
	TMap<FGameplayTag, float> SwapGunStats;

	//Damage effect for friendly fire
	UPROPERTY(EditDefaultsOnly, Category="Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

	UFUNCTION(BlueprintCallable)
	void SetMod(UShopData_Item* Mod);

	UFUNCTION(BlueprintCallable)
	TArray<UShopData_Item*> GetMods();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UShopData_Item>> CurrentMods;

	//rumble
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haptics")
	TObjectPtr<UForceFeedbackEffect> WeaponFireRumbleEffect;

private:
#pragma region gunVars

	//Gun Mesh
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GunMesh", meta=(AllowPrivateAccess=true))
	TObjectPtr<USkeletalMeshComponent> GunMesh;

	UPROPERTY(EditAnywhere, Category="GunMesh")
	TArray<TObjectPtr<USkeletalMesh>> GunMeshes;

	//Magazine Mesh
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GunMesh", meta=(AllowPrivateAccess=true))
	TObjectPtr<USkeletalMeshComponent> MagMesh;

	UPROPERTY(EditAnywhere, Category="GunMesh")
	TArray<TObjectPtr<USkeletalMesh>> MagMeshes;

	//effects

	FActiveGameplayEffectHandle AmmoGameplayEffect;

	FActiveGameplayEffectHandle PureModPlayEffect;

	FActiveGameplayEffectHandle GunGameplayEffect;

	//keeps track of the current gun mod effects on the player
	FActiveGameplayEffectHandle GunModGameplayEffect;

	//GUN TYPE INFO

	UPROPERTY(EditAnywhere, Category="Weapon")
	float GunRange = 10000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun", meta = (AllowPrivateAccess = true))
	float ChainBounceRange = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun", meta = (AllowPrivateAccess = true))
	float CritDamageMultiplier = 1.5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun", meta = (AllowPrivateAccess = true))
	float CriticalDistance = 20;

	// Current Gun
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun", meta = (AllowPrivateAccess = true))
	EGunType GunType = EGunType::Pistol;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun", meta = (AllowPrivateAccess = true))
	EAmmoType AmmoType = EAmmoType::Bullet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun", meta = (AllowPrivateAccess = true))
	float ReloadTime = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun", meta = (AllowPrivateAccess = true))
	int32 CurrentMagazineClipSize = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun", meta = (AllowPrivateAccess = true))
	int32 CurrentMagazineNumBullets = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun", meta = (AllowPrivateAccess = true))
	int32 CurrentMagazineCurrentReserves = 50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun", meta = (AllowPrivateAccess = true))
	int32 CurrentMagazineMaxReserves = 100;

	// Capsule Trace/Aim Assist

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gun", meta = (AllowPrivateAccess = true))
	int32 CapRadius = 25;

	// Tracer/Ammo trail
	UPROPERTY(Transient)
	TObjectPtr<UGunTracerData> TracerData;

	//counter for recoil and pitch
	float RecoilTimeElapsed;
	float OriginalPitch;
	float TargetPitch;
	float LastRecoilAmount = 0.0f;

	// recoil using noise 
	float LastRecoilPitch = 0.0f;
	float LastRecoilYaw = 0.0f;

	float RecoilNoiseFrequency = 5.0f;
	UPROPERTY(EditDefaultsOnly, Category="Recoil")
	float RecoilPitchIntensity = 1.75f;
	UPROPERTY(EditDefaultsOnly, Category="Recoil")
	float RecoilYawIntensity = 1.75f;
	float RecoilCurveStrength = 5.0f;

	float RecoilDuration = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category="Recoil")
	float RecoilDampen = 3.5f;

	UPROPERTY(EditDefaultsOnly, Category="Recoil")
	float RecoilStepDuration = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category="Recoil")
	float RecoilMaxDuration = 0.3f;

#pragma endregion

public:
	//Bools
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="STUPIDBOOLS")
	bool bCanFire = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="STUPIDBOOLS")
	bool bReloading = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="STUPIDBOOLS")
	bool bAiming = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="STUPIDBOOLS")
	bool bRecoiling = false;

	bool bPromptReload = false;

	bool bShouldPlayReloadVoice = true;

	//HUD Functions
	AControlHUD* GetPlayerHUD() const;

	//Apply the Gameplay Effect
	virtual void PowerStationAbility(AActor* Target);

	//Called when the PowerStationShip dies
	UFUNCTION()
	virtual void HandleOnPowerStationDeath();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	FVector GetMuzzleTransform() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AimOffset")
	float XOffset = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AimOffset")
	float YOffset = 0;

#pragma region gunFunctions

	//Gun Visual Function

	void ChangeGunMesh(int8 Index);

	void ChangeMagMesh(int8 Index);

	//Gun Functions

	UFUNCTION(BlueprintCallable, Category = "Y25|Gun")
	void UpdateMagazineSize();

	UFUNCTION(BlueprintCallable, Category = "Y25|Gun")
	void ReloadGun();

	UFUNCTION(BlueprintCallable, Category = "Y25|Gun")
	void ShootGun();

	UFUNCTION(BlueprintCallable, Category = "Y25|Gun")
	FVector GetSpreadPoint() const;

	UFUNCTION(BlueprintCallable, Category = "Y25|Gun")
	void LineTrace(const FVector& TraceStart);

	UFUNCTION(BlueprintCallable, Category = "Y25|Gun")
	void BulletChainLineTraceEffect(FVector& LaunchDirection, const FHitResult& Hit);

	UFUNCTION(BlueprintCallable, Category = "Y25|Gun")
	void DealDamage(float DamageToDeal, FVector& LaunchDirection, ABaseEnemy* Target);

	UFUNCTION(BlueprintCallable, Category = "Y25|Gun")
	void DealAllyDamage(float DamageToDeal, FVector& LaunchDirection, AMainCharacter* Target);

	UFUNCTION(BlueprintCallable, Category = "Y25|Gun")
	void ChainBounce(APawn* HitEnemy, FVector& LaunchDirection);

	UFUNCTION(BlueprintCallable, Category = "Y25|Gun")
	void ChainBounceHelper(TSet<APawn*> CollidedTargets, APawn* HitEnemy, float RemainingBounces, FVector LaunchDirection);

	UFUNCTION(BlueprintCallable, Category = "Y25|Gun")
	ABaseEnemy* FindNearestPawn(const float MaxDistance, const FVector& HitLocation, const TSet<APawn*> PreviousTargets) const;

	UFUNCTION(BlueprintCallable, Category = "Y25|Gun")
	void LaserLineTraceEffect(FVector& LaunchDirection, const TArray<FHitResult>& Hits);

	UFUNCTION(BlueprintCallable, Category = "Y25|Gun")
	void SpawnGrenade(FVector& SpawnLocation) const;

	UFUNCTION(BlueprintCallable, Category = "Y25|Gun")
	void AddAmmoToReserve(const int32 AmountToAdd);

	//Get/Set New Gun Types
#pragma region GetterSetter
	UFUNCTION(BlueprintCallable, Category="GetSet")
	void SetGunType(EGunType NewType);

	UFUNCTION(BlueprintCallable, Category="GetSet")
	EGunType GetGunType() const;

	UFUNCTION(BlueprintCallable, Category="GetSet")
	void SetAmmoType(EAmmoType NewAmmo);

	UFUNCTION(BlueprintCallable, Category="GetSet")
	EAmmoType GetAmmoType() const;

	UFUNCTION()
	USkeletalMeshComponent* GetGunMesh();

	UFUNCTION()
	USkeletalMeshComponent* GetMagMesh();

	UFUNCTION(Category="GetSet")
	void SetReloadTime(float NewTime);

	UFUNCTION(Category="GetSet")
	float GetReloadTime() const;

	UFUNCTION(Category="GetSet")
	void SetCurrentClipSize(int32 NewCurrSize);

	UFUNCTION(Category="GetSet")
	int32 GetCurrentClipSize() const;

	UFUNCTION(Category="GetSet")
	void SetCurrentNumBullets(int32 NewNumBullets);

	UFUNCTION(Category="GetSet")
	int32 GetCurrentNumBullets() const;

	UFUNCTION(Category="GetSet")
	int32 GetCurrentMagazineClipSize() const;

	UFUNCTION(Category="GetSet")
	void SetCurrentMagazineClipSize(int32 NewCurrentMagazineClipSize);

	UFUNCTION(Category="GetSet")
	int32 GetCurrentMagazineNumBullets() const;

	UFUNCTION(Category="GetSet")
	void SetCurrentMagazineNumBullets(int32 NewCurrentMagazineNumBullets);

	UFUNCTION(Category="GetSet")
	int32 GetCurrentMagazineCurrentReserves() const;

	UFUNCTION(Category="GetSet")
	void SetCurrentMagazineCurrentReserves(int32 NewCurrentMagazineCurrentReserves);

	UFUNCTION(Category="GetSet")
	int32 GetCurrentMagazineMaxReserves() const;

	UFUNCTION(Category="GetSet")
	void SetCurrentMagazineMaxReserves(int32 NewCurrentMagazineMaxReserves);

	UFUNCTION(Category="GetSet")
	bool CanFire() const;

	UFUNCTION(Category="GetSet")
	void SetCanFire(bool CanFire);

	UFUNCTION(Category="GetSet")
	bool GetReloading() const;

	UFUNCTION(Category="GetSet")
	void SetReloading(bool Reloading);

	UFUNCTION(Category="GetSet")
	bool GetAiming() const;

	UFUNCTION(Category="GetSet")
	void SetAiming(bool Aiming);

	UFUNCTION(Category="GetSet")
	void SetCurrentReserves(float NewCurrReserves);

	UFUNCTION(Category="GetSet")
	float GetCurrentReserves() const;

	UFUNCTION(Category="GetSet")
	void SetMaxReserves(float NewMaxReserves);

	UFUNCTION(Category="GetSet")
	float GetMaxReserves() const;

	UFUNCTION(Category="GetSet")
	void SetGunRange(float NewGunRange);

	UFUNCTION(Category="GetSet")
	float GetGunRange() const;

	UFUNCTION(Category="GetSet")
	void SetChainBounceRange(float NewChainBounceRange);

	UFUNCTION(Category="GetSet")
	float GetChainBounceRange() const;

	UFUNCTION(Category="GetSet")
	void SetCritDamageMultiplier(float NewCritDamageMultiplier);

	UFUNCTION(Category="GetSet")
	float GetCritDamageMultiplier() const;

	UFUNCTION(Category="GetSet")
	void SetCriticalDistance(float NewCriticalDistance);

	UFUNCTION(Category="GetSet")
	float GetCriticalDistance() const;

#pragma endregion GetterSetter

#pragma endregion

protected:
	//Check if Effect should apply
	bool IsPowerStationShipAlive = false;

	bool IsCommandArmoryShipAlive = false;

	virtual void BeginPlay() override;

	virtual void PostInitializeComponents() override;

	int32& GetStat(FName Name) const;

	float& GetTrueStat(FName Name) const;

	void UpdateAccuracy();

	//AttributeSet
	UPROPERTY()
	TObjectPtr<UAttributeSet_Gun> AttributeSet_Gun;

	UPROPERTY(Transient)
	//UAbilitySystemComponent* AbilitySystemComponent;
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	//Slow Down Enemy Effect
	UPROPERTY(EditAnywhere, Category="ShipEffect")
	TSubclassOf<UGameplayEffect> PowerStationShipEffect;

	//Increase Reload Speed Effect
	UPROPERTY(EditAnywhere, Category="ShipEffect")
	TSubclassOf<UGameplayEffect> ArmoryShipEffect;

	//Projectile Type To Fire
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grenade")
	TSubclassOf<AGrenadeProjectile> GrenadeClass;
};
