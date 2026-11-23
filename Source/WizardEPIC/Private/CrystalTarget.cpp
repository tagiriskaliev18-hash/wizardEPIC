#include "CrystalTarget.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(CrystalTarget)

// Skeleton build: hit tests, shatter and respawn are implemented in the next step.

ACrystalTarget::ACrystalTarget()
{
	PrimaryActorTick.bCanEverTick = true;

	CrystalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CrystalMesh"));
	SetRootComponent(CrystalMesh);
	CrystalMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CrystalMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(CrystalMesh);
}

void ACrystalTarget::BeginPlay()
{
	Super::BeginPlay();
}

void ACrystalTarget::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

int32 ACrystalTarget::ApplyImpactRadial(const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	return 0;
}

int32 ACrystalTarget::ApplyImpactCone(const FVector& Origin, const FVector& Direction, float HalfAngleDeg, float Range, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	return 0;
}

int32 ACrystalTarget::ApplyLaunchUpward(const FVector& Origin, float Radius, float Height, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	return 0;
}

void ACrystalTarget::Reset()
{
	Super::Reset();
}

FBoxSphereBounds ACrystalTarget::GetBreakableBounds() const
{
	return Definition ? Definition->Bounds.TransformBy(GetActorTransform()) : FBoxSphereBounds(GetActorLocation(), FVector(60.f), 104.f);
}

int32 ACrystalTarget::HitTest(const FVector& Point, float Padding, const FVector& LaunchOrigin, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
	return 0;
}

void ACrystalTarget::Shatter(const FVector& LaunchOrigin, const FSpellImpact& Impact, TArray<FDestructibleEvent>& OutEvents)
{
}

void ACrystalTarget::Respawn()
{
}
