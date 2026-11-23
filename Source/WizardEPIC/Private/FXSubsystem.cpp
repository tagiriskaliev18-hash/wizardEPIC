#include "FXSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/SoftObjectPath.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(FXSubsystem)

TStatId UWizardFXSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWizardFXSubsystem, STATGROUP_Tickables);
}

namespace
{
	constexpr int32 MaxSpriteKinds = 11;
	constexpr int32 MaxSpritesPerLayer = 512;
	constexpr int32 MaxDebrisActive = 180;
	constexpr int32 MaxPoolMeshes = 256;
	constexpr int32 MaxPoolLights = 24;

	bool IsAdditiveSpriteKind(EFXSpriteKind Kind)
	{
		switch (Kind)
		{
		case EFXSpriteKind::Spark:
		case EFXSpriteKind::Ember:
		case EFXSpriteKind::Flash:
		case EFXSpriteKind::Glow:
		case EFXSpriteKind::Sparkle:
		case EFXSpriteKind::Rune:
			return true;
		default:
			return false;
		}
	}

	FName SpriteTextureName(EFXSpriteKind Kind)
	{
		switch (Kind)
		{
		case EFXSpriteKind::Smoke:   return TEXT("T_FX_Smoke");
		case EFXSpriteKind::Dust:    return TEXT("T_FX_Dust");
		case EFXSpriteKind::Leaf:    return TEXT("T_FX_Leaf");
		case EFXSpriteKind::Splinter:return TEXT("T_FX_Splinter");
		case EFXSpriteKind::Shard:   return TEXT("T_FX_Shard");
		case EFXSpriteKind::Spark:   return TEXT("T_FX_Spark");
		case EFXSpriteKind::Ember:   return TEXT("T_FX_Ember");
		case EFXSpriteKind::Flash:   return TEXT("T_FX_Flash");
		case EFXSpriteKind::Glow:    return TEXT("T_FX_Glow");
		case EFXSpriteKind::Sparkle: return TEXT("T_FX_Sparkle");
		case EFXSpriteKind::Rune:    return TEXT("T_FX_Rune");
		default:                     return TEXT("T_FX_Smoke");
		}
	}

	FVector RandomConeDir(const FVector& Axis, float HalfAngleDeg)
	{
		if (HalfAngleDeg >= 179.f)
		{
			return FMath::VRand();
		}
		return FMath::VRandCone(Axis.GetSafeNormal(), FMath::DegreesToRadians(FMath::Clamp(HalfAngleDeg, 0.f, 180.f)));
	}
}

struct UWizardFXSubsystem::FFXRuntimeState
{
	struct FSpriteParticle
	{
		int32 Id = 0;
		EFXSpriteKind Kind = EFXSpriteKind::Smoke;
		EFXSpriteMode Mode = EFXSpriteMode::Billboard;
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Age = 0.f;
		float Life = 1.f;
		float SizeStart = 100.f;
		float SizeEnd = 100.f;
		FLinearColor ColorA = FLinearColor::White;
		FLinearColor ColorB = FLinearColor::White;
		float AlphaStart = 1.f;
		float AlphaEnd = 0.f;
		float FadeIn = 0.f;
		float GravityScale = 0.f;
		float Drag = 1.f;
		FVector Rotation = FVector::ZeroVector;
		FVector RotationSpeed = FVector::ZeroVector;
		float Stretch = 1.f;
	};

	struct FActiveMesh
	{
		TWeakObjectPtr<UStaticMeshComponent> Comp;
		TWeakObjectPtr<UMaterialInstanceDynamic> MID;
		FVector BaseScale = FVector::OneVector;
		FVector ScaleStart = FVector::OneVector;
		FVector ScaleEnd = FVector::OneVector;
		float ScaleTime = 0.25f;
		float LifeTime = 0.5f;
		float FadeOutTime = 0.3f;
		float Age = 0.f;
	};

	struct FActiveLight
	{
		TWeakObjectPtr<UPointLightComponent> Comp;
		float PeakIntensity = 5.e6f;
		float LifeTime = 0.5f;
		float AttackTime = 0.02f;
		float Flicker = 0.f;
		float Age = 0.f;
	};

	struct FActiveDecal
	{
		TWeakObjectPtr<UDecalComponent> Comp;
		float LifeTime = 20.f;
		float FadeOutTime = 2.f;
		float Age = 0.f;
	};

	struct FActiveLightning
	{
		int32 Id = 0;
		FFXLightningParams Params;
		float Age = 0.f;
		float RejitTimer = 0.f;
		TArray<TWeakObjectPtr<UStaticMeshComponent>> Segments;
		TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> MIDs;
	};

	struct FActiveDebris
	{
		TWeakObjectPtr<UStaticMeshComponent> Comp;
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		FRotator Spin = FRotator::ZeroRotator;
		float GravityScale = 1.5f;
		float Bounciness = 0.3f;
		float GroundZ = 0.f;
		float Age = 0.f;
		float Life = 2.f;
		bool bBounced = false;
	};

	struct FScheduledCallback
	{
		int32 Id = 0;
		float Remaining = 0.f;
		bool bRealtime = false;
		TFunction<void()> Callback;
	};

	int32 NextHandleId = 1;
	int32 NextSpriteId = 1;

	TArray<FSpriteParticle> Sprites[MaxSpriteKinds];
	TArray<FActiveMesh> ActiveMeshes;
	TArray<TWeakObjectPtr<UStaticMeshComponent>> FreeMeshes;
	TArray<FActiveLight> ActiveLights;
	TArray<TWeakObjectPtr<UPointLightComponent>> FreeLights;
	TArray<FActiveDecal> ActiveDecals;
	TArray<FActiveLightning> ActiveBolts;
	TArray<FActiveDebris> ActiveDebris;
	TArray<FScheduledCallback> Callbacks;

	float HitStopRemaining = 0.f;
	bool bHitStopActive = false;

	TSet<FName> MissingAssets;

	static void RebuildLightningBolt(UWizardFXSubsystem* Self, FActiveLightning& Bolt);
};

UWizardFXSubsystem::~UWizardFXSubsystem() = default;

UWizardFXSubsystem* UWizardFXSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	return World ? World->GetSubsystem<UWizardFXSubsystem>() : nullptr;
}

void UWizardFXSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	State = MakeUnique<FFXRuntimeState>();
	SpriteLayers.SetNum(MaxSpriteKinds);
}

