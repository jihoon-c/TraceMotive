#include "UI/CPBUpperWidget.h"

#include "Subsystems/CPBUISubsystem.h"

void UCPBUpperWidget::RequestMenuAction(ECPBMenuAction MenuAction)
{
	if (UCPBUISubsystem* UISubsystem = GetCPBUISubsystem())
	{
		UISubsystem->RequestMenuAction(MenuAction);
	}
}
