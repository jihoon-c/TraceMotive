#include "Study/CPBPDFViewerAdapter.h"

#include "Materials/MaterialInstanceDynamic.h"

void UCPBPDFViewerAdapter::OpenAsset_Implementation(FName AssetId)
{
	ActiveAssetId = AssetId;
	bIsOpen = true;
}

void UCPBPDFViewerAdapter::CloseAsset_Implementation()
{
	ActiveAssetId = NAME_None;
	bIsOpen = false;
}

void UCPBPDFViewerAdapter::RenderToMaterial_Implementation(UMaterialInstanceDynamic* TargetMaterial)
{
	if (TargetMaterial)
	{
		TargetMaterial->SetScalarParameterValue(TEXT("CPB_IsPDFOpen"), bIsOpen ? 1.0f : 0.0f);
	}
}

void UCPBPDFViewerAdapter::SetSourceAsset(TSoftObjectPtr<UObject> InSourceAsset)
{
	SourceAsset = InSourceAsset;
}
