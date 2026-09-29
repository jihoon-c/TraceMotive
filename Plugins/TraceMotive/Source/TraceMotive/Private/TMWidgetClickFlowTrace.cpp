#include "TMWidgetClickFlowTrace.h"
#include "TMLocalization.h"
#include "TMStyle.h"

#include "TMTraceNoisePolicy.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Widget.h"
#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/PlatformTime.h"
#include "Layout/WidgetPath.h"
#include "Misc/ScopeLock.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "ToolMenus.h"
#include "UObject/Script.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Slate/SObjectWidget.h"

namespace
{
    const FName WidgetClickFlowTabId(TEXT("TraceMotive.WidgetClickFlowTrace"));
    constexpr double SynchronousFlowWindowSeconds = 0.45;
    constexpr double AsyncWatchWindowSeconds = 10.0;
    constexpr int32 MaxFlowEntries = 160;
    constexpr int32 MaxDelegateReceivers = 32;
    bool bWidgetClickFlowTabRegistered = false;
    TWeakPtr<SDockTab> WidgetClickFlowExistingTab;

    struct FWidgetFlowEntry
    {
        double TimeSeconds = 0.0;
        int32 Depth = 0;
        FString ObjectName;
        FString ClassName;
        FString FunctionName;
        FString Kind;
    };

    class SWidgetClickFlowTrace;

    class FWidgetClickFlowInputObserver final : public IInputProcessor
    {
    public:
        explicit FWidgetClickFlowInputObserver(SWidgetClickFlowTrace& InOwner)
            : Owner(InOwner)
        {
        }

        virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
        virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
        virtual const TCHAR* GetDebugName() const override { return TEXT("TMWidgetClickFlowTrace"); }

    private:
        SWidgetClickFlowTrace& Owner;
    };

