#include "Study/CPBVideoViewerAdapter.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "MediaPlayer.h"
#include "MediaSource.h"

void UCPBVideoViewerAdapter::OpenAsset_Implementation(FName AssetId)
{
	ActiveAssetId = AssetId;

	if (MediaPlayer && MediaSource)
	{
		MediaPlayer->OpenSource(MediaSource);
		MediaPlayer->Play();
	}
}

void UCPBVideoViewerAdapter::CloseAsset_Implementation()
{
	if (MediaPlayer)
	{
		MediaPlayer->Close();
	}

	ActiveAssetId = NAME_None;
}

void UCPBVideoViewerAdapter::RenderToMaterial_Implementation(UMaterialInstanceDynamic* TargetMaterial)
{
	if (TargetMaterial)
	{
		TargetMaterial->SetScalarParameterValue(TEXT("CPB_IsVideoOpen"), ActiveAssetId.IsNone() ? 0.0f : 1.0f);
	}
}

void UCPBVideoViewerAdapter::ConfigureMedia(UMediaPlayer* InMediaPlayer, UMediaSource* InMediaSource)
{
	MediaPlayer = InMediaPlayer;
	MediaSource = InMediaSource;
}
