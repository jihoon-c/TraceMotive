#include "TMBlueprintRuntimeErrorTrace.h"
#include "TMInvestigationSession.h"
#include "TMEngineCompatibility.h"
#include "TMPIEErrorLogAnalyzer.h"
#include "TMStyle.h"



#include "TMLocalization.h"



#include "TMReportFormatter.h"
#include "Runtime/Launch/Resources/Version.h"

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
#include "Blueprint/BlueprintExceptionInfo.h"
#endif





#include "Containers/Ticker.h"

#include "Components/ActorComponent.h"

#include "Editor.h"

#include "Editor/EditorEngine.h"

#include "Framework/Docking/TabManager.h"

#include "HAL/PlatformApplicationMisc.h"

#include "Kismet2/BlueprintEditorUtils.h"

#include "Kismet2/KismetDebugUtilities.h"

#include "Kismet2/KismetEditorUtilities.h"

#include "Styling/AppStyle.h"

#include "ToolMenus.h"

#include "UObject/Script.h"

#include "UObject/Stack.h"

#include "Widgets/Docking/SDockTab.h"

#include "Widgets/Input/SButton.h"

#include "Widgets/Input/SCheckBox.h"

#include "Widgets/Images/SImage.h"

#include "Widgets/Layout/SBorder.h"

#include "Widgets/Layout/SBox.h"

#include "Widgets/Layout/SScrollBox.h"

#include "Widgets/Layout/SWidgetSwitcher.h"

#include "Widgets/SBoxPanel.h"

#include "Widgets/Text/STextBlock.h"

#include "Widgets/Views/SListView.h"

#include "Widgets/Views/STableRow.h"

#include "Widgets/Views/STableViewBase.h"



#define LOCTEXT_NAMESPACE "TMBlueprintRuntimeErrorTrace"



namespace

{

    const FName BlueprintRuntimeErrorTraceTabId(TEXT("TraceMotive.BlueprintRuntimeErrorTrace"));

    TWeakPtr<SDockTab> ExistingErrorTraceTab;

    bool bErrorTraceTabSpawnerRegistered = false;

    constexpr float RuntimeErrorFlushIntervalSeconds = 0.05f;

    constexpr int32 RuntimeErrorMaxEventsPerFlush = 64;



    bool IsTrackableRuntimeErrorType(EBlueprintExceptionType::Type Type)

    {

        return Type != EBlueprintExceptionType::Breakpoint

            && Type != EBlueprintExceptionType::Tracepoint

            && Type != EBlueprintExceptionType::WireTracepoint;

    }



    FString GetExceptionTypeText(EBlueprintExceptionType::Type Type)

    {

        switch (Type)

        {

        case EBlueprintExceptionType::Breakpoint:

            return TEXT("Breakpoint");

        case EBlueprintExceptionType::Tracepoint:

            return TEXT("Tracepoint");

        case EBlueprintExceptionType::WireTracepoint:

            return TEXT("Wire Tracepoint");

        case EBlueprintExceptionType::AccessViolation:

            return TEXT("Access Violation");

        case EBlueprintExceptionType::InfiniteLoop:

            return TEXT("Infinite Loop");

        case EBlueprintExceptionType::NonFatalError:

            return TEXT("Non Fatal Error");

        case EBlueprintExceptionType::FatalError:

            return TEXT("Fatal Error");

        case EBlueprintExceptionType::AbortExecution:

            return TEXT("Abort Execution");

        default:

            return TEXT("Blueprint Error");

        }

    }



    FString GetObjectDisplayName(const UObject* Object)

    {

        return Object ? Object->GetName() : FString(TEXT("<unknown>"));

    }



    FString GetClassDisplayName(const UClass* Class)

    {

        if (!Class)

        {

            return TEXT("<unknown>");

        }



        FString ClassName = Class->GetName();

        ClassName.RemoveFromEnd(TEXT("_C"));

        return ClassName;

    }



    FString GetActorLabelSafe(const AActor* Actor)

    {

        return Actor ? Actor->GetActorLabel() : FString(TEXT("<none>"));

    }



    AActor* ResolveOwnerActor(const UObject* Object)

    {

        if (!Object)

        {

            return nullptr;

        }



        if (AActor* Actor = const_cast<AActor*>(Cast<AActor>(Object)))

        {

            return Actor;

        }



        if (const UActorComponent* Component = Cast<UActorComponent>(Object))

        {

            return Component->GetOwner();

        }



        return const_cast<AActor*>(Object->GetTypedOuter<AActor>());

    }



    UWorld* ResolveWorld(const UObject* Object, const AActor* OwnerActor)

    {

        if (OwnerActor)

        {

            return OwnerActor->GetWorld();

        }



        return Object ? Object->GetWorld() : nullptr;

    }



    FString GetLevelName(const AActor* OwnerActor)

    {

        if (!OwnerActor || !OwnerActor->GetLevel())

        {

            return TEXT("<unknown>");

        }



        return OwnerActor->GetLevel()->GetOuter()

            ? OwnerActor->GetLevel()->GetOuter()->GetName()

            : OwnerActor->GetLevel()->GetName();

    }



    UBlueprint* ResolveBlueprintFromContext(const UObject* Object, const UEdGraphNode* GraphNode)

    {

        if (GraphNode)

        {

            if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(GraphNode))

            {

                return Blueprint;

            }

        }



        const UClass* ObjectClass = Object ? Object->GetClass() : nullptr;

