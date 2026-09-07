#include "TMWidgetLifecycleTrace.h"
#include "TMPerformanceGuard.h"
#include "TMLocalization.h"
#include "TMStyle.h"
#include "TMTraceNoisePolicy.h"

#include "TMReportFormatter.h"



#include "Blueprint/UserWidget.h"

#include "Blueprint/WidgetTree.h"

#include "Components/Widget.h"

#include "Editor.h"

#include "Engine/World.h"

#include "Framework/Docking/TabManager.h"

#include "HAL/PlatformApplicationMisc.h"

#include "HAL/PlatformTime.h"

#include "Misc/DateTime.h"

#include "Misc/PackageName.h"

#include "Styling/AppStyle.h"

#include "ToolMenus.h"

#include "UObject/Script.h"

#include "UObject/Stack.h"

#include "UObject/UObjectIterator.h"

#include "Widgets/Docking/SDockTab.h"

#include "Widgets/Input/SButton.h"

#include "Widgets/Input/SEditableTextBox.h"

#include "Widgets/Layout/SBorder.h"

#include "Widgets/Layout/SScrollBox.h"

#include "Widgets/Layout/SSeparator.h"

#include "Widgets/Layout/SSplitter.h"

#include "Widgets/SBoxPanel.h"

#include "Widgets/SCompoundWidget.h"

#include "Widgets/Text/STextBlock.h"



namespace

{

const FName WidgetTraceTabId(TEXT("TraceMotive.WidgetLifecycleTrace"));

TWeakPtr<SDockTab> ExistingTab;

bool bTabRegistered = false;



uint64 ObjKey(const UObject* Obj){ return reinterpret_cast<uint64>(Obj); }

FSlateFontInfo Font(const FName Style, int32 Size){ return FCoreStyle::GetDefaultFontStyle(Style, Size); }

FString ObjName(const UObject* Obj){ return Obj ? Obj->GetName() : FString(TEXT("<None>")); }

FString ClassName(const UObject* Obj){ return Obj && Obj->GetClass() ? Obj->GetClass()->GetName() : FString(TEXT("<No Class>")); }

FString FuncName(const UFunction* Fn)

{

    if (!Fn) return FString();

    FString Name = Fn->GetName();

    if (Name == TEXT("ReceiveBeginPlay")) return TEXT("BeginPlay");

    if (Name == TEXT("UserConstructionScript")) return TEXT("ConstructionScript");

    return Name;

}

FString WorldType(const UWorld* World)

{

    if (!World) return TEXT("<No World>");

    switch (World->WorldType)

    {

    case EWorldType::PIE: return TEXT("PIE");

    case EWorldType::Editor: return TEXT("Editor");

    case EWorldType::Game: return TEXT("Game");

    case EWorldType::EditorPreview: return TEXT("EditorPreview");

    default: return TEXT("Other");

    }

}

FString VisibilityText(ESlateVisibility V)

{

    switch (V)

    {

    case ESlateVisibility::Visible: return TEXT("Visible");

    case ESlateVisibility::Collapsed: return TEXT("Collapsed");

    case ESlateVisibility::Hidden: return TEXT("Hidden");

    case ESlateVisibility::HitTestInvisible: return TEXT("HitTestInvisible");

    case ESlateVisibility::SelfHitTestInvisible: return TEXT("SelfHitTestInvisible");

    default: return TEXT("Unknown");

    }

}

bool IsOn(ESlateVisibility V)

{

    return V == ESlateVisibility::Visible || V == ESlateVisibility::HitTestInvisible || V == ESlateVisibility::SelfHitTestInvisible;

}

FString WidgetFile(const UUserWidget* Widget)

{

    if (!Widget || !Widget->GetClass()) return TEXT("<No Widget>");

    if (const UObject* GeneratedBy = Widget->GetClass()->ClassGeneratedBy)

    {

        return FPackageName::GetShortName(GeneratedBy->GetOutermost()->GetName());

    }

    return Widget->GetClass()->GetName();

}

FString WidgetPath(const UUserWidget* Widget)

{

    if (!Widget || !Widget->GetClass()) return TEXT("<No Path>");

    if (const UObject* GeneratedBy = Widget->GetClass()->ClassGeneratedBy) return GeneratedBy->GetPathName();

    return Widget->GetClass()->GetPathName();

}

FString WidgetLocation(const UUserWidget* Widget)

{

    if (!Widget) return TEXT("<No Widget>");

    const UWorld* World = Widget->GetWorld();

    const UObject* Outer = Widget->GetOuter();

    const APlayerController* PC = Widget->GetOwningPlayer();

    return FString::Printf(TEXT("World=%s | Map=%s | InViewport=%s | OwningPlayer=%s | Outer=%s"),

        *WorldType(World),

        World ? *World->GetMapName() : TEXT("<No Map>"),

        Widget->IsInViewport() ? TEXT("Yes") : TEXT("No"),

        PC ? *PC->GetName() : TEXT("<None>"),

        Outer ? *Outer->GetPathName() : TEXT("<None>"));

}

bool ShouldTrack(const UUserWidget* Widget)

{

    if (!Widget || Widget->IsTemplate() || !Widget->GetClass()) return false;

    const FString N = Widget->GetClass()->GetName();

    return !N.StartsWith(TEXT("SKEL_")) && !N.StartsWith(TEXT("REINST_")) && !N.StartsWith(TEXT("TRASHCLASS_"));

}



struct FRecentSource { FString Summary; double Time = 0.0; };

struct FTrackedWidget

{

