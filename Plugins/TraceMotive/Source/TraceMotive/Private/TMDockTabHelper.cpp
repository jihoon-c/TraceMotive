#include "TMDockTabHelper.h"



#include "Framework/Docking/TabManager.h"

#include "Widgets/Docking/SDockTab.h"



namespace

{

    int32 GTMDockTabSerial = 0;

    TArray<TWeakPtr<SDockTab>> GTMOpenDynamicTabs;

}



TSharedPtr<SDockTab> TMDockTab::OpenDockTab(

    const TCHAR* Prefix,

    const FText& Title,

    const TSharedRef<SWidget>& Content,

    FSimpleDelegate OnClosed)

{

    const FName TabId(*FString::Printf(TEXT("TraceMotive.%s.%d"), Prefix, ++GTMDockTabSerial));



    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(

        TabId,

        FOnSpawnTab::CreateLambda([TabId, Title, Content, OnClosed](const FSpawnTabArgs&)

        {

            return SNew(SDockTab)

                .TabRole(ETabRole::NomadTab)

                .Label(Title)

                .OnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([TabId, OnClosed](TSharedRef<SDockTab>) mutable

                {

                    OnClosed.ExecuteIfBound();

                    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);

                }))

                [

                    Content

                ];

        }))

        .SetDisplayName(Title)

        .SetMenuType(ETabSpawnerMenuType::Hidden);



    TSharedPtr<SDockTab> OpenedTab = FGlobalTabmanager::Get()->TryInvokeTab(TabId);

    GTMOpenDynamicTabs.RemoveAll([](const TWeakPtr<SDockTab>& Tab) { return !Tab.IsValid(); });

    GTMOpenDynamicTabs.Add(OpenedTab);

    return OpenedTab;

}



void TMDockTab::CloseAll()

{

    TArray<TSharedPtr<SDockTab>> TabsToClose;

    for (const TWeakPtr<SDockTab>& WeakTab : GTMOpenDynamicTabs)

    {

        if (TSharedPtr<SDockTab> Tab = WeakTab.Pin()) TabsToClose.Add(Tab);

    }

    GTMOpenDynamicTabs.Reset();

    for (const TSharedPtr<SDockTab>& Tab : TabsToClose)

    {

        Tab->RequestCloseTab();

    }

}

