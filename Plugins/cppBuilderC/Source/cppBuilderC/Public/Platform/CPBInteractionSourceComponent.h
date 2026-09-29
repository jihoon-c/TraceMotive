#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/CPBTypes.h"
#include "Platform/CPBInteractionSourceInterface.h"
#include "CPBInteractionSourceComponent.generated.h"

class UCPBPlatformSubsystem;

UCLASS(ClassGroup = (CPB), Blueprintable, meta = (BlueprintSpawnableComponent))
class CPPBUILDERC_API UCPBInteractionSourceComponent : public UActorComponent, public ICPBInteractionSourceInterface
{
	GENERATED_BODY()

public:
	UCPBInteractionSourceComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "CPB|Interaction")
	virtual bool GetInteractionRay(FVector& OutStart, FVector& OutDirection) const override;

	UFUNCTION(BlueprintPure, Category = "CPB|Interaction")
	virtual bool IsInteractionTriggered() const override { return bInteractionTriggered; }

	UFUNCTION(BlueprintCallable, Category = "CPB|Interaction")
	void SetInteractionTriggered(bool bNewInteractionTriggered);

	UFUNCTION(BlueprintPure, Category = "CPB|Interaction")
	float GetInteractionRange() const { return InteractionRange; }

private:
	UPROPERTY(EditDefaultsOnly, Category = "CPB|Interaction", meta = (ClampMin = "1.0"))
	float InteractionRange = 10000.0f;

	UPROPERTY(Transient)
	bool bInteractionTriggered = false;

	UPROPERTY(Transient)
	ECPBActivePlatform CachedPlatform = ECPBActivePlatform::CBT;

	bool GetScreenInteractionRay(FVector& OutStart, FVector& OutDirection) const;
	bool GetVRInteractionRay(FVector& OutStart, FVector& OutDirection) const;
	UCPBPlatformSubsystem* GetPlatformSubsystem() const;
};