    TWeakObjectPtr<UUserWidget> Widget;

    FString ObjectName, FileName, Class, AssetPath, Location, CreatedBy;

    bool bInViewport = false;

    int32 ChildCount = 0;

};

struct FTrackedChild

{

    TWeakObjectPtr<UWidget> Widget;

    TWeakObjectPtr<UUserWidget> Owner;

    ESlateVisibility Visibility = ESlateVisibility::Visible;

};

struct FTraceEvent

{

    int32 Id = 0;

    double Time = 0.0;

    FDateTime Stamp;

    FString Type, File, Widget, Child, Summary, Source, Details;

};



class SWidgetLifecycleTrace : public SCompoundWidget

{

public:

    SLATE_BEGIN_ARGS(SWidgetLifecycleTrace) {}

    SLATE_END_ARGS()

    ~SWidgetLifecycleTrace() override { UnregisterHooks(); }



    void Construct(const FArguments&)

    {

        StartTime = FPlatformTime::Seconds();

        RegisterHooks();

        Discover(TEXT("InitialScan"));



        ChildSlot

        [

            SNew(SBorder).Padding(10).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

            [

                SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight()[BuildTitleBar()]

                + SVerticalBox::Slot().AutoHeight().Padding(0,8,0,8)[BuildToolbar()]

                + SVerticalBox::Slot().FillHeight(1.0f)[BuildMainArea()]

                + SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[BuildDetailsArea()]

            ]

        ];

        RefreshAll();

    }



    void Tick(const FGeometry& Geo, const double Now, const float Dt) override

    {

        SCompoundWidget::Tick(Geo, Now, Dt);

        const double T = FPlatformTime::Seconds();

        if (T - LastUpdate >= 0.12) { LastUpdate = T; UpdateTracked(); }

        if (bDirty && T - LastRefresh >= 0.20) RefreshAll();

    }



private:

    TSharedRef<SWidget> BuildTitleBar()

    {

        return SNew(SHorizontalBox)

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,7,0)

        [SNew(STextBlock).Text(TMLoc::Text(TEXT("Trace"), TEXT("Trace"))).Font(Font(TEXT("Regular"),9)).ColorAndOpacity(FLinearColor(0.55f,0.58f,0.64f))]

        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)

