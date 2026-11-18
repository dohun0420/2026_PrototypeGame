// Copyright Epic Games, Inc. All Rights Reserved.


#include "MyProjectPrototypePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "MyProjectPrototype.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "ColonyTravelSubsystem.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"

void AMyProjectPrototypePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);
			SetCrosshairVisible(false);

		} else {

			UE_LOG(LogMyProjectPrototype, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}

	if (IsLocalController() && CrosshairWidgetClass)
	{
		CrosshairWidget = CreateWidget<UUserWidget>(
			this, CrosshairWidgetClass);

		if (CrosshairWidget)
		{
			CrosshairWidget->AddToViewport();
		}
	}
}

void AMyProjectPrototypePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindKey(
			EKeys::E,
			IE_Pressed,
			this,
			&ThisClass::TryColonyTravelInteraction);
	}

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

void AMyProjectPrototypePlayerController::TryColonyTravelInteraction()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UColonyTravelSubsystem* TravelSubsystem =
			GameInstance->GetSubsystem<UColonyTravelSubsystem>())
		{
			TravelSubsystem->TryInteract(this);
		}
	}
}

bool AMyProjectPrototypePlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AMyProjectPrototypePlayerController::SetCrosshairVisible(
	bool bVisible)
{
	if (CrosshairWidget)
	{
		CrosshairWidget->SetVisibility(
			bVisible
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Hidden);
	}
}
