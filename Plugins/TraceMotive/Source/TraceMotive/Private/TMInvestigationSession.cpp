#include "TMInvestigationSession.h"
#include "TMLocalization.h"

#include "TMAudioPlaybackTrace.h"
#include "TMBlueprintRuntimeErrorTrace.h"
#include "TMClickDiagnostics.h"
#include "TMCollisionPairAnalyzer.h"
#include "TMEnhancedOutlinerSearch.h"
#include "TMPackageProgress.h"
#include "TMStyle.h"
#include "TMDockTabHelper.h"
#include "TMPerformanceGuard.h"
#include "TMSettings.h"
#include "TMSupportBundle.h"
#include "TMVariableValueTrace.h"
#include "TMWidgetClickFlowTrace.h"
#include "TMWidgetLifecycleTrace.h"
#include "SInstanceReferenceTracker.h"

#include "Components/ActorComponent.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Framework/Docking/TabManager.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Styling/AppStyle.h"
#include "UObject/UnrealType.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    const FName InvestigationTabId(TEXT("TraceMotive.InvestigationSession"));
    bool bTabRegistered = false;
    bool bLoaded = false;
    TWeakPtr<SDockTab> ExistingTab;

    FString SessionString(const TCHAR* English, const TCHAR* Korean)
    {
        return TMLoc::String(English, Korean);
    }

    struct FInvestigation
    {
        FString Id;
        FString Name;
        FString Scenario;
        FString Notes;
        FString Target;
        FString CreatedUtc;
        FString UpdatedUtc;
        TArray<FString> History;
        TArray<FString> Evidence;
        TMap<FString, FString> Before;
        TMap<FString, FString> After;
    };

    TArray<FInvestigation> Investigations;
    int32 ActiveIndex = INDEX_NONE;

    FString StorePath()
    {
        return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TraceMotive"), TEXT("Investigations.json"));
    }

    FString NowUtc()
    {
        return FDateTime::UtcNow().ToIso8601();
    }

    AActor* SelectedActor()
    {
        if (!GEditor)
        {
            return nullptr;
        }
        if (USelection* Selection = GEditor->GetSelectedActors())
        {
            for (FSelectionIterator It(*Selection); It; ++It)
            {
                if (AActor* Actor = Cast<AActor>(*It))
                {
                    return Actor;
                }
            }
        }
        return nullptr;
    }

    FString ScenarioName(const FName Id)
    {
        if (Id == TEXT("Click")) return SessionString(TEXT("Click does not work"), TEXT("클릭이 동작하지 않음"));
        if (Id == TEXT("Collision")) return SessionString(TEXT("Actors pass through each other"), TEXT("액터가 서로 통과함"));
        if (Id == TEXT("Audio")) return SessionString(TEXT("Unknown audio source"), TEXT("알 수 없는 오디오 출처"));
        if (Id == TEXT("RuntimeError")) return SessionString(TEXT("Blueprint runtime error"), TEXT("Blueprint 런타임 오류"));
        if (Id == TEXT("Variable")) return SessionString(TEXT("Unexpected value change"), TEXT("예상치 못한 값 변경"));
        if (Id == TEXT("Packaging")) return SessionString(TEXT("Packaging failed or stalled"), TEXT("패키징 실패 또는 정체"));
        return Id.ToString();
    }

    void WriteStringMap(const TSharedRef<FJsonObject>& Parent, const TCHAR* Field, const TMap<FString, FString>& Values)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        for (const TPair<FString, FString>& Pair : Values)
        {
            Object->SetStringField(Pair.Key, Pair.Value);
        }
        Parent->SetObjectField(Field, Object);
    }

    void ReadStringMap(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Field, TMap<FString, FString>& Out)
    {
        Out.Reset();
        const TSharedPtr<FJsonObject>* Object = nullptr;
        if (Parent.IsValid() && Parent->TryGetObjectField(Field, Object) && Object && Object->IsValid())
        {
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Object)->Values)
            {
                FString Value;
                if (Pair.Value.IsValid() && Pair.Value->TryGetString(Value))
                {
                    Out.Add(Pair.Key, MoveTemp(Value));
                }
            }
        }
    }

    void Save()
    {
        const UTraceMotiveSettings* Settings = UTraceMotiveSettings::Get();
        if (Settings && !Settings->bAutoSaveInvestigations) return;
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(StorePath()), true);
        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetNumberField(TEXT("version"), 1);
        Root->SetNumberField(TEXT("activeIndex"), ActiveIndex);
        TArray<TSharedPtr<FJsonValue>> Items;
        for (const FInvestigation& Item : Investigations)
        {
            TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
            Object->SetStringField(TEXT("id"), Item.Id);
            Object->SetStringField(TEXT("name"), Item.Name);
            Object->SetStringField(TEXT("scenario"), Item.Scenario);
            Object->SetStringField(TEXT("notes"), Item.Notes);
            Object->SetStringField(TEXT("target"), Item.Target);
            Object->SetStringField(TEXT("createdUtc"), Item.CreatedUtc);
            Object->SetStringField(TEXT("updatedUtc"), Item.UpdatedUtc);
            TArray<TSharedPtr<FJsonValue>> History;
            for (const FString& Value : Item.History) History.Add(MakeShared<FJsonValueString>(Value));
            Object->SetArrayField(TEXT("history"), History);
            TArray<TSharedPtr<FJsonValue>> Evidence;
            for (const FString& Value : Item.Evidence) Evidence.Add(MakeShared<FJsonValueString>(Value));
            Object->SetArrayField(TEXT("evidence"), Evidence);
            WriteStringMap(Object, TEXT("before"), Item.Before);
            WriteStringMap(Object, TEXT("after"), Item.After);
            Items.Add(MakeShared<FJsonValueObject>(Object));
        }
        Root->SetArrayField(TEXT("investigations"), Items);
        FString Json;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
        if (FJsonSerializer::Serialize(Root, Writer))
        {
            FFileHelper::SaveStringToFile(Json, *StorePath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        }
    }

    void Load()
    {
        if (bLoaded) return;
        bLoaded = true;
        FString Json;
        if (!FFileHelper::LoadFileToString(Json, *StorePath())) return;
        TSharedPtr<FJsonObject> Root;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()) return;
        const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
        if (!Root->TryGetArrayField(TEXT("investigations"), Items) || !Items) return;
        for (const TSharedPtr<FJsonValue>& Value : *Items)
        {
            const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
            if (!Object.IsValid()) continue;
            FInvestigation Item;
            Object->TryGetStringField(TEXT("id"), Item.Id);
            Object->TryGetStringField(TEXT("name"), Item.Name);
            Object->TryGetStringField(TEXT("scenario"), Item.Scenario);
            Object->TryGetStringField(TEXT("notes"), Item.Notes);
            Object->TryGetStringField(TEXT("target"), Item.Target);
            Object->TryGetStringField(TEXT("createdUtc"), Item.CreatedUtc);
            Object->TryGetStringField(TEXT("updatedUtc"), Item.UpdatedUtc);
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (Object->TryGetArrayField(TEXT("history"), Values) && Values)
                for (const TSharedPtr<FJsonValue>& Entry : *Values) Item.History.Add(Entry->AsString());
            Values = nullptr;
            if (Object->TryGetArrayField(TEXT("evidence"), Values) && Values)
                for (const TSharedPtr<FJsonValue>& Entry : *Values) Item.Evidence.Add(Entry->AsString());
            ReadStringMap(Object, TEXT("before"), Item.Before);
            ReadStringMap(Object, TEXT("after"), Item.After);
            Investigations.Add(MoveTemp(Item));
        }
        double SavedIndex = INDEX_NONE;
        Root->TryGetNumberField(TEXT("activeIndex"), SavedIndex);
        ActiveIndex = Investigations.IsValidIndex(static_cast<int32>(SavedIndex)) ? static_cast<int32>(SavedIndex) : (Investigations.IsEmpty() ? INDEX_NONE : 0);
    }

    FInvestigation* Active()
    {
        Load();
        return Investigations.IsValidIndex(ActiveIndex) ? &Investigations[ActiveIndex] : nullptr;
    }

    void Touch(FInvestigation& Item, const FString& Event)
    {
        Item.UpdatedUtc = NowUtc();
        if (!Event.IsEmpty())
        {
            Item.History.Insert(FString::Printf(TEXT("%s | %s"), *Item.UpdatedUtc, *Event), 0);
            if (Item.History.Num() > 100) Item.History.SetNum(100);
        }
        Save();
    }

    void SnapshotObject(UObject* Object, const FString& Prefix, TMap<FString, FString>& Out)
    {
        if (!Object) return;
        for (TFieldIterator<FProperty> It(Object->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
        {
            FProperty* Property = *It;
            if (!Property || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated) ||
                !Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible)) continue;
            FString Value;
            Property->ExportText_InContainer(0, Value, Object, Object, Object, PPF_None);
            Value.ReplaceInline(TEXT("\r"), TEXT(" "));
            Value.ReplaceInline(TEXT("\n"), TEXT(" "));
            if (Value.Len() > 512) Value = Value.Left(509) + TEXT("...");
            Out.Add(Prefix + TEXT(".") + Property->GetName(), MoveTemp(Value));
            if (Out.Num() >= TMPerf::MaxSnapshotFields()) return;
        }
    }

    bool CaptureSelection(TMap<FString, FString>& Out, FString& OutTarget)
    {
        AActor* Actor = SelectedActor();
        if (!Actor) return false;
        Out.Reset();
        OutTarget = Actor->GetPathName();
        SnapshotObject(Actor, TEXT("Actor"), Out);
        TInlineComponentArray<UActorComponent*> Components(Actor);
        for (UActorComponent* Component : Components)
        {
            SnapshotObject(Component, FString::Printf(TEXT("Component[%s]"), *Component->GetName()), Out);
            if (Out.Num() >= TMPerf::MaxSnapshotFields()) break;
        }
        return true;
    }

    TArray<FName> NextTools(const FString& Scenario)
    {
        if (Scenario == TEXT("Click")) return { TEXT("WidgetClickFlow"), TEXT("WidgetLifecycle"), TEXT("RuntimeError") };
        if (Scenario == TEXT("Collision")) return { TEXT("InstanceTrace"), TEXT("Variable") };
        if (Scenario == TEXT("Audio")) return { TEXT("InstanceTrace"), TEXT("Variable") };
        if (Scenario == TEXT("RuntimeError")) return { TEXT("InstanceTrace"), TEXT("Variable") };
        if (Scenario == TEXT("Variable")) return { TEXT("InstanceTrace"), TEXT("RuntimeError") };
        if (Scenario == TEXT("Packaging")) return { TEXT("RuntimeError"), TEXT("AssetSearch") };
        return {};
    }

    FTMSupportBundleInput MakeSupportBundleInput(const FInvestigation& Item)
    {
        FTMSupportBundleInput Input;
        Input.InvestigationId = Item.Id;
        Input.InvestigationName = Item.Name;
        Input.Scenario = ScenarioName(FName(*Item.Scenario));
        Input.Notes = Item.Notes;
        Input.Target = Item.Target;
        Input.CreatedUtc = Item.CreatedUtc;
        Input.UpdatedUtc = Item.UpdatedUtc;
        Input.History = Item.History;
        Input.Evidence = Item.Evidence;
        Input.BeforeFieldCount = Item.Before.Num();
        Input.AfterFieldCount = Item.After.Num();
        Input.SnapshotDiff = TMInvestigationSession::CompareSnapshots(Item.Before, Item.After).Lines;
        return Input;
    }

    FString ToolLabel(FName Tool)
    {
        if (Tool == TEXT("Click")) return SessionString(TEXT("Click Event Diagnostics"), TEXT("클릭 이벤트 진단"));
        if (Tool == TEXT("WidgetClickFlow")) return SessionString(TEXT("Widget Click Flow Trace"), TEXT("위젯 클릭 흐름 추적"));
        if (Tool == TEXT("WidgetLifecycle")) return SessionString(TEXT("Widget Lifecycle Trace"), TEXT("위젯 생명주기 추적"));
        if (Tool == TEXT("Collision")) return SessionString(TEXT("Collision Pair Analyzer"), TEXT("충돌 쌍 분석"));
        if (Tool == TEXT("Audio")) return SessionString(TEXT("Audio Playback Trace"), TEXT("오디오 재생 추적"));
        if (Tool == TEXT("RuntimeError")) return SessionString(TEXT("Runtime Error Investigation"), TEXT("런타임 오류 분석"));
        if (Tool == TEXT("Variable")) return SessionString(TEXT("Variable Value Trace"), TEXT("변수 값 추적"));
        if (Tool == TEXT("Packaging")) return SessionString(TEXT("Package Progress"), TEXT("패키징 진행"));
        if (Tool == TEXT("InstanceTrace")) return SessionString(TEXT("Instance Reference Trace"), TEXT("인스턴스 참조 추적"));
        if (Tool == TEXT("AssetSearch")) return SessionString(TEXT("Enhanced Outliner Search"), TEXT("향상된 아웃라이너 검색"));
        return Tool.ToString();
    }

    void RouteTool(FName Tool)
    {
        if (Tool == TEXT("Click")) TMClickDiagnostics::OpenWindow();
        else if (Tool == TEXT("WidgetClickFlow")) TMWidgetClickFlowTrace::OpenWindow();
        else if (Tool == TEXT("WidgetLifecycle")) TMWidgetLifecycleTrace::OpenWindow();
        else if (Tool == TEXT("Collision")) TMCollisionPairAnalyzer::OpenWindowAndAnalyzeSelection();
        else if (Tool == TEXT("Audio")) TMAudioPlaybackTrace::OpenWindow();
        else if (Tool == TEXT("RuntimeError")) TMBlueprintRuntimeErrorTrace::OpenWindow();
        else if (Tool == TEXT("Variable")) TMVariableValueTrace::OpenWindow();
        else if (Tool == TEXT("Packaging")) TMPackageProgress::OpenWindow();
        else if (Tool == TEXT("AssetSearch")) TMEnhancedOutlinerSearch::OpenWindow();
        else if (Tool == TEXT("InstanceTrace"))
        {
            if (AActor* Actor = SelectedActor())
            {
                TMDockTab::OpenDockTab(TEXT("InvestigationInstanceTrace"),
                    FText::FromString(FString::Printf(TEXT("Instance Reference Trace - %s"), *Actor->GetActorLabel())),
                    SNew(SInstanceReferenceTracker, Actor));
            }
        }
    }

    class STMInvestigationWidget : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(STMInvestigationWidget) {}
        SLATE_END_ARGS()

        void Construct(const FArguments&)
        {
            ChildSlot[SAssignNew(Host, SBox)[Build()]];
        }

    private:
        void Rebuild() { if (Host.IsValid()) Host->SetContent(Build()); }

        TSharedRef<SWidget> Button(const FString& Label, TFunction<void()> Action, bool bEnabled = true)
        {
            return SNew(SButton).IsEnabled(bEnabled).Text(FText::FromString(Label)).OnClicked_Lambda([this, Action = MoveTemp(Action)]()
            {
                Action(); Rebuild(); return FReply::Handled();
            });
        }

        TSharedRef<SWidget> Build()
        {
            FInvestigation* Item = Active();
            TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
            Body->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
            [SNew(STextBlock).Text(FText::FromString(SessionString(TEXT("Investigation Sessions"), TEXT("조사 세션")))).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18))];

            TSharedRef<SHorizontalBox> Recent = SNew(SHorizontalBox);
            Recent->AddSlot().AutoWidth().Padding(0, 0, 5, 0)[Button(SessionString(TEXT("New blank session"), TEXT("빈 세션 만들기")), [this]()
            {
                FInvestigation NewItem;
                NewItem.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
                NewItem.Name = SessionString(TEXT("New investigation"), TEXT("새 조사"));
                NewItem.CreatedUtc = NewItem.UpdatedUtc = NowUtc();
                Investigations.Insert(MoveTemp(NewItem), 0); ActiveIndex = 0; Save();
            })];
            for (int32 Index = 0; Index < FMath::Min(Investigations.Num(), 5); ++Index)
            {
                Recent->AddSlot().AutoWidth().Padding(0, 0, 5, 0)[Button(Investigations[Index].Name, [this, Index]() { ActiveIndex = Index; Save(); })];
            }
            Body->AddSlot().AutoHeight().Padding(0, 0, 0, 10)[Recent];

            if (!Item)
            {
                Body->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(SessionString(TEXT("Choose a quick diagnosis in TraceMotiveTools or create a blank session."), TEXT("TraceMotiveTools에서 빠른 진단을 선택하거나 빈 세션을 만드세요."))))];
                return SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel"))).Padding(14)[Body];
            }

            Body->AddSlot().AutoHeight().Padding(0, 0, 0, 5)
            [SNew(SEditableTextBox).Text(FText::FromString(Item->Name)).OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
            { if (FInvestigation* ActiveItem = Active()) { ActiveItem->Name = Text.ToString(); Touch(*ActiveItem, TEXT("Renamed investigation")); Rebuild(); } })];
            Body->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
            [SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Scenario: %s\nTarget: %s"), *ScenarioName(FName(*Item->Scenario)), Item->Target.IsEmpty() ? TEXT("Select an actor when a snapshot or instance handoff is needed.") : *Item->Target))).AutoWrapText(true)];
            Body->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
            [SNew(SMultiLineEditableTextBox).Text(FText::FromString(Item->Notes)).HintText(FText::FromString(SessionString(TEXT("Reproduction steps and notes"), TEXT("재현 단계와 메모")))).OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
            { if (FInvestigation* ActiveItem = Active()) { ActiveItem->Notes = Text.ToString(); Touch(*ActiveItem, TEXT("Updated notes")); } })];

            TSharedRef<SHorizontalBox> SnapshotButtons = SNew(SHorizontalBox);
            SnapshotButtons->AddSlot().AutoWidth().Padding(0, 0, 5, 0)[Button(SessionString(TEXT("Capture Before"), TEXT("수정 전 캡처")), [this]()
            { if (FInvestigation* ActiveItem = Active()) { if (CaptureSelection(ActiveItem->Before, ActiveItem->Target)) Touch(*ActiveItem, TEXT("Captured Before snapshot")); } })];
            SnapshotButtons->AddSlot().AutoWidth().Padding(0, 0, 5, 0)[Button(SessionString(TEXT("Capture After"), TEXT("수정 후 캡처")), [this]()
            {
                if (FInvestigation* ActiveItem = Active())
                {
                    FString Target;
                    TMap<FString, FString> Captured;
                    if (CaptureSelection(Captured, Target))
                    {
                        if (!ActiveItem->Before.IsEmpty() && !ActiveItem->Target.IsEmpty() && ActiveItem->Target != Target)
                        {
                            ActiveItem->Evidence.Insert(FString::Printf(TEXT("%s | Snapshot comparison rejected: select the same actor used for Before (%s)."), *NowUtc(), *ActiveItem->Target), 0);
                            Touch(*ActiveItem, TEXT("Rejected After snapshot for a different actor"));
                        }
                        else
                        {
                            ActiveItem->After = MoveTemp(Captured);
                            ActiveItem->Target = Target;
                            Touch(*ActiveItem, TEXT("Captured After snapshot"));
                        }
                    }
                }
            })];
            SnapshotButtons->AddSlot().AutoWidth()[Button(SessionString(TEXT("Clear snapshots"), TEXT("스냅샷 지우기")), [this]()
            { if (FInvestigation* ActiveItem = Active()) { ActiveItem->Before.Reset(); ActiveItem->After.Reset(); Touch(*ActiveItem, TEXT("Cleared snapshots")); } })];
            Body->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[SnapshotButtons];

            const FTMSnapshotDiff Diff = TMInvestigationSession::CompareSnapshots(Item->Before, Item->After);
            FString DiffText = FString::Printf(TEXT("Before %d fields | After %d fields | Changed %d | Added %d | Removed %d"), Item->Before.Num(), Item->After.Num(), Diff.Changed, Diff.Added, Diff.Removed);
            for (const FString& Line : Diff.Lines) DiffText += TEXT("\n") + Line;
            Body->AddSlot().AutoHeight().Padding(0, 0, 0, 10)
            [SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed"))).Padding(8)[SNew(STextBlock).Text(FText::FromString(DiffText)).AutoWrapText(true)]];

            Body->AddSlot().AutoHeight().Padding(0, 0, 0, 4)[SNew(STextBlock).Text(FText::FromString(SessionString(TEXT("Continue investigation in"), TEXT("다음 도구에서 조사 계속")))).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 11))];
            TSharedRef<SHorizontalBox> Handoffs = SNew(SHorizontalBox);
            for (const FName Tool : NextTools(Item->Scenario))
            {
                Handoffs->AddSlot().AutoWidth().Padding(0, 0, 5, 0)[Button(ToolLabel(Tool), [Tool]() { TMInvestigationSession::OpenTool(Tool, TEXT("Recommended handoff")); })];
            }
            Body->AddSlot().AutoHeight().Padding(0, 0, 0, 10)[Handoffs];

            Body->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
            [SNew(STextBlock).Text(FText::FromString(SessionString(TEXT("Customer support bundle"), TEXT("고객 지원 번들")))).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 11))];
            TSharedRef<SHorizontalBox> SupportButtons = SNew(SHorizontalBox);
            SupportButtons->AddSlot().AutoWidth().Padding(0, 0, 5, 0)[Button(SessionString(TEXT("Preview contents"), TEXT("포함 내용 미리보기")), [this]()
            {
                if (const FInvestigation* ActiveItem = Active())
                {
                    SupportStatus = TMSupportBundle::BuildPreview(MakeSupportBundleInput(*ActiveItem)).Text;
                }
            })];
            SupportButtons->AddSlot().AutoWidth()[Button(SessionString(TEXT("Export ZIP"), TEXT("ZIP 내보내기")), [this]()
            {
                if (FInvestigation* ActiveItem = Active())
                {
                    const FTMSupportBundleResult Result = TMSupportBundle::Export(MakeSupportBundleInput(*ActiveItem));
                    if (Result.bSuccess)
                    {
                        SupportStatus = FString::Printf(TEXT("%s\n\nCreated:\n%s"), *Result.Preview.Text, *Result.BundlePath);
                        Touch(*ActiveItem, TEXT("Exported privacy-redacted support bundle"));
                        FPlatformProcess::ExploreFolder(*FPaths::GetPath(Result.BundlePath));
                    }
                    else
                    {
                        SupportStatus = FString::Printf(TEXT("Support bundle export failed: %s"), *Result.Error);
                    }
                }
            })];
            Body->AddSlot().AutoHeight().Padding(0, 0, 0, 6)[SupportButtons];
            Body->AddSlot().AutoHeight().Padding(0, 0, 0, 10)
            [SNew(SBorder).Visibility_Lambda([this]() { return SupportStatus.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
                .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed"))).Padding(8)
                [SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(SupportStatus); }).AutoWrapText(true)]];

            FString Activity = SessionString(TEXT("Evidence and activity"), TEXT("근거 및 활동 기록"));
            for (const FString& Entry : Item->Evidence) Activity += TEXT("\n• ") + Entry;
            for (const FString& Entry : Item->History) Activity += TEXT("\n• ") + Entry;
            Body->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Activity)).AutoWrapText(true)];

            return SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel"))).Padding(14)
            [SNew(SScrollBox) + SScrollBox::Slot()[Body]];
        }

        TSharedPtr<SBox> Host;
        FString SupportStatus;
    };

    TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs&)
    {
        TSharedRef<SDockTab> Tab = SNew(SDockTab).TabRole(ETabRole::NomadTab)
            .Label(FText::FromString(SessionString(TEXT("Investigation Sessions"), TEXT("조사 세션"))))
            .OnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([](TSharedRef<SDockTab>) { ExistingTab.Reset(); }))
            [SNew(STMInvestigationWidget)];
        ExistingTab = Tab;
        return Tab;
    }
}

