#pragma once

#include "CoreMinimal.h"
#include "UI/CPBUserWidgetBase.h"
#include "CPBLowerWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class CPPBUILDERC_API UCPBLowerWidget : public UCPBUserWidgetBase
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "CPB|UI|Lower")
	void RequestPlaybackState(ECPBPlaybackState PlaybackState);

	UFUNCTION(BlueprintCallable, Category = "CPB|UI|Lower")
	void RequestFreeMoveEnabled(bool bFreeMoveEnabled);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "CPB|UI|Lower")
	void BP_OnPlaybackStateChanged(ECPBPlaybackState PlaybackState);

	UFUNCTION(BlueprintImplementableEvent, Category = "CPB|UI|Lower")
	void BP_OnFreeMoveChanged(bool bFreeMoveEnabled);

private:
	UFUNCTION()
	void HandlePlaybackStateChanged(ECPBPlaybackState PlaybackState);

	UFUNCTION()
	void HandleFreeMoveChanged(bool bFreeMoveEnabled);
};
