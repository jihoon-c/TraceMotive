#pragma once

#include "CoreMinimal.h"
#include "UI/CPBUserWidgetBase.h"
#include "CPBSequenceElementWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class CPPBUILDERC_API UCPBSequenceElementWidget : public UCPBUserWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CPB|UI|Sequence")
	void SetSequenceItem(const FCPBSequenceListItem& InItem);

	UFUNCTION(BlueprintPure, Category = "CPB|UI|Sequence")
	FCPBSequenceListItem GetSequenceItem() const { return Item; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "CPB|UI|Sequence")
	void BP_OnSequenceItemChanged(const FCPBSequenceListItem& InItem);

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "CPB|UI|Sequence", meta = (AllowPrivateAccess = "true"))
	FCPBSequenceListItem Item;
};
