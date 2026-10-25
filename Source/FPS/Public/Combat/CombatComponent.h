// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatComponent.generated.h"


class AWeapon;
class UWeaponData;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPS_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	// Cycle to the next weapon in the inventory
	void InitiateCycleWeapon();

	void InitiateFireWeaponPressed();
	void InitiateFireWeaponReleased();
	
	void InitiateReloadWeapon();
	
	void InitiateAimPressed();
	void InitiateAimReleased();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FPS|Weapon")
	TObjectPtr<UWeaponData> WeaponData;

	void Equip(AWeapon* Weapon);
	void SpawnInventory();
	void DestroyInventory();

	UPROPERTY(BlueprintReadOnly, Replicated)
	bool bAiming;
	
protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentWeapon, meta=(AllowPrivateAccess=true))
	TObjectPtr<AWeapon> CurrentWeapon;

	UFUNCTION()
	void OnRep_CurrentWeapon(AWeapon* LastWeapon);
	
	UPROPERTY(Transient, Replicated)
	TArray<AWeapon*> Inventory;
	
	UPROPERTY(EditDefaultsOnly, Category="FPS|Weapon")
	TArray<TSubclassOf<AWeapon>> DefaultWeaponClasses;

	AWeapon* SpawnWeapon(TSubclassOf<AWeapon> WeaponClass) const;

	UFUNCTION(Server, Reliable)
	void Server_Aim(bool bPressed);

	void Local_Aim(bool bPressed);
};