        return ObjectClass ? Cast<UBlueprint>(ObjectClass->ClassGeneratedBy) : nullptr;

    }



    UEdGraphNode* ResolveCurrentGraphNode()

    {

        return FKismetDebugUtilities::GetCurrentInstruction();

    }



    FString BuildSignature(const FTMBlueprintRuntimeErrorEntry& Entry)

    {

        const FString NodeKey = Entry.GraphNode.IsValid()

            ? Entry.GraphNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens)

            : Entry.NodeTitle;



        return FString::Printf(TEXT("%s|%s|%s|%s|%s"),

            *Entry.ObjectPath,

            *Entry.OwnerActorName,

            *Entry.FunctionName,

            *NodeKey,

            *Entry.ErrorMessage);

    }



    FString BuildInstanceDedupKey(const FTMBlueprintRuntimeErrorEntry& Entry)

    {

        if (const AActor* OwnerActor = Entry.OwnerActor.Get())

        {

            return OwnerActor->GetPathName();

        }



        if (const UObject* ActiveObject = Entry.ActiveObject.Get())

        {

            return ActiveObject->GetPathName();

        }



        return FString::Printf(TEXT("%s|%s|%s|%s"),

            *Entry.WorldName,

            *Entry.LevelName,

            *Entry.InstanceClass,

            *Entry.InstanceName);

    }



    void MergeEntryIntoInstanceSummary(FTMBlueprintRuntimeErrorEntry& Summary, const FTMBlueprintRuntimeErrorEntry& Entry)

    {

        const int32 ExistingCount = Summary.Count;

        const int32 ExistingGroupCount = Summary.UniqueGroupCount;

        const double ExistingFirstSeenSeconds = Summary.FirstSeenSeconds;



        if (Entry.LastSeenSeconds >= Summary.LastSeenSeconds)

        {

            Summary = Entry;

            Summary.bInstanceSummary = true;

        }



        Summary.Count = ExistingCount + Entry.Count;

        Summary.UniqueGroupCount = ExistingGroupCount + 1;

        Summary.FirstSeenSeconds = FMath::Min(ExistingFirstSeenSeconds, Entry.FirstSeenSeconds);

        Summary.Signature = FString::Printf(TEXT("InstanceSummary|%s"), *BuildInstanceDedupKey(Summary));

    }



    FTMBlueprintRuntimeErrorEntry BuildErrorEntry(const UObject* ActiveObject, const FFrame& StackFrame, const FBlueprintExceptionInfo& Info)

    {

        const UObject* ContextObject = ActiveObject ? ActiveObject : StackFrame.Object;

        AActor* OwnerActor = ResolveOwnerActor(ContextObject);

        UWorld* World = ResolveWorld(ContextObject, OwnerActor);

        UEdGraphNode* GraphNode = ResolveCurrentGraphNode();

        UBlueprint* Blueprint = ResolveBlueprintFromContext(ContextObject, GraphNode);



        FTMBlueprintRuntimeErrorEntry Entry;

        Entry.ErrorType = GetExceptionTypeText(Info.GetType());

        Entry.ErrorMessage = Info.GetDescription().ToString();

        Entry.ActiveObject = const_cast<UObject*>(ContextObject);

        Entry.OwnerActor = OwnerActor;

        Entry.Blueprint = Blueprint;

        Entry.GraphNode = GraphNode;

        Entry.InstanceName = OwnerActor ? GetActorLabelSafe(OwnerActor) : GetObjectDisplayName(ContextObject);

        Entry.InstanceClass = GetClassDisplayName(ContextObject ? ContextObject->GetClass() : nullptr);

        Entry.OwnerActorName = OwnerActor ? GetActorLabelSafe(OwnerActor) : TEXT("<none>");

        Entry.OwnerActorClass = OwnerActor ? GetClassDisplayName(OwnerActor->GetClass()) : TEXT("<none>");

        Entry.ObjectPath = ContextObject ? ContextObject->GetPathName() : TEXT("<unknown>");

        Entry.WorldName = World ? World->GetName() : TEXT("<unknown>");

        Entry.LevelName = GetLevelName(OwnerActor);

        Entry.BlueprintName = Blueprint ? Blueprint->GetName() : TEXT("<unknown>");

        Entry.FunctionName = StackFrame.Node ? StackFrame.Node->GetName() : TEXT("<unknown>");

        Entry.GraphName = GraphNode && GraphNode->GetGraph() ? GraphNode->GetGraph()->GetName() : TEXT("<unknown>");

        Entry.NodeTitle = GraphNode ? GraphNode->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("<unknown>");

        Entry.FirstSeenSeconds = FPlatformTime::Seconds();

        Entry.LastSeenSeconds = Entry.FirstSeenSeconds;

        Entry.Count = 1;

        Entry.UniqueGroupCount = 1;

        Entry.bInstanceSummary = false;

        Entry.Signature = BuildSignature(Entry);

        return Entry;

    }



    FString FormatRelativeSeconds(double Seconds)

    {

        return FString::Printf(TEXT("%.2fs"), Seconds);

    }



    FString FormatEntryForClipboard(const FTMBlueprintRuntimeErrorEntry& Entry)

    {

        return FString::Printf(

            TEXT("[%s] x%d %s\nInstance=%s | InstanceClass=%s | OwnerActor=%s | OwnerClass=%s\nObject=%s\nWorld=%s | Level=%s\nBlueprint=%s | Graph=%s | Function=%s | Node=%s\nMessage=%s"),

            *FormatRelativeSeconds(Entry.LastSeenSeconds),

            Entry.Count,

            *Entry.ErrorType,

            *Entry.InstanceName,

            *Entry.InstanceClass,

            *Entry.OwnerActorName,

            *Entry.OwnerActorClass,

            *Entry.ObjectPath,

            *Entry.WorldName,

            *Entry.LevelName,

            *Entry.BlueprintName,

            *Entry.GraphName,

            *Entry.FunctionName,

            *Entry.NodeTitle,

            *Entry.ErrorMessage);

    }



    FString FormatClassSummaryForClipboard(const FTMBlueprintRuntimeErrorClassSummary& Summary)

    {

        TArray<FString> SortedInstances = Summary.InstanceNames.Array();

        SortedInstances.Sort();



        return FString::Printf(

            TEXT("Class=%s | Blueprint=%s | Events=%d | UniqueErrorGroups=%d | UniqueInstances=%d\nLastInstance=%s | LastFunction=%s | LastNode=%s\nLastMessage=%s\nInstances=%s"),

            *Summary.ClassName,

            *Summary.BlueprintName,

            Summary.TotalEventCount,

            Summary.UniqueErrorGroupCount,

            Summary.UniqueInstanceCount,

            *Summary.LastInstanceName,

            *Summary.LastFunctionName,

            *Summary.LastNodeTitle,

            *Summary.LastErrorMessage,

            *FString::Join(SortedInstances, TEXT(", ")));

    }



    void SelectEntryActor(const FTMBlueprintRuntimeErrorEntry& Entry)

    {

        AActor* Actor = Entry.OwnerActor.Get();

        if (!Actor || !GEditor)

        {

            return;

        }



        GEditor->SelectNone(false, true);

        GEditor->SelectActor(Actor, true, true);

        GEditor->NoteSelectionChange();

    }



    void FocusEntryActor(const FTMBlueprintRuntimeErrorEntry& Entry)

    {

        AActor* Actor = Entry.OwnerActor.Get();

        if (!Actor || !GEditor)

        {

            return;

        }



        GEditor->MoveViewportCamerasToActor(*Actor, true);

    }



    void OpenEntryNode(const FTMBlueprintRuntimeErrorEntry& Entry)

    {

        if (UEdGraphNode* Node = Entry.GraphNode.Get())

        {

            FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Node);

            return;

        }



        if (UBlueprint* Blueprint = Entry.Blueprint.Get())

        {

            GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Blueprint);

        }

    }



    void OpenClassSummarySource(const FTMBlueprintRuntimeErrorClassSummary& Summary)

    {

        if (UEdGraphNode* Node = Summary.LastGraphNode.Get())

        {

            FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Node);

            return;

        }



        if (UBlueprint* Blueprint = Summary.Blueprint.Get())

        {

            GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Blueprint);

        }

    }



    class FTMBlueprintRuntimeErrorTraceManager

    {

    public:

        static FTMBlueprintRuntimeErrorTraceManager& Get()

        {

            static FTMBlueprintRuntimeErrorTraceManager Instance;

            return Instance;

        }



        void AddSubscriber(const TSharedRef<STMBlueprintRuntimeErrorTrace>& Subscriber)

        {

            PruneSubscribers();



            for (const TWeakPtr<STMBlueprintRuntimeErrorTrace>& WeakSubscriber : Subscribers)

            {

                const TSharedPtr<STMBlueprintRuntimeErrorTrace> ExistingSubscriber = WeakSubscriber.Pin();

                if (ExistingSubscriber.Get() == &Subscriber.Get())

                {

                    RegisterHookIfNeeded();

                    return;

                }

            }



            Subscribers.Add(Subscriber);

            RegisterHookIfNeeded();

        }



        void RemoveSubscriber(const STMBlueprintRuntimeErrorTrace* Subscriber)

        {

            Subscribers.RemoveAll([Subscriber](const TWeakPtr<STMBlueprintRuntimeErrorTrace>& WeakSubscriber)

            {

                const TSharedPtr<STMBlueprintRuntimeErrorTrace> ExistingSubscriber = WeakSubscriber.Pin();

                return !ExistingSubscriber.IsValid() || ExistingSubscriber.Get() == Subscriber;

            });



            if (Subscribers.Num() == 0)

            {

                UnregisterHook();

            }

        }



    private:

        void RegisterHookIfNeeded()

        {

            if (ScriptExceptionHandle.IsValid())

            {

                return;

            }



            ScriptExceptionHandle = FBlueprintCoreDelegates::OnScriptException.AddRaw(this, &FTMBlueprintRuntimeErrorTraceManager::HandleScriptException);

        }



        void UnregisterHook()

        {

            if (ScriptExceptionHandle.IsValid())

            {

                FBlueprintCoreDelegates::OnScriptException.Remove(ScriptExceptionHandle);

                ScriptExceptionHandle.Reset();

            }

        }



        void PruneSubscribers()

        {

            Subscribers.RemoveAll([](const TWeakPtr<STMBlueprintRuntimeErrorTrace>& WeakSubscriber)

            {

                return !WeakSubscriber.IsValid();

            });

        }



        void HandleScriptException(const UObject* ActiveObject, const FFrame& StackFrame, const FBlueprintExceptionInfo& Info)

        {

            PruneSubscribers();



            if (Subscribers.Num() == 0)

            {

                UnregisterHook();

                return;

            }



            if (!IsTrackableRuntimeErrorType(Info.GetType()))

            {

                return;

            }



            FTMBlueprintRuntimeErrorEntry Entry = BuildErrorEntry(ActiveObject, StackFrame, Info);

            for (const TWeakPtr<STMBlueprintRuntimeErrorTrace>& WeakSubscriber : Subscribers)

            {

                if (TSharedPtr<STMBlueprintRuntimeErrorTrace> Subscriber = WeakSubscriber.Pin())

                {

                    Subscriber->AddErrorEvent(Entry);

                }

            }

        }



        TArray<TWeakPtr<STMBlueprintRuntimeErrorTrace>> Subscribers;

        FDelegateHandle ScriptExceptionHandle;

    };



    class STMBlueprintRuntimeErrorRow : public SMultiColumnTableRow<TSharedPtr<FTMBlueprintRuntimeErrorEntry>>

    {

    public:

        SLATE_BEGIN_ARGS(STMBlueprintRuntimeErrorRow) {}

            SLATE_ARGUMENT(TSharedPtr<FTMBlueprintRuntimeErrorEntry>, Entry)

        SLATE_END_ARGS()



        void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)

        {

            Entry = InArgs._Entry;

            SMultiColumnTableRow<TSharedPtr<FTMBlueprintRuntimeErrorEntry>>::Construct(

                SMultiColumnTableRow<TSharedPtr<FTMBlueprintRuntimeErrorEntry>>::FArguments()

                    .Padding(FMargin(0.0f, 2.0f)),

                OwnerTable);

        }



        virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override

        {

            if (!Entry.IsValid())

            {

                return SNullWidget::NullWidget;

            }



            if (ColumnName == TEXT("Error"))

            {

                return SNew(SBorder)

                    .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))

                    .Padding(FMargin(8.0f, 6.0f))

                    [

                        SNew(SVerticalBox)

                        + SVerticalBox::Slot().AutoHeight()

                        [

                            SNew(SHorizontalBox)

                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)

                            [

                                SNew(SImage)

                                .Image(FAppStyle::GetBrush("Icons.Error"))

                                .ColorAndOpacity(FLinearColor(1.0f, 0.25f, 0.18f))

                            ]

                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)

                            [

                                SNew(SBorder)

                                .BorderImage(FAppStyle::GetBrush("WhiteBrush"))

                                .BorderBackgroundColor(FLinearColor(0.7f, 0.1f, 0.08f, 0.35f))

                                .Padding(FMargin(6.0f, 2.0f))

                                [

                                    SNew(STextBlock)

                                    .Text(FText::FromString(FString::Printf(TEXT("x%d"), Entry->Count)))

                                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))

                                    .ColorAndOpacity(FLinearColor(1.0f, 0.82f, 0.78f))

                                ]

                            ]

                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)

                            [

                                SNew(SBorder)

                                .Visibility(Entry->bInstanceSummary ? EVisibility::Visible : EVisibility::Collapsed)

                                .BorderImage(FAppStyle::GetBrush("WhiteBrush"))

                                .BorderBackgroundColor(FLinearColor(0.1f, 0.35f, 0.75f, 0.35f))

                                .Padding(FMargin(6.0f, 2.0f))

                                [

                                    SNew(STextBlock)

                                    .Text(FText::FromString(FString::Printf(TEXT("%d groups"), Entry->UniqueGroupCount)))

                                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))

                                    .ColorAndOpacity(FLinearColor(0.78f, 0.88f, 1.0f))

                                ]

                            ]

                            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

                            [

                                SNew(STextBlock)

                                .Text(FText::FromString(Entry->bInstanceSummary

                                    ? FString::Printf(TEXT("Latest: %s"), *Entry->ErrorMessage)

                                    : Entry->ErrorMessage))

                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))

                                .ColorAndOpacity(FLinearColor(1.0f, 0.88f, 0.84f))

                                .AutoWrapText(true)

                            ]

                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(26.0f, 4.0f, 0.0f, 0.0f)

                        [

                            SNew(STextBlock)

                            .Text(FText::FromString(Entry->bInstanceSummary

                                ? FString::Printf(TEXT("%s | %s | %s | instance dedup merged %d unique error groups"), *Entry->BlueprintName, *Entry->GraphName, *Entry->NodeTitle, Entry->UniqueGroupCount)

                                : FString::Printf(TEXT("%s | %s | %s"), *Entry->BlueprintName, *Entry->GraphName, *Entry->NodeTitle)))

                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))

                            .ColorAndOpacity(FLinearColor(0.68f, 0.74f, 0.82f))

                            .AutoWrapText(true)

                        ]

                    ];

            }



            if (ColumnName == TEXT("Instance"))

            {

                return SNew(SBorder)

                    .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))

                    .Padding(FMargin(8.0f, 6.0f))

                    [

                        SNew(SVerticalBox)

                        + SVerticalBox::Slot().AutoHeight()

                        [

                            SNew(STextBlock)

                            .Text(FText::FromString(Entry->InstanceName))

                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))

                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)

                        [

                            SNew(STextBlock)

                            .Text(FText::FromString(FString::Printf(TEXT("Class: %s | Owner: %s"), *Entry->InstanceClass, *Entry->OwnerActorName)))

                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))

                            .ColorAndOpacity(FLinearColor(0.66f, 0.70f, 0.76f))

                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)

                        [

                            SNew(STextBlock)

                            .Visibility(Entry->bInstanceSummary ? EVisibility::Visible : EVisibility::Collapsed)

                            .Text(FText::FromString(FString::Printf(TEXT("Instance filter: %d events across %d error groups"), Entry->Count, Entry->UniqueGroupCount)))

                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))

                            .ColorAndOpacity(FLinearColor(0.58f, 0.72f, 0.92f))

                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)

                        [

                            SNew(STextBlock)

                            .Text(FText::FromString(FString::Printf(TEXT("World: %s | Level: %s"), *Entry->WorldName, *Entry->LevelName)))

                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))

                            .ColorAndOpacity(FLinearColor(0.54f, 0.58f, 0.64f))

                        ]

                    ];

            }



            if (ColumnName == TEXT("Actions"))

            {

                return SNew(SBorder)

                    .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))

                    .Padding(FMargin(8.0f, 6.0f))

                    [

                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)

                        [

                            SNew(SButton)

                            .Text(TMLoc::Text(TEXT("Select"), TEXT("선택")))

                            .ToolTipText(TMLoc::Text(TEXT("Select the runtime owner actor instance in the editor."), TEXT("에디터에서 런타임 Owner 액터 인스턴스를 선택합니다.")))

                            .IsEnabled(Entry->OwnerActor.IsValid())

                            .OnClicked_Lambda([Entry = Entry]()

                            {

                                SelectEntryActor(*Entry);

                                return FReply::Handled();

                            })

                        ]

                        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)

                        [

                            SNew(SButton)

                            .Text(TMLoc::Text(TEXT("Focus"), TEXT("포커스")))

                            .ToolTipText(TMLoc::Text(TEXT("Move the active viewport camera to the runtime owner actor."), TEXT("활성 뷰포트 카메라를 런타임 Owner 액터로 이동합니다.")))

                            .IsEnabled(Entry->OwnerActor.IsValid())

                            .OnClicked_Lambda([Entry = Entry]()

                            {

                                FocusEntryActor(*Entry);

                                return FReply::Handled();

                            })

                        ]

                        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)

                        [

                            SNew(SButton)

                            .Text(TMLoc::Text(TEXT("Node"), TEXT("노드")))

                            .ToolTipText(TMLoc::Text(TEXT("Open the matching Blueprint node when available."), TEXT("가능한 경우 일치하는 Blueprint 노드를 엽니다.")))

                            .OnClicked_Lambda([Entry = Entry]()

                            {

                                OpenEntryNode(*Entry);

                                return FReply::Handled();

                            })

                        ]

                        + SHorizontalBox::Slot().AutoWidth()

                        [

                            SNew(SButton)

                            .Text(TMLoc::Text(TEXT("Copy"), TEXT("복사")))

                            .ToolTipText(TMLoc::Text(TEXT("Copy this error's debug info."), TEXT("이 오류의 디버그 정보를 복사합니다.")))

                            .OnClicked_Lambda([Entry = Entry]()

                            {

                                const FString Text = FormatEntryForClipboard(*Entry);

                                FPlatformApplicationMisc::ClipboardCopy(*Text);

                                return FReply::Handled();

                            })

                        ]

                    ];

            }



            return SNullWidget::NullWidget;

        }



    private:

        TSharedPtr<FTMBlueprintRuntimeErrorEntry> Entry;

    };



    class STMBlueprintRuntimeErrorClassRow : public SMultiColumnTableRow<TSharedPtr<FTMBlueprintRuntimeErrorClassSummary>>

    {

    public:

        SLATE_BEGIN_ARGS(STMBlueprintRuntimeErrorClassRow) {}

            SLATE_ARGUMENT(TSharedPtr<FTMBlueprintRuntimeErrorClassSummary>, Summary)

        SLATE_END_ARGS()



        void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)

        {

            Summary = InArgs._Summary;

            SMultiColumnTableRow<TSharedPtr<FTMBlueprintRuntimeErrorClassSummary>>::Construct(

                SMultiColumnTableRow<TSharedPtr<FTMBlueprintRuntimeErrorClassSummary>>::FArguments()

                    .Padding(FMargin(0.0f, 2.0f)),

                OwnerTable);

        }



        virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override

        {

            if (!Summary.IsValid())

            {

                return SNullWidget::NullWidget;

            }



            if (ColumnName == TEXT("Class"))

            {

                return SNew(SBorder)

                    .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))

                    .Padding(FMargin(8.0f, 6.0f))

                    [

                        SNew(SVerticalBox)

                        + SVerticalBox::Slot().AutoHeight()

                        [

                            SNew(SHorizontalBox)

                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)

                            [

                                SNew(SImage)

                                .Image(FAppStyle::GetBrush("ClassIcon.Blueprint"))

                                .ColorAndOpacity(FLinearColor(0.35f, 0.65f, 1.0f))

                            ]

                            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

                            [

                                SNew(STextBlock)

                                .Text(FText::FromString(Summary->ClassName))

                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))

                                .ColorAndOpacity(FLinearColor::White)

                            ]

                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(24.0f, 3.0f, 0.0f, 0.0f)

                        [

                            SNew(STextBlock)

                            .Text(FText::FromString(FString::Printf(TEXT("Blueprint: %s | Last Node: %s"), *Summary->BlueprintName, *Summary->LastNodeTitle)))

                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))

                            .ColorAndOpacity(FLinearColor(0.66f, 0.72f, 0.80f))

                            .AutoWrapText(true)

                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(24.0f, 2.0f, 0.0f, 0.0f)

                        [

                            SNew(STextBlock)

                            .Text(FText::FromString(Summary->LastErrorMessage))

                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))

                            .ColorAndOpacity(FLinearColor(0.92f, 0.78f, 0.74f))

                            .AutoWrapText(true)

                        ]

                    ];

            }



            if (ColumnName == TEXT("Stats"))

            {

                return SNew(SBorder)

                    .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))

                    .Padding(FMargin(8.0f, 6.0f))

                    [

                        SNew(SVerticalBox)

                        + SVerticalBox::Slot().AutoHeight()

                        [

                            SNew(STextBlock)

                            .Text(FText::FromString(FString::Printf(TEXT("%d events"), Summary->TotalEventCount)))

                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))

                            .ColorAndOpacity(FLinearColor(1.0f, 0.82f, 0.78f))

                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)

                        [

                            SNew(STextBlock)

                            .Text(FText::FromString(FString::Printf(TEXT("%d error groups | %d instances"), Summary->UniqueErrorGroupCount, Summary->UniqueInstanceCount)))

                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))

                            .ColorAndOpacity(FLinearColor(0.68f, 0.72f, 0.78f))

                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)

                        [

                            SNew(STextBlock)

                            .Text(FText::FromString(FString::Printf(TEXT("Last instance: %s"), *Summary->LastInstanceName)))

                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))

                            .ColorAndOpacity(FLinearColor(0.56f, 0.61f, 0.68f))

                            .AutoWrapText(true)

                        ]

                    ];

            }



            if (ColumnName == TEXT("Actions"))

            {

                return SNew(SBorder)

                    .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))

                    .Padding(FMargin(8.0f, 6.0f))

                    [

                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)

                        [

                            SNew(SButton)

                            .Text(TMLoc::Text(TEXT("Open"), TEXT("열기")))

                            .ToolTipText(TMLoc::Text(TEXT("Open the most recent Blueprint node for this class, or the Blueprint asset."), TEXT("이 클래스의 가장 최근 Blueprint 노드 또는 Blueprint 에셋을 엽니다.")))

                            .OnClicked_Lambda([Summary = Summary]()

                            {

                                OpenClassSummarySource(*Summary);

                                return FReply::Handled();

                            })

                        ]

                        + SHorizontalBox::Slot().AutoWidth()

                        [

                            SNew(SButton)

                            .Text(TMLoc::Text(TEXT("Copy"), TEXT("복사")))

                            .ToolTipText(TMLoc::Text(TEXT("Copy this class summary."), TEXT("이 클래스 요약을 복사합니다.")))

                            .OnClicked_Lambda([Summary = Summary]()

                            {

                                const FString Text = FormatClassSummaryForClipboard(*Summary);

                                FPlatformApplicationMisc::ClipboardCopy(*Text);

                                return FReply::Handled();

                            })

                        ]

                    ];

            }



            return SNullWidget::NullWidget;

        }



    private:

        TSharedPtr<FTMBlueprintRuntimeErrorClassSummary> Summary;

    };



    TSharedRef<SDockTab> SpawnErrorTraceTab(const FSpawnTabArgs&)

    {

        TSharedRef<SDockTab> Tab = SNew(SDockTab)

            .TabRole(ETabRole::NomadTab)

            .Label(TMLoc::Text(TEXT("Runtime Error Investigation"), TEXT("런타임 오류 분석")))

            .OnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([](TSharedRef<SDockTab>)

            {

                ExistingErrorTraceTab.Reset();

            }))

            [

                SNew(STMBlueprintRuntimeErrorTrace)

            ];



        ExistingErrorTraceTab = Tab;

        return Tab;

    }



    void RegisterErrorTraceTabSpawner()

    {

        if (bErrorTraceTabSpawnerRegistered)

        {

            return;

        }



        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(

            BlueprintRuntimeErrorTraceTabId,

            FOnSpawnTab::CreateStatic(&SpawnErrorTraceTab))

            .SetDisplayName(TMLoc::Text(TEXT("Runtime Error Investigation"), TEXT("런타임 오류 분석")))

            .SetTooltipText(TMLoc::Text(TEXT("Track Blueprint runtime errors with live PIE instance information."), TEXT("PIE 인스턴스 정보와 함께 블루프린트 런타임 오류를 추적합니다.")))

            .SetIcon(FSlateIcon(TMStyle::GetStyleSetName(), "TraceMotive.RuntimeErrorTrace"));



        bErrorTraceTabSpawnerRegistered = true;

    }

}



