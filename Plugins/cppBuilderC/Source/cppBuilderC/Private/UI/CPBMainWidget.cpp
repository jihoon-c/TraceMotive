#include "UI/CPBMainWidget.h"

#include "Subsystems/CPBUISubsystem.h"

void UCPBMainWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UCPBUISubsystem* UISubsystem = GetCPBUISubsystem())
	{
		UISubsystem->RegisterMainWidget(this);
		BP_OnMainWidgetRegistered();
	}
}

void UCPBMainWidget::NativeDestruct()
{
	if (UCPBUISubsystem* UISubsystem = GetCPBUISubsystem())
	{
		UISubsystem->UnregisterMainWidget(this);
	}

	Super::NativeDestruct();
}

void UCPBMainWidget::RequestMenuAction(ECPBMenuAction MenuAction)
{
	if (UCPBUISubsystem* UISubsystem = GetCPBUISubsystem())
	{
		UISubsystem->RequestMenuAction(MenuAction);
	}
}

void UCPBMainWidget::RequestSidePanelTab(ECPBSidePanelTab SidePanelTab)
{
	if (UCPBUISubsystem* UISubsystem = GetCPBUISubsystem())
	{
		UISubsystem->SetActiveSidePanelTab(SidePanelTab);
	}
}

void UCPBMainWidget::RequestPlaybackState(ECPBPlaybackState PlaybackState)
{
	if (UCPBUISubsystem* UISubsystem = GetCPBUISubsystem())
	{
		UISubsystem->SetPlaybackState(PlaybackState);
	}
}

void UCPBMainWidget::RequestFreeMoveEnabled(bool bFreeMoveEnabled)
{
	if (UCPBUISubsystem* UISubsystem = GetCPBUISubsystem())
	{
		UISubsystem->SetFreeMoveEnabled(bFreeMoveEnabled);
	}
}
