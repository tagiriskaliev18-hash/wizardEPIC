#include "WizardCharacter.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "SpellDataAsset.h"
#include "UObject/SoftObjectPath.h"
#include "WizardAbilityComponent.h"
#include "WizardTypes.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardCharacter)

namespace
{
	template <typename T>
	T* TryLoadAssetIfExists(const TCHAR* ObjectPath)
	{
		const FSoftObjectPath SoftPath(ObjectPath);
		const FString PackageName = SoftPath.GetLongPackageName();
		if (!PackageName.IsEmpty() && FPackageName::DoesPackageExist(PackageName))
		{
			return Cast<T>(SoftPath.TryLoad());
		}
		return nullptr;
	}

	template <typename T>
	UClass* TryLoadClassIfExists(const TCHAR* ClassPath)
	{
		const FSoftClassPath SoftPath(ClassPath);
		const FString PackageName = SoftPath.GetLongPackageName();
		if (!PackageName.IsEmpty() && FPackageName::DoesPackageExist(PackageName))
		{
			return SoftPath.TryLoadClass<T>();
		}
		return nullptr;
	}

	FTransform GetRefBoneComponentTransform(const USkeletalMesh* SkelMesh, FName BoneName, bool& bOutFound)
	{
		bOutFound = false;
		if (!SkelMesh)
		{
			return FTransform::Identity;
		}
		const FReferenceSkeleton& RefSkel = SkelMesh->GetRefSkeleton();
		int32 BoneIdx = RefSkel.FindBoneIndex(BoneName);
		if (BoneIdx == INDEX_NONE)
		{
			return FTransform::Identity;
		}
		const TArray<FTransform>& RefPoses = RefSkel.GetRefBonePose();
		FTransform CS = FTransform::Identity;
		while (BoneIdx != INDEX_NONE && RefPoses.IsValidIndex(BoneIdx))
		{
			CS = CS * RefPoses[BoneIdx];
			BoneIdx = RefSkel.GetParentIndex(BoneIdx);
		}
		bOutFound = true;
		return CS;
	}

	FLinearColor GetElementCrystalColor(ESpellElement Element)
	{
		switch (Element)
		{
		case ESpellElement::Fire:      return FLinearColor(1.00f, 0.34f, 0.04f, 1.f); // Crimson-Gold
		case ESpellElement::Earth:     return FLinearColor(0.20f, 0.92f, 0.36f, 1.f); // Amber-Emerald
		case ESpellElement::Wind:      return FLinearColor(0.42f, 0.95f, 1.00f, 1.f); // Icy-Cyan
		case ESpellElement::Lightning: return FLinearColor(0.48f, 0.32f, 1.00f, 1.f); // Electric-Violet
		default:                       return FLinearColor(1.00f, 0.34f, 0.04f, 1.f);
		}
	}

	constexpr float StaffCrystalLocalZ = 106.f;
}

AWizardCharacter::AWizardCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
		Move->RotationRate = FRotator(0.f, 640.f, 0.f);
		Move->JumpZVelocity = 620.f;
		Move->AirControl = 0.45f;
		Move->MaxWalkSpeed = 520.f;
	}

	if (USkeletalMeshComponent* CharMesh = GetMesh())
	{
		CharMesh->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
		CharMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 420.f;
	CameraBoom->SocketOffset = FVector(0.f, 70.f, 75.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;
	CameraBoom->bDoCollisionTest = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->FieldOfView = 82.f;

	AbilityComponent = CreateDefaultSubobject<UWizardAbilityComponent>(TEXT("AbilityComponent"));
	AbilityComponent->WalkSpeed = 520.f;

	HatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HatMesh"));
	HatMesh->SetupAttachment(GetMesh());
	HatMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HatMesh->SetCastShadow(true);

	CloakMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CloakMesh"));
	CloakMesh->SetupAttachment(GetMesh());
	CloakMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CloakMesh->SetCastShadow(true);

	StaffMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaffMesh"));
	StaffMesh->SetupAttachment(GetMesh());
	StaffMesh->SetRelativeLocation(FVector(24.f, 28.f, 95.f));
	StaffMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaffMesh->SetCastShadow(true);

	StaffOrbMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaffOrbMesh"));
	StaffOrbMesh->SetupAttachment(StaffMesh);
	StaffOrbMesh->SetRelativeLocation(FVector(0.f, 0.f, StaffCrystalLocalZ));
	StaffOrbMesh->SetRelativeScale3D(FVector(0.08f));
	StaffOrbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaffOrbMesh->SetCastShadow(false);

	StaffRuneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaffRuneMesh"));
	StaffRuneMesh->SetupAttachment(StaffMesh);
	StaffRuneMesh->SetRelativeLocation(FVector(0.f, 0.f, StaffCrystalLocalZ));
	StaffRuneMesh->SetRelativeRotation(FRotator(68.f, 0.f, 0.f));
	StaffRuneMesh->SetRelativeScale3D(FVector(0.36f));
	StaffRuneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaffRuneMesh->SetCastShadow(false);

	StaffLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StaffLight"));
	StaffLight->SetupAttachment(StaffMesh);
	StaffLight->SetRelativeLocation(FVector(0.f, 0.f, StaffCrystalLocalZ));
	StaffLight->SetIntensityUnits(ELightUnits::Lumens);
	StaffLight->SetIntensity(48.f);
	StaffLight->SetAttenuationRadius(380.f);
	StaffLight->SetCastShadows(false);
	StaffLight->SetLightColor(ActiveOrbColor);
}

void AWizardCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AttachWizardGear();
}

