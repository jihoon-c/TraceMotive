#pragma once



#include "Containers/Ticker.h"

#include "CoreMinimal.h"

#include "Input/Reply.h"

#include "Types/SlateEnums.h"

#include "Widgets/Input/SButton.h"

#include "Widgets/Input/SCheckBox.h"

#include "Widgets/SCompoundWidget.h"

#include "Widgets/Views/STreeView.h"



class AActor;

class UActorComponent;

class ITableRow;

class SExpandableArea;

class SBox;

class SScrollBox;

class STableViewBase;

class STextBlock;

class SWidget;

class SVerticalBox;



class SInstanceReferenceTracker : public SCompoundWidget

{

public:

    SLATE_BEGIN_ARGS(SInstanceReferenceTracker) {}

    SLATE_END_ARGS()



    virtual ~SInstanceReferenceTracker() override;



    void Construct(const FArguments& InArgs, AActor* InTargetActor);

    virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

    virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;



private:

    friend class FInstanceTraceGlobalManager;



    void RunTrackerTick(float InDeltaTime);



    void HandlePreBeginPIE(bool bIsSimulating);

    void HandlePostPIEStarted(bool bIsSimulating);

    void HandleEndPIE(bool bIsSimulating);

    void HandleRenderStateDirty(UActorComponent& Component);

    void HandleObjectPropertyChanged(UObject* Object, struct FPropertyChangedEvent& PropertyChangedEvent);

#if DO_BLUEPRINT_GUARD

    void HandleBlueprintScriptEnter(const struct FBlueprintContextTracker& ContextTracker, const UObject* ContextObject, const UFunction* ContextFunction);

    void HandleBlueprintScriptExit(const struct FBlueprintContextTracker& ContextTracker);

#endif



    AActor* ResolveTargetActor();

    AActor* FindMatchingTargetInWorld(class UWorld* World) const;

    bool IsMatchingTrackedActor(const AActor* Candidate) const;



    struct FTrackedComponentTreeNode

    {

        FString Key;

        FString DisplayName;

        FString TypeName;

        TWeakObjectPtr<UObject> Object;

        TArray<TSharedPtr<FTrackedComponentTreeNode>> Children;

        int32 ChangeCount = 0;

        int32 Depth = 0;

    };



    struct FTrackedStateChangeEvent

    {

        FString NodeKey;

        FString StateKey;

        FString StateLabel;

        FString Category;

        FString Confidence;

        FString Diagnostic;

        FString ActionSummary;

        FString ActionSource;

        FString ActionImpact;

        FString OldValue;

        FString NewValue;

        FString Reason;

        FString CallerSummary;

        FDateTime Timestamp;

        FDateTime LastTimestamp;

        double RelativeSeconds = 0.0;

        double LastRelativeSeconds = 0.0;

        double PieRelativeSeconds = 0.0;

        double LastPieRelativeSeconds = 0.0;

        FString TimelineLabel;

        FString LastTimelineLabel;

        FLinearColor Color = FLinearColor::White;

        FString CoalesceSignature;

        int32 EventId = 0;

        int32 RepeatCount = 1;

    };



    struct FRecentBlueprintExecutionSource

    {

        FString Summary;

        FString InstanceName;

        FString ObjectName;

        FString ComponentName;

        FString ActorName;

        FString ClassName;

        FString FunctionName;

        double TimestampSeconds = 0.0;

    };



    enum class EWatchRuleMode : uint8

    {

        All,

        Visibility,

        Reference,

        Lifecycle,

        Transform

    };



    TSharedRef<SWidget> BuildHeaderBar();

    TSharedRef<SWidget> BuildMainPanel();

    TSharedRef<SWidget> BuildRawLogDrawer();

    TSharedRef<SWidget> BuildMetricCard(const FString& Label, TAttribute<FText> ValueText) const;

    TSharedRef<SWidget> BuildFilterChip(const FString& Label, TAttribute<ECheckBoxState> IsChecked, FOnCheckStateChanged OnChanged) const;

    TSharedRef<SWidget> BuildMoreFilterMenu();

    TSharedRef<SWidget> BuildWatchRuleMenu();

    TSharedRef<SWidget> BuildToolbarButton(const FName IconName, const FString& Label, FOnClicked OnClicked) const;

    TSharedRef<SWidget> BuildChangeOverviewPanel(const TArray<TSharedPtr<FTrackedStateChangeEvent>>& VisibleEvents) const;

