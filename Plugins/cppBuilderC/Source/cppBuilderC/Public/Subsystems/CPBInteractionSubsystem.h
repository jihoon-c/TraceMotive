#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/CPBDelegates.h"
#include "CPBInteractionSubsystem.generated.h"

class AActor;
class AController;

UCLASS()
class CPPBUILDERC_API UCPBInteractionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "CPB|Interaction")
	FCPBOnInteractionDispatched OnInteractionDispatched;

	UPROPERTY(BlueprintAssignable, Category = "CPB|Interaction")
	FCPBOnHoverChanged OnHoverChanged;

	UFUNCTION(BlueprintCallable, Category = "CPB|Interaction")
	void DispatchInteraction(AActor* InteractableActor, AController* InstigatorController);

	UFUNCTION(BlueprintCallable, Category = "CPB|Interaction")
	void DispatchHoverChanged(AActor* InteractableActor, bool bIsHovered);
};