void UWizardFXSubsystem::Deinitialize()
{
	ClearTransient();
	if (FXHost && IsValid(FXHost))
	{
		FXHost->Destroy();
	}
	FXHost = nullptr;
	SpriteLayers.Empty();
	MeshPoolAll.Empty();
	LightPoolAll.Empty();
	MeshCache.Empty();
	MaterialCache.Empty();
	TextureCache.Empty();
	State.Reset();
	Super::Deinitialize();
}

AActor* UWizardFXSubsystem::EnsureHost()
{
	if (FXHost && IsValid(FXHost))
	{
		return FXHost;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.Name = MakeUniqueObjectName(World, AActor::StaticClass(), TEXT("WizardFXHost"));
	Params.ObjectFlags |= RF_Transient;
	FXHost = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (FXHost)
	{
		USceneComponent* Root = NewObject<USceneComponent>(FXHost, TEXT("FXRoot"));
		FXHost->SetRootComponent(Root);
		Root->RegisterComponent();
	}
	return FXHost;
}

UInstancedStaticMeshComponent* UWizardFXSubsystem::EnsureSpriteLayer(EFXSpriteKind Kind)
{
	const int32 Idx = static_cast<int32>(Kind);
	if (!SpriteLayers.IsValidIndex(Idx))
	{
		return nullptr;
	}
	if (SpriteLayers[Idx] && IsValid(SpriteLayers[Idx]))
	{
		return SpriteLayers[Idx];
	}

	AActor* Host = EnsureHost();
	if (!Host)
	{
		return nullptr;
	}

	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(Host);
	ISM->SetupAttachment(Host->GetRootComponent());
	ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISM->SetCastShadow(false);
	ISM->bReceivesDecals = false;
	ISM->NumCustomDataFloats = 4;

	UStaticMesh* Quad = GetFXMesh(TEXT("SM_FX_Quad"));
	if (!Quad)
	{
		Quad = Cast<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Plane.Plane")).TryLoad());
	}
	if (Quad)
	{
		ISM->SetStaticMesh(Quad);
	}

	UMaterialInterface* BaseMat = GetFXMaterial(IsAdditiveSpriteKind(Kind) ? TEXT("M_FX_Add") : TEXT("M_FX_Alpha"));
	if (!BaseMat)
	{
		BaseMat = Cast<UMaterialInterface>(FSoftObjectPath(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")).TryLoad());
	}
	if (BaseMat)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, ISM);
		if (UTexture2D* Tex = GetFXTexture(SpriteTextureName(Kind)))
		{
			MID->SetTextureParameterValue(TEXT("Tex"), Tex);
		}
		MID->SetVectorParameterValue(TEXT("Tint"), FLinearColor::White);
		MID->SetScalarParameterValue(TEXT("Fade"), 1.f);
		ISM->SetMaterial(0, MID);
	}

	ISM->RegisterComponent();
	SpriteLayers[Idx] = ISM;
	return ISM;
}

UStaticMeshComponent* UWizardFXSubsystem::AcquirePoolMesh()
{
	if (!State)
	{
		return nullptr;
	}

	while (!State->FreeMeshes.IsEmpty())
	{
		UStaticMeshComponent* Comp = State->FreeMeshes.Pop().Get();
		if (Comp && IsValid(Comp))
		{
			Comp->SetVisibility(true);
			Comp->SetHiddenInGame(false);
			return Comp;
		}
	}

	if (MeshPoolAll.Num() >= MaxPoolMeshes)
	{
		return nullptr;
	}

	AActor* Host = EnsureHost();
	if (!Host)
	{
		return nullptr;
	}

	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Host);
	Comp->SetupAttachment(Host->GetRootComponent());
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetCastShadow(false);
	Comp->bReceivesDecals = false;
	Comp->RegisterComponent();
	MeshPoolAll.Add(Comp);
	return Comp;
}