        [SNew(STextBlock).Text(TMLoc::Text(TEXT("Widget lifecycle trace"), TEXT("Widget lifecycle trace"))).Font(Font(TEXT("Bold"),11))]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6,0)

        [SNew(SButton).ContentPadding(FMargin(7,2)).Text(TMLoc::Text(TEXT("Refresh"), TEXT("Refresh"))).ToolTipText(TMLoc::Text(TEXT("Refresh live widgets"), TEXT("Refresh live widgets"))).OnClicked(this, &SWidgetLifecycleTrace::OnRefresh)];

    }



    TSharedRef<SWidget> BuildToolbar()

    {

        return SNew(SHorizontalBox)

        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0,0,8,0)

        [

            SAssignNew(SearchBox, SEditableTextBox)

            .HintText(TMLoc::Text(TEXT("Search widget or class"), TEXT("Search widget or class")))

            .OnTextChanged_Lambda([this](const FText& T){ SearchText = T.ToString(); RefreshWidgets(); })

        ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)

        [SNew(SButton).Text(TMLoc::Text(TEXT("All sources"), TEXT("All sources"))).ToolTipText(TMLoc::Text(TEXT("Currently shows every source."), TEXT("Currently shows every source.")))]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)

        [SNew(SButton).Text_Lambda([this](){ return FText::FromString(bShowOnlyOpen ? TEXT("Filter: Open") : TEXT("Filter")); }).OnClicked_Lambda([this](){ bShowOnlyOpen = !bShowOnlyOpen; RefreshWidgets(); return FReply::Handled(); })]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

        [SNew(SButton).Text(TMLoc::Text(TEXT("Copy report"), TEXT("Copy report"))).OnClicked(this, &SWidgetLifecycleTrace::OnCopy)];

    }



    TSharedRef<SWidget> BuildMainArea()

    {

        return SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder"))).Padding(0)

        [

            SNew(SSplitter)

            + SSplitter::Slot().Value(0.54f)

            [

                SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel"))).Padding(10)

                [

                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[BuildWidgetListHeader()]

                    + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(OpenBox,SVerticalBox)]]

                ]

            ]

            + SSplitter::Slot().Value(0.46f)

            [

                SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel"))).Padding(10)

                [

                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[BuildEventListHeader()]

                    + SVerticalBox::Slot().FillHeight(1)[SAssignNew(EventBox,SScrollBox)]

                ]

            ]

        ];

    }



    TSharedRef<SWidget> BuildWidgetListHeader()

    {

        return SNew(SHorizontalBox)

        + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(TMLoc::Text(TEXT("Live UUserWidget"), TEXT("Live UUserWidget"))).Font(Font(TEXT("Regular"),9)).ColorAndOpacity(FLinearColor(0.44f,0.48f,0.54f))]

        + SHorizontalBox::Slot().AutoWidth()[SAssignNew(WidgetCountText, STextBlock).Text(this, &SWidgetLifecycleTrace::WidgetCountTextValue).Font(Font(TEXT("Regular"),9)).ColorAndOpacity(FLinearColor(0.50f,0.54f,0.60f))];

    }



    TSharedRef<SWidget> BuildEventListHeader()

    {

        return SNew(SHorizontalBox)

        + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(TMLoc::Text(TEXT("Event log"), TEXT("Event log"))).Font(Font(TEXT("Regular"),9)).ColorAndOpacity(FLinearColor(0.44f,0.48f,0.54f))]

        + SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(TMLoc::Text(TEXT("Live"), TEXT("Live"))).Font(Font(TEXT("Regular"),9)).ColorAndOpacity(FLinearColor(0.50f,0.54f,0.60f))];

    }



    TSharedRef<SWidget> BuildDetailsArea()

    {

        return SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder"))).Padding(10)

        [

            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)

            [SAssignNew(DetailsTitleText, STextBlock).Text(this, &SWidgetLifecycleTrace::DetailsTitle).Font(Font(TEXT("Bold"),9))]

            + SVerticalBox::Slot().AutoHeight()

            [SAssignNew(DetailsBodyText, STextBlock).Text(this, &SWidgetLifecycleTrace::DetailsBody).AutoWrapText(true).Font(Font(TEXT("Regular"),8)).ColorAndOpacity(FLinearColor(0.54f,0.58f,0.64f))]

        ];

    }



    FText WidgetCountTextValue() const

    {

        return FText::FromString(FString::Printf(TEXT("%d tracked"), GetVisibleWidgetCount()));

    }



    int32 GetVisibleWidgetCount() const

    {

        int32 Count = 0;

        for (const TPair<uint64,FTrackedWidget>& P : Widgets)

        {

            if (ShouldDisplayWidget(P.Value)) ++Count;

        }

        return Count;

    }



    bool ShouldDisplayWidget(const FTrackedWidget& S) const

    {

        const UUserWidget* W = S.Widget.Get();

        if (!W) return false;

        if (bShowOnlyOpen && !W->IsInViewport()) return false;

        if (!SearchText.IsEmpty())

        {

            return S.FileName.Contains(SearchText, ESearchCase::IgnoreCase)

                || S.ObjectName.Contains(SearchText, ESearchCase::IgnoreCase)

                || S.Class.Contains(SearchText, ESearchCase::IgnoreCase)

                || S.AssetPath.Contains(SearchText, ESearchCase::IgnoreCase);

        }

        return true;

    }



    void RegisterHooks()

    {

        ObjectConstructedHandle = FCoreUObjectDelegates::OnObjectConstructed.AddSP(this, &SWidgetLifecycleTrace::OnObjectConstructed);

        PrePIEHandle = FEditorDelegates::PreBeginPIE.AddSP(this, &SWidgetLifecycleTrace::OnPrePIE);

        PostPIEHandle = FEditorDelegates::PostPIEStarted.AddSP(this, &SWidgetLifecycleTrace::OnPostPIE);

        EndPIEHandle = FEditorDelegates::EndPIE.AddSP(this, &SWidgetLifecycleTrace::OnEndPIE);

#if DO_BLUEPRINT_GUARD

        BpEnterHandle = FBlueprintContextTracker::OnEnterScriptContext.AddSP(this, &SWidgetLifecycleTrace::OnBpEnter);

        BpExitHandle = FBlueprintContextTracker::OnExitScriptContext.AddSP(this, &SWidgetLifecycleTrace::OnBpExit);

#endif

    }

    void UnregisterHooks()

    {

        if (ObjectConstructedHandle.IsValid()) FCoreUObjectDelegates::OnObjectConstructed.Remove(ObjectConstructedHandle);

        if (PrePIEHandle.IsValid()) FEditorDelegates::PreBeginPIE.Remove(PrePIEHandle);

        if (PostPIEHandle.IsValid()) FEditorDelegates::PostPIEStarted.Remove(PostPIEHandle);

        if (EndPIEHandle.IsValid()) FEditorDelegates::EndPIE.Remove(EndPIEHandle);

#if DO_BLUEPRINT_GUARD

        if (BpEnterHandle.IsValid()) FBlueprintContextTracker::OnEnterScriptContext.Remove(BpEnterHandle);

        if (BpExitHandle.IsValid()) FBlueprintContextTracker::OnExitScriptContext.Remove(BpExitHandle);

#endif

    }

    void Discover(const FString& Reason)

    {

        for (TObjectIterator<UUserWidget> It; It; ++It) RegisterWidget(*It, Reason);

    }

    void RegisterWidget(UUserWidget* W, const FString& Reason)

    {

        if (!ShouldTrack(W) || Widgets.Contains(ObjKey(W))) return;

        FTrackedWidget S;

        S.Widget = W; S.ObjectName = W->GetName(); S.FileName = WidgetFile(W); S.Class = ClassName(W); S.AssetPath = WidgetPath(W);

        S.Location = WidgetLocation(W); S.CreatedBy = SourceSummary(W, Reason); S.bInViewport = W->IsInViewport();

        Widgets.Add(ObjKey(W), S);

        AddEvent(TEXT("Created"), S.FileName, S.ObjectName, TEXT(""), FString::Printf(TEXT("%s created"), *S.FileName), S.CreatedBy, S.Location);

        ScanChildren(W, true);

        if (S.bInViewport) AddEvent(TEXT("Opened"), S.FileName, S.ObjectName, TEXT(""), FString::Printf(TEXT("%s is already in viewport"), *S.FileName), S.CreatedBy, S.Location);

    }

    void OnObjectConstructed(UObject* Obj) { RegisterWidget(Cast<UUserWidget>(Obj), TEXT("ObjectConstructed")); }

    void OnPrePIE(bool) { AddEvent(TEXT("PIE"), TEXT("-"), TEXT("-"), TEXT(""), TEXT("PIE starting. Widget trace armed."), TEXT(""), TEXT("")); }

    void OnPostPIE(bool) { Discover(TEXT("PIEStarted")); AddEvent(TEXT("PIE"), TEXT("-"), TEXT("-"), TEXT(""), TEXT("PIE started. Existing widgets scanned."), TEXT(""), TEXT("")); }

    void OnEndPIE(bool) { Recent.Reset(); AddEvent(TEXT("PIE"), TEXT("-"), TEXT("-"), TEXT(""), TEXT("PIE ended. Trace remains armed."), TEXT(""), TEXT("")); }



    void UpdateTracked()

    {

        TArray<uint64> RemoveWidgets;

        for (TPair<uint64,FTrackedWidget>& P : Widgets)

        {

            FTrackedWidget& S = P.Value;

            UUserWidget* W = S.Widget.Get();

            if (!W)

            {

                AddEvent(TEXT("Destroyed"), S.FileName, S.ObjectName, TEXT(""), FString::Printf(TEXT("%s object expired"), *S.FileName), TEXT(""), S.Location);

                RemoveWidgets.Add(P.Key); continue;

            }

            const bool bNowOpen = W->IsInViewport();

            if (bNowOpen != S.bInViewport)

            {

                S.bInViewport = bNowOpen; S.Location = WidgetLocation(W);

                AddEvent(bNowOpen ? TEXT("Opened") : TEXT("Closed"), S.FileName, S.ObjectName, TEXT(""), FString::Printf(TEXT("%s %s viewport"), *S.FileName, bNowOpen ? TEXT("entered") : TEXT("left")), SourceSummary(W, bNowOpen ? TEXT("ViewportOpen") : TEXT("ViewportClose")), S.Location);

            }

            ScanChildren(W, false);

        }

        for (uint64 K : RemoveWidgets) Widgets.Remove(K);

        TArray<uint64> RemoveChildren;

        for (const TPair<uint64,FTrackedChild>& P : Children) if (!P.Value.Widget.IsValid() || !P.Value.Owner.IsValid()) RemoveChildren.Add(P.Key);

        for (uint64 K : RemoveChildren) Children.Remove(K);

        const double Now = FPlatformTime::Seconds();

        for (int32 I = Recent.Num()-1; I >= 0; --I) if (Now - Recent[I].Time > 2.0) Recent.RemoveAt(I);

        for (auto It = LastEventSeconds.CreateIterator(); It; ++It) if (Now - It.Value() > 8.0) It.RemoveCurrent();

    }



    void ScanChildren(UUserWidget* Owner, bool bInitial)

    {

        if (!Owner || !Owner->WidgetTree) return;

        TArray<UWidget*> All; Owner->WidgetTree->GetAllWidgets(All);

        if (FTrackedWidget* S = Widgets.Find(ObjKey(Owner))) S->ChildCount = All.Num();

        const FString File = WidgetFile(Owner);

        for (UWidget* Child : All)

        {

            if (!Child || Child->IsTemplate()) continue;

            const uint64 K = ObjKey(Child);

            const ESlateVisibility V = Child->GetVisibility();

            FTrackedChild* Old = Children.Find(K);

            if (!Old)

            {

                FTrackedChild NewChild; NewChild.Widget = Child; NewChild.Owner = Owner; NewChild.Visibility = V; Children.Add(K, NewChild);

                continue;

            }

            if (Old->Visibility != V)

            {

                const ESlateVisibility Prev = Old->Visibility; Old->Visibility = V;

                AddEvent(TEXT("Visibility"), File, Owner->GetName(), Child->GetName(), FString::Printf(TEXT("%s -> %s (%s => %s)"), *Child->GetName(), IsOn(V) ? TEXT("ON") : TEXT("OFF"), *VisibilityText(Prev), *VisibilityText(V)), SourceSummary(Child, TEXT("VisibilityChanged")), ChildDetails(Owner, Child, Prev, V));

            }

        }

    }



    FString ChildDetails(const UUserWidget* Owner, const UWidget* Child, ESlateVisibility Prev, ESlateVisibility Now) const

    {

        return FString::Printf(TEXT("Owner path: %s -> %s\nTransition: %s -> %s\nChild class: %s\nController hint: %s"),

            Owner ? *Owner->GetName() : TEXT("<No Owner>"),

            Child ? *Child->GetName() : TEXT("<No Child>"),

            *VisibilityText(Prev), *VisibilityText(Now),

            Child ? *ClassName(Child) : TEXT("<No Child Class>"),

            TEXT("see Controller field above"));

    }



    FString SourceSummary(const UObject* Affected, const FString& Reason) const

    {

#if DO_BLUEPRINT_GUARD

        if (const FBlueprintContextTracker* Tracker = FBlueprintContextTracker::TryGet())

        {

            const FString Active = SourceFromContext(Tracker, nullptr, nullptr, Affected, Reason);

            if (!Active.IsEmpty()) return Active;

        }

#endif

        const double Now = FPlatformTime::Seconds();

        for (int32 I = Recent.Num()-1; I >= 0; --I)

        {

            if (Now - Recent[I].Time <= 0.50) return Recent[I].Summary + TEXT(" | Confidence=RecentBlueprintContext");

        }

        if (const UWidget* Widget = Cast<UWidget>(Affected))

        {

            if (const UUserWidget* Owner = Widget->GetTypedOuter<UUserWidget>())

            {

                return FString::Printf(TEXT("ControllerFallback=OwnerWidget | Class=%s | File=%s | Confidence=OwnerFallback"), *ClassName(Owner), *WidgetFile(Owner));

            }

        }

        if (const UUserWidget* UserWidget = Cast<UUserWidget>(Affected))

        {

            return FString::Printf(TEXT("ControllerFallback=Self | Class=%s | File=%s | Confidence=SelfFallback"), *ClassName(UserWidget), *WidgetFile(UserWidget));

        }

        return TEXT("Controller=<unknown> | Confidence=Unavailable");

    }

