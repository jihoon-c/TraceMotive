#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CPBStudyAssetViewer.generated.h"

class UMaterialInstanceDynamic;

UINTERFACE(BlueprintType)
class CPPBUILDERC_API UCPBStudyAssetViewer : public UInterface
{
	GENERATED_BODY()
};

class CPPBUILDERC_API ICPBStudyAssetViewer
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "CPB|Study Asset")
	void OpenAsset(FName AssetId);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "CPB|Study Asset")
	void CloseAsset();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "CPB|Study Asset")
	void RenderToMaterial(UMaterialInstanceDynamic* TargetMaterial);
};
