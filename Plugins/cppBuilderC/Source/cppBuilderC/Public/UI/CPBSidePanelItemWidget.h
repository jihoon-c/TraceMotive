#pragma once

#include "CoreMinimal.h"
#include "UI/CPBUserWidgetBase.h"
#include "CPBSidePanelItemWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class CPPBUILDERC_API UCPBSidePanelItemWidget : public UCPBUserWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CPB|UI|Side Panel")
	void SetSidePanelItem(const FCPBSidePanelItem& InItem);

	UFUNCTION(BlueprintPure, Category = "CPB|UI|Side Panel")
	FCPBSidePanelItem GetSidePanelItem() const { return Item; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "CPB|UI|Side Panel")
	void BP_OnSidePanelItemChanged(const FCPBSidePanelItem& InItem);

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "CPB|UI|Side Panel", meta = (AllowPrivateAccess = "true"))
	FCPBSidePanelItem Item;
};
