#include "Platform/CPBViewModeComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Subsystems/CPBPlatformSubsystem.h"

UCPBViewModeComponent::UCPBViewModeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCPBViewModeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UCPBPlatformSubsystem* PlatformSubsystem = GetPlatformSubsystem())
	{
		CachedPlatform = PlatformSubsystem->GetActivePlatform();
		PlatformSubsystem->OnPlatformInitialized.AddDynamic(this, &UCPBViewModeComponent::HandlePlatformInitialized);
	}

	SetupView();
}

void UCPBViewModeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCPBPlatformSubsystem* PlatformSubsystem = GetPlatformSubsystem())
	{
		PlatformSubsystem->OnPlatformInitialized.RemoveDynamic(this, &UCPBViewModeComponent::HandlePlatformInitialized);
	}

	TeardownView();
	Super::EndPlay(EndPlayReason);
}

void UCPBViewModeComponent::SetupView()
{
	CachedPlatform = GetActivePlatform();
}

void UCPBViewModeComponent::TeardownView()
{
}

FTransform UCPBViewModeComponent::GetViewTransform() const
{
	if (CachedPlatform != ECPBActivePlatform::VR)
	{
		if (const UCameraComponent* CameraComponent = FindTaggedCameraComponent())
		{
			return CameraComponent->GetComponentTransform();
		}
	}
	else
	{
		if (const USceneComponent* VRViewOrigin = FindTaggedSceneComponent(PreferredVRViewOriginTag))
		{
			return VRViewOrigin->GetComponentTransform();
		}
	}

	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorTransform() : FTransform::Identity;
}

ECPBActivePlatform UCPBViewModeComponent::GetActivePlatform() const
{
	if (const UCPBPlatformSubsystem* PlatformSubsystem = GetPlatformSubsystem())
	{
		return PlatformSubsystem->GetActivePlatform();
	}

	return ECPBActivePlatform::CBT;
}

void UCPBViewModeComponent::HandlePlatformInitialized(ECPBActivePlatform ActivePlatform)
{
	CachedPlatform = ActivePlatform;
	SetupView();
}

UCPBPlatformSubsystem* UCPBViewModeComponent::GetPlatformSubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UCPBPlatformSubsystem>() : nullptr;
}

UCameraComponent* UCPBViewModeComponent::FindTaggedCameraComponent() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	TArray<UCameraComponent*> CameraComponents;
	Owner->GetComponents<UCameraComponent>(CameraComponents);
	for (UCameraComponent* CameraComponent : CameraComponents)
	{
		if (CameraComponent && CameraComponent->ComponentHasTag(PreferredCameraTag))
		{
			return CameraComponent;
		}
	}

	return CameraComponents.Num() > 0 ? CameraComponents[0] : nullptr;
}

USceneComponent* UCPBViewModeComponent::FindTaggedSceneComponent(FName ComponentTag) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	TArray<USceneComponent*> SceneComponents;
	Owner->GetComponents<USceneComponent>(SceneComponents);
	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (SceneComponent && SceneComponent->ComponentHasTag(ComponentTag))
		{
			return SceneComponent;
		}
	}

	return Owner->GetRootComponent();
}