void UWizardFXSubsystem::ReturnPoolMesh(UStaticMeshComponent* Comp)
{
	if (!Comp || !IsValid(Comp) || !State)
	{
		return;
	}

	if (FXHost && IsValid(FXHost) && Comp->GetAttachParent() != FXHost->GetRootComponent())
	{
		Comp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Comp->AttachToComponent(FXHost->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetVisibility(false);
	Comp->SetHiddenInGame(true);
	State->FreeMeshes.AddUnique(Comp);
}

UPointLightComponent* UWizardFXSubsystem::AcquirePoolLight()
{
	if (!State)
	{
		return nullptr;
	}

	while (!State->FreeLights.IsEmpty())
	{
		UPointLightComponent* Light = State->FreeLights.Pop().Get();
		if (Light && IsValid(Light))
		{
			Light->SetIntensityUnits(ELightUnits::Lumens);
			Light->SetVisibility(true);
			return Light;
		}
	}

	if (LightPoolAll.Num() >= MaxPoolLights)
	{
		return nullptr;
	}

	AActor* Host = EnsureHost();
	if (!Host)
	{
		return nullptr;
	}

	UPointLightComponent* Light = NewObject<UPointLightComponent>(Host);
	Light->SetupAttachment(Host->GetRootComponent());
	Light->SetIntensityUnits(ELightUnits::Lumens);
	Light->RegisterComponent();
	LightPoolAll.Add(Light);
	return Light;
}

void UWizardFXSubsystem::ReturnPoolLight(UPointLightComponent* Light)
{
	if (!Light || !IsValid(Light) || !State)
	{
		return;
	}

	if (FXHost && IsValid(FXHost) && Light->GetAttachParent() != FXHost->GetRootComponent())
	{
		Light->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Light->AttachToComponent(FXHost->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	Light->SetIntensity(0.f);
	Light->SetVisibility(false);
	State->FreeLights.AddUnique(Light);
}

void UWizardFXSubsystem::FFXRuntimeState::RebuildLightningBolt(UWizardFXSubsystem* Self, FActiveLightning& Bolt)
{
	// Release previous segments
	for (TWeakObjectPtr<UStaticMeshComponent>& Seg : Bolt.Segments)
	{
		if (UStaticMeshComponent* Comp = Seg.Get())
		{
			Self->ReleaseMesh(Comp);
		}
	}
	Bolt.Segments.Reset();
	Bolt.MIDs.Reset();

	const FFXLightningParams& P = Bolt.Params;
	const int32 Subdiv = FMath::Clamp(P.Subdivisions, 1, 5);
	const int32 SegCount = 1 << Subdiv;

	TArray<FVector> Points;
	Points.SetNum(SegCount + 1);
	Points[0] = P.Start;
	Points[SegCount] = P.End;

	FVector MainDir = (P.End - P.Start).GetSafeNormal();
	FVector SideA, SideB;
	MainDir.FindBestAxisVectors(SideA, SideB);

	float Jag = P.Jag;
	for (int32 Step = SegCount; Step > 1; Step >>= 1)
	{
		const int32 Half = Step >> 1;
		for (int32 i = 0; i < SegCount; i += Step)
		{
			const FVector Mid = 0.5f * (Points[i] + Points[i + Step]);
			const float OffA = FMath::FRandRange(-Jag, Jag);
			const float OffB = FMath::FRandRange(-Jag, Jag);
			Points[i + Half] = Mid + SideA * OffA + SideB * OffB;
		}
		Jag *= 0.55f;
	}

	auto SpawnSegment = [&](const FVector& A, const FVector& B, float Width)
	{
		const FVector Delta = B - A;
		const float Len = static_cast<float>(Delta.Size());
		if (Len < 1.f)
		{
			return;
		}

		FFXMeshParams MP;
		MP.MeshName = TEXT("SM_FX_Bolt");
		MP.MaterialName = TEXT("M_FX_Bolt");
		MP.LifeTime = 0.f; // Managed by the bolt
		MP.bCastShadow = false;
		MP.bCollision = false;
		MP.VectorParams.Add(TEXT("Tint"), P.Color);
		MP.ScalarParams.Add(TEXT("EmissiveScale"), 7.5f);
		MP.ScalarParams.Add(TEXT("Fade"), 1.f);

		const FRotator Rot = Delta.Rotation();
		const FVector Mid = 0.5f * (A + B);
		// Normalize any bolt mesh (100cm Forge OBJ or fallback 100cm Cube) to true 1cm unit scale
		UStaticMesh* BoltMesh = Self->GetFXMesh(TEXT("SM_FX_Bolt"));
		const float MeshSpanX = (BoltMesh && BoltMesh->GetBounds().BoxExtent.X > 0.01f) ? (BoltMesh->GetBounds().BoxExtent.X * 2.f) : 100.f;
		const float UnitScale = 1.f / MeshSpanX;
		MP.Transform = FTransform(Rot, Mid, FVector(Len * UnitScale, Width * UnitScale, Width * UnitScale));

		if (UStaticMeshComponent* Comp = Self->SpawnMesh(MP))
		{
			Bolt.Segments.Add(Comp);
			if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(0)))
			{
				Bolt.MIDs.Add(MID);
			}
		}
	};

	for (int32 i = 0; i < SegCount; ++i)
	{
		const float T = static_cast<float>(i) / static_cast<float>(SegCount);
		const float W = FMath::Lerp(P.Thickness, P.Thickness * 0.45f, T);
		SpawnSegment(Points[i], Points[i + 1], W);
	}

	// Branches
	const int32 Branches = FMath::Clamp(P.BranchCount, 0, 5);
	for (int32 b = 0; b < Branches; ++b)
	{
		const int32 AnchorIdx = FMath::RandRange(FMath::Max(1, SegCount / 5), FMath::Max(1, (SegCount * 4) / 5));
		FVector Prev = Points[AnchorIdx];
		FVector BranchDir = (MainDir + FMath::VRand() * 0.65f).GetSafeNormal();
		const int32 BranchSegs = 3;
		const float SegLen = P.BranchLength / static_cast<float>(BranchSegs);
		for (int32 s = 0; s < BranchSegs; ++s)
		{
			const FVector Next = Prev + (BranchDir + FMath::VRand() * 0.35f).GetSafeNormal() * SegLen;
			SpawnSegment(Prev, Next, P.Thickness * P.BranchThicknessScale * (1.f - 0.25f * s));
			Prev = Next;
		}
	}
}

void UWizardFXSubsystem::Tick(float DeltaTime)
{
	if (!State)
	{
		return;
	}

	UWorld* World = GetWorld();
	const float Dilation = World ? FMath::Max(0.001f, World->GetWorldSettings()->TimeDilation) : 1.f;
	const float RealDelta = FMath::Min(0.1f, DeltaTime / Dilation);

	// 1. Hit-stop recovery
	if (State->bHitStopActive)
	{
		State->HitStopRemaining -= RealDelta;
		if (State->HitStopRemaining <= 0.f)
		{
			State->bHitStopActive = false;
			if (World)
			{
				UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
			}
		}
	}

	// 2. Scheduled callbacks
	if (!State->Callbacks.IsEmpty())
	{
		TArray<TFunction<void()>> ToRun;
		for (int32 i = State->Callbacks.Num() - 1; i >= 0; --i)
		{
			auto& CB = State->Callbacks[i];
			CB.Remaining -= CB.bRealtime ? RealDelta : DeltaTime;
			if (CB.Remaining <= 0.f)
			{
				ToRun.Add(MoveTemp(CB.Callback));
				State->Callbacks.RemoveAtSwap(i);
			}
		}
		for (auto& Fn : ToRun)
		{
			if (Fn)
			{
				Fn();
			}
		}
	}

	// 3. Camera location for billboards
	FVector CamLoc = FVector::ZeroVector;
	if (World)
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (PC->PlayerCameraManager)
			{
				CamLoc = PC->PlayerCameraManager->GetCameraLocation();
			}
		}
	}

	// 4. Sprites update
	for (int32 LayerIdx = 0; LayerIdx < MaxSpriteKinds; ++LayerIdx)
	{
		auto& Particles = State->Sprites[LayerIdx];
		if (Particles.IsEmpty())
		{
			if (SpriteLayers.IsValidIndex(LayerIdx) && SpriteLayers[LayerIdx] && SpriteLayers[LayerIdx]->GetInstanceCount() > 0)
			{
				SpriteLayers[LayerIdx]->ClearInstances();
			}
			continue;
		}

		UInstancedStaticMeshComponent* ISM = EnsureSpriteLayer(static_cast<EFXSpriteKind>(LayerIdx));
		if (!ISM)
		{
			Particles.Reset();
			continue;
		}

		for (int32 i = Particles.Num() - 1; i >= 0; --i)
		{
			auto& P = Particles[i];
			P.Age += DeltaTime;
			if (P.Age >= P.Life)
			{
				Particles.RemoveAtSwap(i);
				continue;
			}

			P.Velocity.Z += P.GravityScale * 980.f * DeltaTime;
			if (P.Drag > 0.f)
			{
				P.Velocity *= FMath::Exp(-P.Drag * DeltaTime);
			}
			P.Position += P.Velocity * DeltaTime;
			P.Rotation += P.RotationSpeed * DeltaTime;
		}

		ISM->ClearInstances();
		for (const auto& P : Particles)
		{
			const float T = FMath::Clamp(P.Age / FMath::Max(0.001f, P.Life), 0.f, 1.f);
			const float Size = FMath::Lerp(P.SizeStart, P.SizeEnd, T);
			const float Scale = Size * 0.01f; // Quad is 100x100 cm

			float Alpha = FMath::Lerp(P.AlphaStart, P.AlphaEnd, T);
			if (P.FadeIn > KINDA_SMALL_NUMBER && P.Age < P.FadeIn)
			{
				Alpha *= FMath::Clamp(P.Age / P.FadeIn, 0.f, 1.f);
			}
			const FLinearColor Col = FMath::Lerp(P.ColorA, P.ColorB, T);

			FQuat Quat = FQuat::Identity;
			FVector ScaleVec(Scale, Scale, Scale);

			if (P.Mode == EFXSpriteMode::Billboard)
			{
				const FVector ToCam = (CamLoc - P.Position).GetSafeNormal();
				const FRotator FaceRot = ToCam.Rotation();
				// Plane lies in XY (normal +Z), so rotate +Z to face camera
				Quat = FQuat(FaceRot) * FQuat(FRotator(-90.f, 0.f, 0.f)) * FQuat(FVector::UpVector, FMath::DegreesToRadians(P.Rotation.X));
			}
			else if (P.Mode == EFXSpriteMode::VelocityAligned)
			{
				const FVector VelDir = P.Velocity.GetSafeNormal();
				const FVector ToCam = (CamLoc - P.Position).GetSafeNormal();
				const FVector Right = FVector::CrossProduct(ToCam, VelDir).GetSafeNormal();
				const FVector Normal = FVector::CrossProduct(VelDir, Right).GetSafeNormal();
				const FMatrix M(VelDir, Right, Normal, FVector::ZeroVector);
				Quat = M.ToQuat();
				ScaleVec = FVector(Scale * FMath::Max(1.f, P.Stretch), Scale, Scale);
			}
			else
			{
				Quat = FQuat(FRotator(P.Rotation.Y, P.Rotation.Z, P.Rotation.X));
			}

			const int32 InstIdx = ISM->AddInstance(FTransform(Quat, P.Position, ScaleVec), true);
			ISM->SetCustomDataValue(InstIdx, 0, Col.R, false);
			ISM->SetCustomDataValue(InstIdx, 1, Col.G, false);
			ISM->SetCustomDataValue(InstIdx, 2, Col.B, false);
			ISM->SetCustomDataValue(InstIdx, 3, Alpha, false);
		}
		ISM->MarkRenderStateDirty();
	}

	// 5. Pooled meshes update
	for (int32 i = State->ActiveMeshes.Num() - 1; i >= 0; --i)
	{
		auto& M = State->ActiveMeshes[i];
		UStaticMeshComponent* Comp = M.Comp.Get();
		if (!Comp || !IsValid(Comp))
		{
			State->ActiveMeshes.RemoveAtSwap(i);
			continue;
		}

		M.Age += DeltaTime;
		if (M.LifeTime > 0.f && M.Age >= M.LifeTime)
		{
			ReturnPoolMesh(Comp);
			State->ActiveMeshes.RemoveAtSwap(i);
			continue;
		}

		if (M.ScaleTime > KINDA_SMALL_NUMBER)
		{
			const float ST = FMath::Clamp(M.Age / M.ScaleTime, 0.f, 1.f);
			const float Eased = 1.f - FMath::Pow(1.f - ST, 3.f);
			const FVector CurScale = FMath::Lerp(M.ScaleStart, M.ScaleEnd, Eased);
			Comp->SetRelativeScale3D(M.BaseScale * CurScale);
		}

		if (M.LifeTime > 0.f && M.FadeOutTime > KINDA_SMALL_NUMBER && M.MID.IsValid())
		{
			const float Remaining = M.LifeTime - M.Age;
			if (Remaining < M.FadeOutTime)
			{
				M.MID->SetScalarParameterValue(TEXT("Fade"), FMath::Clamp(Remaining / M.FadeOutTime, 0.f, 1.f));
			}
		}
	}

	// 6. Pooled lights update
	for (int32 i = State->ActiveLights.Num() - 1; i >= 0; --i)
	{
		auto& L = State->ActiveLights[i];
		UPointLightComponent* Comp = L.Comp.Get();
		if (!Comp || !IsValid(Comp))
		{
			State->ActiveLights.RemoveAtSwap(i);
			continue;
		}

		L.Age += DeltaTime;
		if (L.Age >= L.LifeTime)
		{
			ReturnPoolLight(Comp);
			State->ActiveLights.RemoveAtSwap(i);
			continue;
		}

		float Env = 1.f;
		if (L.AttackTime > KINDA_SMALL_NUMBER && L.Age < L.AttackTime)
		{
			Env = L.Age / L.AttackTime;
		}
		else
		{
			const float DecayT = FMath::Clamp((L.Age - L.AttackTime) / FMath::Max(0.01f, L.LifeTime - L.AttackTime), 0.f, 1.f);
			Env = FMath::Pow(1.f - DecayT, 2.2f);
		}

		if (L.Flicker > KINDA_SMALL_NUMBER)
		{
			Env *= FMath::FRandRange(1.f - L.Flicker, 1.f);
		}
		Comp->SetIntensity(L.PeakIntensity * FMath::Max(0.f, Env));
	}

	// 7. Decals update
	for (int32 i = State->ActiveDecals.Num() - 1; i >= 0; --i)
	{
		auto& D = State->ActiveDecals[i];
		UDecalComponent* Comp = D.Comp.Get();
		if (!Comp || !IsValid(Comp))
		{
			State->ActiveDecals.RemoveAtSwap(i);
			continue;
		}

		if (D.LifeTime > 0.f)
		{
			D.Age += DeltaTime;
			if (D.Age >= D.LifeTime)
			{
				Comp->DestroyComponent();
				State->ActiveDecals.RemoveAtSwap(i);
			}
		}
	}

	// 8. Lightning bolts update
	for (int32 i = State->ActiveBolts.Num() - 1; i >= 0; --i)
	{
		auto& B = State->ActiveBolts[i];
		B.Age += DeltaTime;
		if (B.Params.LifeTime > 0.f && B.Age >= B.Params.LifeTime)
		{
			for (auto& Seg : B.Segments)
			{
				if (UStaticMeshComponent* Comp = Seg.Get())
				{
					ReleaseMesh(Comp);
				}
			}
			State->ActiveBolts.RemoveAtSwap(i);
			continue;
		}

		if (B.Params.RejitInterval > KINDA_SMALL_NUMBER)
		{
			B.RejitTimer += DeltaTime;
			if (B.RejitTimer >= B.Params.RejitInterval)
			{
				B.RejitTimer = 0.f;
				FFXRuntimeState::RebuildLightningBolt(this, B);
			}
		}

		if (B.Params.LifeTime > 0.f)
		{
			const float Fade = FMath::Clamp(1.f - (B.Age / B.Params.LifeTime), 0.f, 1.f);
			for (auto& MID : B.MIDs)
			{
				if (MID.IsValid())
				{
					MID->SetScalarParameterValue(TEXT("Fade"), Fade);
				}
			}
		}
	}

	// 9. Physics-lite debris chips
	for (int32 i = State->ActiveDebris.Num() - 1; i >= 0; --i)
	{
		auto& D = State->ActiveDebris[i];
		UStaticMeshComponent* Comp = D.Comp.Get();
		if (!Comp || !IsValid(Comp))
		{
			State->ActiveDebris.RemoveAtSwap(i);
			continue;
		}

		D.Age += DeltaTime;
		if (D.Age >= D.Life)
		{
			ReturnPoolMesh(Comp);
			State->ActiveDebris.RemoveAtSwap(i);
			continue;
		}

		D.Velocity.Z -= 980.f * D.GravityScale * DeltaTime;
		D.Position += D.Velocity * DeltaTime;
		D.Rotation += D.Spin * DeltaTime;

		if (D.Position.Z <= D.GroundZ)
		{
			D.Position.Z = D.GroundZ;
			if (!D.bBounced && FMath::Abs(D.Velocity.Z) > 80.f)
			{
				D.bBounced = true;
				D.Velocity.Z = FMath::Abs(D.Velocity.Z) * D.Bounciness;
				D.Velocity.X *= 0.6f;
				D.Velocity.Y *= 0.6f;
				D.Spin *= 0.5f;
			}
			else
			{
				D.Velocity = FVector::ZeroVector;
				D.Spin = FRotator::ZeroRotator;
			}
		}

		Comp->SetWorldLocationAndRotation(D.Position, D.Rotation);
	}
}