void AWizardCharacter::BeginPlay()
{
	if (AbilityComponent)
	{
		AbilityComponent->OnSpellSelected.AddUniqueDynamic(this, &AWizardCharacter::HandleSpellSelected);
		AbilityComponent->OnChargeChanged.AddUniqueDynamic(this, &AWizardCharacter::HandleChargeChanged);
		AbilityComponent->OnSpellCast.AddUniqueDynamic(this, &AWizardCharacter::HandleSpellCast);
	}

	Super::BeginPlay();

	if (USkeletalMeshComponent* CharMesh = GetMesh())
	{
		USkeletalMesh* SkelMesh = TryLoadAssetIfExists<USkeletalMesh>(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
		if (!SkelMesh)
		{
			SkelMesh = TryLoadAssetIfExists<USkeletalMesh>(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
		}
		if (SkelMesh)
		{
			CharMesh->SetSkeletalMesh(SkelMesh);
			CharMesh->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
			CharMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		}

		if (UClass* AnimCls = TryLoadClassIfExists<UAnimInstance>(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C")))
		{
			CharMesh->SetAnimInstanceClass(AnimCls);
		}

		ApplyWizardRobeTint();
	}

	// Load sculpted wizard hat & battle-mage cloak
	if (HatMesh)
	{
		if (UStaticMesh* HatStatic = TryLoadAssetIfExists<UStaticMesh>(TEXT("/Game/Wizard/Environment/Meshes/SM_WizardHat.SM_WizardHat")))
		{
			HatMesh->SetStaticMesh(HatStatic);
		}
	}

	if (CloakMesh)
	{
		if (UStaticMesh* CloakStatic = TryLoadAssetIfExists<UStaticMesh>(TEXT("/Game/Wizard/Environment/Meshes/SM_WizardCloak.SM_WizardCloak")))
		{
			CloakMesh->SetStaticMesh(CloakStatic);
		}
	}

	// Load upgraded gnarled forked oak staff with faceted floating elemental crystal
	if (StaffMesh)
	{
		if (UStaticMesh* StaffStatic = TryLoadAssetIfExists<UStaticMesh>(TEXT("/Game/Wizard/Environment/Meshes/SM_WizardStaff.SM_WizardStaff")))
		{
			StaffMesh->SetStaticMesh(StaffStatic);
		}

		// Create dynamic material instance for the sculpted crystal slot on SM_WizardStaff
		int32 CrystalSlotIdx = INDEX_NONE;
		const TArray<FName> SlotNames = StaffMesh->GetMaterialSlotNames();
		for (int32 i = 0; i < SlotNames.Num(); ++i)
		{
			const FString Lower = SlotNames[i].ToString().ToLower();
			if (Lower.Contains(TEXT("crystal")) || Lower.Contains(TEXT("gem")) || Lower.Contains(TEXT("orb")))
			{
				CrystalSlotIdx = i;
				break;
			}
		}
		if (CrystalSlotIdx == INDEX_NONE && StaffMesh->GetNumMaterials() >= 2)
		{
			CrystalSlotIdx = 1;
		}
		if (CrystalSlotIdx != INDEX_NONE)
		{
			UMaterialInterface* CrystalMat = TryLoadAssetIfExists<UMaterialInterface>(TEXT("/Game/Wizard/FX/Materials/M_StaffCrystal.M_StaffCrystal"));
			if (!CrystalMat)
			{
				CrystalMat = StaffMesh->GetMaterial(CrystalSlotIdx);
			}
			if (CrystalMat)
			{
				StaffCrystalMID = UMaterialInstanceDynamic::Create(CrystalMat, StaffMesh);
				if (StaffCrystalMID)
				{
					StaffMesh->SetMaterial(CrystalSlotIdx, StaffCrystalMID);
				}
			}
		}
	}

	// Subtle inner glow pinpoint inside the sculpted crystal (keeps GetStaffOrbLocation exact)
	if (StaffOrbMesh)
	{
		UStaticMesh* OrbStatic = TryLoadAssetIfExists<UStaticMesh>(TEXT("/Game/Wizard/FX/Meshes/SM_FX_Sphere.SM_FX_Sphere"));
		if (!OrbStatic)
		{
			OrbStatic = TryLoadAssetIfExists<UStaticMesh>(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		}
		if (OrbStatic)
		{
			StaffOrbMesh->SetStaticMesh(OrbStatic);
		}

		UMaterialInterface* OrbMat = TryLoadAssetIfExists<UMaterialInterface>(TEXT("/Game/Wizard/FX/Materials/M_FX_Core.M_FX_Core"));
		if (OrbMat)
		{
			StaffOrbMID = UMaterialInstanceDynamic::Create(OrbMat, StaffOrbMesh);
			if (StaffOrbMID)
			{
				StaffOrbMID->SetScalarParameterValue(TEXT("Fade"), 1.f);
				StaffOrbMesh->SetMaterial(0, StaffOrbMID);
			}
		}
	}

	// Floating geometric arcane rune circle around the staff crystal
	if (StaffRuneMesh)
	{
		if (UStaticMesh* RuneStatic = TryLoadAssetIfExists<UStaticMesh>(TEXT("/Game/Wizard/FX/Meshes/SM_RuneCircle.SM_RuneCircle")))
		{
			StaffRuneMesh->SetStaticMesh(RuneStatic);
		}
		UMaterialInterface* RuneMat = TryLoadAssetIfExists<UMaterialInterface>(TEXT("/Game/Wizard/FX/Materials/M_FX_RuneCircle.M_FX_RuneCircle"));
		if (RuneMat)
		{
			StaffRuneMID = UMaterialInstanceDynamic::Create(RuneMat, StaffRuneMesh);
			if (StaffRuneMID)
			{
				StaffRuneMID->SetScalarParameterValue(TEXT("Fade"), 1.f);
				StaffRuneMesh->SetMaterial(0, StaffRuneMID);
			}
		}
	}

	AttachWizardGear();

	if (AbilityComponent)
	{
		if (const USpellDataAsset* CurrentSpell = AbilityComponent->GetSelectedSpell())
		{
			ActiveOrbColor = GetElementCrystalColor(CurrentSpell->Element);
		}
		CurrentChargeT01 = AbilityComponent->GetChargeFraction();
	}

	UpdateStaffVisuals();
}

void AWizardCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CastBurstTimer > 0.f)
	{
		CastBurstTimer = FMath::Max(0.f, CastBurstTimer - DeltaTime);
		UpdateStaffVisuals();
	}

	// Continuous graceful spin of the arcane rune circle around the floating staff crystal
	if (StaffRuneMesh && StaffRuneMesh->IsRegistered())
	{
		const float BurstFactor = FMath::Clamp(CastBurstTimer / 0.85f, 0.f, 1.f);
		const float SpinSpeed = 52.f + 220.f * CurrentChargeT01 + 320.f * BurstFactor;
		RuneSpinAngle = FMath::Fmod(RuneSpinAngle + SpinSpeed * DeltaTime, 360.f);
		StaffRuneMesh->SetRelativeRotation(FRotator(68.f, RuneSpinAngle, 0.f));
	}
}

FVector AWizardCharacter::GetStaffOrbLocation() const
{
	if (StaffOrbMesh && StaffOrbMesh->IsRegistered())
	{
		return StaffOrbMesh->GetComponentLocation();
	}
	if (StaffMesh && StaffMesh->IsRegistered())
	{
		return StaffMesh->GetComponentLocation() + StaffMesh->GetComponentQuat().RotateVector(FVector(0.f, 0.f, StaffCrystalLocalZ));
	}
	return GetActorLocation() + GetActorForwardVector() * 55.f + FVector(0.f, 0.f, 55.f);
}

void AWizardCharacter::AttachWizardGear()
{
	USkeletalMeshComponent* CharMesh = GetMesh();
	const USkeletalMesh* SkelMesh = CharMesh ? CharMesh->GetSkeletalMeshAsset() : nullptr;

	// Self-calibrate OBJ Y-axis handedness from SM_WizardCloak's asymmetric trailing cape bounds:
	// Manny's back in ComponentSpace is -Y. If the cape extends further in +Y, rotate 180 deg yaw.
	bool bFlipY = false;
	if (CloakMesh && CloakMesh->GetStaticMesh())
	{
		const FBox CloakBox = CloakMesh->GetStaticMesh()->GetBoundingBox();
		bFlipY = (FMath::Abs(CloakBox.Max.Y) > FMath::Abs(CloakBox.Min.Y));
	}
	const FQuat BaseAlignCS = bFlipY ? FQuat(FRotator(0.f, 180.f, 0.f)) : FQuat::Identity;

	// Attach battle-mage hooded mantle & flared cloak to spine_03
	if (CloakMesh && CharMesh)
	{
		static const FName SpineSocket(TEXT("spine_03"));
		bool bFoundBone = false;
		const FTransform SpineCS = GetRefBoneComponentTransform(SkelMesh, SpineSocket, bFoundBone);
		if (bFoundBone && CharMesh->DoesSocketExist(SpineSocket))
		{
			// Anchor collar smoothly over Manny's upper torso/shoulders with hem sweeping cleanly above boots
			const FTransform DesiredCS(BaseAlignCS, FVector(0.f, -2.f, SpineCS.GetLocation().Z + 4.f), FVector::OneVector);
			const FTransform LocalT = DesiredCS.GetRelativeTransform(SpineCS);
			CloakMesh->AttachToComponent(CharMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SpineSocket);
			CloakMesh->SetRelativeLocationAndRotation(LocalT.GetLocation(), LocalT.GetRotation());
		}
		else
		{
			CloakMesh->AttachToComponent(CharMesh, FAttachmentTransformRules::KeepRelativeTransform);
			CloakMesh->SetRelativeLocationAndRotation(FVector(0.f, -2.f, 128.f), BaseAlignCS);
		}
	}

	// Attach wide-brimmed pointed wizard hat to head socket
	if (HatMesh && CharMesh)
	{
		static const FName HeadSocket(TEXT("head"));
		bool bFoundBone = false;
		const FTransform HeadCS = GetRefBoneComponentTransform(SkelMesh, HeadSocket, bFoundBone);
		// Slight regal backward tilt (-5 deg around X in Manny ComponentSpace where +Y is forward)
		const FQuat HatTiltCS = FQuat(FVector(1.f, 0.f, 0.f), FMath::DegreesToRadians(-5.f)) * BaseAlignCS;
		if (bFoundBone && CharMesh->DoesSocketExist(HeadSocket))
		{
			const FTransform DesiredCS(HatTiltCS, HeadCS.GetLocation() + FVector(0.f, -1.5f, 11.5f), FVector::OneVector);
			const FTransform LocalT = DesiredCS.GetRelativeTransform(HeadCS);
			HatMesh->AttachToComponent(CharMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HeadSocket);
			HatMesh->SetRelativeLocationAndRotation(LocalT.GetLocation(), LocalT.GetRotation());
		}
		else
		{
			HatMesh->AttachToComponent(CharMesh, FAttachmentTransformRules::KeepRelativeTransform);
			HatMesh->SetRelativeLocationAndRotation(FVector(0.f, -1.5f, 172.f), HatTiltCS);
		}
	}

	AttachStaffToHand();
}

void AWizardCharacter::AttachStaffToHand()
{
	USkeletalMeshComponent* CharMesh = GetMesh();
	if (!StaffMesh || !CharMesh)
	{
		return;
	}

	static const FName HandSocket(TEXT("hand_r"));
	if (CharMesh->DoesSocketExist(HandSocket))
	{
		StaffMesh->AttachToComponent(CharMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HandSocket);
		StaffMesh->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
		StaffMesh->SetRelativeRotation(FRotator(-68.f, 0.f, 0.f));
	}
	else
	{
		StaffMesh->AttachToComponent(CharMesh, FAttachmentTransformRules::KeepRelativeTransform);
		StaffMesh->SetRelativeLocation(FVector(24.f, 28.f, 95.f));
		StaffMesh->SetRelativeRotation(FRotator::ZeroRotator);
	}
}

void AWizardCharacter::ApplyWizardRobeTint()
{
	USkeletalMeshComponent* CharMesh = GetMesh();
	if (!CharMesh)
	{
		return;
	}

	// Keep the underlying mannequin in deep shadowed obsidian cloth so only the sculpted cloak/hat gold gleams
	const FLinearColor ShadowCloth(0.015f, 0.018f, 0.042f, 1.f);
	UMaterialInterface* ObsidianMat = TryLoadAssetIfExists<UMaterialInterface>(
		TEXT("/Game/Wizard/Environment/Materials/M_WizardRobe_Obsidian.M_WizardRobe_Obsidian"));

	const int32 NumMaterials = CharMesh->GetNumMaterials();
	for (int32 SlotIdx = 0; SlotIdx < NumMaterials; ++SlotIdx)
	{
		if (ObsidianMat)
		{
			CharMesh->SetMaterial(SlotIdx, ObsidianMat);
		}
		UMaterialInterface* BaseMat = CharMesh->GetMaterial(SlotIdx);
		if (!BaseMat)
		{
			continue;
		}
		if (UMaterialInstanceDynamic* MID = CharMesh->CreateDynamicMaterialInstance(SlotIdx, BaseMat))
		{
			MID->SetVectorParameterValue(TEXT("Tint"), ShadowCloth);
			MID->SetVectorParameterValue(TEXT("BaseColor"), ShadowCloth);
			MID->SetVectorParameterValue(TEXT("ClothColor"), ShadowCloth);
			MID->SetVectorParameterValue(TEXT("BaseColorTint"), ShadowCloth);
		}
	}
}

void AWizardCharacter::UpdateStaffVisuals()
{
	const float BurstFactor = FMath::Clamp(CastBurstTimer / 0.85f, 0.f, 1.f);
	const float Energy = FMath::Clamp(CurrentChargeT01 + BurstFactor * 0.75f, 0.f, 1.4f);

	if (StaffOrbMesh)
	{
		StaffOrbMesh->SetRelativeScale3D(FVector(0.08f * FMath::Lerp(1.f, 2.2f, Energy)));
	}

	if (StaffOrbMID)
	{
		StaffOrbMID->SetVectorParameterValue(TEXT("Tint"), ActiveOrbColor);
		StaffOrbMID->SetScalarParameterValue(TEXT("EmissiveScale"), FMath::Lerp(5.5f, 14.f, Energy));
		StaffOrbMID->SetScalarParameterValue(TEXT("Fade"), 1.f);
	}

	if (StaffCrystalMID)
	{
		StaffCrystalMID->SetVectorParameterValue(TEXT("ElementColor"), ActiveOrbColor);
		StaffCrystalMID->SetScalarParameterValue(TEXT("EmissiveScale"), FMath::Lerp(8.5f, 22.f, Energy));
	}

	if (StaffRuneMesh)
	{
		// Scale smoothly around staff crystal (expands during charge and cast bursts!)
		const float RuneScale = FMath::Lerp(0.36f, 0.62f, FMath::Clamp(Energy, 0.f, 1.f));
		StaffRuneMesh->SetRelativeScale3D(FVector(RuneScale));
	}

	if (StaffRuneMID)
	{
		StaffRuneMID->SetVectorParameterValue(TEXT("Color"), ActiveOrbColor);
		StaffRuneMID->SetScalarParameterValue(TEXT("EmissiveScale"), FMath::Lerp(10.f, 24.f, Energy));
		StaffRuneMID->SetScalarParameterValue(TEXT("Fade"), 1.f);
	}

	if (StaffLight)
	{
		StaffLight->SetLightColor(ActiveOrbColor);
		StaffLight->SetIntensity(FMath::Lerp(48.f, 145.f, FMath::Clamp(Energy, 0.f, 1.f)));
	}
}

void AWizardCharacter::HandleSpellSelected(int32 SlotIndex, const USpellDataAsset* Spell)
{
	if (Spell)
	{
		ActiveOrbColor = GetElementCrystalColor(Spell->Element);
	}
	else
	{
		ActiveOrbColor = GetElementCrystalColor(static_cast<ESpellElement>(FMath::Clamp(SlotIndex, 0, 3)));
	}
	UpdateStaffVisuals();
}

void AWizardCharacter::HandleChargeChanged(float ChargeT01)
{
	CurrentChargeT01 = FMath::Clamp(ChargeT01, 0.f, 1.f);
	UpdateStaffVisuals();
}

void AWizardCharacter::HandleSpellCast(int32 SlotIndex, float ChargeT01)
{
	CastBurstTimer = 0.85f;
	UpdateStaffVisuals();
}
