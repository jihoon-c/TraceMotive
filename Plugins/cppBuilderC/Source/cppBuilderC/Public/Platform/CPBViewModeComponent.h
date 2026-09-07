#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/CPBTypes.h"
#include "Platform/CPBViewModeInterface.h"
#include "CPBViewModeComponent.generated.h"

class UCameraComponent;
class UCPBPlatformSubsystem;

UCLASS(ClassGroup = (CPB), Blueprintable, meta = (BlueprintSpawnableComponent))
class CPPBUILDERC_API UCPBViewModeComponent : public UActorComponent, public ICPBViewModeInterface
{
	GENERATED_BODY()

public:
	UCPBViewModeComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "CPB|View")
	virtual void SetupView() override;

	UFUNCTION(BlueprintCallable, Category = "CPB|View")
	virtual void TeardownView() override;

	UFUNCTION(BlueprintPure, Category = "CPB|View")
	virtual FTransform GetViewTransform() const override;

	UFUNCTION(BlueprintPure, Category = "CPB|View")
	ECPBActivePlatform GetActivePlatform() const;

private:
	UPROPERTY(EditDefaultsOnly, Category = "CPB|View")
	FName PreferredCameraTag = TEXT("CPB_ViewCamera");

	UPROPERTY(EditDefaultsOnly, Category = "CPB|View")
	FName PreferredVRViewOriginTag = TEXT("CPB_VRViewOrigin");

	UPROPERTY(Transient)
	ECPBActivePlatform CachedPlatform = ECPBActivePlatform::CBT;

	UFUNCTION()
	void HandlePlatformInitialized(ECPBActivePlatform ActivePlatform);

	UCPBPlatformSubsystem* GetPlatformSubsystem() const;
	UCameraComponent* FindTaggedCameraComponent() const;
	USceneComponent* FindTaggedSceneComponent(FName ComponentTag) const;
};
