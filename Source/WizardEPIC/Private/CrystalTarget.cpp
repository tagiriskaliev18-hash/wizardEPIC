#include "CrystalTarget.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FXSubsystem.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"
#include "WizardAudioSubsystem.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(CrystalTarget)

ACrystalTarget::ACrystalTarget()
{
	PrimaryActorTick.bCanEverTick = true;

	CrystalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CrystalMesh"));
	CrystalMesh->SetupAttachment(RootComponent);
	CrystalMesh->SetMobility(EComponentMobility::Movable);
	CrystalMesh->SetCollisionProfileName(TEXT("BlockAll"));

	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(CrystalMesh);
	GlowLight->SetIntensityUnits(ELightUnits::Lumens);
	GlowLight->SetIntensity(35.f);
	GlowLight->SetAttenuationRadius(280.f);
	GlowLight->SetLightColor(Color);
	GlowLight->SetCastShadows(false);
}

void ACrystalTarget::BeginPlay()
{
	Super::BeginPlay();

	if (CrystalMesh && !CrystalMesh->GetStaticMesh())
	{
		if (Definition && Definition->WholeMesh)
		{
			CrystalMesh->SetStaticMesh(Definition->WholeMesh);
		}
		else if (UStaticMesh* Shard = Cast<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Wizard/FX/Meshes/SM_FX_Shard")).TryLoad()))
		{
			CrystalMesh->SetStaticMesh(Shard);
		}
	}

	if (GlowLight)
	{
		GlowLight->SetLightColor(Color);
	}
	bAlive = true;
	Age = 0.f;
}

void ACrystalTarget::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bAlive || !CrystalMesh)
	{
		return;
	}

	Age += DeltaTime;
	const float GrowAlpha = GrowTime > KINDA_SMALL_NUMBER ? FMath::Clamp(Age / GrowTime, 0.f, 1.f) : 1.f;
	const float EasedScale = 1.f - FMath::Pow(1.f - GrowAlpha, 3.f);
	CrystalMesh->SetRelativeScale3D(FVector(FMath::Max(0.01f, EasedScale)));
	CrystalMesh->AddLocalRotation(FRotator(0.f, 42.f * DeltaTime, 0.f));

	if (GlowLight)
	{
		GlowLight->SetIntensityUnits(ELightUnits::Lumens);
		GlowLight->SetIntensity(35.f * EasedScale);
	}
}

int32 ACrystalTarget::ApplyImpactRadial(const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	return HitTest(Impact.Origin, Impact.Radius, Impact.Origin, Impact, OutEvents);
}

int32 ACrystalTarget::ApplyImpactCone(const FVector& Origin, const FVector& Direction, float HalfAngleDeg, float Range, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	if (!bAlive)
	{
		return 0;
	}

	const FVector Centre = GetActorLocation();
	const FVector To = Centre - Origin;
	const float Dist = static_cast<float>(To.Size());
	if (Dist > Range + 80.f || Dist < KINDA_SMALL_NUMBER)
	{
		return 0;
	}
	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(HalfAngleDeg, 1.f, 89.f)));
	if (FVector::DotProduct(Direction.GetSafeNormal(), To / Dist) < CosHalf)
	{
		return 0;
	}

	Shatter(Origin, Impact, OutEvents);
	return 1;
}

int32 ACrystalTarget::ApplyLaunchUpward(const FVector& Origin, float Radius, float Height, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	if (!bAlive)
	{
		return 0;
	}
	const FVector Centre = GetActorLocation();
	const float DZ = static_cast<float>(Centre.Z - Origin.Z);
	if (DZ >= -120.f && DZ <= Height + 80.f && FVector::Dist2D(Origin, Centre) <= Radius + 60.f)
	{
		Shatter(Origin, Impact, OutEvents);
		return 1;
	}
	return 0;
}

void ACrystalTarget::Reset()
{
	Super::Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnHandle);
	}
	Respawn();
	Age = GrowTime; // Instant full size on arena reset
}

FBoxSphereBounds ACrystalTarget::GetBreakableBounds() const
{
	if (CrystalMesh && CrystalMesh->GetStaticMesh())
	{
		return CrystalMesh->Bounds;
	}
	return FBoxSphereBounds(GetActorLocation(), FVector(65.f), 110.f);
}

int32 ACrystalTarget::HitTest(const FVector& Point, float Padding, const FVector& LaunchOrigin, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	if (!bAlive)
	{
		return 0;
	}
	const FBoxSphereBounds B = GetBreakableBounds();
	if (FVector::Dist(Point, B.Origin) <= B.SphereRadius + Padding)
	{
		Shatter(LaunchOrigin, Impact, OutEvents);
		return 1;
	}
	return 0;
}

void ACrystalTarget::Shatter(const FVector& LaunchOrigin, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	bAlive = false;
	if (CrystalMesh)
	{
		CrystalMesh->SetVisibility(false);
		CrystalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (GlowLight)
	{
		GlowLight->SetIntensity(0.f);
	}

	const FVector Loc = GetActorLocation();
	if (UWizardFXSubsystem* FX = UWizardFXSubsystem::Get(this))
	{
		FFXBurstSpec Sparkles;
		Sparkles.Kind = EFXSpriteKind::Sparkle;
		Sparkles.Mode = EFXSpriteMode::Billboard;
		Sparkles.Count = 26;
		Sparkles.SpeedMin = 260.f;
		Sparkles.SpeedMax = 880.f;
		Sparkles.UpBias = 0.35f;
		Sparkles.LifeMin = 0.45f;
		Sparkles.LifeMax = 0.95f;
		Sparkles.SizeMin = 28.f;
		Sparkles.SizeMax = 68.f;
		Sparkles.SizeEndMul = 0.1f;
		Sparkles.ColorA = Color * 3.f;
		Sparkles.ColorB = FLinearColor::White * 2.f;
		FX->Burst(Sparkles, FTransform(Loc), 1.f, FLinearColor::White);

		FFXDebrisParams Shards;
		Shards.MeshName = TEXT("SM_FX_Shard");
		Shards.MaterialName = TEXT("M_FX_Chip_Crystal");
		Shards.Count = 12;
		Shards.Origin = FTransform(Loc);
		Shards.SpeedMin = 350.f;
		Shards.SpeedMax = 950.f;
		Shards.UpBias = 0.45f;
		Shards.SizeMin = 12.f;
		Shards.SizeMax = 26.f;
		FX->SpawnDebris(Shards);

		FFXLightParams Flash;
		Flash.Color = Color;
		Flash.Intensity = 2.5e6f;
		Flash.AttenuationRadius = 1200.f;
		Flash.Location = Loc;
		Flash.LifeTime = 0.32f;
		Flash.bCastShadows = false;
		FX->SpawnLight(Flash);
	}

	if (UWizardAudioSubsystem* Audio = UWizardAudioSubsystem::Get(this))
	{
		Audio->PlayAt(TEXT("Crystal"), Loc, 1.f, 1.f);
	}

	FDestructibleEvent Ev;
	FillEvent(Ev, Loc, 90.f, &Impact);
	Ev.Material = EDestructibleMaterial::Crystal;
	Ev.Points = Score;
	Ev.bIsCrystal = true;
	OutEvents.Add(Ev);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RespawnHandle, this, &ACrystalTarget::Respawn, RespawnTime, false);
	}
}

void ACrystalTarget::Respawn()
{
	bAlive = true;
	Age = 0.f;
	if (CrystalMesh)
	{
		CrystalMesh->SetVisibility(true);
		CrystalMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		CrystalMesh->SetRelativeScale3D(FVector(0.01f));
	}
}
