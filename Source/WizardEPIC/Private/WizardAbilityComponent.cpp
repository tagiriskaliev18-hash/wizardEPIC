#include "WizardAbilityComponent.h"

#include "CameraFX.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "FXSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HUDWidgets.h"
#include "SpellCasting.h"
#include "SpellDataAsset.h"
#include "UObject/SoftObjectPath.h"
#include "WizardAudioSubsystem.h"
#include "WizardGameMode.h"
#include "WizardHUD.h"
#include "WizardPlayerController.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardAbilityComponent)

namespace
{
	USpellDataAsset* CreateFallbackSpell(UObject* Outer, int32 SlotIndex)
	{
		USpellDataAsset* Spell = NewObject<USpellDataAsset>(Outer);
		if (!Spell)
		{
			return nullptr;
		}

		switch (SlotIndex)
		{
		case 0: // Fire
			Spell->Element = ESpellElement::Fire;
			Spell->DisplayName = NSLOCTEXT("Wizard", "SpellFire", "Пепельная сфера");
			Spell->Delivery = ESpellDelivery::Projectile;
			Spell->Cooldown = 1.0f;
			Spell->Damage = 135.f;
			Spell->DamageRadius = 640.f;
			Spell->LaunchSpeed = 1550.f;
			Spell->UpBias = 0.35f;
			Spell->Color = FLinearColor(1.f, 0.48f, 0.10f);
			Spell->CastSound = TEXT("Cast_Fire");
			Spell->ImpactSound = TEXT("Explosion_Fire");
			Spell->TravelSound = TEXT("Whoosh");
			break;
		case 1: // Earth
			Spell->Element = ESpellElement::Earth;
			Spell->DisplayName = NSLOCTEXT("Wizard", "SpellEarth", "Земляной клин");
			Spell->Delivery = ESpellDelivery::GroundLine;
			Spell->Cooldown = 1.6f;
			Spell->Damage = 150.f;
			Spell->DamageRadius = 420.f;
			Spell->LaunchSpeed = 1650.f;
			Spell->UpBias = 0.85f;
			Spell->Color = FLinearColor(0.79f, 0.54f, 0.24f);
			Spell->CastSound = TEXT("Cast_Earth");
			Spell->ImpactSound = TEXT("Erupt_Earth");
			break;
		case 2: // Wind
			Spell->Element = ESpellElement::Wind;
			Spell->DisplayName = NSLOCTEXT("Wizard", "SpellWind", "Эхо бури");
			Spell->Delivery = ESpellDelivery::Cone;
			Spell->Cooldown = 1.1f;
			Spell->Damage = 105.f;
			Spell->ConeRange = 1750.f;
			Spell->ConeHalfAngle = 24.f;
			Spell->LaunchSpeed = 1900.f;
			Spell->UpBias = 0.28f;
			Spell->Color = FLinearColor(0.75f, 0.91f, 0.89f);
			Spell->CastSound = TEXT("Cast_Wind");
			Spell->ImpactSound = TEXT("Gust_Wind");
			break;
		case 3: // Lightning
		default:
			Spell->Element = ESpellElement::Lightning;
			Spell->DisplayName = NSLOCTEXT("Wizard", "SpellLightning", "Грозовой жезл");
			Spell->Delivery = ESpellDelivery::Strike;
			Spell->Cooldown = 1.8f;
			Spell->Damage = 160.f;
			Spell->DamageRadius = 520.f;
			Spell->LaunchSpeed = 1450.f;
			Spell->StrikeCount = 3;
			Spell->StrikeInterval = 0.09f;
			Spell->StrikeShatterBonus = 3.f;
			Spell->Color = FLinearColor(0.80f, 0.92f, 1.f);
			Spell->CastSound = TEXT("Cast_Lightning");
			Spell->ImpactSound = TEXT("Thunder");
			break;
		}
		return Spell;
	}
}

UWizardAbilityComponent::UWizardAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UWizardAbilityComponent::BeginPlay()
{
	Super::BeginPlay();
	SpellCache.SetNum(4);

	for (int32 i = 0; i < 4; ++i)
	{
		GetSpell(i);
	}

	if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
		{
			Move->MaxWalkSpeed = WalkSpeed;
		}
	}

	SelectSpell(0);
}

void UWizardAbilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPendingCast();
	Super::EndPlay(EndPlayReason);
}

void UWizardAbilityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bCharging)
	{
		USpellDataAsset* Spell = GetSelectedSpell();
		const float ChargeDuration = Spell ? FMath::Max(0.05f, Spell->ChargeTime) : 1.f;
		const float MinHold = Spell ? Spell->MinChargeTime : 0.22f;

		ChargeHoldTime += DeltaTime;
		const float Effective = FMath::Max(0.f, ChargeHoldTime - MinHold);
		const float NewT = FMath::Clamp(Effective / ChargeDuration, 0.f, 1.f);
		if (!FMath::IsNearlyEqual(NewT, ChargeT01, 0.005f))
		{
			ChargeT01 = NewT;
			OnChargeChanged.Broadcast(ChargeT01);
		}
	}

	if (bCastPending)
	{
		CastDelayTimer -= DeltaTime;
		if (CastDelayTimer <= 0.f)
		{
			ApplyPendingCast();
		}
	}

	if (DashTimeLeft > 0.f)
	{
		DashTimeLeft = FMath::Max(0.f, DashTimeLeft - DeltaTime);
		if (DashTimeLeft <= 0.f)
		{
			if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
			{
				if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
				{
					Move->Velocity *= 0.45f;
				}
			}
		}
	}
}

USpellDataAsset* UWizardAbilityComponent::GetSelectedSpell() const
{
	return GetSpell(SelectedIndex);
}

USpellDataAsset* UWizardAbilityComponent::GetSpell(int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= 4)
	{
		return nullptr;
	}

	UWizardAbilityComponent* MutableThis = const_cast<UWizardAbilityComponent*>(this);
	if (MutableThis->SpellCache.Num() < 4)
	{
		MutableThis->SpellCache.SetNum(4);
	}

	if (!MutableThis->SpellCache[SlotIndex])
	{
		USpellDataAsset* Loaded = Cast<USpellDataAsset>(FSoftObjectPath(WizardPaths::Spells[SlotIndex]).TryLoad());
		if (!Loaded)
		{
			Loaded = CreateFallbackSpell(MutableThis, SlotIndex);
		}
		MutableThis->SpellCache[SlotIndex] = Loaded;
	}

	return MutableThis->SpellCache[SlotIndex];
}

bool UWizardAbilityComponent::SelectSpell(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= 4)
	{
		return false;
	}
	if (bCharging)
	{
		bCharging = false;
		ChargeHoldTime = 0.f;
		ChargeT01 = 0.f;
		OnChargeChanged.Broadcast(0.f);
	}
	SelectedIndex = SlotIndex;
	OnSpellSelected.Broadcast(SelectedIndex, GetSelectedSpell());
	return true;
}

bool UWizardAbilityComponent::CycleSpell(int32 Delta)
{
	const int32 Next = ((SelectedIndex + Delta) % 4 + 4) % 4;
	return SelectSpell(Next);
}

bool UWizardAbilityComponent::GetCooldown(int32 SlotIndex, float& OutFractionRemaining, float& OutSecondsRemaining) const
{
	OutFractionRemaining = 0.f;
	OutSecondsRemaining = 0.f;

	if (SlotIndex < 0 || SlotIndex >= 4)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float Rem = CooldownEnd[SlotIndex] - World->GetTimeSeconds();
	if (Rem <= 0.f)
	{
		return false;
	}

	const USpellDataAsset* Spell = GetSpell(SlotIndex);
	const float Total = Spell ? FMath::Max(0.01f, Spell->Cooldown) : 1.f;
	OutSecondsRemaining = Rem;
	OutFractionRemaining = FMath::Clamp(Rem / Total, 0.f, 1.f);
	return true;
}

bool UWizardAbilityComponent::StartCharge()
{
	float Frac = 0.f, Sec = 0.f;
	if (GetCooldown(SelectedIndex, Frac, Sec) || bCastPending)
	{
		return false;
	}

	bCharging = true;
	ChargeHoldTime = 0.f;
	ChargeT01 = 0.f;
	OnChargeChanged.Broadcast(0.f);

	if (USpellDataAsset* Spell = GetSelectedSpell())
	{
		if (UWizardAudioSubsystem* Audio = UWizardAudioSubsystem::Get(this))
		{
			if (!Spell->ChargeSound.IsNone())
			{
				Audio->Play2D(Spell->ChargeSound, 0.45f, 1.f);
			}
		}
	}
	return true;
}

