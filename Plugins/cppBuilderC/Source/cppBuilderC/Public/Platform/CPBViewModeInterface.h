#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CPBViewModeInterface.generated.h"

UINTERFACE(BlueprintType)
class CPPBUILDERC_API UCPBViewModeInterface : public UInterface
{
	GENERATED_BODY()
};

class CPPBUILDERC_API ICPBViewModeInterface
{
	GENERATED_BODY()

public:
	virtual void SetupView() = 0;
	virtual void TeardownView() = 0;
	virtual FTransform GetViewTransform() const = 0;
};
