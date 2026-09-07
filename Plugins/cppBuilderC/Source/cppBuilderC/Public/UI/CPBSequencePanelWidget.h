#pragma once

#include "CoreMinimal.h"
#include "UI/CPBUserWidgetBase.h"
#include "CPBSequencePanelWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class CPPBUILDERC_API UCPBSequencePanelWidget : public UCPBUserWidgetBase
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "CPB|UI|Sequence")
	void RefreshSequenceItems(const TArray<FCPBSequenceListItem>& InItems);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "CPB|UI|Sequence")
	void BP_OnSequenceItemsRefreshed(const TArray<FCPBSequenceListItem>& InItems);

private:
	UFUNCTION()
	void HandleSequenceListChanged(const TArray<FCPBSequenceListItem>& Items);
};
