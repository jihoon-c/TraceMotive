#include "UI/CPBSubtitleWidget.h"

#include "Subsystems/CPBUISubsystem.h"

void UCPBSubtitleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UCPBUISubsystem* UISubsystem = GetCPBUISubsystem())
	{
		UISubsystem->OnSubtitleChanged.AddUniqueDynamic(this, &UCPBSubtitleWidget::HandleSubtitleChanged);
		ApplySubtitle(UISubsystem->GetCurrentSubtitle(), !UISubsystem->GetCurrentSubtitle().IsEmpty());
	}
}

void UCPBSubtitleWidget::NativeDestruct()
{
	if (UCPBUISubsystem* UISubsystem = GetCPBUISubsystem())
	{
		UISubsystem->OnSubtitleChanged.RemoveDynamic(this, &UCPBSubtitleWidget::HandleSubtitleChanged);
	}

	Super::NativeDestruct();
}

void UCPBSubtitleWidget::ApplySubtitle(const FText& SubtitleText, bool bVisible)
{
	CurrentSubtitle = SubtitleText;
	BP_OnSubtitleApplied(SubtitleText, bVisible);
}

void UCPBSubtitleWidget::HandleSubtitleChanged(const FText& SubtitleText, bool bVisible)
{
	ApplySubtitle(SubtitleText, bVisible);
}