    TSharedRef<SWidget> BuildEventCard(const TSharedPtr<FTrackedStateChangeEvent>& Event);

    TSharedRef<SWidget> BuildSimpleEventCard(const TSharedPtr<FTrackedStateChangeEvent>& Event) const;

    TSharedRef<SWidget> BuildEventCardAccentBar(const FLinearColor& AccentColor) const;

    TSharedRef<SWidget> BuildEventCardBody(const TSharedPtr<FTrackedStateChangeEvent>& Event, const FLinearColor& AccentColor, bool bExpanded, bool bTraceVisible);

    TSharedRef<SWidget> BuildEventCardExpandableContent(const TSharedPtr<FTrackedStateChangeEvent>& Event, const FLinearColor& AccentColor, bool bExpanded, bool bTraceVisible);

    TSharedRef<SWidget> BuildEventCardHeader(const TSharedPtr<FTrackedStateChangeEvent>& Event, const FLinearColor& AccentColor) const;

    TSharedRef<SWidget> BuildEventCardSummary(const TSharedPtr<FTrackedStateChangeEvent>& Event, const FLinearColor& AccentColor, bool bTraceVisible) const;

    TSharedRef<SWidget> BuildEventTraceDetails(const TSharedPtr<FTrackedStateChangeEvent>& Event, const FString& ActorMeta, const FString& ClassMeta, const FString& RefMeta) const;

    TSharedRef<SWidget> BuildEventCardActions(const TSharedPtr<FTrackedStateChangeEvent>& Event, bool bExpanded);

    TSharedRef<SWidget> BuildEventMetaCell(const FString& Label, const FString& Value) const;

    TSharedRef<SWidget> BuildEventValueChip(const FString& Value) const;

    FString GetSimpleEventCategoryLabel(const TSharedPtr<FTrackedStateChangeEvent>& Event) const;

    FString GetSimpleEventSourceInstance(const TSharedPtr<FTrackedStateChangeEvent>& Event) const;

    FString GetSimpleEventSourceClass(const TSharedPtr<FTrackedStateChangeEvent>& Event) const;

    FString GetSimpleEventSourceFunction(const TSharedPtr<FTrackedStateChangeEvent>& Event) const;

    FString ExtractEventMetaValue(const FString& Source, const FString& Token) const;

    void ToggleEventExpansion(const TSharedPtr<FTrackedStateChangeEvent>& Event);

    TSharedRef<SWidget> BuildEventFlowSummary(const TArray<TSharedPtr<FTrackedStateChangeEvent>>& VisibleEvents) const;

    TSharedRef<SWidget> BuildConfidenceGraph(const TSharedPtr<FTrackedStateChangeEvent>& Event) const;

    FOptionalSize GetRawLogDrawerHeightOverride() const;

    void UpdateRawLogDrawerHeightOverride();

    void LoadUserLayoutSettings();

    void SaveUserLayoutSettings() const;

    void HandleHierarchySlotResized(float NewSizeCoefficient);

    void HandleEventsSlotResized(float NewSizeCoefficient);

    FReply HandleRawLogResizeMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

    FReply HandleRawLogResizeMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

    FReply HandleRawLogResizeMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

    void CopyDeduplicatedRawLogToClipboard() const;

    FString BuildDeduplicatedRawLogText() const;

    TSharedRef<SWidget> BuildHighlightedLogLine(const FString& Line, const FLinearColor& FallbackColor, const FString& CopyKey) const;



    void RefreshTrackedComponentTree(AActor* Target);

    TSharedRef<ITableRow> GenerateComponentTreeRow(TSharedPtr<FTrackedComponentTreeNode> InNode, const TSharedRef<STableViewBase>& OwnerTable);

    void GetComponentTreeChildren(TSharedPtr<FTrackedComponentTreeNode> InNode, TArray<TSharedPtr<FTrackedComponentTreeNode>>& OutChildren) const;

    void HandleComponentTreeSelectionChanged(TSharedPtr<FTrackedComponentTreeNode> InNode, ESelectInfo::Type SelectInfo);

    void CollectEventsForTreeNode(const TSharedPtr<FTrackedComponentTreeNode>& InNode, TArray<TSharedPtr<FTrackedStateChangeEvent>>& OutEvents) const;

    int32 GetVisibleChangeCountForTreeNode(const TSharedPtr<FTrackedComponentTreeNode>& InNode) const;

    void AddStateChangeEvent(const FString& StateKey, const FString& OldValue, const FString& NewValue, const FString& Reason, const FString& CallerSummary, const FLinearColor& Color);

