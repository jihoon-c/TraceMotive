#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/CPBInteractable.h"
#include "CPBInteractorBase.generated.h"

class USceneComponent;
class USphereComponent;
class UCPBInteractionSubsystem;

UCLASS(Abstract, Blueprintable)
class CPPBUILDERC_API ACPBInteractorBase : public AActor, public ICPBInteractable
{
	GENERATED_BODY()

public:
	ACPBInteractorBase();

	virtual void OnInteract_Implementation(AController* InstigatorController) override;
	virtual void OnHoverBegin_Implementation() override;
	virtual void OnHoverEnd_Implementation() override;
	virtual bool CanInteract_Implementation() const override;

	UFUNCTION(BlueprintCallable, Category = "CPB|Interaction")
	void SetInteractionEnabled(bool bNewInteractionEnabled);

	UFUNCTION(BlueprintPure, Category = "CPB|Interaction")
	FName GetAssemblyId() const { return AssemblyId; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CPB|Interaction")
	TObjectPtr<USceneComponent> RootSceneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CPB|Interaction")
	TObjectPtr<USphereComponent> InteractionBounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Interaction")
	FName AssemblyId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Interaction")
	bool bInteractionEnabled = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CPB|Interaction")
	FName InteractionCollisionProfileName = TEXT("Interactor");

	UCPBInteractionSubsystem* GetInteractionSubsystem() const;
};
