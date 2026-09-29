#include "UI/CPBSequenceElementWidget.h"

void UCPBSequenceElementWidget::SetSequenceItem(const FCPBSequenceListItem& InItem)
{
	Item = InItem;
	BP_OnSequenceItemChanged(Item);
}
