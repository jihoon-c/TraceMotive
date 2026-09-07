#pragma once

#include "CoreMinimal.h"
#include "UI/CPBUserWidgetBase.h"
#include "CPBPopupWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class CPPBUILDERC_API UCPBPopupWidget : public UCPBUserWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CPB|UI|Popup")
	void OpenPopup();

	UFUNCTION(BlueprintCallable, Category = "CPB|UI|Popup")
	void ClosePopup();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "CPB|UI|Popup")
	void BP_OnPopupOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "CPB|UI|Popup")
	void BP_OnPopupClosed();
};