void STMBlueprintRuntimeErrorTrace::Construct(const FArguments& InArgs)

{

    ChildSlot

    [

        SNew(SBorder)

        .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))

        .Padding(10.0f)

        [

            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight()

            [

                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

                [

                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()

                    [

                        SNew(STextBlock)

                        .Text(TMLoc::Text(TEXT("Runtime Error Investigation"), TEXT("런타임 오류 분석")))

                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)

                    [

                        SNew(STextBlock)

                        .Text(this, &STMBlueprintRuntimeErrorTrace::GetSummaryText)

                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))

                        .ColorAndOpacity(FLinearColor(0.68f, 0.72f, 0.78f))

                    ]

                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)

                [

                    SNew(SButton)

                    .Text(TMLoc::Text(TEXT("Add to Investigation"), TEXT("조사에 추가")))

                    .OnClicked(this, &STMBlueprintRuntimeErrorTrace::AddToInvestigation)

                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)

                [

                    SNew(SCheckBox)

                    .ToolTipText(TMLoc::Text(TEXT("Show only one latest row per runtime instance. Counts still include repeated events and merged error groups."), TEXT("런타임 인스턴스마다 최신 행 하나만 표시합니다. 카운트에는 반복 이벤트와 병합된 오류 그룹이 계속 포함됩니다.")))

                    .IsChecked_Lambda([this]()

                    {

                        return bDeduplicateInstances ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;

                    })

                    .OnCheckStateChanged(this, &STMBlueprintRuntimeErrorTrace::OnInstanceDedupChanged)

                    [

                        SNew(STextBlock)

                        .Text(TMLoc::Text(TEXT("Dedup Instances"), TEXT("인스턴스 중복 제거")))

                    ]

                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)

                [

                    SNew(SButton)

                    .Text(TMLoc::Text(TEXT("Live Trace"), TEXT("실시간 추적")))

                    .ToolTipText(TMLoc::Text(TEXT("Show errors captured while PIE is running."), TEXT("PIE 실행 중 캡처한 오류를 봅니다.")))

                    .OnClicked(this, &STMBlueprintRuntimeErrorTrace::ShowLiveTrace)

                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)

                [

                    SNew(SButton)

                    .Text(TMLoc::Text(TEXT("Completed PIE Logs"), TEXT("완료된 PIE 로그")))

                    .ToolTipText(TMLoc::Text(TEXT("Analyze the latest completed PIE log when live capture was not open."), TEXT("실시간 추적을 열지 못했을 때 최근 완료된 PIE 로그를 분석합니다.")))

                    .OnClicked(this, &STMBlueprintRuntimeErrorTrace::ShowCompletedPIELogs)

                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)

                [

                    SNew(SButton)

                    .Text(this, &STMBlueprintRuntimeErrorTrace::GetViewToggleText)

                    .ToolTipText(TMLoc::Text(TEXT("Toggle between detailed error groups and deduplicated error classes."), TEXT("상세 오류 그룹 보기와 중복 제거된 오류 클래스 보기를 전환합니다.")))

                    .OnClicked(this, &STMBlueprintRuntimeErrorTrace::ToggleClassView)

                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)

                [

                    SNew(SButton)

                    .Text(TMLoc::Text(TEXT("Copy Classes"), TEXT("클래스 복사")))

                    .ToolTipText(TMLoc::Text(TEXT("Copy the deduplicated class summary."), TEXT("중복 제거된 클래스 요약을 복사합니다.")))

                    .OnClicked(this, &STMBlueprintRuntimeErrorTrace::CopyClassSummary)

                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)

                [

                    SNew(SButton)

                    .Text(TMLoc::Text(TEXT("Copy All"), TEXT("전체 복사")))

                    .OnClicked(this, &STMBlueprintRuntimeErrorTrace::CopyAll)

                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                [

                    SNew(SButton)

                    .Text(TMLoc::Text(TEXT("Clear"), TEXT("지우기")))

                    .OnClicked(this, &STMBlueprintRuntimeErrorTrace::Clear)

                ]

            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 8.0f)

            [

                SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush("WhiteBrush"))

                .Visibility_Lambda([this]()

                {

                    return ActiveViewIndex == 2 ? EVisibility::Collapsed : EVisibility::Visible;

                })

                .BorderBackgroundColor(FLinearColor(0.08f, 0.08f, 0.08f, 0.55f))

                .Padding(FMargin(8.0f, 5.0f))

                [

                    SNew(STextBlock)

                    .Text(this, &STMBlueprintRuntimeErrorTrace::GetLatestText)

                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))

                    .ColorAndOpacity(FLinearColor(0.78f, 0.82f, 0.88f))

                    .AutoWrapText(true)

                ]

            ]

            + SVerticalBox::Slot().FillHeight(1.0f)

            [

                SAssignNew(ViewSwitcher, SWidgetSwitcher)

                .WidgetIndex_Lambda([this]()

                {

                    return ActiveViewIndex;

                })

                + SWidgetSwitcher::Slot()

                [

                    SAssignNew(ListView, SListView<FErrorEntryPtr>)

                    .ListItemsSource(&FilteredEntries)

                    .SelectionMode(ESelectionMode::Single)

                    .HeaderRow

                    (

                        SNew(SHeaderRow)

                        + SHeaderRow::Column(TEXT("Error"))

                        .DefaultLabel(TMLoc::Text(TEXT("Error"), TEXT("오류")))

                        .FillWidth(0.46f)

                        + SHeaderRow::Column(TEXT("Instance"))

                        .DefaultLabel(TMLoc::Text(TEXT("Runtime Instance"), TEXT("런타임 인스턴스")))

                        .FillWidth(0.34f)

                        + SHeaderRow::Column(TEXT("Actions"))

                        .DefaultLabel(TMLoc::Text(TEXT("Actions"), TEXT("동작")))

                        .FillWidth(0.20f)

                    )

                    .OnGenerateRow(this, &STMBlueprintRuntimeErrorTrace::GenerateRow)

                ]

                + SWidgetSwitcher::Slot()

                [

                    SAssignNew(ClassListView, SListView<FClassSummaryPtr>)

                    .ListItemsSource(&ClassSummaries)

                    .SelectionMode(ESelectionMode::Single)

                    .HeaderRow

                    (

                        SNew(SHeaderRow)

                        + SHeaderRow::Column(TEXT("Class"))

                        .DefaultLabel(TMLoc::Text(TEXT("Class"), TEXT("Class")))

                        .FillWidth(0.50f)

                        + SHeaderRow::Column(TEXT("Stats"))

                        .DefaultLabel(TMLoc::Text(TEXT("Stats"), TEXT("통계")))

                        .FillWidth(0.30f)

                        + SHeaderRow::Column(TEXT("Actions"))

                        .DefaultLabel(TMLoc::Text(TEXT("Actions"), TEXT("동작")))

                        .FillWidth(0.20f)

                    )

                    .OnGenerateRow(this, &STMBlueprintRuntimeErrorTrace::GenerateClassRow)

                ]

                + SWidgetSwitcher::Slot()

                [

                    TMPIEErrorLogAnalyzer::CreatePanel()

                ]

            ]

        ]

    ];



    FTMBlueprintRuntimeErrorTraceManager::Get().AddSubscriber(SharedThis(this));

    PendingFlushTickerHandle = FTSTicker::GetCoreTicker().AddTicker(

        FTickerDelegate::CreateSP(this, &STMBlueprintRuntimeErrorTrace::TickFlushPendingEvents),

        RuntimeErrorFlushIntervalSeconds);

}



