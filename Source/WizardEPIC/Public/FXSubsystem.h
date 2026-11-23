// FXSubsystem.h - procedural VFX framework.
//
// Everything a spell needs to look spectacular without authored Niagara assets: CPU sprite
// particles (one instanced-quad layer per sprite kind), pooled emissive meshes, pooled point
// lights, decals, procedural lightning bolts, physics-lite debris and a callback scheduler.
// All meshes, materials and textures come from Scripts/forge under /Game/Wizard/FX/.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WizardTypes.h"
#include "FXSubsystem.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class UPointLightComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Handle for a scheduled callback or a lightning bolt. Id 0 is invalid. */
USTRUCT(BlueprintType)
struct FFXHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = FX)
	int32 Id = 0;

	bool IsValid() const { return Id != 0; }
};

UENUM(BlueprintType)
enum class EFXSpriteMode : uint8
{
	/** Faces the camera. Only Rotation.X (roll) is used. */
	Billboard,
	/** Card stretched along its velocity (sparks, streaks). */
	VelocityAligned,
	/** Freely tumbling card (leaves, splinters). */
	Tumble
};

/** One sprite particle. Sizes are world-space diameters in cm. */
USTRUCT(BlueprintType)
struct FFXSpriteParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sprite)
	EFXSpriteKind Kind = EFXSpriteKind::Smoke;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sprite)
	EFXSpriteMode Mode = EFXSpriteMode::Billboard;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sprite)
	FVector Position = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sprite)
	FVector Velocity = FVector::ZeroVector;

	/** Lifetime in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sprite)
	float Life = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sprite)
	float Size = 100.f;

	/** Diameter at the end of the life (linear interpolation). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sprite)
	float SizeEnd = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sprite)
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sprite)
	float AlphaStart = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sprite)
	float AlphaEnd = 0.f;

	/** Seconds the alpha takes to ramp up from 0 at spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sprite)
	float FadeIn = 0.f;

	/** -1 = full gravity downward, 1 = upward. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sprite)
	float GravityScale = 0.f;

	/** Per-second velocity damping: v *= exp(-Drag * dt). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sprite)
	float Drag = 1.f;

	/** Start rotation in degrees (X = roll for billboards). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sprite)
	FVector Rotation = FVector::ZeroVector;

	/** Degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sprite)
	FVector RotationSpeed = FVector::ZeroVector;

	/** VelocityAligned only: length = Size * Stretch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Sprite)
	float Stretch = 1.f;
};

/** A randomized burst of sprites of one kind. Every value between Min/Max is uniform-random per particle. */
USTRUCT(BlueprintType)
struct FFXBurstSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	EFXSpriteKind Kind = EFXSpriteKind::Smoke;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	EFXSpriteMode Mode = EFXSpriteMode::Billboard;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	int32 Count = 12;

	/** Particles are spread evenly over this many seconds (0 = all at once). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	float EmissionTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	float SpeedMin = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	float SpeedMax = 600.f;

	/** Half angle of the emission cone around the origin's X axis. 180 = full sphere. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	float ConeHalfAngle = 180.f;

	/** Fraction of the speed added straight up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	float UpBias = 0.f;

	/** Particles start at a random point inside this sphere around the origin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	float SpawnRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	float LifeMin = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	float LifeMax = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	float SizeMin = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	float SizeMax = 120.f;

	/** Size at the end of the life = start size * SizeEndMul. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	float SizeEndMul = 1.5f;

	/** Color lerps from ColorA to ColorB over the life. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	FLinearColor ColorA = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	FLinearColor ColorB = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	float AlphaStart = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	float AlphaEnd = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	float FadeIn = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	float GravityScale = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	float Drag = 1.5f;

	/** Max random roll speed in deg/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	float RotationSpeedMax = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Burst)
	float Stretch = 1.f;
};

/** A pooled static mesh that plays a scale/fade envelope and returns to the pool. */
USTRUCT(BlueprintType)
struct FFXMeshParams
{
	GENERATED_BODY()

