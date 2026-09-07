#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CPBMainPawn.generated.h"

class UCameraComponent;
class UCPBInteractionSourceComponent;
class UCPBViewModeComponent;
class USceneComponent;

UCLASS(Blueprintable)
class CPPBUILDERC_API ACPBMainPawn : public APawn
{
	GENERATED_BODY()

public:
	ACPBMainPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	UFUNCTION(BlueprintPure, Category = "CPB|View")
	FTransform GetCurrentViewTransform() const;

	UFUNCTION(BlueprintCallable, Category = "CPB|Interaction")
	void HandleInteractPressed();

	UFUNCTION(BlueprintCallable, Category = "CPB|Interaction")
	void HandleInteractReleased();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CPB|Components")
	TObjectPtr<USceneComponent> RootSceneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CPB|Components")
	TObjectPtr<UCameraComponent> ScreenCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CPB|Components")
	TObjectPtr<UCPBViewModeComponent> ViewModeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CPB|Components")
	TObjectPtr<UCPBInteractionSourceComponent> InteractionSourceComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CPB|Interaction")
	TEnumAsByte<ECollisionChannel> InteractionTraceChannel = ECC_Visibility;

private:
	UPROPERTY(Transient)
	TObjectPtr<AActor> LastHoveredActor;

	void UpdateHoverTarget();
	AActor* TraceInteractableActor() const;
	void BindControllerDelegates(AController* NewController);
	void UnbindControllerDelegates(AController* OldController);
};
