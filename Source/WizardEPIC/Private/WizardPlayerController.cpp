#include "WizardPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "CameraFX.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "WizardAbilityComponent.h"
#include "WizardAudioSubsystem.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WizardPlayerController)

AWizardPlayerController::AWizardPlayerController()
{
	bShowMouseCursor = false;
}

void AWizardPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (PlayerCameraManager)
	{
		PlayerCameraManager->AddNewCameraModifier(UWizardCameraModifier::StaticClass());
		PlayerCameraManager->ViewPitchMin = -55.f;
		PlayerCameraManager->ViewPitchMax = 45.f;
	}

	bMouseCaptured = true;
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());

	if (UWizardAudioSubsystem* Audio = UWizardAudioSubsystem::Get(this))
	{
		Audio->StartAmbient();
	}
}

void AWizardPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (APawn* P = GetPawn())
	{
		float Forward = 0.f;
		if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up))
		{
			Forward += 1.f;
		}
		if (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down))
		{
			Forward -= 1.f;
		}

		float Right = 0.f;
		if (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right))
		{
			Right += 1.f;
		}
		if (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left))
		{
			Right -= 1.f;
		}

		if (!FMath::IsNearlyZero(Forward) || !FMath::IsNearlyZero(Right))
		{
			const FRotator YawRot(0.f, GetControlRotation().Yaw, 0.f);
			const FRotationMatrix RotMatrix(YawRot);
			if (!FMath::IsNearlyZero(Forward))
			{
				P->AddMovementInput(RotMatrix.GetUnitAxis(EAxis::X), Forward);
			}
			if (!FMath::IsNearlyZero(Right))
			{
				P->AddMovementInput(RotMatrix.GetUnitAxis(EAxis::Y), Right);
			}
		}
	}
}

void AWizardPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	InputComponent->BindAxisKey(EKeys::MouseX, this, &AWizardPlayerController::OnLookYaw);
	InputComponent->BindAxisKey(EKeys::MouseY, this, &AWizardPlayerController::OnLookPitch);

	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AWizardPlayerController::OnJumpPressed);
	InputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &AWizardPlayerController::OnJumpReleased);

	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AWizardPlayerController::OnCastPressed);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AWizardPlayerController::OnCastReleased);

	// Bind explicit slot delegates using FInputKeyBinding
	{
		FInputKeyBinding Bind1(FInputChord(EKeys::One), IE_Pressed);
		Bind1.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &AWizardPlayerController::OnSelectSlot, 0);
		InputComponent->KeyBindings.Add(MoveTemp(Bind1));

		FInputKeyBinding Bind2(FInputChord(EKeys::Two), IE_Pressed);
		Bind2.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &AWizardPlayerController::OnSelectSlot, 1);
		InputComponent->KeyBindings.Add(MoveTemp(Bind2));

		FInputKeyBinding Bind3(FInputChord(EKeys::Three), IE_Pressed);
		Bind3.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &AWizardPlayerController::OnSelectSlot, 2);
		InputComponent->KeyBindings.Add(MoveTemp(Bind3));

		FInputKeyBinding Bind4(FInputChord(EKeys::Four), IE_Pressed);
		Bind4.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &AWizardPlayerController::OnSelectSlot, 3);
		InputComponent->KeyBindings.Add(MoveTemp(Bind4));

		FInputKeyBinding BindWheelUp(FInputChord(EKeys::MouseScrollUp), IE_Pressed);
		BindWheelUp.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &AWizardPlayerController::OnCycleSpell, -1);
		InputComponent->KeyBindings.Add(MoveTemp(BindWheelUp));

		FInputKeyBinding BindWheelDown(FInputChord(EKeys::MouseScrollDown), IE_Pressed);
		BindWheelDown.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &AWizardPlayerController::OnCycleSpell, 1);
		InputComponent->KeyBindings.Add(MoveTemp(BindWheelDown));
	}

	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AWizardPlayerController::OnDashPressed);
	InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AWizardPlayerController::OnDashPressed);

	InputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &AWizardPlayerController::OnSprintPressed);
	InputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &AWizardPlayerController::OnSprintReleased);

	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AWizardPlayerController::OnResetPressed);
	InputComponent->BindKey(EKeys::H, IE_Pressed, this, &AWizardPlayerController::OnHintsPressed);
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AWizardPlayerController::ToggleMouseCapture);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AWizardPlayerController::OnEscapePressed);
}

