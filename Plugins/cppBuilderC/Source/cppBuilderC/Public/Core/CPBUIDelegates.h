#pragma once

#include "CoreMinimal.h"
#include "Core/CPBTypes.h"
#include "CPBUIDelegates.generated.h"

UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCPBOnSubtitleChanged, const FText&, SubtitleText, bool, bVisible);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPBOnMenuActionRequested, ECPBMenuAction, MenuAction);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPBOnSidePanelTabChanged, ECPBSidePanelTab, ActiveTab);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPBOnSidePanelItemsChanged, const TArray<FCPBSidePanelItem>&, Items);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPBOnSequenceListChanged, const TArray<FCPBSequenceListItem>&, Items);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPBOnPlaybackStateChanged, ECPBPlaybackState, PlaybackState);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPBOnFreeMoveChanged, bool, bFreeMoveEnabled);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCPBOnNarrationChanged, FName, NarrationId, const FText&, SubtitleText);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPBOnNarrationFinished, FName, NarrationId);
