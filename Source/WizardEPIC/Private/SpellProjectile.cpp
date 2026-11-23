#include "SpellProjectile.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FXSubsystem.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SpellCasting.h"
#include "SpellDataAsset.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"
#include "WizardAudioSubsystem.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(SpellProjectile)

AWizardProjectile::AWizardProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(28.f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetNotifyRigidBodyCollision(true);
	Collision->OnComponentHit.AddDynamic(this, &AWizardProjectile::OnComponentHit);
	RootComponent = Collision;

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCastShadow(false);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Collision);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(1.5e6f);
	Light->SetAttenuationRadius(1100.f);
	Light->SetCastShadows(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->InitialSpeed = 4200.f;
	Movement->MaxSpeed = 6000.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->ProjectileGravityScale = 0.05f;
}

AWizardProjectile* AWizardProjectile::Launch(USpellDataAsset* InSpell, float InChargeT01, const FVector& Start, const FVector& Direction, AActor* Caster)
{
	if (!InSpell || !Caster)
	{
		return nullptr;
	}
	UWorld* World = Caster->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FVector DirN = Direction.GetSafeNormal();
	FActorSpawnParameters Params;
	Params.Owner = Caster;
	Params.Instigator = Cast<APawn>(Caster);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AWizardProjectile* Proj = World->SpawnActor<AWizardProjectile>(AWizardProjectile::StaticClass(), Start, DirN.Rotation(), Params);
	if (!Proj)
	{
		return nullptr;
	}

	Proj->Spell = InSpell;
	Proj->ChargeT01 = FMath::Clamp(InChargeT01, 0.f, 1.f);
	Proj->CasterActor = Caster;

	const float ChargeScale = InSpell->GetChargeScale(Proj->ChargeT01);
	if (Proj->Collision)
	{
		Proj->Collision->SetSphereRadius(InSpell->ProjectileRadius * FMath::Lerp(1.f, 1.45f, Proj->ChargeT01));
		Proj->Collision->IgnoreActorWhenMoving(Caster, true);
	}
	if (Proj->Movement)
	{
		Proj->Movement->InitialSpeed = InSpell->ProjectileSpeed;
		Proj->Movement->MaxSpeed = InSpell->ProjectileSpeed * 1.25f;
		Proj->Movement->ProjectileGravityScale = InSpell->ProjectileGravityScale;
		Proj->Movement->Velocity = DirN * InSpell->ProjectileSpeed;
	}
	if (Proj->Light)
	{
		Proj->Light->SetLightColor(InSpell->Color);
		Proj->Light->SetIntensity(1.5e6f * ChargeScale);
	}

	if (UWizardFXSubsystem* FX = UWizardFXSubsystem::Get(World))
	{
		if (Proj->Visual)
		{
			UStaticMesh* Mesh = FX->GetFXMesh(TEXT("SM_FX_Sphere"));
			if (!Mesh)
			{
				Mesh = Cast<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")).TryLoad());
			}
			if (Mesh)
			{
				Proj->Visual->SetStaticMesh(Mesh);
			}

			UMaterialInterface* Mat = FX->GetFXMaterial(TEXT("M_FX_Core"));
			if (!Mat)
			{
				Mat = Cast<UMaterialInterface>(FSoftObjectPath(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")).TryLoad());
			}
			if (Mat)
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, Proj->Visual);
				MID->SetVectorParameterValue(TEXT("Tint"), InSpell->Color);
				MID->SetScalarParameterValue(TEXT("EmissiveScale"), 28.f);
				MID->SetScalarParameterValue(TEXT("Fade"), 1.f);
				Proj->Visual->SetMaterial(0, MID);
			}

			const float VisualScale = (InSpell->ProjectileRadius / 50.f) * FMath::Lerp(1.f, 1.55f, Proj->ChargeT01);
			Proj->Visual->SetRelativeScale3D(FVector(VisualScale));
		}

		FX->SpawnNiagaraAttached(InSpell->NiagaraTravel, Proj->Collision, NAME_None, ChargeScale);
	}

	if (UWizardAudioSubsystem* Audio = UWizardAudioSubsystem::Get(World))
	{
		if (!InSpell->TravelSound.IsNone())
		{
			Audio->PlayAttached(InSpell->TravelSound, Proj->Collision, NAME_None, 0.75f, 1.f);
		}
	}

	World->GetTimerManager().SetTimer(Proj->LifeHandle, Proj, &AWizardProjectile::OnLifeExpired, FMath::Max(0.2f, InSpell->ProjectileLife), false);
	return Proj;
}

void AWizardProjectile::BeginPlay()
{
	Super::BeginPlay();
}

void AWizardProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bDetonated || !Spell)
	{
		return;
	}

	Age += DeltaTime;
	if (UWizardFXSubsystem* FX = UWizardFXSubsystem::Get(this))
	{
		const float ChargeScale = Spell->GetChargeScale(ChargeT01);
		const FVector Loc = GetActorLocation();
		const FVector Back = -GetActorForwardVector();

		FFXSpriteParams Ember;
		Ember.Kind = EFXSpriteKind::Ember;
		Ember.Mode = EFXSpriteMode::VelocityAligned;
		Ember.Position = Loc + FMath::VRand() * (14.f * ChargeScale);
		Ember.Velocity = Back * FMath::FRandRange(180.f, 450.f) + FMath::VRand() * 140.f;
		Ember.Life = FMath::FRandRange(0.22f, 0.45f);
		Ember.Size = 22.f * ChargeScale;
		Ember.SizeEnd = 4.f;
		Ember.Color = Spell->Color * 3.5f;
		Ember.AlphaStart = 1.f;
		Ember.AlphaEnd = 0.f;
		Ember.Stretch = 2.2f;
		FX->SpawnSprite(Ember);

		FFXSpriteParams Smoke;
		Smoke.Kind = EFXSpriteKind::Smoke;
		Smoke.Mode = EFXSpriteMode::Billboard;
		Smoke.Position = Loc + Back * 18.f;
		Smoke.Velocity = Back * 90.f + FVector(0.f, 0.f, 45.f);
		Smoke.Life = FMath::FRandRange(0.35f, 0.65f);
		Smoke.Size = 48.f * ChargeScale;
		Smoke.SizeEnd = 130.f * ChargeScale;
		Smoke.Color = FLinearColor(0.22f, 0.16f, 0.14f);
		Smoke.AlphaStart = 0.48f;
		Smoke.AlphaEnd = 0.f;
		Smoke.FadeIn = 0.05f;
		Smoke.Drag = 2.f;
		FX->SpawnSprite(Smoke);
	}
}

void AWizardProjectile::Detonate(const FHitResult* Hit)
{
	if (bDetonated)
	{
		return;
	}
	bDetonated = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LifeHandle);
	}

	if (Spell)
	{
		const FVector ImpactLoc = Hit ? FVector(Hit->ImpactPoint) : GetActorLocation();
		const FVector ImpactDir = Hit ? FVector(Hit->ImpactNormal) : GetActorForwardVector();
		const FSpellImpact Impact = Spell->MakeImpact(ImpactLoc, ImpactDir, ChargeT01, CasterActor.Get());
		UWizardSpellLibrary::Impact(this, Impact);
	}

	Destroy();
}

void AWizardProjectile::OnComponentHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor && OtherActor == CasterActor.Get())
	{
		return;
	}
	Detonate(&Hit);
}

void AWizardProjectile::OnLifeExpired()
{
	Detonate(nullptr);
}