bool AWizardPlayerController::GetAim(FVector& OutPoint, FVector& OutDirection, float MaxRange) const
{
	FVector CamLoc;
	FRotator CamRot;
	GetView(CamLoc, CamRot);

	OutDirection = CamRot.Vector().GetSafeNormal();
	const FVector End = CamLoc + OutDirection * MaxRange;

	UWorld* World = GetWorld();
	if (!World)
	{
		OutPoint = End;
		return false;
	}

	FCollisionQueryParams QP(SCENE_QUERY_STAT(WizardAim), false);
	if (APawn* P = GetPawn())
	{
		QP.AddIgnoredActor(P);
	}

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, CamLoc, End, ECC_Visibility, QP))
	{
		OutPoint = Hit.ImpactPoint;
		return true;
	}

	OutPoint = End;
	return false;
}

void AWizardPlayerController::GetView(FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = PlayerCameraManager ? PlayerCameraManager->GetCameraLocation() : GetFocalLocation();
	OutRotation = PlayerCameraManager ? PlayerCameraManager->GetCameraRotation() : GetControlRotation();
}

void AWizardPlayerController::ToggleMouseCapture()
{
	bMouseCaptured = !bMouseCaptured;
	bShowMouseCursor = !bMouseCaptured;
	if (bMouseCaptured)
	{
		SetInputMode(FInputModeGameOnly());
	}
	else
	{
		SetInputMode(FInputModeGameAndUI());
	}
}

UWizardAbilityComponent* AWizardPlayerController::GetAbilities() const
{
	APawn* ControlledPawn = GetPawn();
	return ControlledPawn ? ControlledPawn->FindComponentByClass<UWizardAbilityComponent>() : nullptr;
}

void AWizardPlayerController::OnCastPressed()
{
	if (UWizardAbilityComponent* Ab = GetAbilities())
	{
		Ab->StartCharge();
	}
}

void AWizardPlayerController::OnCastReleased()
{
	if (UWizardAbilityComponent* Ab = GetAbilities())
	{
		Ab->ReleaseCast();
	}
}

void AWizardPlayerController::OnSelectSlot(int32 SlotIndex)
{
	if (UWizardAbilityComponent* Ab = GetAbilities())
	{
		Ab->SelectSpell(SlotIndex);
	}
}

void AWizardPlayerController::OnCycleSpell(int32 Delta)
{
	if (UWizardAbilityComponent* Ab = GetAbilities())
	{
		Ab->CycleSpell(Delta);
	}
}

void AWizardPlayerController::OnDashPressed()
{
	if (UWizardAbilityComponent* Ab = GetAbilities())
	{
		Ab->Dash();
	}
}

void AWizardPlayerController::OnSprintPressed()
{
	if (UWizardAbilityComponent* Ab = GetAbilities())
	{
		Ab->SetSprinting(true);
	}
}

void AWizardPlayerController::OnSprintReleased()
{
	if (UWizardAbilityComponent* Ab = GetAbilities())
	{
		Ab->SetSprinting(false);
	}
}

void AWizardPlayerController::OnResetPressed()
{
	if (UWizardAbilityComponent* Ab = GetAbilities())
	{
		Ab->RequestArenaReset();
	}
}

void AWizardPlayerController::OnHintsPressed()
{
	if (UWizardAbilityComponent* Ab = GetAbilities())
	{
		Ab->SetHintsVisible(!Ab->bHintsVisible);
	}
}

void AWizardPlayerController::OnLookYaw(float Val)
{
	if (bMouseCaptured && !FMath::IsNearlyZero(Val))
	{
		AddYawInput(Val * 1.1f);
	}
}

void AWizardPlayerController::OnLookPitch(float Val)
{
	if (bMouseCaptured && !FMath::IsNearlyZero(Val))
	{
		AddPitchInput(-Val * 1.1f);
	}
}

void AWizardPlayerController::OnJumpPressed()
{
	if (ACharacter* Char = Cast<ACharacter>(GetPawn()))
	{
		Char->Jump();
	}
}

void AWizardPlayerController::OnJumpReleased()
{
	if (ACharacter* Char = Cast<ACharacter>(GetPawn()))
	{
		Char->StopJumping();
	}
}

void AWizardPlayerController::OnEscapePressed()
{
	if (bMouseCaptured)
	{
		ToggleMouseCapture();
	}
}
