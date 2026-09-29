#include "UI/CPBSidePanelWidget.h"

#include "Subsystems/CPBUISubsystem.h"

void UCPBSidePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UCPBUISubsystem* UISubsystem = GetCPBUISubsystem())
	{
		PanelTab = UISubsystem->GetActiveSidePanelTab();
		CachedItems = UISubsystem->GetSidePanelItems();
		UISubsystem->OnSidePanelTabChanged.AddUniqueDynamic(this, &UCPBSidePanelWidget::HandleSidePanelTabChanged);
		UISubsystem->OnSidePanelItemsChanged.AddUniqueDynamic(this, &UCPBSidePanelWidget::HandleSidePanelItemsChanged);
		RefreshSidePanel(PanelTab, CachedItems);
	}
}

void UCPBSidePanelWidget::NativeDestruct()
{
	if (UCPBUISubsystem* UISubsystem = GetCPBUISubsystem())
	{
		UISubsystem->OnSidePanelTabChanged.RemoveDynamic(this, &UCPBSidePanelWidget::HandleSidePanelTabChanged);
		UISubsystem->OnSidePanelItemsChanged.RemoveDynamic(this, &UCPBSidePanelWidget::HandleSidePanelItemsChanged);
	}

	Super::NativeDestruct();
}

void UCPBSidePanelWidget::RefreshSidePanel(ECPBSidePanelTab InTab, const TArray<FCPBSidePanelItem>& InItems)
{
	PanelTab = InTab;
	CachedItems = InItems;
	BP_OnSidePanelRefreshed(InTab, InItems);
}

void UCPBSidePanelWidget::HandleSidePanelTabChanged(ECPBSidePanelTab ActiveTab)
{
	RefreshSidePanel(ActiveTab, CachedItems);
}

void UCPBSidePanelWidget::HandleSidePanelItemsChanged(const TArray<FCPBSidePanelItem>& Items)
{
	RefreshSidePanel(PanelTab, Items);
}
