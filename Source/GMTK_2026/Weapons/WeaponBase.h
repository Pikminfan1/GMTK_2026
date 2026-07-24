// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class USkeletalMeshComponent;
class AProjectileBase;
class UCameraComponent;

UENUM(BlueprintType)
enum class EWeaponFireMode : uint8
{
	Hitscan,
	Projectile
};

/**
 * Abstract base for anything the player (or later, an enemy) can wield and fire.
 * Covers both hitscan and projectile weapons through the same class rather than
 * splitting into two hierarchies - which behavior a given weapon uses is just a
 * property (FireMode), not a difference in class structure.
 */
UCLASS()
class GMTK_2026_API AWeaponBase : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AWeaponBase();
	
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void Fire();
	
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void Reload();
	
	/** Attaches this weapon's ownership to whoever is wielding it (player or enemy). */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void Equip(AActor* NewOwnerActor);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Cached once in Equip() rather than re-cast on every shot - null if the current owner has no camera to aim from.
	UPROPERTY()
	TObjectPtr<UCameraComponent> CachedAimCamera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;
	
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	EWeaponFireMode FireMode = EWeaponFireMode::Hitscan;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	FName MuzzleSocketName = "Muzzle";
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	float Damage = 10.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	float HitscanRange = 5000.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	TSubclassOf<AProjectileBase> ProjectileClass;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	int32 MaxAmmo = 30;
	
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	int32 CurrentAmmo = 0;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Debug")
	bool bShowDebugTrace = true;
	
	void FireHitscan();
	void FireProjectile();

};
