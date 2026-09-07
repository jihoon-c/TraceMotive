#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Core/CPBTypes.h"
#include "CPBGameInstance.generated.h"

UCLASS(Blueprintable)
class CPPBUILDERC_API UCPBGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "CPB|Platform")
	ECPBActivePlatform GetStartupPlatform() const { return StartupPlatform; }

	UFUNCTION(BlueprintCallable, Category = "CPB|Platform")
	void SetStartupPlatform(ECPBActivePlatform InStartupPlatform);

	UFUNCTION(BlueprintPure, Category = "CPB|Platform")
	bool IsVRStartupPlatform() const { return StartupPlatform == ECPBActivePlatform::VR; }

private:
	// Project Settings or a BP-derived GameInstance chooses this value once per build/runtime profile.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CPB|Platform", meta = (AllowPrivateAccess = "true"))
	ECPBActivePlatform StartupPlatform = ECPBActivePlatform::CBT;
};