    class SWidgetClickFlowTrace : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SWidgetClickFlowTrace) {}
        SLATE_END_ARGS()

        ~SWidgetClickFlowTrace() override
        {
            StopCapture();
            RemoveInputObserver();
        }

        void Construct(const FArguments&)
        {
            ChildSlot
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
                .Padding(10.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(TMLoc::Text(TEXT("Widget Click Flow Trace"), TEXT("Widget Click Flow Trace")))
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15))
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 8)
                    [
                        SNew(STextBlock)
                        .Text(TMLoc::Text(TEXT("Arms one PIE click, captures its direct Blueprint flow, then watches owner callbacks and bound OnClicked delegates for up to 10 seconds. Tick-like calls and repeats are filtered."), TEXT("Arms one PIE click, captures its direct Blueprint flow, then watches owner callbacks and bound OnClicked delegates for up to 10 seconds. Tick-like calls and repeats are filtered.")))
                        .AutoWrapText(true)
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
                        [
                            SNew(SButton)
                            .Text_Lambda([this]() { return FText::FromString(bArmed ? TEXT("Waiting for PIE click...") : TEXT("Trace next UMG click")); })
                            .OnClicked(this, &SWidgetClickFlowTrace::ArmNextClick)
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SButton)
                            .Text(TMLoc::Text(TEXT("Clear"), TEXT("Clear")))
                            .OnClicked(this, &SWidgetClickFlowTrace::Clear)
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 4)
                    [
                        SAssignNew(StatusText, STextBlock)
                        .Text(this, &SWidgetClickFlowTrace::GetStatusText)
                        .AutoWrapText(true)
                        .ColorAndOpacity(FLinearColor(0.66f, 0.72f, 0.82f))
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                    [
                        SAssignNew(TargetText, STextBlock)
                        .Text(this, &SWidgetClickFlowTrace::GetTargetText)
                        .AutoWrapText(true)
                        .ColorAndOpacity(FLinearColor(0.78f, 0.86f, 0.96f))
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SSeparator)
                    ]
                    + SVerticalBox::Slot().FillHeight(1.0f).Padding(0, 8, 0, 0)
                    [
                        SAssignNew(FlowBox, SScrollBox)
                    ]
                ]
            ];

            RefreshFlow();
        }

        virtual void Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime) override
        {
            SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);

            if (bCapturing && FPlatformTime::Seconds() >= CaptureEndSeconds)
            {
                StopCapture();
                Status = Entries.Num() > 0
                    ? FString::Printf(TEXT("Capture complete: %d entries. Direct flow and target-owned or bound OnClicked delegate callbacks were observed for up to 10 seconds."), Entries.Num())
                    : TEXT("No Blueprint entry was captured. The widget may use native handling, a delayed callback, or no Blueprint event.");
                bDirty = true;
            }

            if (bDirty && FPlatformTime::Seconds() - LastUiRefreshSeconds >= 0.20)
            {
                RefreshFlow();
            }
        }

        FReply ArmNextClick()
        {
            StopCapture();
            bArmed = true;
            Status = TEXT("Armed. Click a UMG widget in the PIE window once.");
            InstallInputObserver();
            bDirty = true;
            return FReply::Handled();
        }

        FReply Clear()
        {
            StopCapture();
            RemoveInputObserver();
            bArmed = false;
            Entries.Reset();
            TargetWidget.Reset();
            TargetOwner.Reset();
            Status = TEXT("Ready. Click 'Trace next UMG click' to start a short, target-scoped capture.");
            bDirty = true;
            return FReply::Handled();
        }

        void HandlePointer(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
        {
            if (!bArmed)
            {
                return;
            }

            UUserWidget* Owner = nullptr;
            UWidget* Widget = ResolveWidgetUnderPointer(SlateApp, MouseEvent, Owner);
            if (!Widget || !Owner)
            {
                Status = TEXT("The clicked Slate path did not resolve to a live UMG widget. Try clicking a visible widget inside PIE.");
                bDirty = true;
                return;
            }

            BeginCapture(Widget, Owner);
        }

    private:
        static bool IsWidgetOwnedBy(const UObject* Object, const UUserWidget* Owner)
        {
            if (!Object || !Owner)
            {
                return false;
            }

            if (Object == Owner)
            {
                return true;
            }

            if (const UWidget* Widget = Cast<UWidget>(Object))
            {
                return Widget->GetTypedOuter<UUserWidget>() == Owner;
            }

            return Object->GetTypedOuter<UUserWidget>() == Owner;
        }

        bool IsBoundDelegateReceiver(const UObject* Object) const
        {
            if (!Object)
            {
                return false;
            }

            for (const TWeakObjectPtr<UObject>& Receiver : DelegateReceivers)
            {
                if (Receiver.Get() == Object)
                {
                    return true;
                }
            }
            return false;
        }

        void CollectBoundDelegateReceivers(UWidget* Widget)
        {
            DelegateReceivers.Reset();
            const UButton* Button = Cast<UButton>(Widget);
            if (!Button)
            {
                return;
            }

            for (UObject* Receiver : Button->OnClicked.GetAllObjects())
            {
                if (Receiver && DelegateReceivers.Num() < MaxDelegateReceivers)
                {
                    DelegateReceivers.AddUnique(Receiver);
                }
            }
        }

        static UWidget* ResolveWidgetUnderPointer(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent, UUserWidget*& OutOwner)
        {
            OutOwner = nullptr;
            const FWidgetPath Path = SlateApp.LocateWindowUnderMouse(
                MouseEvent.GetScreenSpacePosition(),
                SlateApp.GetInteractiveTopLevelWindows(),
                false,
                MouseEvent.GetUserIndex());

            if (!Path.IsValid())
            {
                return nullptr;
            }

            for (int32 PathIndex = Path.Widgets.Num() - 1; PathIndex >= 0; --PathIndex)
            {
                const TSharedRef<SWidget> SlateWidget = Path.Widgets[PathIndex].Widget;
                if (SlateWidget->GetType() == FName(TEXT("SObjectWidget")))
                {
                    const TSharedRef<SObjectWidget> ObjectWidget = StaticCastSharedRef<SObjectWidget>(SlateWidget);
                    OutOwner = Cast<UUserWidget>(ObjectWidget->GetWidgetObject());
                    if (OutOwner)
                    {
                        break;
                    }
                }
            }

            if (!OutOwner || !OutOwner->WidgetTree)
            {
                return nullptr;
            }

            TArray<UWidget*> Widgets;
            OutOwner->WidgetTree->GetAllWidgets(Widgets);
            for (int32 PathIndex = Path.Widgets.Num() - 1; PathIndex >= 0; --PathIndex)
            {
                const TSharedPtr<SWidget> PathWidget = Path.Widgets[PathIndex].Widget;
                for (UWidget* Candidate : Widgets)
                {
                    if (Candidate && Candidate->GetCachedWidget().Get() == PathWidget.Get())
                    {
                        return Candidate;
                    }
                }
            }

            return OutOwner;
        }

        void BeginCapture(UWidget* Widget, UUserWidget* Owner)
        {
            bArmed = false;
            RemoveInputObserver();
            TargetWidget = Widget;
            TargetOwner = Owner;
            Entries.Reset();
            CollectBoundDelegateReceivers(Widget);
            bFlowStarted = false;
            LastFlowEntrySeconds = 0.0;
            CaptureStartedSeconds = FPlatformTime::Seconds();
            CaptureEndSeconds = CaptureStartedSeconds + AsyncWatchWindowSeconds;
            bCapturing = true;
            Status = DelegateReceivers.Num() > 0
                ? FString::Printf(TEXT("Capturing direct flow, then %d bound OnClicked delegate receiver(s), for up to 10 seconds..."), DelegateReceivers.Num())
                : TEXT("Capturing direct flow, then target-owned async callbacks for up to 10 seconds...");

            if (Cast<UButton>(Widget))
            {
                AddEntry(TEXT("UI event"), Owner, nullptr, 0, TEXT("UButton click captured; waiting for OnClicked Blueprint flow."));
            }
            else
            {
                AddEntry(TEXT("UI event"), Owner, nullptr, 0, TEXT("Widget pointer event captured; waiting for Blueprint flow."));
            }

#if DO_BLUEPRINT_GUARD
            BlueprintEnterHandle = FBlueprintContextTracker::OnEnterScriptContext.AddSP(SharedThis(this), &SWidgetClickFlowTrace::HandleBlueprintEnter);
#endif
            bDirty = true;
        }

        void StopCapture()
        {
            bCapturing = false;
#if DO_BLUEPRINT_GUARD
            if (BlueprintEnterHandle.IsValid())
            {
                FBlueprintContextTracker::OnEnterScriptContext.Remove(BlueprintEnterHandle);
                BlueprintEnterHandle.Reset();
            }
#endif
        }

        void InstallInputObserver()
        {
            RemoveInputObserver();
            if (FSlateApplication::IsInitialized())
            {
                InputObserver = MakeShared<FWidgetClickFlowInputObserver>(*this);
                FSlateApplication::Get().RegisterInputPreProcessor(InputObserver, 0);
            }
        }

        void RemoveInputObserver()
        {
            if (InputObserver.IsValid() && FSlateApplication::IsInitialized())
            {
                FSlateApplication::Get().UnregisterInputPreProcessor(InputObserver);
            }
            InputObserver.Reset();
        }

#if DO_BLUEPRINT_GUARD
        void HandleBlueprintEnter(const FBlueprintContextTracker& Tracker, const UObject* Object, const UFunction* Function)
        {
            if (!IsInGameThread() || !bCapturing || !Object || !Function)
            {
                return;
            }

            const double Now = FPlatformTime::Seconds();
            if (Now > CaptureEndSeconds || Entries.Num() >= MaxFlowEntries)
            {
                return;
            }

            const FString FunctionName = Function->GetName();
            if (TMTraceNoise::IsTickLike(FunctionName) || FunctionName.Contains(TEXT("Evaluate"), ESearchCase::IgnoreCase))
            {
                return;
            }

            const bool bOwnedByTarget = IsWidgetOwnedBy(Object, TargetOwner.Get());
            const bool bBoundDelegateReceiver = IsBoundDelegateReceiver(Object);
            const bool bInSynchronousWindow = Now - CaptureStartedSeconds <= SynchronousFlowWindowSeconds;
            if (!bFlowStarted && !bOwnedByTarget)
            {
                return;
            }

            if (bFlowStarted && !bOwnedByTarget && !bBoundDelegateReceiver
                && (!bInSynchronousWindow || Now - LastFlowEntrySeconds > 0.10))
            {
                return;
            }

            const TArrayView<const FFrame* const> Stack = Tracker.GetCurrentScriptStack();
            const int32 Depth = FMath::Clamp(Stack.Num() - 1, 0, 24);
            const FString Signature = FString::Printf(TEXT("%s|%s|%d"), *Object->GetPathName(), *FunctionName, Depth);
            if (Signature == LastSignature && Now - LastFlowEntrySeconds < 0.02)
            {
                return;
            }

            bFlowStarted = true;
            LastFlowEntrySeconds = Now;
            LastSignature = Signature;
            const FString Kind = bBoundDelegateReceiver
                ? TEXT("Delegate callback")
                : (bOwnedByTarget && !bInSynchronousWindow ? TEXT("Async owner callback") : (bOwnedByTarget ? TEXT("Blueprint") : TEXT("Follow-up")));
            AddEntry(Kind, Object, Function, Depth, FString());
            bDirty = true;
        }
#endif

        void AddEntry(const FString& Kind, const UObject* Object, const UFunction* Function, int32 Depth, const FString& OverrideFunction)
        {
            TSharedPtr<FWidgetFlowEntry> Entry = MakeShared<FWidgetFlowEntry>();
            Entry->TimeSeconds = FPlatformTime::Seconds() - CaptureStartedSeconds;
            Entry->Depth = Depth;
            Entry->Kind = Kind;
            Entry->ObjectName = Object ? Object->GetName() : TEXT("-");
            Entry->ClassName = Object && Object->GetClass() ? Object->GetClass()->GetName() : TEXT("-");
            Entry->FunctionName = !OverrideFunction.IsEmpty() ? OverrideFunction : (Function ? Function->GetName() : TEXT("-"));
            Entries.Add(Entry);
        }

        FText GetStatusText() const { return FText::FromString(Status); }

        FText GetTargetText() const
        {
            if (!TargetWidget.IsValid())
            {
                return TMLoc::Text(TEXT("Target: -"), TEXT("Target: -"));
            }
            const UWidget* Widget = TargetWidget.Get();
            const UUserWidget* Owner = TargetOwner.Get();
            return FText::FromString(FString::Printf(TEXT("Target: %s (%s) | Owner: %s"), *Widget->GetName(), *Widget->GetClass()->GetName(), Owner ? *Owner->GetClass()->GetName() : TEXT("-")));
        }

        void RefreshFlow()
        {
            if (!FlowBox.IsValid())
            {
                return;
            }

            FlowBox->ClearChildren();
            if (Entries.Num() == 0)
            {
                FlowBox->AddSlot()
                [
                    SNew(STextBlock)
                    .Text(TMLoc::Text(TEXT("No flow captured yet."), TEXT("No flow captured yet.")))
                    .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                ];
            }

            for (const TSharedPtr<FWidgetFlowEntry>& Entry : Entries)
            {
                if (!Entry.IsValid())
                {
                    continue;
                }
                const float LeftPadding = 4.0f + Entry->Depth * 12.0f;
                const FLinearColor Color = Entry->Kind == TEXT("UI event")
                    ? FLinearColor(0.35f, 0.78f, 1.0f)
                    : (Entry->Kind == TEXT("Delegate callback") ? FLinearColor(0.98f, 0.74f, 0.28f)
                    : (Entry->Kind == TEXT("Async owner callback") ? FLinearColor(0.86f, 0.58f, 0.98f)
                    : (Entry->Kind == TEXT("Follow-up") ? FLinearColor(0.98f, 0.74f, 0.28f) : FLinearColor(0.70f, 0.90f, 0.70f))));
                FlowBox->AddSlot().Padding(LeftPadding, 0, 4, 5)
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
                    .Padding(6.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(TEXT("[%0.3fs] %s | %s.%s"), Entry->TimeSeconds, *Entry->Kind, *Entry->ObjectName, *Entry->FunctionName)))
                        .AutoWrapText(true)
                        .ColorAndOpacity(Color)
                    ]
                ];
            }
            LastUiRefreshSeconds = FPlatformTime::Seconds();
            bDirty = false;
        }

        TSharedPtr<FWidgetClickFlowInputObserver> InputObserver;
        FDelegateHandle BlueprintEnterHandle;
        TWeakObjectPtr<UWidget> TargetWidget;
        TWeakObjectPtr<UUserWidget> TargetOwner;
        TArray<TWeakObjectPtr<UObject>> DelegateReceivers;
        TArray<TSharedPtr<FWidgetFlowEntry>> Entries;
        TSharedPtr<SScrollBox> FlowBox;
        TSharedPtr<STextBlock> StatusText;
        TSharedPtr<STextBlock> TargetText;
        FString Status = TEXT("Ready. Click 'Trace next UMG click' to start a short, target-scoped capture.");
        FString LastSignature;
        double CaptureStartedSeconds = 0.0;
        double CaptureEndSeconds = 0.0;
        double LastFlowEntrySeconds = 0.0;
        double LastUiRefreshSeconds = 0.0;
        bool bArmed = false;
        bool bCapturing = false;
        bool bFlowStarted = false;
        bool bDirty = false;
    };

    bool FWidgetClickFlowInputObserver::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
    {
        Owner.HandlePointer(SlateApp, MouseEvent);
        return false;
    }

    TSharedRef<SDockTab> SpawnWidgetClickFlowTab(const FSpawnTabArgs&)
    {
        TSharedRef<SDockTab> Tab = SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            .Label(TMLoc::Text(TEXT("Widget Click Flow Trace"), TEXT("Widget Click Flow Trace")))
            .OnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([](TSharedRef<SDockTab>) { WidgetClickFlowExistingTab.Reset(); }))
            [
                SNew(SWidgetClickFlowTrace)
            ];
        WidgetClickFlowExistingTab = Tab;
        return Tab;
    }

    void RegisterWidgetClickFlowTabSpawner()
    {
        if (bWidgetClickFlowTabRegistered)
        {
            return;
        }
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(WidgetClickFlowTabId, FOnSpawnTab::CreateStatic(&SpawnWidgetClickFlowTab))
            .SetDisplayName(TMLoc::Text(TEXT("Widget Click Flow Trace"), TEXT("Widget Click Flow Trace")))
            .SetTooltipText(TMLoc::Text(TEXT("Capture the short Blueprint function flow triggered by one PIE UMG click."), TEXT("Capture the short Blueprint function flow triggered by one PIE UMG click.")))
            .SetIcon(FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.WidgetClickFlowTrace")));
        bWidgetClickFlowTabRegistered = true;
    }
}