#if DO_BLUEPRINT_GUARD

    FString SourceFromContext(const FBlueprintContextTracker* Tracker, const UObject* ContextObject, const UFunction* ContextFunction, const UObject* Affected, const FString& Reason) const

    {

        const UObject* SourceObject = ContextObject;

        const UFunction* SourceFunction = ContextFunction;

        const UFunction* NativeFunction = nullptr;

        TArray<FString> Stack;

        if (Tracker)

        {

            const TArrayView<const FFrame* const> ScriptStack = Tracker->GetCurrentScriptStack();

            for (int32 I = ScriptStack.Num()-1; I >= 0; --I)

            {

                const FFrame* Frame = ScriptStack[I]; if (!Frame) continue;

                if (!SourceObject && Frame->Object) SourceObject = Frame->Object;

                if (!SourceFunction && Frame->Node) SourceFunction = Frame->Node;

                if (Frame->CurrentNativeFunction)

                {

                    const FString N = Frame->CurrentNativeFunction->GetName();

                    if (!NativeFunction || N.Contains(TEXT("Visibility"), ESearchCase::IgnoreCase) || N.Contains(TEXT("Widget"), ESearchCase::IgnoreCase) || N.Contains(TEXT("Viewport"), ESearchCase::IgnoreCase) || N.Contains(TEXT("Create"), ESearchCase::IgnoreCase)) NativeFunction = Frame->CurrentNativeFunction;

                }

                if (Frame->Object && Frame->Node) Stack.Add(FString::Printf(TEXT("%s.%s"), *ObjName(Frame->Object), *FuncName(Frame->Node)));

            }

        }

        if (!SourceObject && !SourceFunction && !NativeFunction) return FString();

        FString Summary = FString::Printf(TEXT("Controller=RuntimeBlueprint | Object=%s | Class=%s | Function=%s | NativeCall=%s | Affected=%s | Reason=%s | Confidence=ActiveScriptStack"),

            SourceObject ? *ObjName(SourceObject) : TEXT("<unknown object>"), SourceObject ? *ClassName(SourceObject) : TEXT("<No Class>"),

            SourceFunction ? *FuncName(SourceFunction) : TEXT("<unknown function>"), NativeFunction ? *NativeFunction->GetName() : TEXT("<unknown native call>"),

            Affected ? *Affected->GetName() : TEXT("<unknown widget>"), Reason.IsEmpty() ? TEXT("<unknown reason>") : *Reason);

        if (Stack.Num() > 0)

        {

            if (Stack.Num() > 6) { Stack.SetNum(6); Stack.Add(TEXT("...")); }

            Summary += FString::Printf(TEXT(" | ScriptStack=%s"), *FString::Join(Stack, TEXT(" > ")));

        }

        return Summary;

    }

    void OnBpEnter(const FBlueprintContextTracker& Tracker, const UObject* Obj, const UFunction* Fn)

    {

        if (!IsInGameThread()) return;

        const FString S = SourceFromContext(&Tracker, Obj, Fn, nullptr, TEXT("ScriptEnter"));

        if (S.IsEmpty()) return;

        FRecentSource R; R.Summary = S; R.Time = FPlatformTime::Seconds(); Recent.Add(MoveTemp(R));

        while (Recent.Num() > 128) Recent.RemoveAt(0);

    }

    void OnBpExit(const FBlueprintContextTracker&) {}

