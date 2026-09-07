#pragma once

#include "CoreMinimal.h"
#include "UI/CPBUserWidgetBase.h"
#include "CPBSidePanelWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class CPPBUILDERC_API UCPBSidePanelWidget : public UCPBUserWidgetBase
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "CPB|UI|Side Panel")
	void RefreshSidePanel(ECPBSidePanelTab InTab, const TArray<FCPBSidePanelItem>& InItems);

	UFUNCTION(BlueprintPure, Category = "CPB|UI|Side Panel")
	ECPBSidePanelTab GetPanelTab() const { return PanelTab; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "CPB|UI|Side Panel")
	void BP_OnSidePanelRefreshed(ECPBSidePanelTab InTab, const TArray<FCPBSidePanelItem>& InItems);

private:
	UFUNCTION()
	void HandleSidePanelTabChanged(ECPBSidePanelTab ActiveTab);

	UFUNCTION()
	void HandleSidePanelItemsChanged(const TArray<FCPBSidePanelItem>& Items);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "CPB|UI|Side Panel", meta = (AllowPrivateAccess = "true"))
	ECPBSidePanelTab PanelTab = ECPBSidePanelTab::Parts;

	UPROPERTY(Transient)
	TArray<FCPBSidePanelItem> CachedItems;
};
