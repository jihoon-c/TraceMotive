#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/CPBDelegates.h"
#include "CPBStudyAssetSubsystem.generated.h"

class UMaterialInstanceDynamic;

UCLASS()
class CPPBUILDERC_API UCPBStudyAssetSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "CPB|Study Asset")
	FCPBOnStudyAssetOpened OnStudyAssetOpened;

	UPROPERTY(BlueprintAssignable, Category = "CPB|Study Asset")
	FCPBOnStudyAssetClosed OnStudyAssetClosed;

	UPROPERTY(BlueprintAssignable, Category = "CPB|Study Asset")
	FCPBOnStudyAssetRenderRequested OnStudyAssetRenderRequested;

	UFUNCTION(BlueprintCallable, Category = "CPB|Study Asset")
	bool OpenAsset(FName AssetId, ECPBStudyAssetType AssetType, UObject* ViewerObject);

	UFUNCTION(BlueprintCallable, Category = "CPB|Study Asset")
	void CloseActiveAsset();

	UFUNCTION(BlueprintCallable, Category = "CPB|Study Asset")
	void RenderActiveAssetToMaterial(UMaterialInstanceDynamic* TargetMaterial);

	UFUNCTION(BlueprintPure, Category = "CPB|Study Asset")
	FName GetActiveAssetId() const { return ActiveAssetId; }

	UFUNCTION(BlueprintPure, Category = "CPB|Study Asset")
	ECPBStudyAssetType GetActiveAssetType() const { return ActiveAssetType; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UObject> ActiveViewerObject;

	UPROPERTY(Transient)
	FName ActiveAssetId = NAME_None;

	UPROPERTY(Transient)
	ECPBStudyAssetType ActiveAssetType = ECPBStudyAssetType::None;
};