STMBlueprintRuntimeErrorTrace::~STMBlueprintRuntimeErrorTrace()

{

    if (PendingFlushTickerHandle.IsValid())

    {

        FTSTicker::GetCoreTicker().RemoveTicker(PendingFlushTickerHandle);

        PendingFlushTickerHandle.Reset();

    }



    FTMBlueprintRuntimeErrorTraceManager::Get().RemoveSubscriber(this);

}



void STMBlueprintRuntimeErrorTrace::AddErrorEvent(const FTMBlueprintRuntimeErrorEntry& Event)

{

    const int32 FirstCandidate = FMath::Max(0, PendingEvents.Num() - 64);
    for (int32 Index = PendingEvents.Num() - 1; Index >= FirstCandidate; --Index)
    {
        if (PendingEvents[Index].Signature == Event.Signature)
        {
            PendingEvents[Index].Count += FMath::Max(1, Event.Count);
            PendingEvents[Index].LastSeenSeconds = Event.LastSeenSeconds;
            PendingEvents[Index].ActiveObject = Event.ActiveObject;
            PendingEvents[Index].OwnerActor = Event.OwnerActor;
            LatestMessage = FString::Printf(TEXT("%s on %s.%s | %s (grouped)"),
                *Event.ErrorType, *Event.InstanceName, *Event.FunctionName, *Event.ErrorMessage);
            return;
        }
    }

    PendingEvents.Add(Event);
    if (PendingEvents.Num() > 256)
    {
        TMEngineCompatibility::RemoveAtNoShrink(PendingEvents, 0, PendingEvents.Num() - 256);
    }

    LatestMessage = FString::Printf(TEXT("%s on %s.%s | %s"),

        *Event.ErrorType,

        *Event.InstanceName,

        *Event.FunctionName,

        *Event.ErrorMessage);

}



