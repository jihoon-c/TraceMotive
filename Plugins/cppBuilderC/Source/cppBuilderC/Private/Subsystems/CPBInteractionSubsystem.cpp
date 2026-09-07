#include "Subsystems/CPBInteractionSubsystem.h"

void UCPBInteractionSubsystem::DispatchInteraction(AActor* InteractableActor, AController* InstigatorController)
{
	OnInteractionDispatched.Broadcast(InteractableActor, InstigatorController);
}

void UCPBInteractionSubsystem::DispatchHoverChanged(AActor* InteractableActor, bool bIsHovered)
{
	OnHoverChanged.Broadcast(InteractableActor, bIsHovered);
}
