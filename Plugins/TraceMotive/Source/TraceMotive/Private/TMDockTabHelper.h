#pragma once



#include "CoreMinimal.h"

#include "Delegates/Delegate.h"

#include "Widgets/SWidget.h"



class SDockTab;



namespace TMDockTab

{

    void CloseAll();

    TSharedPtr<SDockTab> OpenDockTab(

        const TCHAR* Prefix,

        const FText& Title,

        const TSharedRef<SWidget>& Content,

        FSimpleDelegate OnClosed = FSimpleDelegate());

}

