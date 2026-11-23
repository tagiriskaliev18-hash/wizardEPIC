// WizardAbilityComponent.h - the player's spellcasting and traversal kit.
//
// Attached by the game mode to the possessed pawn at runtime, so the animated template
// character stays a pure Blueprint and all gameplay lives in C++. Input is bound by the player
// controller, which forwards to this component.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WizardTypes.h"
#include "WizardAbilityComponent.generated.h"

class UAudioComponent;
class USpellDataAsset;
class UWizardCameraShake;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSpellSelected, int32, SlotIndex, const USpellDataAsset*, Spell);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSpellCast, int32, SlotIndex, float, ChargeT01);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChargeChanged, float, ChargeT01);

UCLASS(ClassGroup = (Wizard), meta = (BlueprintSpawnableComponent))
class WIZARDEPIC_API UWizardAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWizardAbilityComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ----- Spell selection ------------------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = Wizard)
	int32 GetSelectedIndex() const { return SelectedIndex; }

	UFUNCTION(BlueprintPure, Category = Wizard)
	USpellDataAsset* GetSelectedSpell() const;

	/** Spell of a slot (loads the data asset on first use). Null when the asset is missing. */
	UFUNCTION(BlueprintPure, Category = Wizard)
	USpellDataAsset* GetSpell(int32 SlotIndex) const;

	UFUNCTION(BlueprintCallable, Category = Wizard)
	bool SelectSpell(int32 SlotIndex);

	/** Cycles the selection by Delta slots (wheel). */
	UFUNCTION(BlueprintCallable, Category = Wizard)
	bool CycleSpell(int32 Delta);

	UFUNCTION(BlueprintPure, Category = Wizard)
	bool GetCooldown(int32 SlotIndex, float& OutFractionRemaining, float& OutSecondsRemaining) const;

	UFUNCTION(BlueprintPure, Category = Wizard)
	float GetChargeFraction() const { return ChargeT01; }

	UFUNCTION(BlueprintPure, Category = Wizard)
	bool IsCharging() const { return bCharging; }

	// ----- Casting --------------------------------------------------------------------------
	/** Starts charging the selected spell (mouse button down). */
	UFUNCTION(BlueprintCallable, Category = Wizard)
	bool StartCharge();

	/** Releases the cast (mouse button up). ForceChargeT01 >= 0 overrides the held time. */
	UFUNCTION(BlueprintCallable, Category = Wizard)
	bool ReleaseCast(float ForceChargeT01 = -1.f);

	/** Casts right now at a world point, bypassing input and cooldown checks (scripts, smoke test). */
	UFUNCTION(BlueprintCallable, Category = Wizard)
	void CastAt(const FVector& AimPoint, float ForceChargeT01 = 1.f);

	// ----- Aim ------------------------------------------------------------------------------
	/** Screen-centre aim: first blocking hit along the camera ray, else a far point. */
	UFUNCTION(BlueprintCallable, Category = Wizard)
	bool GetAim(FVector& OutPoint, FVector& OutDirection) const;

	/** World location of the hand socket (falls back to the actor location). */
	FVector GetHandLocation() const;

	// ----- Traversal ------------------------------------------------------------------------
	/** Forward burst with a cooldown. Also used by the smoke test. */
	UFUNCTION(BlueprintCallable, Category = Wizard)
	bool Dash();

	UFUNCTION(BlueprintCallable, Category = Wizard)
	void SetSprinting(bool bSprinting);

	UFUNCTION(BlueprintPure, Category = Wizard)
	bool IsSprinting() const { return bSprinting; }

	UFUNCTION(BlueprintPure, Category = Wizard)
	float GetDashCooldownFraction() const;

	// ----- Misc -----------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = Wizard)
	void RequestArenaReset();

	UFUNCTION(BlueprintCallable, Category = Wizard)
	void SetHintsVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = Wizard)
	void ToggleMouseCapture();

	/** Sends the camera toward a world point over a short blend (scripts). */
	UFUNCTION(BlueprintCallable, Category = Wizard)
	void LookAt(const FVector& WorldPoint, float BlendTime = 0.f);

	/** Turns the camera to an absolute rotation (scripts). */
	UFUNCTION(BlueprintCallable, Category = Wizard)
	void SetViewRotation(const FRotator& Rotation, float BlendTime = 0.f);

	UPROPERTY(BlueprintAssignable, Category = Wizard)
	FOnSpellSelected OnSpellSelected;

	UPROPERTY(BlueprintAssignable, Category = Wizard)
	FOnSpellCast OnSpellCast;

	UPROPERTY(BlueprintAssignable, Category = Wizard)
	FOnChargeChanged OnChargeChanged;

	// ----- Tunables -------------------------------------------------------------------------
	float WalkSpeed = 420.f;
	float SprintSpeed = 760.f;

	/** Dash launch speed in cm/s and its duration. */
	float DashSpeed = 2100.f;
	float DashDuration = 0.22f;
	float DashCooldown = 1.3f;

	bool bHintsVisible = true;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<USpellDataAsset>> SpellCache;

	int32 SelectedIndex = 0;

	/** Cooldown end time per slot (world seconds). */
	float CooldownEnd[4] = { 0.f, 0.f, 0.f, 0.f };

	bool bCharging = false;
	bool bCastPending = false;
	float ChargeHoldTime = 0.f;
	float ChargeT01 = 0.f;
	float CastDelayTimer = 0.f;
	int32 PendingCastSlot = 0;
	float PendingChargeT01 = 0.f;

	bool bSprinting = false;
	float DashCooldownEnd = 0.f;
	float DashTimeLeft = 0.f;
	FVector DashDirection = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveChargeSound = nullptr;

	void ApplyPendingCast();
	void CancelPendingCast();
	void StopChargeSound(float FadeOut = 0.08f);
};
