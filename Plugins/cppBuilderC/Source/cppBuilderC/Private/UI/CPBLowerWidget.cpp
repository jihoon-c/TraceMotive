#include "UI/CPBLowerWidget.h"

#include "Subsystems/CPBUISubsystem.h"

void UCPBLowerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UCPBUISubsystem* UISubsystem = GetCPBUISubsystem())
	{
		UISubsystem->OnPlaybackStateChanged.AddUniqueDynamic(this, &UCPBLowerWidget::HandlePlaybackStateChanged);
		UISubsystem->OnFreeMoveChanged.AddUniqueDynamic(this, &UCPBLowerWidget::HandleFreeMoveChanged);
		BP_OnPlaybackStateChanged(UISubsystem->GetPlaybackState());
		BP_OnFreeMoveChanged(UISubsystem->IsFreeMoveEnabled());
	}
}

void UCPBLowerWidget::NativeDestruct()
{
	if (UCPBUISubsystem* UISubsystem = GetCPBUISubsystem())
	{
		UISubsystem->OnPlaybackStateChanged.RemoveDynamic(this, &UCPBLowerWidget::HandlePlaybackStateChanged);
		UISubsystem->OnFreeMoveChanged.RemoveDynamic(this, &UCPBLowerWidget::HandleFreeMoveChanged);
	}

	Super::NativeDestruct();
}

void UCPBLowerWidget::RequestPlaybackState(ECPBPlaybackState PlaybackState)
{
	if (UCPBUISubsystem* UISubsystem = GetCPBUISubsystem())
	{
		UISubsystem->SetPlaybackState(PlaybackState);
	}
}

void UCPBLowerWidget::RequestFreeMoveEnabled(bool bFreeMoveEnabled)
{
	if (UCPBUISubsystem* UISubsystem = GetCPBUISubsystem())
	{
		UISubsystem->SetFreeMoveEnabled(bFreeMoveEnabled);
	}
}

void UCPBLowerWidget::HandlePlaybackStateChanged(ECPBPlaybackState PlaybackState)
{
	BP_OnPlaybackStateChanged(PlaybackState);
}

void UCPBLowerWidget::HandleFreeMoveChanged(bool bFreeMoveEnabled)
{
	BP_OnFreeMoveChanged(bFreeMoveEnabled);
}