#endif



    bool ShouldSuppressDuplicate(const FString& Signature, double Now) const

    {

        if (const double* Last = LastEventSeconds.Find(Signature))

        {

            return Now - *Last < 0.85;

        }

        return false;

    }



    void AddEvent(const FString& Type, const FString& File, const FString& Widget, const FString& Child, const FString& Summary, const FString& Source, const FString& Details)

    {

        const double Now = FPlatformTime::Seconds();

        if (TMTraceNoise::IsTickLike(Type) || TMTraceNoise::IsTickLike(Summary))
        {
            return;
        }

        const FString Signature = TMTraceNoise::StableWidgetSignature(Type, File, Widget, Child, Summary);
        if (const double* Last = LastEventSeconds.Find(Signature))
        {
            if (Now - *Last < TMTraceNoise::DuplicateWindowSeconds(Type))
            {
                return;
            }
        }
        LastEventSeconds.Add(Signature, Now);
        if (LastEventSeconds.Num() > 1024)
        {
            for (auto It = LastEventSeconds.CreateIterator(); It; ++It)
            {
                if (Now - It.Value() > 15.0) It.RemoveCurrent();
            }
        }



        TSharedPtr<FTraceEvent> E = MakeShared<FTraceEvent>();

        E->Id = NextId++; E->Time = Now - StartTime; E->Stamp = FDateTime::Now(); E->Type = Type; E->File = File; E->Widget = Widget; E->Child = Child; E->Summary = Summary; E->Source = Source.IsEmpty() ? TEXT("Controller=<unknown>") : Source; E->Details = Details;

        Events.Add(E); while (Events.Num() > TMPerf::PIEEventHistoryLimit()) Events.RemoveAt(0);

        SelectedEvent = E;

        bDirty = true;

    }



    FLinearColor EventColor(const FString& Type) const

    {

        if (Type == TEXT("Visibility")) return FLinearColor(0.39f,0.64f,0.92f);

        if (Type == TEXT("Opened")) return FLinearColor(0.26f,0.60f,0.95f);

        if (Type == TEXT("Closed")) return FLinearColor(0.90f,0.62f,0.20f);

        if (Type == TEXT("Destroyed")) return FLinearColor(0.95f,0.42f,0.42f);

        if (Type == TEXT("Created")) return FLinearColor(0.32f,0.76f,0.38f);

        return FLinearColor(0.58f,0.62f,0.68f);

    }



    TSharedRef<SWidget> Badge(const FString& Text, const FLinearColor& Color) const

    {

        return SNew(SBorder).Padding(FMargin(6,2)).BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(Color.CopyWithNewOpacity(0.18f))

        [SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(TEXT("Bold"),7)).ColorAndOpacity(Color)];

    }



    TSharedRef<SWidget> WidgetCard(const FTrackedWidget& S) const

    {

        const UUserWidget* W = S.Widget.Get(); const bool bOpen = W && W->IsInViewport(); const FString Loc = W ? WidgetLocation(W) : S.Location;

        return SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel"))).Padding(10)

        [

            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight()

            [

                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,7,0)[SNew(STextBlock).Text(TMLoc::Text(TEXT(">"), TEXT(">"))).Font(Font(TEXT("Bold"),12)).ColorAndOpacity(FLinearColor(0.55f,0.58f,0.64f))]

                + SHorizontalBox::Slot().FillWidth(1)

                [

                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(S.FileName)).Font(Font(TEXT("Bold"),10))]

                    + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s / %s"), *S.Class, *S.ObjectName))).Font(Font(TEXT("Regular"),8)).ColorAndOpacity(FLinearColor(0.55f,0.59f,0.65f))]

                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Badge(bOpen ? TEXT("Alive") : TEXT("Not in viewport"), bOpen ? FLinearColor(0.35f,0.74f,0.38f) : FLinearColor(0.88f,0.60f,0.20f))]

            ]

            + SVerticalBox::Slot().AutoHeight().Padding(20,6,0,0)

            [SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Child widgets %d | %s"), S.ChildCount, *CompactLocation(Loc)))).AutoWrapText(true).Font(Font(TEXT("Regular"),8)).ColorAndOpacity(FLinearColor(0.48f,0.52f,0.58f))]

            + SVerticalBox::Slot().AutoHeight().Padding(20,4,0,0)

            [SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Created by: %s"), *CompactSource(S.CreatedBy)))).AutoWrapText(true).Font(Font(TEXT("Regular"),8)).ColorAndOpacity(FLinearColor(0.50f,0.54f,0.60f))]

        ];

    }



    FString CompactLocation(const FString& Loc) const

    {

        FString Out = Loc;

        Out.ReplaceInline(TEXT("World="), TEXT("World: "));

        Out.ReplaceInline(TEXT(" | Map="), TEXT(" | Map: "));

        Out.ReplaceInline(TEXT(" | InViewport="), TEXT(" | InViewport: "));

        Out.ReplaceInline(TEXT(" | OwningPlayer="), TEXT(" | OwningPlayer: "));

        Out.ReplaceInline(TEXT(" | Outer="), TEXT(" | Outer: "));

        return Out;

    }



    FString CompactSource(const FString& Source) const

    {

        FString Out = Source;

        Out.ReplaceInline(TEXT("Controller=RuntimeBlueprint | "), TEXT(""));

        Out.ReplaceInline(TEXT("ControllerFallback=OwnerWidget | "), TEXT(""));

        Out.ReplaceInline(TEXT("ControllerFallback=Self | "), TEXT(""));

        if (Out.Len() > 140) Out = Out.Left(137) + TEXT("...");

        return Out;

    }



    TSharedRef<SWidget> EventCard(const TSharedPtr<FTraceEvent>& E)

    {

        const FLinearColor C = EventColor(E->Type);

        return SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder"))).Padding(8)

        [

            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight()

            [

                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,7,0)[Badge(E->Type, C)]

                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(E->Child.IsEmpty() ? E->File : E->Child)).Font(Font(TEXT("Bold"),9))]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%05.2f"), E->Time))).Font(Font(TEXT("Regular"),7)).ColorAndOpacity(FLinearColor(0.58f,0.62f,0.68f))]

            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)

            [SNew(STextBlock).Text(FText::FromString(EventSummaryLine(E))).AutoWrapText(true).Font(Font(TEXT("Regular"),8)).ColorAndOpacity(FLinearColor(0.36f,0.39f,0.44f))]

            + SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)

            [

                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock)]

                + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).ContentPadding(FMargin(7,2)).Text(TMLoc::Text(TEXT("Details"), TEXT("Details"))).OnClicked_Lambda([this,E](){ SelectedEvent = E; RefreshDetails(); return FReply::Handled(); })]

            ]

        ];

    }



    FString EventSummaryLine(const TSharedPtr<FTraceEvent>& E) const

    {

        if (!E.IsValid()) return FString();

        if (E->Type == TEXT("Visibility")) return E->Summary.Replace(TEXT("=>"), TEXT("->"));

        if (E->Type == TEXT("Created")) return TEXT("created / constructed");

        if (E->Type == TEXT("Opened")) return TEXT("AddToViewport or viewport enter detected");

        if (E->Type == TEXT("Closed")) return TEXT("RemoveFromParent or viewport leave detected");

        if (E->Type == TEXT("Destroyed")) return TEXT("GC/object expiry detected");

        return E->Summary;

    }



    FText DetailsTitle() const

    {

        if (!SelectedEvent.IsValid()) return TMLoc::Text(TEXT("Details"), TEXT("Details"));

        const FString Name = SelectedEvent->Child.IsEmpty() ? SelectedEvent->File : SelectedEvent->Child;

        return FText::FromString(FString::Printf(TEXT("Details - %s %s"), *Name, *SelectedEvent->Type));

    }



    FText DetailsBody() const

    {

        if (!SelectedEvent.IsValid()) return TMLoc::Text(TEXT("Press an event Details button to show additional log information here."), TEXT("Press an event Details button to show additional log information here."));

        const TSharedPtr<FTraceEvent> E = SelectedEvent;

        return FText::FromString(FString::Printf(TEXT("Owner/Object: %s / %s\nSummary: %s\nTimestamp: %s (%.3fs)\nController: %s\n%s"),

            *E->File, *E->Widget, *E->Summary, *E->Stamp.ToString(TEXT("%H:%M:%S")), E->Time, *E->Source, E->Details.IsEmpty() ? TEXT("Details: <none>") : *E->Details));

    }



    void RefreshAll(){ RefreshWidgets(); RefreshEvents(); RefreshDetails(); if (WidgetCountText.IsValid()) WidgetCountText->Invalidate(EInvalidateWidgetReason::Paint); LastRefresh = FPlatformTime::Seconds(); bDirty = false; }

    void RefreshDetails(){ if (DetailsTitleText.IsValid()) DetailsTitleText->Invalidate(EInvalidateWidgetReason::Paint); if (DetailsBodyText.IsValid()) DetailsBodyText->Invalidate(EInvalidateWidgetReason::Paint); }



    void RefreshWidgets()

    {

        if (!OpenBox.IsValid()) return; OpenBox->ClearChildren(); int32 Added = 0;

        for (const TPair<uint64,FTrackedWidget>& P : Widgets)

        {

            if (!ShouldDisplayWidget(P.Value)) continue;

            OpenBox->AddSlot().AutoHeight().Padding(0,0,0,6)[WidgetCard(P.Value)]; if (++Added >= 100) break;

        }

        if (Added == 0) OpenBox->AddSlot().AutoHeight()[SNew(STextBlock).Text(TMLoc::Text(TEXT("No live UUserWidget captured yet. Keep this tab open, then start PIE or open a UI."), TEXT("No live UUserWidget captured yet. Keep this tab open, then start PIE or open a UI."))).AutoWrapText(true).Font(Font(TEXT("Regular"),9)).ColorAndOpacity(FLinearColor(0.58f,0.62f,0.68f))];

        if (WidgetCountText.IsValid()) WidgetCountText->Invalidate(EInvalidateWidgetReason::Paint);

    }

    void RefreshEvents()

    {

        if (!EventBox.IsValid()) return; EventBox->ClearChildren();

        if (Events.IsEmpty()) { EventBox->AddSlot()[SNew(STextBlock).Text(TMLoc::Text(TEXT("No widget lifecycle event captured yet."), TEXT("No widget lifecycle event captured yet."))).Font(Font(TEXT("Regular"),9)).ColorAndOpacity(FLinearColor(0.58f,0.62f,0.68f))]; return; }

        const int32 Start = FMath::Max(0, Events.Num() - TMPerf::PIEEventHistoryLimit());

        for (int32 I = Start; I < Events.Num(); ++I) EventBox->AddSlot().Padding(0,0,0,6)[EventCard(Events[I])];

        EventBox->ScrollToEnd();

    }

    FReply OnRefresh(){ Discover(TEXT("ManualRefresh")); UpdateTracked(); RefreshAll(); return FReply::Handled(); }

    FReply OnReset(){ Widgets.Reset(); Children.Reset(); Events.Reset(); Recent.Reset(); LastEventSeconds.Reset(); SelectedEvent.Reset(); NextId = 1; StartTime = FPlatformTime::Seconds(); Discover(TEXT("ResetScan")); AddEvent(TEXT("Reset"), TEXT("-"), TEXT("-"), TEXT(""), TEXT("Widget lifecycle trace reset."), TEXT(""), TEXT("")); RefreshAll(); return FReply::Handled(); }

    FString Report() const

    {

        TArray<FString> L;

        L.Add(TEXT("## Live Widgets"));

        for (const TPair<uint64,FTrackedWidget>& P : Widgets)

        {

            L.Add(FString::Printf(TEXT("- %s | Object=%s | Class=%s | Asset=%s | Location=%s | CreatedBy=%s"), *P.Value.FileName, *P.Value.ObjectName, *P.Value.Class, *P.Value.AssetPath, *P.Value.Location.Replace(TEXT("|"),TEXT("/")), *P.Value.CreatedBy.Replace(TEXT("|"),TEXT("/"))));

        }

        L.Add(TEXT(""));

        L.Add(TEXT("## Events"));

        L.Add(TEXT("| Time | Type | Widget | Child | Summary | Controller | Details |"));

        L.Add(TEXT("|---:|---|---|---|---|---|---|"));

        for (const TSharedPtr<FTraceEvent>& E : Events)

        {

            if (E.IsValid())

            {

                L.Add(FString::Printf(TEXT("| %.2fs | %s | %s | %s | %s | %s | %s |"), E->Time, *E->Type.Replace(TEXT("|"),TEXT("/")), *E->File.Replace(TEXT("|"),TEXT("/")), *E->Child.Replace(TEXT("|"),TEXT("/")), *E->Summary.Replace(TEXT("|"),TEXT("/")), *E->Source.Replace(TEXT("|"),TEXT("/")), *E->Details.Replace(TEXT("|"),TEXT("/"))));

            }

        }



        TArray<TMReportFormatter::FMetadataItem> Metadata;

        Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Live Widgets"), FString::FromInt(Widgets.Num())));

        Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Child Widgets"), FString::FromInt(Children.Num())));

        Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Events"), FString::FromInt(Events.Num())));



        return TMReportFormatter::BuildWrappedLegacyReport(

            TEXT("Widget Lifecycle Trace"),

            FString::Printf(TEXT("- Live widgets: %d\n- Child widgets: %d\n- Captured events: %d\n- Confidence: controller/source attribution is best-effort."), Widgets.Num(), Children.Num(), Events.Num()),

            FString::Join(L, TEXT("\n")),

            Metadata);

    }

    FReply OnCopy() const { FPlatformApplicationMisc::ClipboardCopy(*Report()); return FReply::Handled(); }



    TMap<uint64,FTrackedWidget> Widgets;

    TMap<uint64,FTrackedChild> Children;

    TArray<TSharedPtr<FTraceEvent>> Events;

    TArray<FRecentSource> Recent;

    TMap<FString,double> LastEventSeconds;

    TSharedPtr<FTraceEvent> SelectedEvent;

    TSharedPtr<SVerticalBox> OpenBox;

    TSharedPtr<SScrollBox> EventBox;

    TSharedPtr<SEditableTextBox> SearchBox;

    TSharedPtr<STextBlock> WidgetCountText;

    TSharedPtr<STextBlock> DetailsTitleText;

    TSharedPtr<STextBlock> DetailsBodyText;

    FDelegateHandle ObjectConstructedHandle, PrePIEHandle, PostPIEHandle, EndPIEHandle;