	/** Short name under /Game/Wizard/FX/Meshes: SM_FX_Sphere, SM_FX_Ring, SM_FX_Pillar, SM_FX_Crystal, SM_FX_Bolt. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FName MeshName;

	/** Short name under /Game/Wizard/FX/Materials: M_FX_Core, M_FX_Ring, M_FX_Pillar, M_FX_WindRibbon, M_FX_Bolt. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FName MaterialName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FTransform Transform = FTransform::Identity;

	/** Seconds before the mesh returns to the pool. <= 0 = keep until ReleaseMesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float LifeTime = 0.5f;

	/** Scale envelope (multiplies the transform scale): ScaleStart -> ScaleEnd eased out over ScaleTime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FVector ScaleStart = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FVector ScaleEnd = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float ScaleTime = 0.25f;

	/** The material scalar parameter "Fade" goes 1 -> 0 during the last FadeOutTime seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float FadeOutTime = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	bool bCastShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	bool bCollision = false;

	/** Set once on the dynamic material instance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	TMap<FName, float> ScalarParams;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	TMap<FName, FLinearColor> VectorParams;

	/** When set the mesh is attached to this component (socket) instead of living in world space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	TWeakObjectPtr<USceneComponent> AttachTo = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FName AttachSocket = NAME_None;
};

USTRUCT(BlueprintType)
struct FFXLightParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FLinearColor Color = FLinearColor::White;

	/** Peak intensity in candela. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float Intensity = 5.e6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float AttenuationRadius = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float LifeTime = 0.5f;

	/** Seconds to reach the peak intensity, then an exponential decay over the rest of LifeTime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float AttackTime = 0.02f;

	/** 0..1 random intensity flicker. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float Flicker = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	bool bCastShadows = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	TWeakObjectPtr<USceneComponent> AttachTo = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FName AttachSocket = NAME_None;
};

USTRUCT(BlueprintType)
struct FFXDecalParams
{
	GENERATED_BODY()

	/** Short name under /Game/Wizard/FX/Materials: M_Decal_Scorch, M_Decal_Crack, M_Decal_Rune_Fire, ... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FName MaterialName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FRotator Rotation = FRotator::ZeroRotator;

	/** X = projection depth, Y = width, Z = height in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FVector Size = FVector(512.f, 400.f, 400.f);

	/** Seconds before the decal is removed. <= 0 = stays until the arena reset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float LifeTime = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float FadeOutTime = 2.f;
};

USTRUCT(BlueprintType)
struct FFXLightningParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FVector Start = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FVector End = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FLinearColor Color = FLinearColor(0.72f, 0.88f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float LifeTime = 0.35f;

	/** Core width in cm at the start of the bolt, tapering to the end. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float Thickness = 9.f;

	/** Max lateral offset of the displaced midpoints in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float Jag = 120.f;

	/** Midpoint displacement levels: the bolt has 2^Subdivisions segments. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	int32 Subdivisions = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	int32 BranchCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float BranchLength = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float BranchThicknessScale = 0.5f;

	/** Seconds between re-jitters (0 = the bolt keeps its first shape). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float RejitInterval = 0.05f;
};

USTRUCT(BlueprintType)
struct FFXDebrisParams
{
	GENERATED_BODY()

	/** Mesh short name, e.g. SM_FX_Chip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FName MeshName;

	/** Material short name, e.g. M_FX_Chip_Stone, M_FX_Chip_Wood, M_FX_Chip_Pottery, M_FX_Chip_Rock, M_FX_Chip_Crystal, M_FX_Chip_Foliage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FName MaterialName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	int32 Count = 16;

	/** Origin: location + X axis = main flight direction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	FTransform Origin = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float SpeedMin = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float SpeedMax = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float ConeHalfAngle = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float UpBias = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float LifeMin = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float LifeMax = 3.f;

	/** Longest extent in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float SizeMin = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float SizeMax = 22.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float GravityScale = 1.5f;

	/** 0..1 fraction of the vertical speed kept after a ground bounce. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float Bounciness = 0.3f;

	/** Max random tumble in deg/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FX)
	float Spin = 900.f;
};

/**
 * World subsystem that owns all transient VFX. All spawn functions are safe to call with missing
 * assets (they log once and return null/invalid).
 */
UCLASS()
class WIZARDEPIC_API UWizardFXSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWizardFXSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// ----- Sprites -------------------------------------------------------------------------
	/** Spawns one sprite. Returns an id or 0 when the layer is full. */
	int32 SpawnSprite(const FFXSpriteParams& Params);

