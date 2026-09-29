#include "UI/CPBSidePanelItemWidget.h"

void UCPBSidePanelItemWidget::SetSidePanelItem(const FCPBSidePanelItem& InItem)
{
	Item = InItem;
	BP_OnSidePanelItemChanged(Item);
}
