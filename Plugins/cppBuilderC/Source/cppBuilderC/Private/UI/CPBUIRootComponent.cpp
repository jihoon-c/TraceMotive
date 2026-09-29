#include "UI/CPBUIRootComponent.h"

#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/CPBPlatformSubsystem.h"
#include "UI/CPBUserWidgetBase.h"

UCPBUIRootComponent::UCPBUIRootComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCPBUIRootComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bCreateOnBeginPlay)
	{
		InitializeUI();
	}
}

void UCPBUIRootComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ShutdownUI();
	Super::EndPlay(EndPlayReason);
}

void UCPBUIRootComponent::InitializeUI()
{
	if (RootWidget || !MainWidgetClass)
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("CPB UIRootComponent requires a PlayerController owner."));
		return;
	}

	if (IsVRPlatformActive())
	{
		InitializeWorldSpaceUI(PlayerController);
	}
	else
	{
		InitializeViewportUI(PlayerController);
	}
}

void UCPBUIRootComponent::ShutdownUI()
{
	if (RootWidget)
	{
		RootWidget->RemoveFromParent();
		RootWidget = nullptr;
	}

	if (WorldRootWidgetComponent)
	{
		WorldRootWidgetComponent->SetWidget(nullptr);
	}
}

void UCPBUIRootComponent::SetWorldRootWidgetComponent(UWidgetComponent* InWorldRootWidgetComponent)
{
	WorldRootWidgetComponent = InWorldRootWidgetComponent;

	if (RootWidget && WorldRootWidgetComponent)
	{
		RootWidget->SetWorldWidgetComponent(WorldRootWidgetComponent);
		WorldRootWidgetComponent->SetWidget(RootWidget);
	}
}

APlayerController* UCPBUIRootComponent::GetOwningPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

bool UCPBUIRootComponent::IsVRPlatformActive() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UCPBPlatformSubsystem* PlatformSubsystem = GameInstance ? GameInstance->GetSubsystem<UCPBPlatformSubsystem>() : nullptr;
	return PlatformSubsystem && PlatformSubsystem->IsVRPlatform();
}

void UCPBUIRootComponent::InitializeViewportUI(APlayerController* PlayerController)
{
	RootWidget = CreateWidget<UCPBUserWidgetBase>(PlayerController, MainWidgetClass);
	if (RootWidget)
	{
		RootWidget->AddToCPBViewport(ViewportZOrder);
	}
}

void UCPBUIRootComponent::InitializeWorldSpaceUI(APlayerController* PlayerController)
{
	RootWidget = CreateWidget<UCPBUserWidgetBase>(PlayerController, MainWidgetClass);
	if (!RootWidget)
	{
		return;
	}

	if (!WorldRootWidgetComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("CPB VR UI requested but no WorldRootWidgetComponent is assigned."));
		return;
	}

	RootWidget->SetWorldWidgetComponent(WorldRootWidgetComponent);
	WorldRootWidgetComponent->SetWidget(RootWidget);
}