namespace TMInvestigationSession
{
    FTMSnapshotDiff CompareSnapshots(const TMap<FString, FString>& Before, const TMap<FString, FString>& After)
    {
        FTMSnapshotDiff Result;
        for (const TPair<FString, FString>& Pair : Before)
        {
            const FString* NewValue = After.Find(Pair.Key);
            if (!NewValue)
            {
                ++Result.Removed;
                if (Result.Lines.Num() < 200) Result.Lines.Add(FString::Printf(TEXT("- %s = %s"), *Pair.Key, *Pair.Value));
            }
            else if (*NewValue != Pair.Value)
            {
                ++Result.Changed;
                if (Result.Lines.Num() < 200) Result.Lines.Add(FString::Printf(TEXT("~ %s: %s -> %s"), *Pair.Key, *Pair.Value, **NewValue));
            }
        }
        for (const TPair<FString, FString>& Pair : After)
        {
            if (!Before.Contains(Pair.Key))
            {
                ++Result.Added;
                if (Result.Lines.Num() < 200) Result.Lines.Add(FString::Printf(TEXT("+ %s = %s"), *Pair.Key, *Pair.Value));
            }
        }
        Result.Lines.Sort();
        return Result;
    }

    void RegisterTab()
    {
        Load();
        if (bTabRegistered) return;
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(InvestigationTabId, FOnSpawnTab::CreateStatic(&SpawnTab))
            .SetDisplayName(FText::FromString(SessionString(TEXT("Investigation Sessions"), TEXT("조사 세션"))))
            .SetTooltipText(TMLoc::Text(TEXT("Persist a diagnosis, route between tools, and compare Before/After actor state."), TEXT("진단을 저장하고 도구 사이를 이동하며 액터의 수정 전/후 상태를 비교합니다.")))
            .SetIcon(FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.ToolLauncher")));
        bTabRegistered = true;
    }

    void UnregisterTab()
    {
        Save();
        if (bTabRegistered) FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(InvestigationTabId);
        bTabRegistered = false;
        ExistingTab.Reset();
    }

    void OpenWindow()
    {
        RegisterTab();
        ExistingTab = FGlobalTabmanager::Get()->TryInvokeTab(InvestigationTabId);
    }

    void StartScenario(FName ScenarioId)
    {
        Load();
        FInvestigation Item;
        Item.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
        Item.Scenario = ScenarioId.ToString();
        Item.Name = ScenarioName(ScenarioId);
        Item.CreatedUtc = Item.UpdatedUtc = NowUtc();
        if (AActor* Actor = SelectedActor()) Item.Target = Actor->GetPathName();
        Item.History.Add(FString::Printf(TEXT("%s | Started quick diagnosis: %s"), *Item.CreatedUtc, *Item.Name));
        Investigations.Insert(MoveTemp(Item), 0);
        ActiveIndex = 0;
        Save();
        OpenWindow();
        OpenTool(ScenarioId, TEXT("Quick diagnosis"));
    }

    void OpenTool(FName ToolId, const FString& Context)
    {
        if (FInvestigation* Item = Active())
        {
            const FString Detail = Context.IsEmpty() ? ToolLabel(ToolId) : FString::Printf(TEXT("%s (%s)"), *ToolLabel(ToolId), *Context);
            Touch(*Item, TEXT("Opened ") + Detail);
        }
        RouteTool(ToolId);
    }

    void RecordEvidence(const FString& SourceTool, const FString& Summary, const FString& Target)
    {
        if (!Active())
        {
            FInvestigation NewItem;
            NewItem.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
            NewItem.Name = TEXT("Collected investigation");
            NewItem.CreatedUtc = NewItem.UpdatedUtc = NowUtc();
            Investigations.Insert(MoveTemp(NewItem), 0);
            ActiveIndex = 0;
        }
        if (FInvestigation* Item = Active())
        {
            FString Line = FString::Printf(TEXT("%s | %s: %s"), *NowUtc(), *SourceTool, *Summary);
            if (!Target.IsEmpty()) Line += TEXT(" | ") + Target;
            Item->Evidence.Insert(MoveTemp(Line), 0);
            if (Item->Evidence.Num() > 100) Item->Evidence.SetNum(100);
            Touch(*Item, TEXT("Added evidence from ") + SourceTool);
        }
    }
}
