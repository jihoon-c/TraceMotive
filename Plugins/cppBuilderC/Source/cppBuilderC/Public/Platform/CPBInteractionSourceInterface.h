#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CPBInteractionSourceInterface.generated.h"

UINTERFACE(BlueprintType)
class CPPBUILDERC_API UCPBInteractionSourceInterface : public UInterface
{
	GENERATED_BODY()
};

class CPPBUILDERC_API ICPBInteractionSourceInterface
{
	GENERATED_BODY()

public:
	virtual bool GetInteractionRay(FVector& OutStart, FVector& OutDirection) const = 0;
	virtual bool IsInteractionTriggered() const = 0;
};
