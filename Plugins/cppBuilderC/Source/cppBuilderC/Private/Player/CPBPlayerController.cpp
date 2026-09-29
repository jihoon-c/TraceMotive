#include "Player/CPBPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UI/CPBUIRootComponent.h"

ACPBPlayerController::ACPBPlayerController()
{
	UIRootComponent = CreateDefaultSubobject<UCPBUIRootComponent>(TEXT("UIRootComponent"));
}

void ACPBPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (DefaultMappingContext)
	{
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
			{
				InputSubsystem->AddMappingContext(DefaultMappingContext, MappingPriority);
			}
		}
	}
}

void ACPBPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInputComponent || !InteractAction)
	{
		return;
	}

	EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ACPBPlayerController::HandleInteractStarted);
	EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &ACPBPlayerController::HandleInteractCompleted);
	EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Canceled, this, &ACPBPlayerController::HandleInteractCompleted);
}

void ACPBPlayerController::HandleInteractStarted()
{
	OnInteractPressed.Broadcast();
}

void ACPBPlayerController::HandleInteractCompleted()
{
	OnInteractReleased.Broadcast();
}