bool UWizardAbilityComponent::ReleaseCast(float ForceChargeT01)
{
	if (!bCharging && ForceChargeT01 < 0.f)
	{
		return false;
	}

	const float FinalCharge = (ForceChargeT01 >= 0.f) ? FMath::Clamp(ForceChargeT01, 0.f, 1.f) : ChargeT01;
	bCharging = false;
	ChargeHoldTime = 0.f;
	ChargeT01 = 0.f;
	OnChargeChanged.Broadcast(0.f);

	float Frac = 0.f, Sec = 0.f;
	if (GetCooldown(SelectedIndex, Frac, Sec))
	{
		return false;
	}

	USpellDataAsset* Spell = GetSelectedSpell();
	if (!Spell)
	{
		return false;
	}

	PendingCastSlot = SelectedIndex;
	PendingChargeT01 = FinalCharge;
	if (Spell->CastDelay > KINDA_SMALL_NUMBER)
	{
		bCastPending = true;
		CastDelayTimer = Spell->CastDelay;
		UWizardSpellLibrary::PlayCastAnim(this, FinalCharge >= 0.45f);
	}
	else
	{
		ApplyPendingCast();
	}
	return true;
}

void UWizardAbilityComponent::CastAt(const FVector& AimPoint, float ForceChargeT01)
{
	USpellDataAsset* Spell = GetSelectedSpell();
	if (!Spell)
	{
		return;
	}

	const FVector HandLoc = GetHandLocation();
	const FVector Dir = (AimPoint - HandLoc).GetSafeNormal();
	UWizardSpellLibrary::CastSpell(this, Spell, AimPoint, Dir, FMath::Clamp(ForceChargeT01, 0.f, 1.f));
	OnSpellCast.Broadcast(SelectedIndex, FMath::Clamp(ForceChargeT01, 0.f, 1.f));
}

bool UWizardAbilityComponent::GetAim(FVector& OutPoint, FVector& OutDirection) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (const AWizardPlayerController* PC = Pawn ? Cast<AWizardPlayerController>(Pawn->GetController()) : nullptr)
	{
		return PC->GetAim(OutPoint, OutDirection);
	}

	const FVector Start = GetHandLocation();
	const FVector Forward = GetOwner() ? GetOwner()->GetActorForwardVector() : FVector::ForwardVector;
	OutDirection = Forward;
	OutPoint = Start + Forward * 4000.f;
	return false;
}

FVector UWizardAbilityComponent::GetHandLocation() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}

	if (const USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>())
	{
		if (Mesh->DoesSocketExist(TEXT("hand_r")))
		{
			return Mesh->GetSocketLocation(TEXT("hand_r"));
		}
	}
	return Owner->GetActorLocation() + Owner->GetActorForwardVector() * 55.f + FVector(0.f, 0.f, 55.f);
}

bool UWizardAbilityComponent::Dash()
{
	UWorld* World = GetWorld();
	ACharacter* Char = Cast<ACharacter>(GetOwner());
	if (!World || !Char)
	{
		return false;
	}

	const float Now = World->GetTimeSeconds();
	if (Now < DashCooldownEnd)
	{
		return false;
	}

	FVector Dir = Char->GetLastMovementInputVector();
	if (Dir.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		if (AController* Ctrl = Char->GetController())
		{
			Dir = FRotator(0.f, Ctrl->GetControlRotation().Yaw, 0.f).Vector();
		}
		else
		{
			Dir = Char->GetActorForwardVector();
		}
	}
	Dir.Z = 0.f;
	Dir.Normalize();

	DashDirection = Dir;
	DashTimeLeft = DashDuration;
	DashCooldownEnd = Now + DashCooldown;

	Char->LaunchCharacter(Dir * DashSpeed + FVector(0.f, 0.f, 180.f), true, false);
	UWizardSpellLibrary::PlayDashAnim(this);
	UWizardCameraLibrary::Shake(this, UWizardShakeDash::StaticClass(), 1.f);
	UWizardCameraLibrary::FovKick(this, 6.f, 0.06f, 0.28f);

	if (UWizardAudioSubsystem* Audio = UWizardAudioSubsystem::Get(this))
	{
		Audio->PlayAt(TEXT("Dash"), Char->GetActorLocation(), 0.9f, 1.f);
	}
	if (UWizardFXSubsystem* FX = UWizardFXSubsystem::Get(this))
	{
		FFXBurstSpec Burst;
		Burst.Kind = EFXSpriteKind::Glow;
		Burst.Mode = EFXSpriteMode::VelocityAligned;
		Burst.Count = 16;
		Burst.SpeedMin = 250.f;
		Burst.SpeedMax = 700.f;
		Burst.ConeHalfAngle = 45.f;
		Burst.LifeMin = 0.2f;
		Burst.LifeMax = 0.42f;
		Burst.SizeMin = 18.f;
		Burst.SizeMax = 38.f;
		Burst.ColorA = FLinearColor(0.72f, 0.92f, 1.f) * 2.5f;
		Burst.ColorB = FLinearColor(0.25f, 0.65f, 1.f);
		Burst.Stretch = 3.f;
		FX->Burst(Burst, FTransform((-Dir).Rotation(), Char->GetActorLocation()), 1.f);
	}

	return true;
}

