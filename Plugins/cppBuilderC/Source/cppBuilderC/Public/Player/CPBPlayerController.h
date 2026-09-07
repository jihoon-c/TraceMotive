#pragma once

#include "CoreMinimal.h"
#include "Core/CPBDelegates.h"
#include "GameFramework/PlayerController.h"
#include "CPBPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UCPBUIRootComponent;

UCLASS(Blueprintable)
class CPPBUILDERC_API ACPBPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACPBPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UPROPERTY(BlueprintAssignable, Category = "CPB|Input")
	FCPBOnInteractInput OnInteractPressed;

	UPROPERTY(BlueprintAssignable, Category = "CPB|Input")
	FCPBOnInteractInput OnInteractReleased;

	UFUNCTION(BlueprintPure, Category = "CPB|UI")
	UCPBUIRootComponent* GetUIRootComponent() const { return UIRootComponent; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CPB|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCPBUIRootComponent> UIRootComponent;

	UPROPERTY(EditDefaultsOnly, Category = "CPB|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "CPB|Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditDefaultsOnly, Category = "CPB|Input")
	int32 MappingPriority = 0;

	void HandleInteractStarted();
	void HandleInteractCompleted();
};
