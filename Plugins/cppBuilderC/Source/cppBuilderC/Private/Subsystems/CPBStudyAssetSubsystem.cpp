#include "Subsystems/CPBStudyAssetSubsystem.h"

#include "Study/CPBStudyAssetViewer.h"

bool UCPBStudyAssetSubsystem::OpenAsset(FName AssetId, ECPBStudyAssetType AssetType, UObject* ViewerObject)
{
	if (!ViewerObject || !ViewerObject->GetClass()->ImplementsInterface(UCPBStudyAssetViewer::StaticClass()))
	{
		return false;
	}

	if (ActiveViewerObject && ActiveViewerObject != ViewerObject)
	{
		CloseActiveAsset();
	}

	ActiveViewerObject = ViewerObject;
	ActiveAssetId = AssetId;
	ActiveAssetType = AssetType;

	ICPBStudyAssetViewer::Execute_OpenAsset(ViewerObject, AssetId);
	OnStudyAssetOpened.Broadcast(AssetId, AssetType);
	return true;
}

void UCPBStudyAssetSubsystem::CloseActiveAsset()
{
	if (ActiveViewerObject && ActiveViewerObject->GetClass()->ImplementsInterface(UCPBStudyAssetViewer::StaticClass()))
	{
		ICPBStudyAssetViewer::Execute_CloseAsset(ActiveViewerObject);
	}

	const FName ClosedAssetId = ActiveAssetId;
	ActiveViewerObject = nullptr;
	ActiveAssetId = NAME_None;
	ActiveAssetType = ECPBStudyAssetType::None;

	OnStudyAssetClosed.Broadcast(ClosedAssetId);
}

void UCPBStudyAssetSubsystem::RenderActiveAssetToMaterial(UMaterialInstanceDynamic* TargetMaterial)
{
	if (ActiveViewerObject && ActiveViewerObject->GetClass()->ImplementsInterface(UCPBStudyAssetViewer::StaticClass()))
	{
		ICPBStudyAssetViewer::Execute_RenderToMaterial(ActiveViewerObject, TargetMaterial);
		OnStudyAssetRenderRequested.Broadcast(TargetMaterial);
	}
}