int32 UWizardFXSubsystem::SpawnSprite(const FFXSpriteParams& Params)
{
	if (!State)
	{
		return 0;
	}
	const int32 Idx = static_cast<int32>(Params.Kind);
	if (Idx < 0 || Idx >= MaxSpriteKinds)
	{
		return 0;
	}
	auto& Layer = State->Sprites[Idx];
	if (Layer.Num() >= MaxSpritesPerLayer)
	{
		return 0;
	}

	FFXRuntimeState::FSpriteParticle P;
	P.Id = State->NextSpriteId++;
	P.Kind = Params.Kind;
	P.Mode = Params.Mode;
	P.Position = Params.Position;
	P.Velocity = Params.Velocity;
	P.Life = FMath::Max(0.05f, Params.Life);
	P.SizeStart = Params.Size;
	P.SizeEnd = Params.SizeEnd;
	P.ColorA = Params.Color;
	P.ColorB = Params.Color;
	P.AlphaStart = Params.AlphaStart;
	P.AlphaEnd = Params.AlphaEnd;
	P.FadeIn = Params.FadeIn;
	P.GravityScale = Params.GravityScale;
	P.Drag = Params.Drag;
	P.Rotation = Params.Rotation;
	P.RotationSpeed = Params.RotationSpeed;
	P.Stretch = Params.Stretch;
	Layer.Add(P);
	return P.Id;
}

