#pragma once

#include "CoreMinimal.h"
#include "Study/CPBStudyAssetViewer.h"
#include "CPBPDFViewerAdapter.generated.h"

class UMaterialInstanceDynamic;

UCLASS(Blueprintable)
class CPPBUILDERC_API UCPBPDFViewerAdapter : public UObject, public ICPBStudyAssetViewer
{
	GENERATED_BODY()

public:
	virtual void OpenAsset_Implementation(FName AssetId) override;
	virtual void CloseAsset_Implementation() override;
	virtual void RenderToMaterial_Implementation(UMaterialInstanceDynamic* TargetMaterial) override;

	UFUNCTION(BlueprintCallable, Category = "CPB|Study Asset|PDF")
	void SetSourceAsset(TSoftObjectPtr<UObject> InSourceAsset);

private:
	UPROPERTY(EditAnywhere, Category = "CPB|Study Asset|PDF")
	TSoftObjectPtr<UObject> SourceAsset;

	UPROPERTY(Transient)
	FName ActiveAssetId = NAME_None;

	UPROPERTY(Transient)
	bool bIsOpen = false;
};
