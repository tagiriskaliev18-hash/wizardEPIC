#include "WizardAbilityComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardAbilityComponent)

// Skeleton build: input state, charging, dash and casting are implemented in the next step.

UWizardAbilityComponent::UWizardAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SpellCache.SetNum(4);
}

void UWizardAbilityComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UWizardAbilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UWizardAbilityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

USpellDataAsset* UWizardAbilityComponent::GetSelectedSpell() const
{
	return GetSpell(SelectedIndex);
}

USpellDataAsset* UWizardAbilityComponent::GetSpell(int32 SlotIndex) const
{
	return nullptr;
}

bool UWizardAbilityComponent::SelectSpell(int32 SlotIndex)
{
	return false;
}

bool UWizardAbilityComponent::CycleSpell(int32 Delta)
{
	return SelectSpell((SelectedIndex + Delta + 8) % 4);
}

bool UWizardAbilityComponent::GetCooldown(int32 SlotIndex, float& OutFractionRemaining, float& OutSecondsRemaining) const
{
	OutFractionRemaining = 0.f;
	OutSecondsRemaining = 0.f;
	return false;
}

bool UWizardAbilityComponent::StartCharge()
{
	return false;
}

bool UWizardAbilityComponent::ReleaseCast(float ForceChargeT01)
{
	return false;
}

void UWizardAbilityComponent::CastAt(const FVector& AimPoint, float ForceChargeT01)
{
}

bool UWizardAbilityComponent::GetAim(FVector& OutPoint, FVector& OutDirection) const
{
	return false;
}

FVector UWizardAbilityComponent::GetHandLocation() const
{
	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

bool UWizardAbilityComponent::Dash()
{
	return false;
}

void UWizardAbilityComponent::SetSprinting(bool bInSprinting)
{
	bSprinting = bInSprinting;
}

float UWizardAbilityComponent::GetDashCooldownFraction() const
{
	return 0.f;
}

void UWizardAbilityComponent::RequestArenaReset()
{
}

void UWizardAbilityComponent::SetHintsVisible(bool bVisible)
{
	bHintsVisible = bVisible;
}

void UWizardAbilityComponent::ToggleMouseCapture()
{
}

void UWizardAbilityComponent::LookAt(const FVector& WorldPoint, float BlendTime)
{
}

void UWizardAbilityComponent::SetViewRotation(const FRotator& Rotation, float BlendTime)
{
}

void UWizardAbilityComponent::ApplyPendingCast()
{
}

void UWizardAbilityComponent::CancelPendingCast()
{
}
