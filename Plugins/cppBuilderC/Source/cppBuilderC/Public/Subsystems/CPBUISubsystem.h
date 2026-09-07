#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/CPBUIDelegates.h"
#include "CPBUISubsystem.generated.h"

class UCPBMainWidget;

UCLASS()
class CPPBUILDERC_API UCPBUISubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "CPB|UI")
	FCPBOnSubtitleChanged OnSubtitleChanged;

	UPROPERTY(BlueprintAssignable, Category = "CPB|UI")
	FCPBOnMenuActionRequested OnMenuActionRequested;

	UPROPERTY(BlueprintAssignable, Category = "CPB|UI")
	FCPBOnSidePanelTabChanged OnSidePanelTabChanged;

	UPROPERTY(BlueprintAssignable, Category = "CPB|UI")
	FCPBOnSidePanelItemsChanged OnSidePanelItemsChanged;

	UPROPERTY(BlueprintAssignable, Category = "CPB|UI")
	FCPBOnSequenceListChanged OnSequenceListChanged;

	UPROPERTY(BlueprintAssignable, Category = "CPB|UI")
	FCPBOnPlaybackStateChanged OnPlaybackStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "CPB|UI")
	FCPBOnFreeMoveChanged OnFreeMoveChanged;

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void RegisterMainWidget(UCPBMainWidget* InMainWidget);

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void UnregisterMainWidget(UCPBMainWidget* InMainWidget);

	UFUNCTION(BlueprintPure, Category = "CPB|UI")
	UCPBMainWidget* GetMainWidget() const { return MainWidget.Get(); }

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void SetSubtitle(const FText& InSubtitleText);

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void ClearSubtitle();

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void RequestMenuAction(ECPBMenuAction MenuAction);

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void SetActiveSidePanelTab(ECPBSidePanelTab InActiveTab);

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void SetSidePanelItems(const TArray<FCPBSidePanelItem>& InItems);

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void SetSequenceListItems(const TArray<FCPBSequenceListItem>& InItems);

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void SetPlaybackState(ECPBPlaybackState InPlaybackState);

	UFUNCTION(BlueprintCallable, Category = "CPB|UI")
	void SetFreeMoveEnabled(bool bInFreeMoveEnabled);

	UFUNCTION(BlueprintPure, Category = "CPB|UI")
	FText GetCurrentSubtitle() const { return CurrentSubtitle; }

	UFUNCTION(BlueprintPure, Category = "CPB|UI")
	ECPBSidePanelTab GetActiveSidePanelTab() const { return ActiveSidePanelTab; }

	UFUNCTION(BlueprintPure, Category = "CPB|UI")
	ECPBPlaybackState GetPlaybackState() const { return PlaybackState; }

	UFUNCTION(BlueprintPure, Category = "CPB|UI")
	bool IsFreeMoveEnabled() const { return bFreeMoveEnabled; }

	UFUNCTION(BlueprintPure, Category = "CPB|UI")
	TArray<FCPBSidePanelItem> GetSidePanelItems() const { return SidePanelItems; }

	UFUNCTION(BlueprintPure, Category = "CPB|UI")
	TArray<FCPBSequenceListItem> GetSequenceListItems() const { return SequenceListItems; }

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UCPBMainWidget> MainWidget;

	UPROPERTY(Transient)
	FText CurrentSubtitle;

	UPROPERTY(Transient)
	ECPBSidePanelTab ActiveSidePanelTab = ECPBSidePanelTab::Parts;

	UPROPERTY(Transient)
	ECPBPlaybackState PlaybackState = ECPBPlaybackState::Stopped;

	UPROPERTY(Transient)
	bool bFreeMoveEnabled = false;

	UPROPERTY(Transient)
	TArray<FCPBSidePanelItem> SidePanelItems;

	UPROPERTY(Transient)
	TArray<FCPBSequenceListItem> SequenceListItems;
};
