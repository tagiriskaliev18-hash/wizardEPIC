#include "FXSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
TStatId UWizardFXSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWizardFXSubsystem, STATGROUP_Tickables);
}

#include UE_INLINE_GENERATED_CPP_BY_NAME(FXSubsystem)

// Skeleton build: the framework is implemented in the next step. Every entry point is safe to call.

UWizardFXSubsystem* UWizardFXSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	return World ? World->GetSubsystem<UWizardFXSubsystem>() : nullptr;
}

void UWizardFXSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UWizardFXSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UWizardFXSubsystem::Tick(float DeltaTime)
{
}

int32 UWizardFXSubsystem::SpawnSprite(const FFXSpriteParams& Params)
{
	return 0;
}

int32 UWizardFXSubsystem::Burst(const FFXBurstSpec& Spec, const FTransform& Origin, float ScaleMul, const FLinearColor& TintMul)
{
	return 0;
}

UStaticMeshComponent* UWizardFXSubsystem::SpawnMesh(const FFXMeshParams& Params)
{
	return nullptr;
}

void UWizardFXSubsystem::ReleaseMesh(UStaticMeshComponent* Mesh)
{
}

UPointLightComponent* UWizardFXSubsystem::SpawnLight(const FFXLightParams& Params)
{
	return nullptr;
}

void UWizardFXSubsystem::SpawnDecal(const FFXDecalParams& Params)
{
}

FFXHandle UWizardFXSubsystem::SpawnLightning(const FFXLightningParams& Params)
{
	return FFXHandle();
}

void UWizardFXSubsystem::UpdateLightning(FFXHandle Bolt, const FVector& Start, const FVector& End)
{
}

void UWizardFXSubsystem::EndLightning(FFXHandle Bolt)
{
}

void UWizardFXSubsystem::SpawnDebris(const FFXDebrisParams& Params)
{
}

UNiagaraComponent* UWizardFXSubsystem::SpawnNiagara(const TSoftObjectPtr<UNiagaraSystem>& System, const FTransform& Transform, float Scale)
{
	return nullptr;
}

UNiagaraComponent* UWizardFXSubsystem::SpawnNiagaraAttached(const TSoftObjectPtr<UNiagaraSystem>& System, USceneComponent* AttachTo, FName Socket, float Scale)
{
	return nullptr;
}

FFXHandle UWizardFXSubsystem::Delay(float Seconds, TFunction<void()>&& Callback)
{
	return FFXHandle();
}

FFXHandle UWizardFXSubsystem::DelayRealtime(float Seconds, TFunction<void()>&& Callback)
{
	return FFXHandle();
}

void UWizardFXSubsystem::Cancel(FFXHandle Handle)
{
}

void UWizardFXSubsystem::HitStop(float Seconds, float Dilation)
{
}

UStaticMesh* UWizardFXSubsystem::GetFXMesh(FName MeshName)
{
	return nullptr;
}

UMaterialInterface* UWizardFXSubsystem::GetFXMaterial(FName MaterialName)
{
	return nullptr;
}

UTexture2D* UWizardFXSubsystem::GetFXTexture(FName TextureName)
{
	return nullptr;
}

void UWizardFXSubsystem::ClearTransient()
{
}