void STMBlueprintRuntimeErrorTrace::ApplyErrorEvent(const FTMBlueprintRuntimeErrorEntry& Event)

{

    TotalEventCount++;

    LatestMessage = FString::Printf(TEXT("%s on %s.%s | %s"),

        *Event.ErrorType,

        *Event.InstanceName,

        *Event.FunctionName,

        *Event.ErrorMessage);



    if (FErrorEntryPtr* ExistingEntryPtr = EntryBySignature.Find(Event.Signature))

    {

        FErrorEntryPtr ExistingEntry = *ExistingEntryPtr;

        ExistingEntry->Count += FMath::Max(1, Event.Count);

        ExistingEntry->UniqueGroupCount = 1;

        ExistingEntry->bInstanceSummary = false;

        ExistingEntry->LastSeenSeconds = Event.LastSeenSeconds;

        ExistingEntry->ActiveObject = Event.ActiveObject;

        ExistingEntry->OwnerActor = Event.OwnerActor;

        ExistingEntry->Blueprint = Event.Blueprint;

        ExistingEntry->GraphNode = Event.GraphNode;



        Entries.Remove(ExistingEntry);

        Entries.Insert(ExistingEntry, 0);

    }

    else

    {

        FErrorEntryPtr NewEntry = MakeShared<FTMBlueprintRuntimeErrorEntry>(Event);

        Entries.Insert(NewEntry, 0);

        EntryBySignature.Add(Event.Signature, NewEntry);

    }

}



