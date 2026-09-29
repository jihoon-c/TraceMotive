#include "UI/CPBSequencePanelWidget.h"

#include "Subsystems/CPBUISubsystem.h"

void UCPBSequencePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UCPBUISubsystem* UISubsystem = GetCPBUISubsystem())
	{
		UISubsystem->OnSequenceListChanged.AddUniqueDynamic(this, &UCPBSequencePanelWidget::HandleSequenceListChanged);
		RefreshSequenceItems(UISubsystem->GetSequenceListItems());
	}
}

void UCPBSequencePanelWidget::NativeDestruct()
{
	if (UCPBUISubsystem* UISubsystem = GetCPBUISubsystem())
	{
		UISubsystem->OnSequenceListChanged.RemoveDynamic(this, &UCPBSequencePanelWidget::HandleSequenceListChanged);
	}

	Super::NativeDestruct();
}

void UCPBSequencePanelWidget::RefreshSequenceItems(const TArray<FCPBSequenceListItem>& InItems)
{
	BP_OnSequenceItemsRefreshed(InItems);
}

void UCPBSequencePanelWidget::HandleSequenceListChanged(const TArray<FCPBSequenceListItem>& Items)
{
	RefreshSequenceItems(Items);
}
