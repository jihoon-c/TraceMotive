#include "Subsystems/CPBUISubsystem.h"

#include "UI/CPBMainWidget.h"

void UCPBUISubsystem::RegisterMainWidget(UCPBMainWidget* InMainWidget)
{
	MainWidget = InMainWidget;
}

void UCPBUISubsystem::UnregisterMainWidget(UCPBMainWidget* InMainWidget)
{
	if (MainWidget.Get() == InMainWidget)
	{
		MainWidget.Reset();
	}
}

void UCPBUISubsystem::SetSubtitle(const FText& InSubtitleText)
{
	CurrentSubtitle = InSubtitleText;
	OnSubtitleChanged.Broadcast(CurrentSubtitle, true);
}

void UCPBUISubsystem::ClearSubtitle()
{
	CurrentSubtitle = FText::GetEmpty();
	OnSubtitleChanged.Broadcast(CurrentSubtitle, false);
}

void UCPBUISubsystem::RequestMenuAction(ECPBMenuAction MenuAction)
{
	OnMenuActionRequested.Broadcast(MenuAction);
}

void UCPBUISubsystem::SetActiveSidePanelTab(ECPBSidePanelTab InActiveTab)
{
	ActiveSidePanelTab = InActiveTab;
	OnSidePanelTabChanged.Broadcast(ActiveSidePanelTab);
}

void UCPBUISubsystem::SetSidePanelItems(const TArray<FCPBSidePanelItem>& InItems)
{
	SidePanelItems = InItems;
	OnSidePanelItemsChanged.Broadcast(SidePanelItems);
}

void UCPBUISubsystem::SetSequenceListItems(const TArray<FCPBSequenceListItem>& InItems)
{
	SequenceListItems = InItems;
	OnSequenceListChanged.Broadcast(SequenceListItems);
}

void UCPBUISubsystem::SetPlaybackState(ECPBPlaybackState InPlaybackState)
{
	PlaybackState = InPlaybackState;
	OnPlaybackStateChanged.Broadcast(PlaybackState);
}

void UCPBUISubsystem::SetFreeMoveEnabled(bool bInFreeMoveEnabled)
{
	bFreeMoveEnabled = bInFreeMoveEnabled;
	OnFreeMoveChanged.Broadcast(bFreeMoveEnabled);
}