void STMBlueprintRuntimeErrorTrace::FlushPendingEvents(int32 MaxEventsToProcess)

{

    if (PendingEvents.IsEmpty())

    {

        return;

    }



    const int32 EventsToProcess = FMath::Min(PendingEvents.Num(), MaxEventsToProcess);

    for (int32 Index = 0; Index < EventsToProcess; ++Index)

    {

        ApplyErrorEvent(PendingEvents[Index]);

    }



    TMEngineCompatibility::RemoveAtNoShrink(PendingEvents, 0, EventsToProcess);



    RebuildFilteredEntries();

    RebuildClassSummaries();

    RefreshListViews();

}



bool STMBlueprintRuntimeErrorTrace::TickFlushPendingEvents(float /*DeltaTime*/)

{

    FlushPendingEvents(RuntimeErrorMaxEventsPerFlush);

    return true;

}



TSharedRef<ITableRow> STMBlueprintRuntimeErrorTrace::GenerateRow(FErrorEntryPtr Item, const TSharedRef<STableViewBase>& OwnerTable)

{

    return SNew(STMBlueprintRuntimeErrorRow, OwnerTable)

        .Entry(Item);

}



TSharedRef<ITableRow> STMBlueprintRuntimeErrorTrace::GenerateClassRow(FClassSummaryPtr Item, const TSharedRef<STableViewBase>& OwnerTable)

{

    return SNew(STMBlueprintRuntimeErrorClassRow, OwnerTable)

        .Summary(Item);

}



FReply STMBlueprintRuntimeErrorTrace::Clear()