void UWizardAbilityComponent::SetSprinting(bool bInSprinting)
{
	bSprinting = bInSprinting;
	if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
		{
			Move->MaxWalkSpeed = bSprinting ? SprintSpeed : WalkSpeed;
		}
	}
}

float UWizardAbilityComponent::GetDashCooldownFraction() const
{
	const UWorld* World = GetWorld();
	if (!World || DashCooldown <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	const float Rem = DashCooldownEnd - World->GetTimeSeconds();
	return FMath::Clamp(Rem / DashCooldown, 0.f, 1.f);
}

void UWizardAbilityComponent::RequestArenaReset()
{
	if (AWizardGameMode* GM = AWizardGameMode::Get(this))
	{
		GM->ResetArena();
	}
}

void UWizardAbilityComponent::SetHintsVisible(bool bVisible)
{
	bHintsVisible = bVisible;
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			if (AWizardHUD* HUD = Cast<AWizardHUD>(PC->GetHUD()))
			{
				if (UWizardHUDWidget* Root = HUD->RootWidget)
				{
					Root->SetHintsVisible(bVisible);
				}
			}
		}
	}
}

void UWizardAbilityComponent::ToggleMouseCapture()
{
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (AWizardPlayerController* PC = Cast<AWizardPlayerController>(Pawn->GetController()))
		{
			PC->ToggleMouseCapture();
		}
	}
}

void UWizardAbilityComponent::LookAt(const FVector& WorldPoint, float BlendTime)
{
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (AController* Ctrl = Pawn->GetController())
		{
			const FVector Eye = Pawn->GetActorLocation() + FVector(0.f, 0.f, 70.f);
			const FRotator Rot = (WorldPoint - Eye).Rotation();
			Ctrl->SetControlRotation(Rot);
		}
	}
}

void UWizardAbilityComponent::SetViewRotation(const FRotator& Rotation, float BlendTime)
{
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (AController* Ctrl = Pawn->GetController())
		{
			Ctrl->SetControlRotation(Rotation);
		}
	}
}

void UWizardAbilityComponent::ApplyPendingCast()
{
	bCastPending = false;
	CastDelayTimer = 0.f;

	UWorld* World = GetWorld();
	USpellDataAsset* Spell = GetSpell(PendingCastSlot);
	if (!World || !Spell)
	{
		return;
	}

	CooldownEnd[PendingCastSlot] = World->GetTimeSeconds() + Spell->Cooldown;

	FVector AimPoint, AimDir;
	GetAim(AimPoint, AimDir);

	// Turn character gently toward aim yaw
	if (AActor* Owner = GetOwner())
	{
		FRotator ActorRot = Owner->GetActorRotation();
		ActorRot.Yaw = AimDir.Rotation().Yaw;
		Owner->SetActorRotation(ActorRot);
	}

	UWizardSpellLibrary::CastSpell(this, Spell, AimPoint, AimDir, PendingChargeT01);
	OnSpellCast.Broadcast(PendingCastSlot, PendingChargeT01);
}

void UWizardAbilityComponent::CancelPendingCast()
{
	bCastPending = false;
	bCharging = false;
	CastDelayTimer = 0.f;
	ChargeHoldTime = 0.f;
	ChargeT01 = 0.f;
}