namespace TMWidgetClickFlowTrace
{
    void RegisterMenus()
    {
        RegisterWidgetClickFlowTabSpawner();
        auto AddEntry = [](UToolMenu* Menu, const FName Name)
        {
            if (!Menu)
            {
                return;
            }
            Menu->FindOrAddSection(TEXT("TraceMotive")).AddMenuEntry(
                Name,
                TMLoc::Text(TEXT("Widget Click Flow Trace"), TEXT("Widget Click Flow Trace")),
                TMLoc::Text(TEXT("Click a PIE UMG widget once and inspect the short Blueprint function flow it triggers."), TEXT("Click a PIE UMG widget once and inspect the short Blueprint function flow it triggers.")),
                FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.WidgetClickFlowTrace")),
                FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&) { TMWidgetClickFlowTrace::OpenWindow(); }));
        };
        AddEntry(UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window")), TEXT("TMOpenWidgetClickFlowTrace"));
        AddEntry(UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools")), TEXT("TMToolsOpenWidgetClickFlowTrace"));
    }

    void UnregisterMenus()
    {
        if (bWidgetClickFlowTabRegistered)
        {
            FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(WidgetClickFlowTabId);
            bWidgetClickFlowTabRegistered = false;
        }
        WidgetClickFlowExistingTab.Reset();
    }

    void OpenWindow()
    {
        RegisterWidgetClickFlowTabSpawner();
        FGlobalTabmanager::Get()->TryInvokeTab(WidgetClickFlowTabId);
    }
}

