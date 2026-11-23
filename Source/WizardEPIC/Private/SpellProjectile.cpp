#include "SpellProjectile.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(SpellProjectile)

AWizardProjectile::AWizardProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(30.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	Collision->SetGenerateOverlapEvents(false);

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCastShadow(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->bRotationFollowsVelocity = true;
	Movement->InitialSpeed = 4000.f;
	Movement->MaxSpeed = 9000.f;
	Movement->ProjectileGravityScale = 0.05f;
	Movement->bShouldBounce = false;

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Collision);
}

AWizardProjectile* AWizardProjectile::Launch(USpellDataAsset* Spell, float InChargeT01, const FVector& Start, const FVector& Direction, AActor* Caster)
{
	return nullptr;
}

void AWizardProjectile::BeginPlay()
{
	Super::BeginPlay();
}

void AWizardProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AWizardProjectile::Detonate(const FHitResult* Hit)
{
}

void AWizardProjectile::OnComponentHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
}

void AWizardProjectile::OnLifeExpired()
{
}
