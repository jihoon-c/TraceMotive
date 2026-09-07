#include "Platform/CPBInteractionSourceComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/CPBPlatformSubsystem.h"

UCPBInteractionSourceComponent::UCPBInteractionSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCPBInteractionSourceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const UCPBPlatformSubsystem* PlatformSubsystem = GetPlatformSubsystem())
	{
		CachedPlatform = PlatformSubsystem->GetActivePlatform();
	}
}

bool UCPBInteractionSourceComponent::GetInteractionRay(FVector& OutStart, FVector& OutDirection) const
{
	if (CachedPlatform != ECPBActivePlatform::VR)
	{
		return GetScreenInteractionRay(OutStart, OutDirection);
	}

	return GetVRInteractionRay(OutStart, OutDirection);
}

void UCPBInteractionSourceComponent::SetInteractionTriggered(bool bNewInteractionTriggered)
{
	bInteractionTriggered = bNewInteractionTriggered;
}

bool UCPBInteractionSourceComponent::GetScreenInteractionRay(FVector& OutStart, FVector& OutDirection) const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	if (!PlayerController)
	{
		return GetVRInteractionRay(OutStart, OutDirection);
	}

	if (PlayerController->DeprojectMousePositionToWorld(OutStart, OutDirection))
	{
		OutDirection.Normalize();
		return true;
	}

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ViewportSizeX > 0 && ViewportSizeY > 0)
	{
		const float CenterX = static_cast<float>(ViewportSizeX) * 0.5f;
		const float CenterY = static_cast<float>(ViewportSizeY) * 0.5f;
		if (PlayerController->DeprojectScreenPositionToWorld(CenterX, CenterY, OutStart, OutDirection))
		{
			OutDirection.Normalize();
			return true;
		}
	}

	return false;
}

bool UCPBInteractionSourceComponent::GetVRInteractionRay(FVector& OutStart, FVector& OutDirection) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	OutStart = Owner->GetActorLocation();
	OutDirection = Owner->GetActorForwardVector().GetSafeNormal();
	return !OutDirection.IsNearlyZero();
}

UCPBPlatformSubsystem* UCPBInteractionSourceComponent::GetPlatformSubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UCPBPlatformSubsystem>() : nullptr;
}