	/** Spawns a burst. ScaleMul scales sizes and speeds, TintMul tints both colors. Returns the number of spawned sprites. */
	int32 Burst(const FFXBurstSpec& Spec, const FTransform& Origin, float ScaleMul = 1.f, const FLinearColor& TintMul = FLinearColor::White);

	// ----- Pooled meshes -------------------------------------------------------------------
	/** Spawns (or reuses) a pooled mesh component. */
	UStaticMeshComponent* SpawnMesh(const FFXMeshParams& Params);

	/** Returns a mesh to the pool immediately. */
	void ReleaseMesh(UStaticMeshComponent* Mesh);

	// ----- Lights --------------------------------------------------------------------------
	UPointLightComponent* SpawnLight(const FFXLightParams& Params);

	// ----- Decals --------------------------------------------------------------------------
	void SpawnDecal(const FFXDecalParams& Params);

	// ----- Lightning -----------------------------------------------------------------------
	/** Creates a jagged additive bolt between two points. Call UpdateLightning to re-shape it, EndLightning to kill it. */
	FFXHandle SpawnLightning(const FFXLightningParams& Params);
	void UpdateLightning(FFXHandle Bolt, const FVector& Start, const FVector& End);
	void EndLightning(FFXHandle Bolt);

	// ----- Debris --------------------------------------------------------------------------
	/** Physics-lite chips: ballistic flight, one ground bounce, no collision with gameplay. */
	void SpawnDebris(const FFXDebrisParams& Params);

	// ----- Niagara (Fab spell packs) -------------------------------------------------------
	/** Spawns a Niagara system at a location when the soft pointer is valid. Returns null otherwise. */
	UNiagaraComponent* SpawnNiagara(const TSoftObjectPtr<UNiagaraSystem>& System, const FTransform& Transform, float Scale = 1.f);

	/** Spawns a Niagara system attached to a component socket when the soft pointer is valid. Returns null otherwise. */
	UNiagaraComponent* SpawnNiagaraAttached(const TSoftObjectPtr<UNiagaraSystem>& System, USceneComponent* AttachTo, FName Socket, float Scale = 1.f);

	// ----- Scheduler & time ----------------------------------------------------------------
	/** Runs Callback after Seconds of scaled game time. */
	FFXHandle Delay(float Seconds, TFunction<void()>&& Callback);
	void Cancel(FFXHandle Handle);
	/** Counts down in real time (ignores hit-stop dilation). */
	FFXHandle DelayRealtime(float Seconds, TFunction<void()>&& Callback);

	/** Slows world time to Dilation for Seconds of real time (impact "hit-stop"). */
	void HitStop(float Seconds, float Dilation = 0.05f);

	// ----- Shared assets -------------------------------------------------------------------
	/** Loads /Game/Wizard/FX/Meshes/<MeshName> (cached). */
	UStaticMesh* GetFXMesh(FName MeshName);

	/** Loads /Game/Wizard/FX/Materials/<MaterialName> (cached). */
	UMaterialInterface* GetFXMaterial(FName MaterialName);

	/** Loads /Game/Wizard/FX/Textures/<TextureName> (cached). */
	UTexture2D* GetFXTexture(FName TextureName);

	/** Removes every transient effect (sprites, bolts, scheduled callbacks) and returns pooled objects. */
	void ClearTransient();

	virtual ~UWizardFXSubsystem() override;

private:
	struct FFXRuntimeState;
	TUniquePtr<FFXRuntimeState> State;

	UPROPERTY(Transient)
	TObjectPtr<AActor> FXHost = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UInstancedStaticMeshComponent>> SpriteLayers;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UStaticMesh>> MeshCache;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UMaterialInterface>> MaterialCache;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> TextureCache;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> MeshPoolAll;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> LightPoolAll;

	AActor* EnsureHost();
	class UInstancedStaticMeshComponent* EnsureSpriteLayer(EFXSpriteKind Kind);
	UStaticMeshComponent* AcquirePoolMesh();
	void ReturnPoolMesh(UStaticMeshComponent* Comp);
	UPointLightComponent* AcquirePoolLight();
	void ReturnPoolLight(UPointLightComponent* Comp);
};
