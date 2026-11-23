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
}

AWizardCharacter::AWizardCharacter()
{
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

	StaffMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaffMesh"));
	StaffMesh->SetupAttachment(GetMesh());
	StaffMesh->SetRelativeLocation(FVector(24.f, 28.f, 95.f));
	StaffMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaffMesh->SetCastShadow(true);

	StaffOrbMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaffOrbMesh"));
	StaffOrbMesh->SetupAttachment(StaffMesh);
	StaffOrbMesh->SetRelativeLocation(FVector(0.f, 0.f, 155.f));
	StaffOrbMesh->SetRelativeScale3D(FVector(0.18f));
	StaffOrbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaffOrbMesh->SetCastShadow(false);

	StaffLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StaffLight"));
	StaffLight->SetupAttachment(StaffOrbMesh);
	StaffLight->SetIntensityUnits(ELightUnits::Lumens);
	StaffLight->SetIntensity(18.f);
	StaffLight->SetAttenuationRadius(260.f);
	StaffLight->SetCastShadows(false);
	StaffLight->SetLightColor(ActiveOrbColor);
}

void AWizardCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AttachStaffToHand();
}

void AWizardCharacter::BeginPlay()
{
	if (AbilityComponent)
	{
		AbilityComponent->OnSpellSelected.AddUniqueDynamic(this, &AWizardCharacter::HandleSpellSelected);
		AbilityComponent->OnChargeChanged.AddUniqueDynamic(this, &AWizardCharacter::HandleChargeChanged);
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

	if (StaffMesh)
	{
		if (UStaticMesh* StaffStatic = TryLoadAssetIfExists<UStaticMesh>(TEXT("/Game/Wizard/Environment/Meshes/SM_WizardStaff.SM_WizardStaff")))
		{
			StaffMesh->SetStaticMesh(StaffStatic);
		}
		AttachStaffToHand();
	}

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
		if (!OrbMat)
		{
			OrbMat = TryLoadAssetIfExists<UMaterialInterface>(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		}
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

	if (AbilityComponent)
	{
		if (const USpellDataAsset* CurrentSpell = AbilityComponent->GetSelectedSpell())
		{
			ActiveOrbColor = CurrentSpell->Color;
		}
		CurrentChargeT01 = AbilityComponent->GetChargeFraction();
	}

	UpdateStaffVisuals();
}

FVector AWizardCharacter::GetStaffOrbLocation() const
{
	if (StaffOrbMesh && StaffOrbMesh->IsRegistered())
	{
		return StaffOrbMesh->GetComponentLocation();
	}
	if (StaffMesh && StaffMesh->IsRegistered())
	{
		return StaffMesh->GetComponentLocation() + StaffMesh->GetComponentQuat().RotateVector(FVector(0.f, 0.f, 155.f));
	}
	return GetActorLocation() + GetActorForwardVector() * 55.f + FVector(0.f, 0.f, 55.f);
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
		StaffMesh->SetRelativeLocation(FVector(0.f, 0.f, -35.f));
		StaffMesh->SetRelativeRotation(FRotator(-65.f, 0.f, 0.f));
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

	const FLinearColor ObsidianIndigo(0.07f, 0.06f, 0.16f, 1.f);
	const FLinearColor ArcaneGold(0.72f, 0.56f, 0.22f, 1.f);

	const int32 NumMaterials = CharMesh->GetNumMaterials();
	for (int32 SlotIdx = 0; SlotIdx < NumMaterials; ++SlotIdx)
	{
		UMaterialInterface* BaseMat = CharMesh->GetMaterial(SlotIdx);
		if (!BaseMat)
		{
			continue;
		}
		if (UMaterialInstanceDynamic* MID = CharMesh->CreateDynamicMaterialInstance(SlotIdx, BaseMat))
		{
			const bool bAccentSlot = (SlotIdx % 2) == 1;
			const FLinearColor SlotColor = bAccentSlot ? ArcaneGold : ObsidianIndigo;
			MID->SetVectorParameterValue(TEXT("Tint"), SlotColor);
			MID->SetVectorParameterValue(TEXT("BaseColor"), SlotColor);
			MID->SetVectorParameterValue(TEXT("ClothColor"), ObsidianIndigo);
			MID->SetVectorParameterValue(TEXT("BaseColorTint"), SlotColor);
		}
	}
}

void AWizardCharacter::UpdateStaffVisuals()
{
	const float ScaleMul = FMath::Lerp(1.f, 2.4f, CurrentChargeT01);
	if (StaffOrbMesh)
	{
		StaffOrbMesh->SetRelativeScale3D(FVector(0.18f * ScaleMul));
	}

	const float Emissive = FMath::Lerp(3.5f, 10.f, CurrentChargeT01);
	if (StaffOrbMID)
	{
		StaffOrbMID->SetVectorParameterValue(TEXT("Tint"), ActiveOrbColor);
		StaffOrbMID->SetScalarParameterValue(TEXT("EmissiveScale"), Emissive);
		StaffOrbMID->SetScalarParameterValue(TEXT("Fade"), 1.f);
	}

	if (StaffLight)
	{
		StaffLight->SetLightColor(ActiveOrbColor);
		StaffLight->SetIntensity(FMath::Lerp(18.f, 65.f, CurrentChargeT01));
	}
}

void AWizardCharacter::HandleSpellSelected(int32 SlotIndex, const USpellDataAsset* Spell)
{
	if (Spell)
	{
		ActiveOrbColor = Spell->Color;
	}
	UpdateStaffVisuals();
}

void AWizardCharacter::HandleChargeChanged(float ChargeT01)
{
	CurrentChargeT01 = FMath::Clamp(ChargeT01, 0.f, 1.f);
	UpdateStaffVisuals();
}