int32 UWizardFXSubsystem::Burst(const FFXBurstSpec& Spec, const FTransform& Origin, float ScaleMul, const FLinearColor& TintMul)
{
	if (!State || Spec.Count <= 0)
	{
		return 0;
	}

	const int32 Idx = static_cast<int32>(Spec.Kind);
	if (Idx < 0 || Idx >= MaxSpriteKinds)
	{
		return 0;
	}

	auto& Layer = State->Sprites[Idx];
	const FVector Axis = Origin.GetUnitAxis(EAxis::X);
	int32 Spawned = 0;

	for (int32 i = 0; i < Spec.Count; ++i)
	{
		if (Layer.Num() >= MaxSpritesPerLayer)
		{
			break;
		}

		const FVector Dir = RandomConeDir(Axis, Spec.ConeHalfAngle);
		const float Speed = FMath::FRandRange(Spec.SpeedMin, Spec.SpeedMax) * ScaleMul;
		const FVector Vel = (Dir + FVector(0.f, 0.f, Spec.UpBias)).GetSafeNormal() * Speed;
		const FVector Offset = Spec.SpawnRadius > KINDA_SMALL_NUMBER ? FMath::VRand() * FMath::FRandRange(0.f, Spec.SpawnRadius * ScaleMul) : FVector::ZeroVector;
		const float StartSize = FMath::FRandRange(Spec.SizeMin, Spec.SizeMax) * ScaleMul;

		FFXRuntimeState::FSpriteParticle P;
		P.Id = State->NextSpriteId++;
		P.Kind = Spec.Kind;
		P.Mode = Spec.Mode;
		P.Position = Origin.GetLocation() + Offset;
		P.Velocity = Vel;
		P.Life = FMath::Max(0.05f, FMath::FRandRange(Spec.LifeMin, Spec.LifeMax));
		P.SizeStart = StartSize;
		P.SizeEnd = StartSize * Spec.SizeEndMul;
		P.ColorA = Spec.ColorA * TintMul;
		P.ColorB = Spec.ColorB * TintMul;
		P.AlphaStart = Spec.AlphaStart;
		P.AlphaEnd = Spec.AlphaEnd;
		P.FadeIn = Spec.FadeIn;
		P.GravityScale = Spec.GravityScale;
		P.Drag = Spec.Drag;
		P.Rotation = FVector(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f));
		P.RotationSpeed = FVector(
			FMath::FRandRange(-Spec.RotationSpeedMax, Spec.RotationSpeedMax),
			FMath::FRandRange(-Spec.RotationSpeedMax, Spec.RotationSpeedMax),
			FMath::FRandRange(-Spec.RotationSpeedMax, Spec.RotationSpeedMax));
		P.Stretch = Spec.Stretch;
		Layer.Add(P);
		Spawned++;
	}

	return Spawned;
}

