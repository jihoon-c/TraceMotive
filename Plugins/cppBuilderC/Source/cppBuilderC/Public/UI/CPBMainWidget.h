#pragma once

#include "CoreMinimal.h"
#include "UI/CPBUserWidgetBase.h"
#include "CPBMainWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class CPPBUILDERC_API UCPBMainWidget : public UCPBUserWidgetBase
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void RequestMenuAction(ECPBMenuAction MenuAction);

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void RequestSidePanelTab(ECPBSidePanelTab SidePanelTab);

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void RequestPlaybackState(ECPBPlaybackState PlaybackState);

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void RequestFreeMoveEnabled(bool bFreeMoveEnabled);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "CPB|UI")
	void BP_OnMainWidgetRegistered();
};