#if DO_BLUEPRINT_GUARD

    FDelegateHandle BpEnterHandle, BpExitHandle;

#endif

    FString SearchText;

    double StartTime = 0.0, LastUpdate = 0.0, LastRefresh = 0.0;

    int32 NextId = 1;

    bool bDirty = false;

    bool bShowOnlyOpen = false;

};

TSharedRef<SDockTab> SpawnWidgetTraceTab(const FSpawnTabArgs&)

{

    TSharedRef<SDockTab> Tab = SNew(SDockTab)

        .TabRole(ETabRole::NomadTab)

        .Label(TMLoc::Text(TEXT("Widget Lifecycle Trace"), TEXT("Widget Lifecycle Trace")))

        .OnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([](TSharedRef<SDockTab>){ ExistingTab.Reset(); }))

        [SNew(SWidgetLifecycleTrace)];

    ExistingTab = Tab;

    return Tab;

}

void RegisterWidgetTraceTabSpawner()

{

    if (bTabRegistered) return;

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(WidgetTraceTabId, FOnSpawnTab::CreateStatic(&SpawnWidgetTraceTab))

        .SetDisplayName(TMLoc::Text(TEXT("Widget Lifecycle Trace"), TEXT("Widget Lifecycle Trace")))

        .SetTooltipText(TMLoc::Text(TEXT("Trace opened UMG widgets and child widget visibility changes during PIE."), TEXT("Trace opened UMG widgets and child widget visibility changes during PIE.")))

        .SetIcon(FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.WidgetLifecycleTrace")));

    bTabRegistered = true;

}

}



