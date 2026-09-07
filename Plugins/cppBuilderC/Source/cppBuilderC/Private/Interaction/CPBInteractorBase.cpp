#include "Interaction/CPBInteractorBase.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Subsystems/CPBInteractionSubsystem.h"

ACPBInteractorBase::ACPBInteractorBase()
{
	PrimaryActorTick.bCanEverTick = false;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootSceneComponent);

	InteractionBounds = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(RootSceneComponent);
	InteractionBounds->InitSphereRadius(32.0f);
	InteractionBounds->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	InteractionBounds->SetGenerateOverlapEvents(true);
}

void ACPBInteractorBase::OnInteract_Implementation(AController* InstigatorController)
{
	if (!CanInteract_Implementation())
	{
		return;
	}

	if (UCPBInteractionSubsystem* InteractionSubsystem = GetInteractionSubsystem())
	{
		InteractionSubsystem->DispatchInteraction(this, InstigatorController);
	}
}

void ACPBInteractorBase::OnHoverBegin_Implementation()
{
	if (UCPBInteractionSubsystem* InteractionSubsystem = GetInteractionSubsystem())
	{
		InteractionSubsystem->DispatchHoverChanged(this, true);
	}
}

void ACPBInteractorBase::OnHoverEnd_Implementation()
{
	if (UCPBInteractionSubsystem* InteractionSubsystem = GetInteractionSubsystem())
	{
		InteractionSubsystem->DispatchHoverChanged(this, false);
	}
}

bool ACPBInteractorBase::CanInteract_Implementation() const
{
	return bInteractionEnabled;
}

void ACPBInteractorBase::SetInteractionEnabled(bool bNewInteractionEnabled)
{
	bInteractionEnabled = bNewInteractionEnabled;
	if (InteractionBounds)
	{
		InteractionBounds->SetCollisionEnabled(bInteractionEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

UCPBInteractionSubsystem* ACPBInteractorBase::GetInteractionSubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UCPBInteractionSubsystem>() : nullptr;
}
