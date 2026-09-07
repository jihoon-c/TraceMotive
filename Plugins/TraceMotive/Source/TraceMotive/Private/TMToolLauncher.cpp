#include "TMToolLauncher.h"
#include "TMStyle.h"

#include "TMAssetUsageLocator.h"
#include "TMAudioPlaybackTrace.h"
#include "TMBlueprintRuntimeErrorTrace.h"
#include "TMClassFavorites.h"
#include "TMClickDiagnostics.h"
#include "TMCollisionPairAnalyzer.h"
#include "TMContextShortcutHelper.h"
#include "TMDockTabHelper.h"
#include "TMEnhancedOutlinerSearch.h"
#include "TMGlobalSpeedControl.h"
#include "TMInvestigationSession.h"
#include "TMPerformanceGuard.h"
#include "TMLocalization.h"
#include "TMPackageProgress.h"
#include "TMPluginGuide.h"
#include "TMVariableValueTrace.h"
#include "TMWidgetLifecycleTrace.h"
#include "TMWidgetClickFlowTrace.h"
#include "SInstanceReferenceTracker.h"

#include "Editor.h"
#include "ContentBrowserModule.h"
#include "Containers/Ticker.h"
#include "IContentBrowserSingleton.h"
#include "Engine/Selection.h"
#include "GameFramework/Actor.h"
#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "ToolMenus.h"
#include "Modules/ModuleManager.h"
#include "ISettingsModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    const FName ToolLauncherTabId(TEXT("TraceMotive.ToolLauncher"));
    bool bToolLauncherTabRegistered = false;
    TWeakPtr<SDockTab> ExistingToolLauncherTab;

    FText LauncherText(const TCHAR* English, const TCHAR* Korean = nullptr)
    {
        return TMLoc::Text(English, Korean ? Korean : English);
    }

    FSlateFontInfo LauncherFont(const FName Style, int32 Size)
    {
        return FCoreStyle::GetDefaultFontStyle(Style, Size);
    }

    AActor* GetFirstSelectedActor()
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

    void OpenMessageWindow(const FString& Title, const FString& Body)
    {
        TMDockTab::OpenDockTab(
            TEXT("ToolLauncherMessage"),
            FText::FromString(Title),
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
            .Padding(16.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Body))
                .AutoWrapText(true)
            ]);
    }

    void OpenInstanceTraceForSelection()
    {
        if (AActor* Actor = GetFirstSelectedActor())
        {
            TMDockTab::OpenDockTab(
                TEXT("InstanceReferenceTrace"),
                FText::FromString(FString::Printf(TEXT("Instance Reference Trace - %s"), *Actor->GetActorLabel())),
                SNew(SInstanceReferenceTracker, Actor));
            return;
        }

        OpenMessageWindow(TEXT("Instance Reference Trace"), TEXT("Select an actor in the level, then open Instance Reference Trace again."));
    }

    void OpenVisualReferenceHelp()
    {
        OpenMessageWindow(
            TEXT("Visual Reference Search"),
            TEXT("Visual Reference Search is context-based. Open a Blueprint graph, right-click a variable/function/event/dispatcher, then choose Visual Find References or Visual Find Function References."));
    }

    void OpenAssetUsageForCurrentSelection()
    {
        if (FModuleManager::Get().IsModuleLoaded(TEXT("ContentBrowser")))
        {
            TArray<FAssetData> SelectedAssets;
            FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
            ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);
            if (!SelectedAssets.IsEmpty())
            {
                TMAssetUsageLocator::OpenWindowForAsset(SelectedAssets[0]);
                return;
            }
        }

        OpenMessageWindow(
            TEXT("Asset Usage Locator"),
            TEXT("Select an asset in the Content Browser, then click Asset Usage Locator again. You can also right-click the asset and choose Find Exact Blueprint Usage."));
    }

    void RefreshOpenToolTabsForLanguageChange()
    {
        static const TArray<FName> RefreshableTabIds = {
            TEXT("TraceMotive.AudioPlaybackTrace"),
            TEXT("TraceMotive.BlueprintRuntimeErrorTrace"),
            TEXT("TraceMotive.ClassFavorites"),
            TEXT("TraceMotive.ClickEventDiagnostics"),
            TEXT("TraceMotive.CollisionPairAnalyzer"),
            TEXT("TraceMotive.ContextShortcutHelper"),
            TEXT("TraceMotive.EnhancedOutlinerSearch"),
            TEXT("TraceMotive.GlobalSpeedControl"),
            TEXT("TraceMotive.InvestigationSession"),
            TEXT("TraceMotive.PackageProgress"),
            TEXT("TraceMotive.PluginGuide"),
            TEXT("TraceMotive.VariableValueTrace"),
            TEXT("TraceMotive.WidgetClickFlowTrace"),
            TEXT("TraceMotive.WidgetLifecycleTrace")
        };

        TArray<FName> TabsToReopen;
        for (const FName& TabId : RefreshableTabIds)
        {
            if (TSharedPtr<SDockTab> ExistingTab = FGlobalTabmanager::Get()->FindExistingLiveTab(TabId))
            {
                TabsToReopen.Add(TabId);
                ExistingTab->RequestCloseTab();
            }
        }

        if (!TabsToReopen.IsEmpty())
        {
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([TabsToReopen = MoveTemp(TabsToReopen)](float)
            {
                for (const FName& TabId : TabsToReopen)
                {
                    FGlobalTabmanager::Get()->TryInvokeTab(TabId);
                }
                return false;
            }));
        }
    }

    void OpenTraceMotiveSettings()
    {
        if (ISettingsModule* SettingsModule = FModuleManager::LoadModulePtr<ISettingsModule>(TEXT("Settings")))
        {
            SettingsModule->ShowViewer(TEXT("Project"), TEXT("Plugins"), TEXT("TraceMotive"));
        }
    }

    void StopAllActiveWork()
    {
        TMPerf::RequestCancelAll();
        TMPerf::SetSearchBoostEnabled(false);
        TMDockTab::CloseAll();

        static const TArray<FName> WorkTabIds = {
            TEXT("TraceMotive.AudioPlaybackTrace"),
            TEXT("TraceMotive.BlueprintRuntimeErrorTrace"),
            TEXT("TraceMotive.ClickEventDiagnostics"),
            TEXT("TraceMotive.CollisionPairAnalyzer"),
            TEXT("TraceMotive.EnhancedOutlinerSearch"),
            TEXT("TraceMotive.GlobalSpeedControl"),
            TEXT("TraceMotive.PackageProgress"),
            TEXT("TraceMotive.VariableValueTrace"),
            TEXT("TraceMotive.WidgetClickFlowTrace"),
            TEXT("TraceMotive.WidgetLifecycleTrace")
        };
        for (const FName& TabId : WorkTabIds)
        {
            if (TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->FindExistingLiveTab(TabId)) Tab->RequestCloseTab();
        }

        FNotificationInfo Info(LauncherText(TEXT("TraceMotive stopped active searches and traces."), TEXT("TraceMotive의 실행 중인 검색과 추적을 중지했습니다.")));
        Info.ExpireDuration = 4.0f;
        FSlateNotificationManager::Get().AddNotification(Info);
    }

    struct FLauncherTile
    {
        FName Icon;
        FText Name;
        FText Desc;
        FLinearColor Color;
        TFunction<void()> Action;
    };

    const FSlateBrush* GetLauncherIcon(const FName IconName)
    {
        if (const ISlateStyle* Style = FSlateStyleRegistry::FindSlateStyle(TMStyle::GetStyleSetName()))
        {
            const FString LauncherIconName = IconName.ToString().Replace(TEXT("TraceMotive."), TEXT("TraceMotive.Launcher."));
            return Style->GetBrush(FName(*LauncherIconName));
        }
        return FAppStyle::GetBrush(TEXT("Icons.Help"));
    }

    TSharedRef<SWidget> BuildTile(const FLauncherTile& Tile)
    {
        return SNew(SBox)
            .WidthOverride(170.0f)
            .HeightOverride(146.0f)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))
                .OnClicked_Lambda([Action = Tile.Action]()
                {
                    if (Action)
                    {
                        Action();
                    }
                    return FReply::Handled();
                })
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
                    .BorderBackgroundColor(FLinearColor(0.045f, 0.048f, 0.055f, 1.0f))
                    .Padding(10.0f)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 6)
                        [
                            SNew(SBorder)
                            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                            .BorderBackgroundColor(Tile.Color.CopyWithNewOpacity(0.10f))
                            .Padding(9.0f)
                            [
                                SNew(SImage)
                                .Image(GetLauncherIcon(Tile.Icon))
                                .ColorAndOpacity(Tile.Color)
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 4)
                        [
                            SNew(SBox)
                            .HeightOverride(30.0f)
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                .Text(Tile.Name)
                                .Justification(ETextJustify::Center)
                                .AutoWrapText(true)
                                .WrapTextAt(138.0f)
                                .Font(LauncherFont(TEXT("Bold"), 10))
                            ]
                        ]
                        + SVerticalBox::Slot().FillHeight(1.0f).VAlign(VAlign_Top)
                        [
                            SNew(STextBlock)
                            .Text(Tile.Desc)
                            .Justification(ETextJustify::Center)
                            .AutoWrapText(true)
                            .Font(LauncherFont(TEXT("Regular"), 8))
                            .ColorAndOpacity(FLinearColor(0.68f, 0.72f, 0.78f, 1.0f))
                        ]
                    ]
                ]
            ];
    }

    TSharedRef<SWidget> BuildCategory(const FText& Title, const TArray<FLauncherTile>& Tiles)
    {
        TSharedRef<SWrapBox> Wrap = SNew(SWrapBox).UseAllottedSize(true);
        for (const FLauncherTile& Tile : Tiles)
        {
            Wrap->AddSlot().Padding(5.0f)[BuildTile(Tile)];
        }

        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 7)
            [
                SNew(STextBlock)
                .Text(Title)
                .Font(LauncherFont(TEXT("Bold"), 13))
                .ColorAndOpacity(FLinearColor(0.88f, 0.92f, 0.98f, 1.0f))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                Wrap
            ];
    }

    class STMToolLauncherWidget : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(STMToolLauncherWidget) {}
        SLATE_END_ARGS()

        void Construct(const FArguments&)
        {
            ChildSlot
            [
                SAssignNew(ContentHost, SBox)
                [
                    BuildContent()
                ]
            ];
        }

    private:
        void SetLanguage(const bool bUseKorean)
        {
            if (TMLoc::UseKorean() == bUseKorean)
            {
                return;
            }

            TMLoc::SetUseKorean(bUseKorean);
            RefreshOpenToolTabsForLanguageChange();
            if (ContentHost.IsValid())
            {
                ContentHost->SetContent(BuildContent());
            }
        }

        TSharedRef<SWidget> BuildLanguageButton(const bool bForKorean)
        {
            return SNew(SCheckBox)
                .Type(ESlateCheckBoxType::ToggleButton)
                .Padding(0.0f)
                .ToolTipText(bForKorean
                    ? LauncherText(TEXT("Use Korean and refresh open TraceMotive tool tabs"), TEXT("한국어를 사용하고 열려 있는 TraceMotive 도구 탭을 새로고침합니다"))
                    : LauncherText(TEXT("Use English and refresh open TraceMotive tool tabs"), TEXT("영어를 사용하고 열려 있는 TraceMotive 도구 탭을 새로고침합니다")))
                .IsChecked_Lambda([bForKorean]()
                {
                    return TMLoc::UseKorean() == bForKorean ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                })
                .OnCheckStateChanged_Lambda([this, bForKorean](ECheckBoxState State)
                {
                    if (State == ECheckBoxState::Checked)
                    {
                        SetLanguage(bForKorean);
                    }
                })
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                    .BorderBackgroundColor_Lambda([bForKorean]()
                    {
                        return TMLoc::UseKorean() == bForKorean
                            ? FLinearColor(0.18f, 0.50f, 0.82f, 0.82f)
                            : FLinearColor(0.11f, 0.12f, 0.14f, 0.90f);
                    })
                    .Padding(FMargin(10.0f, 4.0f))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(bForKorean ? TEXT("한국어") : TEXT("EN")))
                        .Font(LauncherFont(TEXT("Bold"), 10))
                        .ColorAndOpacity_Lambda([bForKorean]()
                        {
                            return TMLoc::UseKorean() == bForKorean
                                ? FLinearColor::White
                                : FLinearColor(0.58f, 0.62f, 0.68f, 1.0f);
                        })
                    ]
                ]
                ;
        }

        TSharedRef<SWidget> BuildLanguageToggle()
        {
            return SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
                [
                    SNew(STextBlock)
                    .Text(LauncherText(TEXT("Language"), TEXT("언어")))
                    .Font(LauncherFont(TEXT("Regular"), 9))
                    .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                ]
                + SHorizontalBox::Slot().AutoWidth()[BuildLanguageButton(false)]
                + SHorizontalBox::Slot().AutoWidth().Padding(3, 0, 0, 0)[BuildLanguageButton(true)];
        }

        TSharedRef<SWidget> BuildContent()
        {
            // A restrained category palette keeps the launcher readable as more tools are added.
            const FLinearColor SearchColor = TMStyle::GetSearchColor();
            const FLinearColor RuntimeColor = TMStyle::GetRuntimeColor();
            const FLinearColor WorkflowColor = TMStyle::GetWorkflowColor();
            const TArray<FLauncherTile> ScenarioTiles = {
                { TEXT("TraceMotive.ClickEventDiagnostics"), LauncherText(TEXT("Click does not work"), TEXT("클릭이 동작하지 않음")), LauncherText(TEXT("Start input, hit-test, UMG, and Blueprint-flow diagnosis."), TEXT("입력, 히트 테스트, UMG와 Blueprint 흐름 진단을 시작합니다.")), RuntimeColor, [](){ TMInvestigationSession::StartScenario(TEXT("Click")); } },
                { TEXT("TraceMotive.CollisionPairAnalyzer"), LauncherText(TEXT("Actors pass through"), TEXT("액터가 서로 통과함")), LauncherText(TEXT("Analyze the two selected actors and continue into state tracing."), TEXT("선택한 두 액터를 분석하고 상태 추적으로 연결합니다.")), RuntimeColor, [](){ TMInvestigationSession::StartScenario(TEXT("Collision")); } },
                { TEXT("TraceMotive.AudioPlaybackTrace"), LauncherText(TEXT("Unknown audio source"), TEXT("알 수 없는 오디오 출처")), LauncherText(TEXT("Capture playback and follow the suspected owner."), TEXT("재생을 캡처하고 의심되는 소유자를 추적합니다.")), RuntimeColor, [](){ TMInvestigationSession::StartScenario(TEXT("Audio")); } },
                { TEXT("TraceMotive.RuntimeErrorTrace"), LauncherText(TEXT("Blueprint runtime error"), TEXT("Blueprint 런타임 오류")), LauncherText(TEXT("Collect live or completed-PIE errors and inspect the instance."), TEXT("실시간 또는 완료된 PIE 오류를 수집하고 인스턴스를 조사합니다.")), RuntimeColor, [](){ TMInvestigationSession::StartScenario(TEXT("RuntimeError")); } },
                { TEXT("TraceMotive.VariableValueTrace"), LauncherText(TEXT("Unexpected value change"), TEXT("예상치 못한 값 변경")), LauncherText(TEXT("Trace the value, instance, and related runtime evidence."), TEXT("값, 인스턴스와 관련 런타임 근거를 추적합니다.")), RuntimeColor, [](){ TMInvestigationSession::StartScenario(TEXT("Variable")); } },
                { TEXT("TraceMotive.PackageProgress"), LauncherText(TEXT("Packaging failed or stalled"), TEXT("패키징 실패 또는 정체")), LauncherText(TEXT("Monitor the job and preserve failure investigation notes."), TEXT("작업을 모니터링하고 실패 조사 기록을 보존합니다.")), WorkflowColor, [](){ TMInvestigationSession::StartScenario(TEXT("Packaging")); } }
            };
            const TArray<FLauncherTile> SearchTiles = {
                { TEXT("TraceMotive.VisualReferenceSearch"), LauncherText(TEXT("Visual Reference Search"), TEXT("시각 참조 검색")), LauncherText(TEXT("Find variable/function references from Blueprint context."), TEXT("Blueprint 컨텍스트에서 변수와 함수 참조를 찾습니다.")), SearchColor, [](){ OpenVisualReferenceHelp(); } },
                { TEXT("TraceMotive.EnhancedOutlinerSearch"), LauncherText(TEXT("Enhanced Outliner Search"), TEXT("향상된 아웃라이너 검색")), LauncherText(TEXT("Search actors by name, tag, variable, value, and actor arrays."), TEXT("이름, 태그, 변수, 값, 액터 배열로 액터를 검색합니다.")), SearchColor, [](){ TMEnhancedOutlinerSearch::OpenWindow(); } },
                { TEXT("TraceMotive.AssetUsageLocator"), LauncherText(TEXT("Asset Usage Locator"), TEXT("에셋 사용 위치")), LauncherText(TEXT("Analyze the currently selected Content Browser asset."), TEXT("콘텐츠 브라우저에서 현재 선택한 에셋을 분석합니다.")), SearchColor, [](){ OpenAssetUsageForCurrentSelection(); } }
            };

            const TArray<FLauncherTile> RuntimeTiles = {
                { TEXT("TraceMotive.CollisionPairAnalyzer"), LauncherText(TEXT("Collision Pair Analyzer"), TEXT("충돌 쌍 분석")), LauncherText(TEXT("Analyze why two selected actors do or do not block."), TEXT("선택한 두 액터의 충돌 여부를 분석합니다.")), RuntimeColor, [](){ TMInvestigationSession::OpenTool(TEXT("Collision")); } },
                { TEXT("TraceMotive.ClickEventDiagnostics"), LauncherText(TEXT("Click Event Diagnostics"), TEXT("클릭 이벤트 진단")), LauncherText(TEXT("Trace UI, input, collision, and click delivery."), TEXT("UI, 입력, 충돌, 클릭 전달 과정을 추적합니다.")), RuntimeColor, [](){ TMInvestigationSession::OpenTool(TEXT("Click")); } },
                { TEXT("TraceMotive.AudioPlaybackTrace"), LauncherText(TEXT("Audio Playback Trace"), TEXT("오디오 재생 추적")), LauncherText(TEXT("Detect audio playback and likely source."), TEXT("오디오 재생과 추정 출처를 감지합니다.")), RuntimeColor, [](){ TMInvestigationSession::OpenTool(TEXT("Audio")); } },
                { TEXT("TraceMotive.VariableValueTrace"), LauncherText(TEXT("Variable Value Trace"), TEXT("변수 값 추적")), LauncherText(TEXT("Track selected class property changes during PIE."), TEXT("PIE 중 선택한 클래스 프로퍼티 변화를 추적합니다.")), RuntimeColor, [](){ TMInvestigationSession::OpenTool(TEXT("Variable")); } },
                { TEXT("TraceMotive.WidgetLifecycleTrace"), LauncherText(TEXT("Widget Lifecycle Trace"), TEXT("위젯 생명주기 추적")), LauncherText(TEXT("Track live widgets and visibility changes."), TEXT("실행 중 위젯과 가시성 변화를 추적합니다.")), RuntimeColor, [](){ TMInvestigationSession::OpenTool(TEXT("WidgetLifecycle")); } },
                { TEXT("TraceMotive.WidgetClickFlowTrace"), LauncherText(TEXT("Widget Click Flow Trace"), TEXT("위젯 클릭 흐름 추적")), LauncherText(TEXT("Capture the Blueprint flow after one PIE UMG click."), TEXT("PIE UMG 클릭 후 Blueprint 흐름을 캡처합니다.")), RuntimeColor, [](){ TMInvestigationSession::OpenTool(TEXT("WidgetClickFlow")); } },
                { TEXT("TraceMotive.InstanceReferenceTracker"), LauncherText(TEXT("Instance Reference Trace"), TEXT("인스턴스 참조 추적")), LauncherText(TEXT("Trace selected actor references and runtime state."), TEXT("선택한 액터 참조와 런타임 상태를 추적합니다.")), RuntimeColor, [](){ OpenInstanceTraceForSelection(); } },
                { TEXT("TraceMotive.RuntimeErrorTrace"), LauncherText(TEXT("Runtime Error Investigation"), TEXT("런타임 오류 분석")), LauncherText(TEXT("Live Blueprint errors and completed PIE log analysis in one tool."), TEXT("실시간 Blueprint 오류와 완료된 PIE 로그를 한 도구에서 분석합니다.")), RuntimeColor, [](){ TMInvestigationSession::OpenTool(TEXT("RuntimeError")); } }
            };

            const TArray<FLauncherTile> WorkflowTiles = {
                { TEXT("TraceMotive.PackageProgress"), LauncherText(TEXT("Package Progress"), TEXT("패키징 진행")), LauncherText(TEXT("Readable packaging progress and failure hints."), TEXT("읽기 쉬운 패키징 진행 상황과 실패 힌트를 제공합니다.")), WorkflowColor, [](){ TMInvestigationSession::OpenTool(TEXT("Packaging")); } },
                { TEXT("TraceMotive.ToolLauncher"), LauncherText(TEXT("Investigation Sessions"), TEXT("조사 세션")), LauncherText(TEXT("Save investigations, continue across tools, and compare Before/After."), TEXT("조사를 저장하고 도구 간 연결 및 전후 비교를 수행합니다.")), WorkflowColor, [](){ TMInvestigationSession::OpenWindow(); } },
                { TEXT("TraceMotive.PluginGuide"), LauncherText(TEXT("TraceMotive Settings"), TEXT("TraceMotive 설정")), LauncherText(TEXT("Configure safe mode, budgets, limits, history, and reports."), TEXT("안전 모드, 작업 예산, 제한, 기록과 리포트를 설정합니다.")), WorkflowColor, [](){ OpenTraceMotiveSettings(); } },
                { TEXT("TraceMotive.RuntimeErrorTrace"), LauncherText(TEXT("Stop All Active Work"), TEXT("모든 작업 중지")), LauncherText(TEXT("Cancel searches and close active diagnostic traces."), TEXT("검색을 취소하고 실행 중인 진단 추적을 닫습니다.")), FLinearColor(0.95f, 0.30f, 0.24f), [](){ StopAllActiveWork(); } },
                { TEXT("TraceMotive.GlobalSpeedControl"), LauncherText(TEXT("Global Speed Control"), TEXT("전역 배속 제어")), LauncherText(TEXT("Time dilation, audio speed, and Skip to Target."), TEXT("시간 배율, 오디오 속도, 타깃까지 건너뛰기를 제어합니다.")), WorkflowColor, [](){ TMGlobalSpeedControl::OpenWindow(); } },
                { TEXT("TraceMotive.ContextShortcutGuide"), LauncherText(TEXT("Context Shortcut Guide"), TEXT("컨텍스트 단축키 안내")), LauncherText(TEXT("Show shortcuts for the clicked editor context."), TEXT("클릭한 에디터 컨텍스트의 단축키를 표시합니다.")), WorkflowColor, [](){ TMContextShortcutHelper::OpenWindow(); } },
                { TEXT("TraceMotive.ClassFavorites"), LauncherText(TEXT("Asset & Level Favorites"), TEXT("에셋 및 레벨 즐겨찾기")), LauncherText(TEXT("Favorite placeable assets, actor classes, and levels."), TEXT("배치 가능한 에셋, 액터 클래스와 레벨을 즐겨찾습니다.")), WorkflowColor, [](){ TMClassFavorites::OpenFavoritesWindow(); } },
                { TEXT("TraceMotive.PluginGuide"), LauncherText(TEXT("Plugin Guide"), TEXT("플러그인 안내")), LauncherText(TEXT("Open the full feature guide."), TEXT("전체 기능 안내를 엽니다.")), WorkflowColor, [](){ TMPluginGuide::OpenWindow(); } }
            };

            return SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
                .Padding(12.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                    [
                        SNew(STextBlock)
                        .Text(LauncherText(TEXT("TraceMotiveTools — Debug Pathfinder for Unreal")))
                        .AutoWrapText(true)
                        .Font(LauncherFont(TEXT("Bold"), 18))
                    ]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 0, 0, 4)
                    [
                        BuildLanguageToggle()
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
                    [
                        SNew(STextBlock)
                        .Text(LauncherText(TEXT("Launch TraceMotive tools by category. Context-only tools use the current selection or show a short instruction window."), TEXT("카테고리별 TraceMotive 도구를 실행합니다. 컨텍스트 전용 도구는 현재 선택을 사용하거나 짧은 안내 창을 표시합니다.")))
                        .AutoWrapText(true)
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                    [SNew(SSeparator)]
                    + SVerticalBox::Slot().FillHeight(1.0f)
                    [
                        SNew(SScrollBox)
                        + SScrollBox::Slot().Padding(0, 0, 0, 14)[BuildCategory(LauncherText(TEXT("Quick Diagnosis"), TEXT("빠른 진단")), ScenarioTiles)]
                        + SScrollBox::Slot().Padding(0, 0, 0, 14)[BuildCategory(LauncherText(TEXT("Search / Reference"), TEXT("검색 / 참조")), SearchTiles)]
                        + SScrollBox::Slot().Padding(0, 0, 0, 14)[BuildCategory(LauncherText(TEXT("Runtime Diagnostics"), TEXT("런타임 진단")), RuntimeTiles)]
                        + SScrollBox::Slot().Padding(0, 0, 0, 14)[BuildCategory(LauncherText(TEXT("Workflow / Utilities"), TEXT("워크플로 / 유틸리티")), WorkflowTiles)]
                    ]
                ];
        }

        TSharedPtr<SBox> ContentHost;
    };

    TSharedRef<SDockTab> SpawnToolLauncherTab(const FSpawnTabArgs&)
    {
        TSharedRef<SDockTab> Tab = SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            .Label(LauncherText(TEXT("TraceMotiveTools")))
            .OnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([](TSharedRef<SDockTab>) { ExistingToolLauncherTab.Reset(); }))
            [SNew(STMToolLauncherWidget)];
        ExistingToolLauncherTab = Tab;
        return Tab;
    }

    void RegisterToolLauncherTab()
    {
        if (bToolLauncherTabRegistered)
        {
            return;
        }

        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ToolLauncherTabId, FOnSpawnTab::CreateStatic(&SpawnToolLauncherTab))
            .SetDisplayName(LauncherText(TEXT("TraceMotiveTools")))
            .SetTooltipText(LauncherText(TEXT("Open the TraceMotive — Debug Pathfinder for Unreal tool launcher.")))
            .SetIcon(FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.ToolLauncher")));
        bToolLauncherTabRegistered = true;
    }

    void PopulateTraceMotiveToolsMenu(UToolMenu* Menu)
    {
        if (!Menu)
        {
            return;
        }

        auto AddEntry = [Menu](const FName SectionName, const FName EntryName, const FText& Label, const FText& Tooltip, const FName IconName, TFunction<void()> Action)
        {
            Menu->FindOrAddSection(SectionName).AddMenuEntry(
                EntryName,
                Label,
                Tooltip,
                FSlateIcon(TMStyle::GetStyleSetName(), IconName),
                FToolMenuExecuteAction::CreateLambda([Action = MoveTemp(Action)](const FToolMenuContext&) { Action(); }));
        };

        AddEntry(TEXT("TraceMotiveMain"), TEXT("TMToolsOpenLauncher"), LauncherText(TEXT("Open TraceMotiveTools"), TEXT("TraceMotiveTools 열기")), LauncherText(TEXT("Open the categorized TraceMotive tool launcher."), TEXT("카테고리별 TraceMotive 도구 런처를 엽니다.")), TEXT("TraceMotive.ToolLauncher"), [](){ TMToolLauncher::OpenWindow(); });
        AddEntry(TEXT("TraceMotiveMain"), TEXT("TMToolsOpenInvestigations"), LauncherText(TEXT("Investigation Sessions"), TEXT("조사 세션")), LauncherText(TEXT("Save investigations, route between tools, and compare snapshots."), TEXT("조사를 저장하고 도구 간 이동 및 스냅샷 비교를 수행합니다.")), TEXT("TraceMotive.ToolLauncher"), [](){ TMInvestigationSession::OpenWindow(); });
        AddEntry(TEXT("TraceMotiveMain"), TEXT("TMToolsOpenSettings"), LauncherText(TEXT("TraceMotive Settings"), TEXT("TraceMotive 설정")), LauncherText(TEXT("Configure performance and diagnostic defaults."), TEXT("성능 및 진단 기본값을 설정합니다.")), TEXT("TraceMotive.PluginGuide"), [](){ OpenTraceMotiveSettings(); });
        AddEntry(TEXT("TraceMotiveMain"), TEXT("TMToolsStopAll"), LauncherText(TEXT("Stop All Active Work"), TEXT("모든 작업 중지")), LauncherText(TEXT("Cancel active TraceMotive searches and traces."), TEXT("실행 중인 TraceMotive 검색과 추적을 중지합니다.")), TEXT("TraceMotive.RuntimeErrorTrace"), [](){ StopAllActiveWork(); });
        AddEntry(TEXT("TraceMotiveQuickDiagnosis"), TEXT("TMQuickClick"), LauncherText(TEXT("Diagnose: Click does not work"), TEXT("진단: 클릭이 동작하지 않음")), LauncherText(TEXT("Start a persisted click investigation."), TEXT("클릭 조사 세션을 시작합니다.")), TEXT("TraceMotive.ClickEventDiagnostics"), [](){ TMInvestigationSession::StartScenario(TEXT("Click")); });
        AddEntry(TEXT("TraceMotiveQuickDiagnosis"), TEXT("TMQuickCollision"), LauncherText(TEXT("Diagnose: Actors pass through"), TEXT("진단: 액터가 서로 통과함")), LauncherText(TEXT("Start a persisted collision investigation."), TEXT("충돌 조사 세션을 시작합니다.")), TEXT("TraceMotive.CollisionPairAnalyzer"), [](){ TMInvestigationSession::StartScenario(TEXT("Collision")); });
        AddEntry(TEXT("TraceMotiveQuickDiagnosis"), TEXT("TMQuickAudio"), LauncherText(TEXT("Diagnose: Unknown audio source"), TEXT("진단: 알 수 없는 오디오 출처")), LauncherText(TEXT("Start a persisted audio investigation."), TEXT("오디오 조사 세션을 시작합니다.")), TEXT("TraceMotive.AudioPlaybackTrace"), [](){ TMInvestigationSession::StartScenario(TEXT("Audio")); });
        AddEntry(TEXT("TraceMotiveQuickDiagnosis"), TEXT("TMQuickRuntimeError"), LauncherText(TEXT("Diagnose: Blueprint runtime error"), TEXT("진단: Blueprint 런타임 오류")), LauncherText(TEXT("Start a persisted runtime-error investigation."), TEXT("런타임 오류 조사 세션을 시작합니다.")), TEXT("TraceMotive.RuntimeErrorTrace"), [](){ TMInvestigationSession::StartScenario(TEXT("RuntimeError")); });
        AddEntry(TEXT("TraceMotiveQuickDiagnosis"), TEXT("TMQuickVariable"), LauncherText(TEXT("Diagnose: Unexpected value change"), TEXT("진단: 예상치 못한 값 변경")), LauncherText(TEXT("Start a persisted value-change investigation."), TEXT("값 변경 조사 세션을 시작합니다.")), TEXT("TraceMotive.VariableValueTrace"), [](){ TMInvestigationSession::StartScenario(TEXT("Variable")); });
        AddEntry(TEXT("TraceMotiveQuickDiagnosis"), TEXT("TMQuickPackaging"), LauncherText(TEXT("Diagnose: Packaging failed or stalled"), TEXT("진단: 패키징 실패 또는 정체")), LauncherText(TEXT("Start a persisted packaging investigation."), TEXT("패키징 조사 세션을 시작합니다.")), TEXT("TraceMotive.PackageProgress"), [](){ TMInvestigationSession::StartScenario(TEXT("Packaging")); });
        AddEntry(TEXT("TraceMotiveSearch"), TEXT("TMToolsOpenEnhancedOutlinerSearch"), LauncherText(TEXT("Enhanced Outliner Search"), TEXT("향상된 아웃라이너 검색")), LauncherText(TEXT("Search actors by name, tag, variable, and value."), TEXT("이름, 태그, 변수와 값으로 액터를 검색합니다.")), TEXT("TraceMotive.EnhancedOutlinerSearch"), [](){ TMEnhancedOutlinerSearch::OpenWindow(); });
        AddEntry(TEXT("TraceMotiveSearch"), TEXT("TMToolsOpenAssetUsageLocator"), LauncherText(TEXT("Asset Usage Locator"), TEXT("에셋 사용 위치")), LauncherText(TEXT("Analyze the selected Content Browser asset."), TEXT("콘텐츠 브라우저에서 선택한 에셋을 분석합니다.")), TEXT("TraceMotive.AssetUsageLocator"), [](){ OpenAssetUsageForCurrentSelection(); });
        AddEntry(TEXT("TraceMotiveRuntime"), TEXT("TMToolsOpenCollisionAnalyzer"), LauncherText(TEXT("Collision Pair Analyzer"), TEXT("충돌 쌍 분석")), LauncherText(TEXT("Analyze the two selected actors."), TEXT("선택한 두 액터를 분석합니다.")), TEXT("TraceMotive.CollisionPairAnalyzer"), [](){ TMInvestigationSession::OpenTool(TEXT("Collision")); });
        AddEntry(TEXT("TraceMotiveRuntime"), TEXT("TMToolsOpenClickDiagnostics"), LauncherText(TEXT("Click Event Diagnostics"), TEXT("클릭 이벤트 진단")), LauncherText(TEXT("Trace click and touch delivery."), TEXT("클릭과 터치 전달을 추적합니다.")), TEXT("TraceMotive.ClickEventDiagnostics"), [](){ TMInvestigationSession::OpenTool(TEXT("Click")); });
        AddEntry(TEXT("TraceMotiveRuntime"), TEXT("TMToolsOpenAudioPlaybackTrace"), LauncherText(TEXT("Audio Playback Trace"), TEXT("오디오 재생 추적")), LauncherText(TEXT("Trace audio playback during PIE."), TEXT("PIE 중 오디오 재생을 추적합니다.")), TEXT("TraceMotive.AudioPlaybackTrace"), [](){ TMInvestigationSession::OpenTool(TEXT("Audio")); });
        AddEntry(TEXT("TraceMotiveRuntime"), TEXT("TMToolsOpenVariableValueTrace"), LauncherText(TEXT("Variable Value Trace"), TEXT("변수 값 추적")), LauncherText(TEXT("Trace property changes during PIE."), TEXT("PIE 중 프로퍼티 변화를 추적합니다.")), TEXT("TraceMotive.VariableValueTrace"), [](){ TMInvestigationSession::OpenTool(TEXT("Variable")); });
        AddEntry(TEXT("TraceMotiveRuntime"), TEXT("TMToolsOpenWidgetLifecycleTrace"), LauncherText(TEXT("Widget Lifecycle Trace"), TEXT("위젯 생명주기 추적")), LauncherText(TEXT("Trace live widget and visibility changes."), TEXT("실행 중 위젯과 가시성 변화를 추적합니다.")), TEXT("TraceMotive.WidgetLifecycleTrace"), [](){ TMInvestigationSession::OpenTool(TEXT("WidgetLifecycle")); });
        AddEntry(TEXT("TraceMotiveRuntime"), TEXT("TMToolsOpenWidgetClickFlowTrace"), LauncherText(TEXT("Widget Click Flow Trace"), TEXT("위젯 클릭 흐름 추적")), LauncherText(TEXT("Capture the Blueprint flow after one UMG click."), TEXT("UMG 클릭 후 Blueprint 흐름을 캡처합니다.")), TEXT("TraceMotive.WidgetClickFlowTrace"), [](){ TMInvestigationSession::OpenTool(TEXT("WidgetClickFlow")); });
        AddEntry(TEXT("TraceMotiveRuntime"), TEXT("TMToolsOpenInstanceReferenceTrace"), LauncherText(TEXT("Instance Reference Trace"), TEXT("인스턴스 참조 추적")), LauncherText(TEXT("Trace the selected actor instance."), TEXT("선택한 액터 인스턴스를 추적합니다.")), TEXT("TraceMotive.InstanceReferenceTracker"), [](){ OpenInstanceTraceForSelection(); });
        AddEntry(TEXT("TraceMotiveRuntime"), TEXT("TMToolsOpenRuntimeErrors"), LauncherText(TEXT("Runtime Error Investigation"), TEXT("런타임 오류 분석")), LauncherText(TEXT("Investigate live and completed PIE errors."), TEXT("실시간 및 완료된 PIE 오류를 분석합니다.")), TEXT("TraceMotive.RuntimeErrorTrace"), [](){ TMInvestigationSession::OpenTool(TEXT("RuntimeError")); });
        AddEntry(TEXT("TraceMotiveWorkflow"), TEXT("TMToolsOpenPackageProgress"), LauncherText(TEXT("Package Progress"), TEXT("패키징 진행")), LauncherText(TEXT("Monitor packaging stages and failures."), TEXT("패키징 단계와 실패 원인을 확인합니다.")), TEXT("TraceMotive.PackageProgress"), [](){ TMInvestigationSession::OpenTool(TEXT("Packaging")); });
        AddEntry(TEXT("TraceMotiveWorkflow"), TEXT("TMToolsOpenGlobalSpeedControl"), LauncherText(TEXT("Global Speed Control"), TEXT("전역 배속 제어")), LauncherText(TEXT("Control PIE speed and audio pitch."), TEXT("PIE 배속과 오디오 피치를 제어합니다.")), TEXT("TraceMotive.GlobalSpeedControl"), [](){ TMGlobalSpeedControl::OpenWindow(); });
        AddEntry(TEXT("TraceMotiveWorkflow"), TEXT("TMToolsOpenContextShortcuts"), LauncherText(TEXT("Context Shortcut Guide"), TEXT("컨텍스트 단축키 안내")), LauncherText(TEXT("Show shortcuts for the active editor context."), TEXT("활성 에디터 컨텍스트의 단축키를 표시합니다.")), TEXT("TraceMotive.ContextShortcutGuide"), [](){ TMContextShortcutHelper::OpenWindow(); });
        AddEntry(TEXT("TraceMotiveWorkflow"), TEXT("TMToolsOpenFavorites"), LauncherText(TEXT("Asset & Level Favorites"), TEXT("에셋 및 레벨 즐겨찾기")), LauncherText(TEXT("Open placeable asset and level favorites."), TEXT("배치 가능한 에셋과 레벨 즐겨찾기를 엽니다.")), TEXT("TraceMotive.ClassFavorites"), [](){ TMClassFavorites::OpenFavoritesWindow(); });
        AddEntry(TEXT("TraceMotiveHelp"), TEXT("TMToolsOpenPluginGuide"), LauncherText(TEXT("Plugin Guide"), TEXT("플러그인 안내")), LauncherText(TEXT("Open the complete TraceMotive guide."), TEXT("TraceMotive 전체 안내를 엽니다.")), TEXT("TraceMotive.PluginGuide"), [](){ TMPluginGuide::OpenWindow(); });
    }
}

