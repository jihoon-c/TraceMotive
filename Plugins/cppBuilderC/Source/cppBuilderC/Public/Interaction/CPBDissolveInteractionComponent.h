#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CPBDissolveInteractionComponent.generated.h"

class UMaterialInstanceDynamic;
class UMeshComponent;

UCLASS(ClassGroup = (CPB), Blueprintable, meta = (BlueprintSpawnableComponent))
class CPPBUILDERC_API UCPBDissolveInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCPBDissolveInteractionComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "CPB|Interaction|Dissolve")
	void RefreshDynamicMaterials();

	UFUNCTION(BlueprintCallable, Category = "CPB|Interaction|Dissolve")
	void SetDissolveAmount(float DissolveAmount);

	UFUNCTION(BlueprintCallable, Category = "CPB|Interaction|Dissolve")
	void BeginDissolve(float TargetDissolveAmount);

private:
	UPROPERTY(EditAnywhere, Category = "CPB|Interaction|Dissolve")
	TArray<TObjectPtr<UMeshComponent>> TargetMeshComponents;

	UPROPERTY(EditDefaultsOnly, Category = "CPB|Interaction|Dissolve")
	FName DissolveParameterName = TEXT("DissolveAmount");

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;
};