    bool IsStateChangeEventVisible(const TSharedPtr<FTrackedStateChangeEvent>& Event) const;

    bool DoesEventMatchWatchRule(const TSharedPtr<FTrackedStateChangeEvent>& Event) const;

    void SetWatchRuleMode(EWatchRuleMode InMode);

    FString GetWatchRuleLabel() const;

    void RefreshChangeEventList();

    void RefreshSummary();

    void FocusStateChangeEvent(const TSharedPtr<FTrackedStateChangeEvent>& Event);

    void OpenStateChangeBlueprint(const TSharedPtr<FTrackedStateChangeEvent>& Event);

    void TogglePinnedNode(const FString& NodeKey);

    void IgnoreStateKey(const FString& StateKey);

    void ResetSessionEvents();

    void SaveTraceSnapshot(const FString& Reason) const;

    FString BuildTraceSnapshotMarkdown(const FString& Reason) const;



    void RebuildSnapshot(AActor* Target);

    void StartReferenceSnapshotScan(AActor* Target);

    void ProcessReferenceSnapshotScan(double TickStartSeconds, double TimeBudgetSeconds);

    void FinishReferenceSnapshotScan();

    void CancelReferenceSnapshotScan();

    void AddSnapshotEntry(TMap<TWeakObjectPtr<AActor>, TSet<FString>>& Snapshot, AActor* SourceActor, TSet<FString>&& PropertyPaths) const;

    void DetectTargetStateChanges(AActor* Target);

    void DetectTargetStateChangesFromHook(AActor* Target, const FString& Reason, const FString& RuntimeExecutionSummary);

    void QueueHookStateScan(const FString& Reason, const FString& RuntimeExecutionSummary);

    bool IsTargetOrOwnedComponent(const UObject* Object, AActor* Target) const;

    void RefreshCurrentReferencerList(const TMap<TWeakObjectPtr<AActor>, TSet<FString>>& Snapshot);

    void AppendLogLine(const FString& Message, const FLinearColor& Color);

    void AddReferenceEvent(const FString& Verb, AActor* Referencer, const FString& PropertyPath, const FLinearColor& Color);

    FString BuildActionSummary(AActor* Referencer, const FString& PropertyPath) const;

    FString BuildStateChangeCallerSummary(const FString& StateKey) const;

    FString BuildWatchRuleVisibilityCallerSummary(const FString& StateKey, const FString& NewValue, const FString& CallerSummary) const;

    FString BuildCurrentBlueprintExecutionSummary(const UObject* ChangedObject, const FString& Reason) const;

    FString FindRecentBlueprintExecutionSummary() const;

    FString MergeRuntimeExecutionSummary(const FString& RuntimeExecutionSummary, const FString& CallerSummary) const;

    FString BuildTimelineLabel(double NowSeconds, const FDateTime& NowTimestamp) const;

    FString BuildTimelineStatusText() const;

    void AddTimelineMarkerEvent(const FString& MarkerKey, const FString& OldValue, const FString& NewValue, const FString& Reason, const FLinearColor& Color);

    void RequestDeferredUiRefresh(bool bRefreshEvents, bool bRefreshSummary);

    void FlushDeferredUiRefresh(bool bForce);

    bool HasTrackerTickBudget(double TickStartSeconds, double TimeBudgetSeconds) const;

    void ScanBlueprintGraphForTargetReferences(AActor* SourceActor, AActor* Target, TSet<FString>& OutPropertyPaths, double DeadlineSeconds = 0.0) const;



    static void ScanObjectForTargetReferences(UObject* SourceObject, AActor* TargetActor, TSet<FString>& OutPropertyPaths);

    static void ScanStructForTargetReferences(UStruct* StructType, const void* ContainerPtr, AActor* TargetActor, const FString& PathPrefix, TSet<FString>& OutPropertyPaths, int32 Depth = 0);

    static void ScanPropertyForTargetReferences(FProperty* Property, const void* ValuePtr, AActor* TargetActor, const FString& PropertyPath, TSet<FString>& OutPropertyPaths, int32 Depth = 0);

    static TMap<FString, FString> CaptureTargetState(AActor* Target);

    static FString GetActorLabelSafe(const AActor* Actor);

    static FString GetActorClassNameSafe(const AActor* Actor);



    TWeakObjectPtr<AActor> TargetActor;

    FName InitialActorName;

    FString InitialActorLabel;

    FGuid InitialActorGuid;

    FGuid InitialActorInstanceGuid;