namespace TMToolLauncher
{
    void RegisterMenus()
    {
        RegisterToolLauncherTab();

        UToolMenus* ToolMenus = UToolMenus::Get();
        UToolMenu* WindowMenu = ToolMenus->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"));
        UToolMenu* ToolsMenu = ToolMenus->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));

        // Individual features register their spawners and context actions first. Replace
        // their duplicated main-menu sections with one launcher and one categorized submenu.
        WindowMenu->RemoveSection(TEXT("TraceMotive"));
        ToolsMenu->RemoveSection(TEXT("TraceMotive"));

        WindowMenu->FindOrAddSection(TEXT("TraceMotive")).AddMenuEntry(
            TEXT("TMOpenTraceMotiveTools"),
            LauncherText(TEXT("TraceMotiveTools")),
            LauncherText(TEXT("Open the categorized TraceMotive tool launcher."), TEXT("카테고리별 TraceMotive 도구 런처를 엽니다.")),
            FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.ToolLauncher")),
            FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&) { TMToolLauncher::OpenWindow(); }));

        ToolsMenu->FindOrAddSection(TEXT("TraceMotive")).AddSubMenu(
            TEXT("TraceMotive"),
            LauncherText(TEXT("TraceMotive")),
            LauncherText(TEXT("Open TraceMotive tools by category."), TEXT("카테고리별 TraceMotive 도구를 엽니다.")),
            FNewToolMenuDelegate::CreateStatic(&PopulateTraceMotiveToolsMenu),
            false,
            FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.ToolLauncher")));
    }

    void UnregisterMenus()
    {
        if (bToolLauncherTabRegistered)
        {
            FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ToolLauncherTabId);
            bToolLauncherTabRegistered = false;
        }
        ExistingToolLauncherTab.Reset();
    }

    void OpenWindow()
    {
        RegisterToolLauncherTab();
        ExistingToolLauncherTab = FGlobalTabmanager::Get()->TryInvokeTab(ToolLauncherTabId);
    }
}