namespace TMWidgetLifecycleTrace

{

void RegisterMenus()

{

    RegisterWidgetTraceTabSpawner();

    auto AddEntry = [](UToolMenu* Menu, const FName EntryName)

    {

        if (!Menu) return;

        Menu->FindOrAddSection(TEXT("TraceMotive")).AddMenuEntry(

            EntryName,

            TMLoc::Text(TEXT("Widget Lifecycle Trace"), TEXT("Widget Lifecycle Trace")),

            TMLoc::Text(TEXT("Shows opened UMG widget files, creation location, and child widget on/off control source."), TEXT("Shows opened UMG widget files, creation location, and child widget on/off control source.")),

            FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.WidgetLifecycleTrace")),

            FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&){ TMWidgetLifecycleTrace::OpenWindow(); }));

    };

    AddEntry(UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window")), TEXT("TMOpenWidgetLifecycleTrace"));

    AddEntry(UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools")), TEXT("TMToolsOpenWidgetLifecycleTrace"));

}

void UnregisterMenus()

{

    if (bTabRegistered)

    {

        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(WidgetTraceTabId);

        bTabRegistered = false;

    }

    ExistingTab.Reset();

}

void OpenWindow()

{

    RegisterWidgetTraceTabSpawner();

    if (ExistingTab.IsValid()) { FGlobalTabmanager::Get()->TryInvokeTab(WidgetTraceTabId); return; }

    ExistingTab = FGlobalTabmanager::Get()->TryInvokeTab(WidgetTraceTabId);

}

}










