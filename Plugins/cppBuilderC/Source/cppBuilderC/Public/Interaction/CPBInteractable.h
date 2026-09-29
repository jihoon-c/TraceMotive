#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CPBInteractable.generated.h"

class AController;

UINTERFACE(BlueprintType)
class CPPBUILDERC_API UCPBInteractable : public UInterface
{
	GENERATED_BODY()
};

class CPPBUILDERC_API ICPBInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "CPB|Interaction")
	void OnInteract(AController* InstigatorController);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "CPB|Interaction")
	void OnHoverBegin();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "CPB|Interaction")
	void OnHoverEnd();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "CPB|Interaction")
	bool CanInteract() const;
};
