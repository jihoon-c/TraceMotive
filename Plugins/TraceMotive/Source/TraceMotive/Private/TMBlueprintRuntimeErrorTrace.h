#pragma once



#include "Containers/Ticker.h"

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"



class AActor;

class UBlueprint;

class UEdGraphNode;

class SWidgetSwitcher;

enum class ECheckBoxState : uint8;



struct FTMBlueprintRuntimeErrorEntry

{

    FString Signature;

    FString ErrorType;

    FString ErrorMessage;

    FString InstanceName;

    FString InstanceClass;

    FString ObjectPath;

    FString OwnerActorName;

    FString OwnerActorClass;

    FString WorldName;

    FString LevelName;

    FString BlueprintName;

    FString GraphName;

    FString FunctionName;

    FString NodeTitle;

    double FirstSeenSeconds = 0.0;

    double LastSeenSeconds = 0.0;

    int32 Count = 0;

    int32 UniqueGroupCount = 1;

    bool bInstanceSummary = false;



    TWeakObjectPtr<UObject> ActiveObject;

    TWeakObjectPtr<AActor> OwnerActor;

    TWeakObjectPtr<UBlueprint> Blueprint;

    TWeakObjectPtr<UEdGraphNode> GraphNode;

};



struct FTMBlueprintRuntimeErrorClassSummary

{

    FString ClassKey;

    FString ClassName;

    FString BlueprintName;

    FString LastInstanceName;

    FString LastErrorMessage;

    FString LastNodeTitle;

    FString LastFunctionName;

    int32 TotalEventCount = 0;

    int32 UniqueErrorGroupCount = 0;

    int32 UniqueInstanceCount = 0;

    double LastSeenSeconds = 0.0;



    TSet<FString> InstanceNames;

    TWeakObjectPtr<AActor> LastOwnerActor;

    TWeakObjectPtr<UBlueprint> Blueprint;

    TWeakObjectPtr<UEdGraphNode> LastGraphNode;

};



class STMBlueprintRuntimeErrorTrace : public SCompoundWidget

{

public:

    SLATE_BEGIN_ARGS(STMBlueprintRuntimeErrorTrace) {}

    SLATE_END_ARGS()



    void Construct(const FArguments& InArgs);

    virtual ~STMBlueprintRuntimeErrorTrace() override;



    void AddErrorEvent(const FTMBlueprintRuntimeErrorEntry& Event);



private:

    using FErrorEntryPtr = TSharedPtr<FTMBlueprintRuntimeErrorEntry>;

    using FClassSummaryPtr = TSharedPtr<FTMBlueprintRuntimeErrorClassSummary>;



    TSharedRef<ITableRow> GenerateRow(FErrorEntryPtr Item, const TSharedRef<STableViewBase>& OwnerTable);

    TSharedRef<ITableRow> GenerateClassRow(FClassSummaryPtr Item, const TSharedRef<STableViewBase>& OwnerTable);

    FReply Clear();

    FReply CopyAll() const;

    FReply AddToInvestigation() const;

    FReply ToggleClassView();

    FReply ShowLiveTrace();

    FReply ShowCompletedPIELogs();

    void OnInstanceDedupChanged(ECheckBoxState State);

    FReply CopyClassSummary() const;

    FText GetSummaryText() const;

    FText GetLatestText() const;

    FText GetViewToggleText() const;

    FString BuildClipboardText() const;

    FString BuildClassSummaryClipboardText() const;

    void RebuildFilteredEntries();

    void RebuildClassSummaries();

    void RefreshListViews();

    void ApplyErrorEvent(const FTMBlueprintRuntimeErrorEntry& Event);

    void FlushPendingEvents(int32 MaxEventsToProcess);

    bool TickFlushPendingEvents(float DeltaTime);



    TArray<FTMBlueprintRuntimeErrorEntry> PendingEvents;

    TArray<FErrorEntryPtr> Entries;

    TArray<FErrorEntryPtr> FilteredEntries;

    TArray<FClassSummaryPtr> ClassSummaries;

    TMap<FString, FErrorEntryPtr> EntryBySignature;

    TSharedPtr<SListView<FErrorEntryPtr>> ListView;

    TSharedPtr<SListView<FClassSummaryPtr>> ClassListView;

    TSharedPtr<SWidgetSwitcher> ViewSwitcher;

    FTSTicker::FDelegateHandle PendingFlushTickerHandle;

    FString LatestMessage;

    int32 TotalEventCount = 0;

    int32 UniqueInstanceCount = 0;

    // 0: live error groups, 1: live class summary, 2: completed PIE log analysis.
    int32 ActiveViewIndex = 0;

    bool bDeduplicateInstances = false;

};



namespace TMBlueprintRuntimeErrorTrace

{

    void RegisterMenus();

    void UnregisterMenus();

    void OpenWindow();

}

