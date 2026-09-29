#pragma once

#include "CoreMinimal.h"
#include "UI/CPBUserWidgetBase.h"
#include "CPBSubtitleWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class CPPBUILDERC_API UCPBSubtitleWidget : public UCPBUserWidgetBase
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "CPB|UI|Subtitle")
	void ApplySubtitle(const FText& SubtitleText, bool bVisible);

	UFUNCTION(BlueprintPure, Category = "CPB|UI|Subtitle")
	FText GetCurrentSubtitle() const { return CurrentSubtitle; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "CPB|UI|Subtitle")
	void BP_OnSubtitleApplied(const FText& SubtitleText, bool bVisible);

private:
	UFUNCTION()
	void HandleSubtitleChanged(const FText& SubtitleText, bool bVisible);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "CPB|UI|Subtitle", meta = (AllowPrivateAccess = "true"))
	FText CurrentSubtitle;
};
