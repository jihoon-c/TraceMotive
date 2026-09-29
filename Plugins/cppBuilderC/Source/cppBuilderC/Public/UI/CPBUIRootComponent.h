#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CPBUIRootComponent.generated.h"

class APlayerController;
class UCPBUserWidgetBase;
class UWidgetComponent;

UCLASS(ClassGroup = (CPB), Blueprintable, meta = (BlueprintSpawnableComponent))
class CPPBUILDERC_API UCPBUIRootComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCPBUIRootComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void InitializeUI();

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void ShutdownUI();

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void SetWorldRootWidgetComponent(UWidgetComponent* InWorldRootWidgetComponent);

	UFUNCTION(BlueprintPure, Category = "CPB|UI")
	UCPBUserWidgetBase* GetRootWidget() const { return RootWidget; }

	UFUNCTION(BlueprintPure, Category = "CPB|UI")
	bool IsUIInitialized() const { return RootWidget != nullptr; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CPB|UI")
	TSubclassOf<UCPBUserWidgetBase> MainWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CPB|UI")
	bool bCreateOnBeginPlay = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CPB|UI", meta = (ClampMin = "0"))
	int32 ViewportZOrder = 0;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "CPB|UI|VR")
	TObjectPtr<UWidgetComponent> WorldRootWidgetComponent;

private:
	UPROPERTY(Transient)
	TObjectPtr<UCPBUserWidgetBase> RootWidget;

	APlayerController* GetOwningPlayerController() const;
	bool IsVRPlatformActive() const;
	void InitializeViewportUI(APlayerController* PlayerController);
	void InitializeWorldSpaceUI(APlayerController* PlayerController);
};
