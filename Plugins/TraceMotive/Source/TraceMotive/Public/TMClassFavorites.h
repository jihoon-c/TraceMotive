#pragma once



#include "CoreMinimal.h"



struct FToolMenuContext;



namespace TMClassFavorites

{

    void RegisterMenus();

    void UnregisterMenus();

    void AddSelectedAssetsFromContext(const FToolMenuContext& Context);

    void OpenFavoritesWindow();

}

