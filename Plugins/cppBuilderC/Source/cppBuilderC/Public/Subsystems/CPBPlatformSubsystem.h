#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/CPBDelegates.h"
#include "CPBPlatformSubsystem.generated.h"

UCLASS()
class CPPBUILDERC_API UCPBPlatformSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintAssignable, Category = "CPB|Platform")
	FCPBOnPlatformInitialized OnPlatformInitialized;

	UFUNCTION(BlueprintCallable, Category = "CPB|Platform")
	void RefreshActivePlatform();

	UFUNCTION(BlueprintPure, Category = "CPB|Platform")
	ECPBActivePlatform GetActivePlatform() const { return ActivePlatform; }

	UFUNCTION(BlueprintPure, Category = "CPB|Platform")
	bool IsVRPlatform() const;

	static bool IsVRPlatform(ECPBActivePlatform Platform);

private:
	UPROPERTY(Transient)
	ECPBActivePlatform ActivePlatform = ECPBActivePlatform::CBT;

	ECPBActivePlatform ResolveConfiguredPlatform() const;
};
