// WizardCharacter.h - third-person player wizard character: over-shoulder camera,
// movement setup, ability component, held arcane staff + reactive elemental orb & point light.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "WizardCharacter.generated.h"

class UCameraComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USpellDataAsset;
class USpringArmComponent;
class UStaticMeshComponent;
class UWizardAbilityComponent;

UCLASS()
class WIZARDEPIC_API AWizardCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AWizardCharacter();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** World location of the staff tip crystal for spell casting origin. */
	UFUNCTION(BlueprintPure, Category = Wizard)
	FVector GetStaffOrbLocation() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	TObjectPtr<USpringArmComponent> CameraBoom = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	TObjectPtr<UCameraComponent> FollowCamera = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Wizard)
	TObjectPtr<UWizardAbilityComponent> AbilityComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Wizard)
	TObjectPtr<UStaticMeshComponent> HatMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Wizard)
	TObjectPtr<UStaticMeshComponent> CloakMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Wizard)
	TObjectPtr<UStaticMeshComponent> StaffMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Wizard)
	TObjectPtr<UStaticMeshComponent> StaffOrbMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Wizard)
	TObjectPtr<UStaticMeshComponent> StaffRuneMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Wizard)
	TObjectPtr<UPointLightComponent> StaffLight = nullptr;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> StaffOrbMID = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> StaffCrystalMID = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> StaffRuneMID = nullptr;

	FLinearColor ActiveOrbColor = FLinearColor(1.f, 0.36f, 0.05f, 1.f);
	float CurrentChargeT01 = 0.f;
	float CastBurstTimer = 0.f;
	float RuneSpinAngle = 0.f;

	void AttachWizardGear();
	void AttachStaffToHand();
	void ApplyWizardRobeTint();
	void UpdateStaffVisuals();

	UFUNCTION()
	void HandleSpellSelected(int32 SlotIndex, const USpellDataAsset* Spell);

	UFUNCTION()
	void HandleChargeChanged(float ChargeT01);

	UFUNCTION()
	void HandleSpellCast(int32 SlotIndex, float ChargeT01);
};