UStaticMeshComponent* UWizardFXSubsystem::SpawnMesh(const FFXMeshParams& Params)
{
	UStaticMeshComponent* Comp = AcquirePoolMesh();
	if (!Comp)
	{
		return nullptr;
	}

	UStaticMesh* Mesh = GetFXMesh(Params.MeshName);
	if (!Mesh)
	{
		// Fallback to engine basic shapes so visuals still work before forge runs
		if (Params.MeshName == TEXT("SM_FX_Sphere"))
		{
			Mesh = Cast<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")).TryLoad());
		}
		else if (Params.MeshName == TEXT("SM_FX_Pillar"))
		{
			Mesh = Cast<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")).TryLoad());
		}
		else
		{
			Mesh = Cast<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")).TryLoad());
		}
	}
	if (Mesh)
	{
		Comp->SetStaticMesh(Mesh);
	}

	UMaterialInstanceDynamic* MID = nullptr;
	UMaterialInterface* Mat = GetFXMaterial(Params.MaterialName);
	if (!Mat)
	{
		Mat = Cast<UMaterialInterface>(FSoftObjectPath(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")).TryLoad());
	}
	if (Mat)
	{
		MID = UMaterialInstanceDynamic::Create(Mat, Comp);
		for (const auto& Pair : Params.ScalarParams)
		{
			MID->SetScalarParameterValue(Pair.Key, Pair.Value);
		}
		for (const auto& Pair : Params.VectorParams)
		{
			MID->SetVectorParameterValue(Pair.Key, Pair.Value);
		}
		Comp->SetMaterial(0, MID);
	}

	Comp->SetCastShadow(Params.bCastShadow);
	Comp->SetCollisionEnabled(Params.bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (Params.bCollision)
	{
		Comp->SetCollisionResponseToAllChannels(ECR_Block);
		Comp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Comp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	if (Params.AttachTo.IsValid())
	{
		Comp->AttachToComponent(Params.AttachTo.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Params.AttachSocket);
		Comp->SetRelativeTransform(Params.Transform);
	}
	else
	{
		Comp->SetWorldTransform(Params.Transform);
	}

	const FVector BaseScale = Params.Transform.GetScale3D();
	Comp->SetRelativeScale3D(BaseScale * Params.ScaleStart);

	FFXRuntimeState::FActiveMesh Entry;
	Entry.Comp = Comp;
	Entry.MID = MID;
	Entry.BaseScale = BaseScale;
	Entry.ScaleStart = Params.ScaleStart;
	Entry.ScaleEnd = Params.ScaleEnd;
	Entry.ScaleTime = Params.ScaleTime;
	Entry.LifeTime = Params.LifeTime;
	Entry.FadeOutTime = Params.FadeOutTime;
	State->ActiveMeshes.Add(Entry);
	return Comp;
}

void UWizardFXSubsystem::ReleaseMesh(UStaticMeshComponent* Mesh)
{
	if (!Mesh || !State)
	{
		return;
	}
	for (int32 i = State->ActiveMeshes.Num() - 1; i >= 0; --i)
	{
		if (State->ActiveMeshes[i].Comp.Get() == Mesh)
		{
			State->ActiveMeshes.RemoveAtSwap(i);
			break;
		}
	}
	ReturnPoolMesh(Mesh);
}

UPointLightComponent* UWizardFXSubsystem::SpawnLight(const FFXLightParams& Params)
{
	UPointLightComponent* Light = AcquirePoolLight();
	if (!Light)
	{
		return nullptr;
	}

	const float SafePeakIntensity = (Params.Intensity > KINDA_SMALL_NUMBER) ? FMath::Clamp(Params.Intensity, 45.f, 160.f) : 0.f;
	Light->SetIntensityUnits(ELightUnits::Lumens);
	Light->SetLightColor(Params.Color);
	Light->SetAttenuationRadius(Params.AttenuationRadius);
	Light->SetCastShadows(Params.bCastShadows);
	Light->SetIntensity(Params.AttackTime <= KINDA_SMALL_NUMBER ? SafePeakIntensity : 0.f);

	if (Params.AttachTo.IsValid())
	{
		Light->AttachToComponent(Params.AttachTo.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Params.AttachSocket);
	}
	else
	{
		Light->SetWorldLocation(Params.Location);
	}

	FFXRuntimeState::FActiveLight Entry;
	Entry.Comp = Light;
	Entry.PeakIntensity = SafePeakIntensity;
	Entry.LifeTime = FMath::Max(0.05f, Params.LifeTime);
	Entry.AttackTime = Params.AttackTime;
	Entry.Flicker = Params.Flicker;
	State->ActiveLights.Add(Entry);
	return Light;
}

void UWizardFXSubsystem::SpawnDecal(const FFXDecalParams& Params)
{
	UWorld* World = GetWorld();
	if (!World || !State)
	{
		return;
	}

	UMaterialInterface* Mat = GetFXMaterial(Params.MaterialName);
	if (!Mat)
	{
		return;
	}

	UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
		World,
		Mat,
		Params.Size,
		Params.Location,
		Params.Rotation,
		0.f);
	if (Decal)
	{
		if (Params.LifeTime > 0.f && Params.FadeOutTime > 0.f)
		{
			Decal->SetFadeOut(FMath::Max(0.f, Params.LifeTime - Params.FadeOutTime), Params.FadeOutTime, true);
		}
		FFXRuntimeState::FActiveDecal Entry;
		Entry.Comp = Decal;
		Entry.LifeTime = Params.LifeTime;
		Entry.FadeOutTime = Params.FadeOutTime;
		State->ActiveDecals.Add(Entry);
	}
}

FFXHandle UWizardFXSubsystem::SpawnLightning(const FFXLightningParams& Params)
{
	if (!State)
	{
		return FFXHandle();
	}

	FFXRuntimeState::FActiveLightning Bolt;
	Bolt.Id = State->NextHandleId++;
	Bolt.Params = Params;
	FFXRuntimeState::RebuildLightningBolt(this, Bolt);
	State->ActiveBolts.Add(MoveTemp(Bolt));

	FFXHandle Handle;
	Handle.Id = State->ActiveBolts.Last().Id;
	return Handle;
}

void UWizardFXSubsystem::UpdateLightning(FFXHandle BoltHandle, const FVector& Start, const FVector& End)
{
	if (!State || !BoltHandle.IsValid())
	{
		return;
	}
	for (auto& Bolt : State->ActiveBolts)
	{
		if (Bolt.Id == BoltHandle.Id)
		{
			Bolt.Params.Start = Start;
			Bolt.Params.End = End;
			FFXRuntimeState::RebuildLightningBolt(this, Bolt);
			break;
		}
	}
}

void UWizardFXSubsystem::EndLightning(FFXHandle BoltHandle)
{
	if (!State || !BoltHandle.IsValid())
	{
		return;
	}
	for (int32 i = State->ActiveBolts.Num() - 1; i >= 0; --i)
	{
		if (State->ActiveBolts[i].Id == BoltHandle.Id)
		{
			for (auto& Seg : State->ActiveBolts[i].Segments)
			{
				if (UStaticMeshComponent* Comp = Seg.Get())
				{
					ReleaseMesh(Comp);
				}
			}
			State->ActiveBolts.RemoveAtSwap(i);
			break;
		}
	}
}

void UWizardFXSubsystem::SpawnDebris(const FFXDebrisParams& Params)
{
	if (!State || Params.Count <= 0)
	{
		return;
	}

	UStaticMesh* Mesh = GetFXMesh(Params.MeshName.IsNone() ? FName(TEXT("SM_FX_Chip")) : Params.MeshName);
	if (!Mesh)
	{
		Mesh = Cast<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")).TryLoad());
	}
	UMaterialInterface* Mat = GetFXMaterial(Params.MaterialName);
	if (!Mat)
	{
		Mat = Cast<UMaterialInterface>(FSoftObjectPath(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")).TryLoad());
	}

	const FVector Axis = Params.Origin.GetUnitAxis(EAxis::X);
	const float GroundZ = Params.Origin.GetLocation().Z - 200.f;

	// Quick trace down to find approximate ground Z
	float TracedGroundZ = 0.f;
	if (UWorld* World = GetWorld())
	{
		FHitResult Hit;
		const FVector Start = Params.Origin.GetLocation() + FVector(0.f, 0.f, 50.f);
		if (World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, 2000.f), ECC_WorldStatic))
		{
			TracedGroundZ = static_cast<float>(Hit.ImpactPoint.Z) + 6.f;
		}
		else
		{
			TracedGroundZ = FMath::Min(0.f, GroundZ);
		}
	}

	const float UnitInv = GetFXMesh(Params.MeshName.IsNone() ? FName(TEXT("SM_FX_Chip")) : Params.MeshName) ? 0.05f : 0.01f;

	for (int32 i = 0; i < Params.Count; ++i)
	{
		if (State->ActiveDebris.Num() >= MaxDebrisActive)
		{
			break;
		}

		UStaticMeshComponent* Comp = AcquirePoolMesh();
		if (!Comp)
		{
			break;
		}
		if (Mesh)
		{
			Comp->SetStaticMesh(Mesh);
		}
		if (Mat)
		{
			Comp->SetMaterial(0, Mat);
		}
		Comp->SetCastShadow(false);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		const float Size = FMath::FRandRange(Params.SizeMin, Params.SizeMax);
		const FVector Scale(Size * UnitInv, Size * UnitInv * FMath::FRandRange(0.6f, 1.2f), Size * UnitInv * FMath::FRandRange(0.4f, 0.9f));
		Comp->SetWorldScale3D(Scale);

		const FVector Dir = RandomConeDir(Axis, Params.ConeHalfAngle);
		const float Speed = FMath::FRandRange(Params.SpeedMin, Params.SpeedMax);

		FFXRuntimeState::FActiveDebris D;
		D.Comp = Comp;
		D.Position = Params.Origin.GetLocation() + FMath::VRand() * 25.f;
		D.Velocity = (Dir + FVector(0.f, 0.f, Params.UpBias)).GetSafeNormal() * Speed;
		D.Rotation = FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f));
		D.Spin = FRotator(
			FMath::FRandRange(-Params.Spin, Params.Spin),
			FMath::FRandRange(-Params.Spin, Params.Spin),
			FMath::FRandRange(-Params.Spin, Params.Spin));
		D.GravityScale = Params.GravityScale;
		D.Bounciness = Params.Bounciness;
		D.GroundZ = TracedGroundZ;
		D.Life = FMath::Max(0.2f, FMath::FRandRange(Params.LifeMin, Params.LifeMax));
		Comp->SetWorldLocationAndRotation(D.Position, D.Rotation);

		State->ActiveDebris.Add(D);
	}
}

UNiagaraComponent* UWizardFXSubsystem::SpawnNiagara(const TSoftObjectPtr<UNiagaraSystem>& System, const FTransform& Transform, float Scale)
{
	if (System.IsNull())
	{
		return nullptr;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	if (UNiagaraSystem* Loaded = System.LoadSynchronous())
	{
		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			Loaded,
			Transform.GetLocation(),
			Transform.Rotator(),
			Transform.GetScale3D() * Scale,
			true,
			true,
			ENCPoolMethod::AutoRelease);
	}
	return nullptr;
}

UNiagaraComponent* UWizardFXSubsystem::SpawnNiagaraAttached(const TSoftObjectPtr<UNiagaraSystem>& System, USceneComponent* AttachTo, FName Socket, float Scale)
{
	if (System.IsNull() || !AttachTo)
	{
		return nullptr;
	}
	if (UNiagaraSystem* Loaded = System.LoadSynchronous())
	{
		return UNiagaraFunctionLibrary::SpawnSystemAttached(
			Loaded,
			AttachTo,
			Socket,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			FVector(Scale),
			EAttachLocation::SnapToTarget,
			true,
			ENCPoolMethod::AutoRelease);
	}
	return nullptr;
}

FFXHandle UWizardFXSubsystem::Delay(float Seconds, TFunction<void()>&& Callback)
{
	if (!State || !Callback)
	{
		return FFXHandle();
	}
	if (Seconds <= KINDA_SMALL_NUMBER)
	{
		Callback();
		return FFXHandle();
	}

	FFXRuntimeState::FScheduledCallback Entry;
	Entry.Id = State->NextHandleId++;
	Entry.Remaining = Seconds;
	Entry.bRealtime = false;
	Entry.Callback = MoveTemp(Callback);
	State->Callbacks.Add(MoveTemp(Entry));

	FFXHandle H;
	H.Id = State->Callbacks.Last().Id;
	return H;
}

FFXHandle UWizardFXSubsystem::DelayRealtime(float Seconds, TFunction<void()>&& Callback)
{
	if (!State || !Callback)
	{
		return FFXHandle();
	}
	if (Seconds <= KINDA_SMALL_NUMBER)
	{
		Callback();
		return FFXHandle();
	}

	FFXRuntimeState::FScheduledCallback Entry;
	Entry.Id = State->NextHandleId++;
	Entry.Remaining = Seconds;
	Entry.bRealtime = true;
	Entry.Callback = MoveTemp(Callback);
	State->Callbacks.Add(MoveTemp(Entry));

	FFXHandle H;
	H.Id = State->Callbacks.Last().Id;
	return H;
}

void UWizardFXSubsystem::Cancel(FFXHandle Handle)
{
	if (!State || !Handle.IsValid())
	{
		return;
	}
	for (int32 i = State->Callbacks.Num() - 1; i >= 0; --i)
	{
		if (State->Callbacks[i].Id == Handle.Id)
		{
			State->Callbacks.RemoveAtSwap(i);
			break;
		}
	}
}

void UWizardFXSubsystem::HitStop(float Seconds, float Dilation)
{
	UWorld* World = GetWorld();
	if (!State || !World || Seconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	State->HitStopRemaining = FMath::Max(State->HitStopRemaining, Seconds);
	State->bHitStopActive = true;
	UGameplayStatics::SetGlobalTimeDilation(World, FMath::Clamp(Dilation, 0.01f, 1.f));
}

UStaticMesh* UWizardFXSubsystem::GetFXMesh(FName MeshName)
{
	if (MeshName.IsNone() || !State)
	{
		return nullptr;
	}
	if (TObjectPtr<UStaticMesh>* Found = MeshCache.Find(MeshName))
	{
		return Found->Get();
	}
	if (State->MissingAssets.Contains(MeshName))
	{
		return nullptr;
	}
	const FString Path = FString::Printf(TEXT("%s/%s"), WizardPaths::FXMeshes, *MeshName.ToString());
	UStaticMesh* Loaded = Cast<UStaticMesh>(FSoftObjectPath(Path).TryLoad());
	if (Loaded)
	{
		MeshCache.Add(MeshName, Loaded);
	}
	else
	{
		State->MissingAssets.Add(MeshName);
	}
	return Loaded;
}

UMaterialInterface* UWizardFXSubsystem::GetFXMaterial(FName MaterialName)
{
	if (MaterialName.IsNone() || !State)
	{
		return nullptr;
	}
	if (TObjectPtr<UMaterialInterface>* Found = MaterialCache.Find(MaterialName))
	{
		return Found->Get();
	}
	if (State->MissingAssets.Contains(MaterialName))
	{
		return nullptr;
	}
	const FString Path = FString::Printf(TEXT("%s/%s"), WizardPaths::FXMaterials, *MaterialName.ToString());
	UMaterialInterface* Loaded = Cast<UMaterialInterface>(FSoftObjectPath(Path).TryLoad());
	if (Loaded)
	{
		MaterialCache.Add(MaterialName, Loaded);
	}
	else
	{
		State->MissingAssets.Add(MaterialName);
	}
	return Loaded;
}

UTexture2D* UWizardFXSubsystem::GetFXTexture(FName TextureName)
{
	if (TextureName.IsNone() || !State)
	{
		return nullptr;
	}
	if (TObjectPtr<UTexture2D>* Found = TextureCache.Find(TextureName))
	{
		return Found->Get();
	}
	if (State->MissingAssets.Contains(TextureName))
	{
		return nullptr;
	}
	const FString Path = FString::Printf(TEXT("%s/%s"), WizardPaths::FXTextures, *TextureName.ToString());
	UTexture2D* Loaded = Cast<UTexture2D>(FSoftObjectPath(Path).TryLoad());
	if (Loaded)
	{
		TextureCache.Add(TextureName, Loaded);
	}
	else
	{
		State->MissingAssets.Add(TextureName);
	}
	return Loaded;
}

void UWizardFXSubsystem::ClearTransient()
{
	if (!State)
	{
		return;
	}

	if (State->bHitStopActive)
	{
		State->bHitStopActive = false;
		State->HitStopRemaining = 0.f;
		if (UWorld* World = GetWorld())
		{
			UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
		}
	}

	for (int32 i = 0; i < MaxSpriteKinds; ++i)
	{
		State->Sprites[i].Reset();
		if (SpriteLayers.IsValidIndex(i) && SpriteLayers[i] && IsValid(SpriteLayers[i]))
		{
			SpriteLayers[i]->ClearInstances();
		}
	}

	for (auto& M : State->ActiveMeshes)
	{
		if (UStaticMeshComponent* Comp = M.Comp.Get())
		{
			ReturnPoolMesh(Comp);
		}
	}
	State->ActiveMeshes.Reset();

	for (auto& L : State->ActiveLights)
	{
		if (UPointLightComponent* Comp = L.Comp.Get())
		{
			ReturnPoolLight(Comp);
		}
	}
	State->ActiveLights.Reset();

	for (auto& D : State->ActiveDecals)
	{
		if (UDecalComponent* Comp = D.Comp.Get())
		{
			Comp->DestroyComponent();
		}
	}
	State->ActiveDecals.Reset();

	for (auto& B : State->ActiveBolts)
	{
		for (auto& Seg : B.Segments)
		{
			if (UStaticMeshComponent* Comp = Seg.Get())
			{
				ReturnPoolMesh(Comp);
			}
		}
	}
	State->ActiveBolts.Reset();

	for (auto& D : State->ActiveDebris)
	{
		if (UStaticMeshComponent* Comp = D.Comp.Get())
		{
			ReturnPoolMesh(Comp);
		}
	}
	State->ActiveDebris.Reset();

	State->Callbacks.Reset();
}