{

    PendingEvents.Empty();

    Entries.Empty();

    FilteredEntries.Empty();

    ClassSummaries.Empty();

    EntryBySignature.Empty();

    TotalEventCount = 0;

    UniqueInstanceCount = 0;

    LatestMessage.Reset();



    RefreshListViews();



    return FReply::Handled();

}



FReply STMBlueprintRuntimeErrorTrace::CopyAll() const

{

    const FString Text = BuildClipboardText();

    FPlatformApplicationMisc::ClipboardCopy(*Text);

    return FReply::Handled();

}

FReply STMBlueprintRuntimeErrorTrace::AddToInvestigation() const
{
    TMInvestigationSession::RecordEvidence(
        TEXT("Runtime Error Investigation"),
        FString::Printf(TEXT("%d event(s), %d unique instance(s), latest: %s"), TotalEventCount, UniqueInstanceCount, *LatestMessage));
    TMInvestigationSession::OpenWindow();
    return FReply::Handled();
}



FReply STMBlueprintRuntimeErrorTrace::ToggleClassView()

{

    ActiveViewIndex = ActiveViewIndex == 1 ? 0 : 1;

    return FReply::Handled();

}



FReply STMBlueprintRuntimeErrorTrace::ShowLiveTrace()

{

    ActiveViewIndex = 0;

    return FReply::Handled();

}



FReply STMBlueprintRuntimeErrorTrace::ShowCompletedPIELogs()

{

    ActiveViewIndex = 2;

    TMPIEErrorLogAnalyzer::RefreshPanel();

    return FReply::Handled();

}



void STMBlueprintRuntimeErrorTrace::OnInstanceDedupChanged(ECheckBoxState State)

{

    bDeduplicateInstances = (State == ECheckBoxState::Checked);

    RebuildFilteredEntries();

    RefreshListViews();

}



FReply STMBlueprintRuntimeErrorTrace::CopyClassSummary() const

{

    const FString Text = BuildClassSummaryClipboardText();

    FPlatformApplicationMisc::ClipboardCopy(*Text);

    return FReply::Handled();

}



FText STMBlueprintRuntimeErrorTrace::GetSummaryText() const

{

    if (ActiveViewIndex == 2)

    {

        return TMLoc::Text(

            TEXT("Completed PIE log analysis reads Saved/Logs. Instance and function recovery is evidence-based inference."),

            TEXT("완료된 PIE 로그 분석은 Saved/Logs를 읽습니다. 인스턴스와 함수 복원 결과는 근거 기반 추정입니다."));

    }

    return FText::FromString(FString::Printf(

        TEXT("%s %s: %d | %s: %d | %s: %d | %s: %d | %s: %d%s"),

        *TMLoc::String(TEXT("Listening while this window is open."), TEXT("이 창이 열려 있는 동안 추적합니다.")),

        *TMLoc::String(TEXT("Events"), TEXT("이벤트")),

        TotalEventCount,

        *TMLoc::String(TEXT("Unique groups"), TEXT("고유 그룹")),

        Entries.Num(),

        *TMLoc::String(TEXT("Instances"), TEXT("인스턴스")),

        UniqueInstanceCount,

        *TMLoc::String(TEXT("Showing"), TEXT("표시")),

        FilteredEntries.Num(),

        *TMLoc::String(TEXT("Classes"), TEXT("클래스")),

        ClassSummaries.Num(),

        bDeduplicateInstances ? *FString::Printf(TEXT(" | %s"), *TMLoc::String(TEXT("Instance dedup on"), TEXT("인스턴스 중복 제거 켜짐"))) : TEXT("")));

}



FText STMBlueprintRuntimeErrorTrace::GetLatestText() const

{

    return LatestMessage.IsEmpty()

        ? TMLoc::Text(TEXT("No Blueprint runtime errors captured yet. Start PIE with this window open."), TEXT("아직 캡처된 블루프린트 런타임 오류가 없습니다. 이 창을 연 상태로 PIE를 시작하세요."))

        : FText::FromString(FString::Printf(TEXT("%s %s"), *TMLoc::String(TEXT("Latest:"), TEXT("최신:")), *LatestMessage));

}



FText STMBlueprintRuntimeErrorTrace::GetViewToggleText() const

{

    return ActiveViewIndex == 1

        ? TMLoc::Text(TEXT("Show Errors"), TEXT("오류 보기"))

        : TMLoc::Text(TEXT("Show Classes"), TEXT("클래스 보기"));

}



FString STMBlueprintRuntimeErrorTrace::BuildClipboardText() const

{

    TArray<FString> Lines;

    Lines.Add(TEXT("# Runtime Error Investigation - Live Trace"));

    Lines.Add(FString::Printf(TEXT("- Total Events: %d"), TotalEventCount));

    Lines.Add(FString::Printf(TEXT("- Unique Groups: %d"), Entries.Num()));

    Lines.Add(FString::Printf(TEXT("- Unique Instances: %d"), UniqueInstanceCount));

    Lines.Add(FString::Printf(TEXT("- Instance Dedup Filter: %s"), bDeduplicateInstances ? TEXT("On") : TEXT("Off")));

    Lines.Add(FString::Printf(TEXT("- Visible Rows: %d"), FilteredEntries.Num()));

    Lines.Add(TEXT(""));



    const TArray<FErrorEntryPtr>& SourceEntries = FilteredEntries;

    for (const FErrorEntryPtr& Entry : SourceEntries)

    {

        if (Entry.IsValid())

        {

            Lines.Add(FormatEntryForClipboard(*Entry));

            Lines.Add(TEXT(""));

        }

    }



    TArray<TMReportFormatter::FMetadataItem> Metadata;

    Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Total Events"), FString::FromInt(TotalEventCount)));

    Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Unique Groups"), FString::FromInt(Entries.Num())));

    Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Unique Instances"), FString::FromInt(UniqueInstanceCount)));

    Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Visible Rows"), FString::FromInt(FilteredEntries.Num())));

    Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Instance Dedup Filter"), bDeduplicateInstances ? TEXT("On") : TEXT("Off")));



    return TMReportFormatter::BuildWrappedLegacyReport(

        TEXT("Runtime Error Investigation - Live Trace"),

        FString::Printf(TEXT("- Total events: %d\n- Unique groups: %d\n- Unique instances: %d\n- Visible rows: %d\n- Confidence: instance/node matching is best-effort and may be incomplete for dynamic runtime contexts."), TotalEventCount, Entries.Num(), UniqueInstanceCount, FilteredEntries.Num()),

        FString::Join(Lines, TEXT("\n")),

        Metadata);

}



FString STMBlueprintRuntimeErrorTrace::BuildClassSummaryClipboardText() const

{

    TArray<FString> Lines;

    Lines.Add(TEXT("# Blueprint Runtime Error Classes"));

    Lines.Add(FString::Printf(TEXT("- Total Events: %d"), TotalEventCount));

    Lines.Add(FString::Printf(TEXT("- Unique Classes: %d"), ClassSummaries.Num()));

    Lines.Add(TEXT(""));



    for (const FClassSummaryPtr& Summary : ClassSummaries)

    {

        if (Summary.IsValid())

        {

            Lines.Add(FormatClassSummaryForClipboard(*Summary));

            Lines.Add(TEXT(""));

        }

    }



    TArray<TMReportFormatter::FMetadataItem> Metadata;

    Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Total Events"), FString::FromInt(TotalEventCount)));

    Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Unique Classes"), FString::FromInt(ClassSummaries.Num())));



    return TMReportFormatter::BuildWrappedLegacyReport(

        TEXT("Blueprint Runtime Error Classes"),

        FString::Printf(TEXT("- Total events: %d\n- Unique classes: %d\n- Confidence: class grouping is based on collected runtime error entries."), TotalEventCount, ClassSummaries.Num()),

        FString::Join(Lines, TEXT("\n")),

        Metadata);

}



