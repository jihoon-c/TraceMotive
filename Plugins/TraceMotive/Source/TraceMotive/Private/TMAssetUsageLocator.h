#pragma once



#include "CoreMinimal.h"

#include "AssetRegistry/AssetData.h"



struct FToolMenuContext;



namespace TMAssetUsageLocator

{

    void RegisterMenus();

    void OpenWindowForAsset(const FAssetData& AssetData);

}



