#include "UI/CPBPopupWidget.h"

void UCPBPopupWidget::OpenPopup()
{
	BP_OnPopupOpened();
}

void UCPBPopupWidget::ClosePopup()
{
	BP_OnPopupClosed();
}
