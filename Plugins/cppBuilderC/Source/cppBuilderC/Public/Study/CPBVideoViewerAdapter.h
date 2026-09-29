#pragma once

#include "CoreMinimal.h"
#include "Study/CPBStudyAssetViewer.h"
#include "CPBVideoViewerAdapter.generated.h"

class UMaterialInstanceDynamic;
class UMediaPlayer;
class UMediaSource;

UCLASS(Blueprintable)
class CPPBUILDERC_API UCPBVideoViewerAdapter : public UObject, public ICPBStudyAssetViewer
{
	GENERATED_BODY()

public:
	virtual void OpenAsset_Implementation(FName AssetId) override;
	virtual void CloseAsset_Implementation() override;
	virtual void RenderToMaterial_Implementation(UMaterialInstanceDynamic* TargetMaterial) override;

	UFUNCTION(BlueprintCallable, Category = "CPB|Study Asset|Video")
	void ConfigureMedia(UMediaPlayer* InMediaPlayer, UMediaSource* InMediaSource);

private:
	UPROPERTY(EditAnywhere, Category = "CPB|Study Asset|Video")
	TObjectPtr<UMediaPlayer> MediaPlayer;

	UPROPERTY(EditAnywhere, Category = "CPB|Study Asset|Video")
	TObjectPtr<UMediaSource> MediaSource;

	UPROPERTY(Transient)
	FName ActiveAssetId = NAME_None;
};
