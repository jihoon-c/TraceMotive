#pragma once

#include "CoreMinimal.h"
#include "UI/CPBUserWidgetBase.h"
#include "CPBUpperWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class CPPBUILDERC_API UCPBUpperWidget : public UCPBUserWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CPB|UI|Upper")
	void RequestMenuAction(ECPBMenuAction MenuAction);
};