    TWeakObjectPtr<UClass> InitialActorClass;

    TMap<TWeakObjectPtr<AActor>, TSet<FString>> PreviousSnapshot;

    TMap<TWeakObjectPtr<AActor>, TSet<FString>> PendingReferenceSnapshot;

    TMap<FString, FString> PreviousTargetState;

    TArray<TWeakObjectPtr<AActor>> ReferenceScanQueue;

    TArray<FRecentBlueprintExecutionSource> RecentBlueprintExecutionSources;

    TArray<TSharedPtr<FTrackedComponentTreeNode>> ComponentTreeRoots;

    TMap<FString, TSharedPtr<FTrackedComponentTreeNode>> ComponentTreeNodeByKey;

    TMap<FString, TArray<TSharedPtr<FTrackedStateChangeEvent>>> ChangeEventsByNodeKey;

    TArray<TSharedPtr<FTrackedStateChangeEvent>> RecentStateChangeEvents;

    TSet<FString> PinnedNodeKeys;

    TSet<FString> IgnoredStateKeys;

    TSet<int32> ExpandedEventIds;

    mutable TMap<FString, FString> ActionSummaryCache;

    mutable TMap<FString, FString> StateCallerSummaryCache;

    FString LastReferencerListSignature;

    FString LastComponentTreeSignature;

    TSharedPtr<SVerticalBox> CurrentReferencerBox;

    TSharedPtr<STreeView<TSharedPtr<FTrackedComponentTreeNode>>> ComponentTreeView;

    TSharedPtr<FTrackedComponentTreeNode> SelectedComponentTreeNode;

    TSharedPtr<STextBlock> SummaryTextBlock;

    TSharedPtr<SVerticalBox> ChangeEventBox;

    TSharedPtr<SScrollBox> ChangeEventScrollBox;

    TSharedPtr<SBox> RawLogDrawerBox;

    TSharedPtr<SExpandableArea> RawLogDrawer;

    TSharedPtr<STextBlock> RawLogPreviewText;

    TSharedPtr<SScrollBox> LogBox;

    FString EventSearchText;

    FString LastRawLogLine;

    TArray<FString> RawLogCopyOrder;

    TMap<FString, int32> RawLogCopyCounts;

    float StatePollElapsed = 0.0f;

    float ReferenceScanElapsed = 0.0f;

    float TreeRefreshElapsed = 0.0f;

    float HierarchyPanelSplitRatio = 0.30f;

    float RawLogDrawerHeight = 160.0f;

    float RawLogResizeStartHeight = 160.0f;

    FVector2D RawLogResizeStartScreenPosition = FVector2D::ZeroVector;

    int32 LogLineCount = 0;

    int32 NextEventId = 1;

    int32 ReferenceScanQueueIndex = 0;

    int32 PendingHookScanCount = 0;

    double SessionStartSeconds = 0.0;

    double PieStartSeconds = 0.0;

    double PieEndSeconds = 0.0;

    double ReferenceScanStartedSeconds = 0.0;

    double LastHookStateScanSeconds = 0.0;

    double LastDeferredUiRefreshSeconds = 0.0;

    FDateTime PieStartTimestamp;

    FDateTime PieEndTimestamp;

    FString PendingHookReason;

    FString PendingHookRuntimeExecutionSummary;

    TWeakObjectPtr<AActor> ReferenceScanTarget;

    bool bReportedTargetDestroyed = false;

    bool bHasTargetStateSnapshot = false;

    bool bPieActive = false;

    bool bPieStarted = false;

    bool bPieRuntimeBaselineReady = false;

    bool bNeedsReferenceScan = true;

    bool bNeedsTreeRefresh = true;

    bool bReferenceScanInProgress = false;

    bool bPendingReferenceScanRestart = false;

    bool bPendingHookStateScan = false;

    bool bNeedsChangeEventListRefresh = false;

    bool bNeedsSummaryRefresh = false;

    bool bIsResizingRawLogDrawer = false;

    bool bRawLogHeightDirty = false;

    bool bFilterVisibility = true;

    bool bFilterCollision = true;

    bool bFilterTransform = true;

    bool bFilterMaterial = true;

    bool bFilterLifecycle = true;

    bool bFilterReference = true;

    bool bFilterOther = true;

    bool bPinnedOnly = false;

    bool bShowIgnored = false;

    bool bAutoSaveSnapshots = true;

    bool bShowTraceDetails = false;

    EWatchRuleMode WatchRuleMode = EWatchRuleMode::All;

};