void STMBlueprintRuntimeErrorTrace::RebuildFilteredEntries()

{

    TSet<FString> UniqueInstanceKeys;

    for (const FErrorEntryPtr& Entry : Entries)

    {

        if (Entry.IsValid())

        {

            UniqueInstanceKeys.Add(BuildInstanceDedupKey(*Entry));

        }

    }

    UniqueInstanceCount = UniqueInstanceKeys.Num();



    if (!bDeduplicateInstances)

    {

        FilteredEntries = Entries;

        return;

    }



    TMap<FString, FErrorEntryPtr> SummaryByInstance;

    TArray<FString> OrderedInstanceKeys;



    for (const FErrorEntryPtr& Entry : Entries)

    {

        if (!Entry.IsValid())

        {

            continue;

        }



        const FString InstanceKey = BuildInstanceDedupKey(*Entry);

        if (FErrorEntryPtr* ExistingSummary = SummaryByInstance.Find(InstanceKey))

        {

            MergeEntryIntoInstanceSummary(**ExistingSummary, *Entry);

        }

        else

        {

            FErrorEntryPtr NewSummary = MakeShared<FTMBlueprintRuntimeErrorEntry>(*Entry);

            NewSummary->Signature = FString::Printf(TEXT("InstanceSummary|%s"), *InstanceKey);

            NewSummary->UniqueGroupCount = 1;

            NewSummary->bInstanceSummary = true;

            SummaryByInstance.Add(InstanceKey, NewSummary);

            OrderedInstanceKeys.Add(InstanceKey);

        }

    }



    FilteredEntries.Empty(OrderedInstanceKeys.Num());

    for (const FString& InstanceKey : OrderedInstanceKeys)

    {

        if (FErrorEntryPtr* Summary = SummaryByInstance.Find(InstanceKey))

        {

            FilteredEntries.Add(*Summary);

        }

    }



    FilteredEntries.Sort([](const FErrorEntryPtr& Left, const FErrorEntryPtr& Right)

    {

        if (!Left.IsValid() || !Right.IsValid())

        {

            return Left.IsValid();

        }



        return Left->LastSeenSeconds > Right->LastSeenSeconds;

    });

}



void STMBlueprintRuntimeErrorTrace::RebuildClassSummaries()

{

    TMap<FString, FClassSummaryPtr> SummaryByClass;



    for (const FErrorEntryPtr& Entry : Entries)

    {

        if (!Entry.IsValid())

        {

            continue;

        }



        const FString ClassName = !Entry->OwnerActorClass.IsEmpty() && Entry->OwnerActorClass != TEXT("<none>")

            ? Entry->OwnerActorClass

            : Entry->InstanceClass;

        const FString BlueprintName = Entry->BlueprintName;

        const FString ClassKey = FString::Printf(TEXT("%s|%s"), *ClassName, *BlueprintName);



        FClassSummaryPtr Summary;

        if (FClassSummaryPtr* ExistingSummary = SummaryByClass.Find(ClassKey))

        {

            Summary = *ExistingSummary;

        }

        else

        {

            Summary = MakeShared<FTMBlueprintRuntimeErrorClassSummary>();

            Summary->ClassKey = ClassKey;

            Summary->ClassName = ClassName;

            Summary->BlueprintName = BlueprintName;

            SummaryByClass.Add(ClassKey, Summary);

        }



        Summary->TotalEventCount += Entry->Count;

        Summary->UniqueErrorGroupCount++;

        Summary->InstanceNames.Add(Entry->InstanceName);



        if (Entry->LastSeenSeconds >= Summary->LastSeenSeconds)

        {

            Summary->LastSeenSeconds = Entry->LastSeenSeconds;

            Summary->LastInstanceName = Entry->InstanceName;

            Summary->LastErrorMessage = Entry->ErrorMessage;

            Summary->LastNodeTitle = Entry->NodeTitle;

            Summary->LastFunctionName = Entry->FunctionName;

            Summary->LastOwnerActor = Entry->OwnerActor;

            Summary->Blueprint = Entry->Blueprint;

            Summary->LastGraphNode = Entry->GraphNode;

        }

    }



    ClassSummaries.Empty(SummaryByClass.Num());

    for (const TPair<FString, FClassSummaryPtr>& Pair : SummaryByClass)

    {

        if (Pair.Value.IsValid())

        {

            Pair.Value->UniqueInstanceCount = Pair.Value->InstanceNames.Num();

            ClassSummaries.Add(Pair.Value);

        }

    }



    ClassSummaries.Sort([](const FClassSummaryPtr& Left, const FClassSummaryPtr& Right)

    {

        if (!Left.IsValid() || !Right.IsValid())

        {

            return Left.IsValid();

        }



        if (Left->TotalEventCount != Right->TotalEventCount)

        {

            return Left->TotalEventCount > Right->TotalEventCount;

        }



        return Left->ClassName < Right->ClassName;

    });

}



void STMBlueprintRuntimeErrorTrace::RefreshListViews()

{

    if (ListView.IsValid())

    {

        ListView->RequestListRefresh();

    }



    if (ClassListView.IsValid())

    {

        ClassListView->RequestListRefresh();

    }

}



namespace TMBlueprintRuntimeErrorTrace

{

    void RegisterMenus()

    {

        RegisterErrorTraceTabSpawner();



        if (UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window"))

        {

            FToolMenuSection& Section = WindowMenu->FindOrAddSection("TraceMotive");

            Section.AddMenuEntry(

                "TMOpenBlueprintRuntimeErrorTrace",

                TMLoc::Text(TEXT("Runtime Error Investigation"), TEXT("런타임 오류 분석")),

                TMLoc::Text(TEXT("Investigate live Blueprint errors and completed PIE logs in one workspace."), TEXT("실시간 블루프린트 오류와 완료된 PIE 로그를 한 화면에서 분석합니다.")),

                FSlateIcon(TMStyle::GetStyleSetName(), "TraceMotive.RuntimeErrorTrace"),

                FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&)

                {

                    TMBlueprintRuntimeErrorTrace::OpenWindow();

                })

            );

        }

    }



    void UnregisterMenus()

    {

        if (bErrorTraceTabSpawnerRegistered)

        {

            FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(BlueprintRuntimeErrorTraceTabId);

            bErrorTraceTabSpawnerRegistered = false;

        }



        ExistingErrorTraceTab.Reset();

    }



    void OpenWindow()

    {

        RegisterErrorTraceTabSpawner();



        if (ExistingErrorTraceTab.IsValid())

        {

            FGlobalTabmanager::Get()->TryInvokeTab(BlueprintRuntimeErrorTraceTabId);

            return;

        }



        ExistingErrorTraceTab = FGlobalTabmanager::Get()->TryInvokeTab(BlueprintRuntimeErrorTraceTabId);

    }

}



#undef LOCTEXT_NAMESPACE




