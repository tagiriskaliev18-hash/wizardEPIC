// SpellProjectile.h - the fireball (and any other projectile spell): glowing core, light, trail,
// detonation on contact or after its lifetime.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpellProjectile.generated.h"

class UPointLightComponent;
class UProjectileMovementComponent;
class USphereComponent;
class USpellDataAsset;
class UStaticMeshComponent;
class UAudioComponent;

UCLASS()
class WIZARDEPIC_API AWizardProjectile : public AActor
{
	GENERATED_BODY()

public:
	AWizardProjectile();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Projectile)
	TObjectPtr<USphereComponent> Collision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Projectile)
	TObjectPtr<UStaticMeshComponent> Visual = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Projectile)
	TObjectPtr<UProjectileMovementComponent> Movement = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Projectile)
	TObjectPtr<UPointLightComponent> Light = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USpellDataAsset> Spell = nullptr;

	/** 0 = tap cast, 1 = fully charged. */
	float ChargeT01 = 0.f;

	TWeakObjectPtr<AActor> CasterActor;

	/** Spawns and launches a projectile. Returns null when Spell is missing. */
	static AWizardProjectile* Launch(USpellDataAsset* Spell, float InChargeT01, const FVector& Start, const FVector& Direction, AActor* Caster);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Detonates now. Hit may be null (lifetime expired). */
	void Detonate(const FHitResult* Hit);

private:
	float Age = 0.f;
	bool bDetonated = false;
	FTimerHandle LifeHandle;

	UFUNCTION()
	void OnComponentHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void OnLifeExpired();
};
