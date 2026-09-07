#include "TMAudioPlaybackTrace.h"
#include "TMInvestigationSession.h"
#include "TMPerformanceGuard.h"
#include "TMTraceNoisePolicy.h"
#include "TMStyle.h"



#include "TMLocalization.h"



#include "TMReportFormatter.h"





#include "Components/AudioComponent.h"

#include "Components/SceneComponent.h"

#include "Editor.h"

#include "Engine/Engine.h"

#include "Engine/World.h"

#include "Framework/Application/SlateApplication.h"

#include "Framework/Docking/TabManager.h"

#include "HAL/PlatformApplicationMisc.h"

#include "HAL/PlatformTime.h"

#include "Misc/DateTime.h"

#include "Sound/SoundBase.h"

#include "Styling/AppStyle.h"

#include "ToolMenus.h"

#include "UObject/Script.h"

#include "UObject/Stack.h"

#include "UObject/UObjectIterator.h"

#include "UObject/UObjectGlobals.h"

#include "Widgets/Docking/SDockTab.h"

#include "Widgets/Images/SImage.h"

#include "Widgets/Input/SButton.h"

#include "Widgets/Layout/SBorder.h"

#include "Widgets/Layout/SScrollBox.h"

#include "Widgets/Layout/SSeparator.h"

#include "Widgets/SBoxPanel.h"

#include "Widgets/InvalidateWidgetReason.h"

#include "Widgets/Text/STextBlock.h"



#define LOCTEXT_NAMESPACE "TMAudioPlaybackTrace"



namespace

{

    const FName AudioTraceTabId(TEXT("TraceMotive.AudioPlaybackTrace"));

    TWeakPtr<SDockTab> ExistingAudioTraceTab;

    bool bAudioTraceTabSpawnerRegistered = false;



    FString GetAudioTraceWorldTypeText(const UWorld* World)

    {

        if (!World)

        {

            return TEXT("<No World>");

        }



        switch (World->WorldType)

        {

        case EWorldType::PIE:

            return TEXT("PIE");

        case EWorldType::Editor:

            return TEXT("Editor");

        case EWorldType::Game:

            return TEXT("Game");

        case EWorldType::EditorPreview:

            return TEXT("EditorPreview");

        default:

            return TEXT("Other");

        }

    }



    FString GetAudioTraceActorLabelSafe(const AActor* Actor)

    {

        if (!Actor)

        {

            return TEXT("<No Actor>");

        }



#if WITH_EDITOR

        return Actor->GetActorLabel();

#else

        return Actor->GetName();

#endif

    }



    FString GetObjectNameSafe(const UObject* Object)

    {

        return Object ? Object->GetName() : FString(TEXT("<None>"));

    }



    FString GetClassNameSafe(const UObject* Object)

    {

        return Object && Object->GetClass() ? Object->GetClass()->GetName() : FString(TEXT("<No Class>"));

    }



    AActor* ResolveAudioTraceOwnerActor(const UObject* Object)

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



    FString NormalizeAudioTraceBlueprintFunctionName(const UFunction* Function)

    {

        if (!Function)

        {

            return FString();

        }



        FString FunctionName = Function->GetName();

        if (FunctionName == TEXT("ReceiveBeginPlay"))

        {

            return TEXT("BeginPlay");

        }



        if (FunctionName == TEXT("UserConstructionScript"))

        {

            return TEXT("ConstructionScript");

        }



        return FunctionName;

    }



    FString AudioPlayStateToText(EAudioComponentPlayState PlayState)

    {

        switch (PlayState)

        {

        case EAudioComponentPlayState::Playing:

            return TEXT("Playing");

        case EAudioComponentPlayState::Stopped:

            return TEXT("Stopped");

        case EAudioComponentPlayState::Paused:

            return TEXT("Paused");

        case EAudioComponentPlayState::FadingIn:

            return TEXT("FadingIn");

        case EAudioComponentPlayState::FadingOut:

            return TEXT("FadingOut");

        default:

            return TEXT("Unknown");

        }

    }



    bool IsAudioActiveState(EAudioComponentPlayState PlayState)

    {

        return PlayState == EAudioComponentPlayState::Playing

            || PlayState == EAudioComponentPlayState::FadingIn;

    }



    FSlateFontInfo AudioTraceFont(const FName StyleName, int32 Size)

    {

        return FCoreStyle::GetDefaultFontStyle(StyleName, Size);

    }



    struct FTMAudioTraceEvent

    {

        int32 EventId = 0;

        double RelativeSeconds = 0.0;

        FDateTime Timestamp;

        FString State;

        FString SoundName;

        FString ComponentName;

        FString OwnerActorName;

        FString OwnerClassName;

        FString WorldType;

        FString SourceSummary;

        FString LocationText;

        int32 RepeatCount = 1;

    };



    struct FTMRecentAudioScriptSource

    {

        FString Summary;

        FString ActorName;

        FString ClassName;

        FString FunctionName;

        double TimestampSeconds = 0.0;

    };



    class STMAudioPlaybackTraceWidget : public SCompoundWidget

    {

    public:

        SLATE_BEGIN_ARGS(STMAudioPlaybackTraceWidget) {}

        SLATE_END_ARGS()



        ~STMAudioPlaybackTraceWidget() override

        {

            UnregisterHooks();

        }



        void Construct(const FArguments&)

        {

            SessionStartSeconds = FPlatformTime::Seconds();

            RegisterHooks();



            ChildSlot

            [

                SNew(SBorder)

                    .Padding(10.0f)

                    .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

                    [

                        SNew(SVerticalBox)

                            + SVerticalBox::Slot().AutoHeight()

                            [

                                BuildHeader()

                            ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)

                            [

                                SNew(SSeparator)

                            ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)

                            [

                                SAssignNew(StatusText, STextBlock)

                                    .Text(this, &STMAudioPlaybackTraceWidget::GetStatusText)

                                    .Font(AudioTraceFont(TEXT("Regular"), 9))

                                    .ColorAndOpacity(FLinearColor(0.66f, 0.72f, 0.80f))

                            ]

                            + SVerticalBox::Slot().FillHeight(1.0f)

                            [

                                SAssignNew(EventScrollBox, SScrollBox)

                            ]

                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)

                            [

                                SAssignNew(LogPreviewText, STextBlock)

                                    .Text(TMLoc::Text(TEXT("Open this window before PIE to capture audio playback sources."), TEXT("PIE 시작 전에 이 창을 열어 오디오 재생 출처를 캡처하세요.")))

                                    .Font(AudioTraceFont(TEXT("Regular"), 8))

                                    .ColorAndOpacity(FLinearColor(0.55f, 0.58f, 0.64f))

                            ]

                    ]

            ];



            DiscoverExistingAudioComponents();

            RefreshEventList();

        }



    private:

        TSharedRef<SWidget> BuildHeader()

        {

            return SNew(SHorizontalBox)

                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

                [

                    SNew(SVerticalBox)

                        + SVerticalBox::Slot().AutoHeight()

                        [

                            SNew(STextBlock)

                                .Text(TMLoc::Text(TEXT("Audio Playback Trace"), TEXT("Audio Playback Trace")))

                                .Font(AudioTraceFont(TEXT("Bold"), 15))

                        ]

                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)

                        [

                            SNew(STextBlock)

                                .Text(TMLoc::Text(TEXT("Detects AudioComponent playback during Editor/PIE and records the best matched Blueprint or actor source."), TEXT("Detects AudioComponent playback during Editor/PIE and records the best matched Blueprint or actor source.")))

                                .AutoWrapText(true)

                                .Font(AudioTraceFont(TEXT("Regular"), 8))

                                .ColorAndOpacity(FLinearColor(0.62f, 0.66f, 0.72f))

                        ]

                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(8.0f, 0.0f, 0.0f, 0.0f)

                [

                    BuildHeaderButton(FName(TEXT("Icons.Refresh")), TMLoc::Text(TEXT("Reset"), TEXT("초기화")), FOnClicked::CreateSP(this, &STMAudioPlaybackTraceWidget::OnResetClicked))

                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(4.0f, 0.0f, 0.0f, 0.0f)

                [

                    BuildHeaderButton(FName(TEXT("Icons.Plus")), TMLoc::Text(TEXT("Add to Investigation"), TEXT("조사에 추가")), FOnClicked::CreateLambda([this]()

                    {

                        TMInvestigationSession::RecordEvidence(TEXT("Audio Playback Trace"), FString::Printf(TEXT("Captured audio playback events: %d"), Events.Num()));

                        TMInvestigationSession::OpenWindow();

                        return FReply::Handled();

                    }))

                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(4.0f, 0.0f, 0.0f, 0.0f)

                [

                    BuildHeaderButton(FName(TEXT("Icons.Clipboard")), TMLoc::Text(TEXT("Copy Report"), TEXT("리포트 복사")), FOnClicked::CreateSP(this, &STMAudioPlaybackTraceWidget::OnCopyReportClicked))

                ];

        }



        TSharedRef<SWidget> BuildHeaderButton(const FName IconName, const FText& Label, FOnClicked OnClicked) const

        {

            return SNew(SButton)

                .ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))

                .ContentPadding(FMargin(8.0f, 5.0f))

                .OnClicked(OnClicked)

                [

                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                        [

                            SNew(SImage)

                                .Image(FAppStyle::GetBrush(IconName))

                                .ColorAndOpacity(FSlateColor::UseForeground())

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5.0f, 0.0f, 0.0f, 0.0f)

                        [

                            SNew(STextBlock)

                                .Text(Label)

                                .Font(AudioTraceFont(TEXT("Regular"), 8))

                        ]

                ];

        }



        FText GetStatusText() const

        {

            return FText::FromString(FString::Printf(

                TEXT("Events=%d | Active Audio=%d | Hooked Components=%d | PIE=%s | Session %.1fs"),

                Events.Num(),

                ActiveComponentKeys.Num(),

                HookedComponents.Num(),

                bPieActive ? TEXT("Running") : TEXT("Idle"),

                FMath::Max(0.0, FPlatformTime::Seconds() - SessionStartSeconds)));

        }



        void RegisterHooks()

        {

            if (ObjectConstructedHandle.IsValid())

            {

                return;

            }



            ObjectConstructedHandle = FCoreUObjectDelegates::OnObjectConstructed.AddSP(this, &STMAudioPlaybackTraceWidget::HandleObjectConstructed);

            PreBeginPIEHandle = FEditorDelegates::PreBeginPIE.AddSP(this, &STMAudioPlaybackTraceWidget::HandlePreBeginPIE);

            PostPIEStartedHandle = FEditorDelegates::PostPIEStarted.AddSP(this, &STMAudioPlaybackTraceWidget::HandlePostPIEStarted);

            EndPIEHandle = FEditorDelegates::EndPIE.AddSP(this, &STMAudioPlaybackTraceWidget::HandleEndPIE);

#if DO_BLUEPRINT_GUARD

            BlueprintScriptEnterHandle = FBlueprintContextTracker::OnEnterScriptContext.AddSP(this, &STMAudioPlaybackTraceWidget::HandleBlueprintScriptEnter);

            BlueprintScriptExitHandle = FBlueprintContextTracker::OnExitScriptContext.AddSP(this, &STMAudioPlaybackTraceWidget::HandleBlueprintScriptExit);

#endif

        }



        void UnregisterHooks()

        {

            if (ObjectConstructedHandle.IsValid())

            {

                FCoreUObjectDelegates::OnObjectConstructed.Remove(ObjectConstructedHandle);

                ObjectConstructedHandle.Reset();

            }



            if (PreBeginPIEHandle.IsValid())

            {

                FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);

                PreBeginPIEHandle.Reset();

            }



            if (PostPIEStartedHandle.IsValid())

            {

                FEditorDelegates::PostPIEStarted.Remove(PostPIEStartedHandle);

                PostPIEStartedHandle.Reset();

            }



            if (EndPIEHandle.IsValid())

            {

                FEditorDelegates::EndPIE.Remove(EndPIEHandle);

                EndPIEHandle.Reset();

            }



#if DO_BLUEPRINT_GUARD

            if (BlueprintScriptEnterHandle.IsValid())

            {

                FBlueprintContextTracker::OnEnterScriptContext.Remove(BlueprintScriptEnterHandle);

                BlueprintScriptEnterHandle.Reset();

            }



            if (BlueprintScriptExitHandle.IsValid())

            {

                FBlueprintContextTracker::OnExitScriptContext.Remove(BlueprintScriptExitHandle);

                BlueprintScriptExitHandle.Reset();

            }

#endif



            for (const TWeakObjectPtr<UAudioComponent>& WeakComponent : HookedComponents)

            {

                if (UAudioComponent* AudioComponent = WeakComponent.Get())

                {

                    AudioComponent->OnAudioPlayStateChangedNative.RemoveAll(this);

                    AudioComponent->OnAudioFinishedNative.RemoveAll(this);

                }

            }



            HookedComponents.Reset();

            HookedComponentKeys.Reset();

        }



        void DiscoverExistingAudioComponents()

        {

            for (TObjectIterator<UAudioComponent> It; It; ++It)

            {

                RegisterAudioComponent(*It);

            }

        }



        void HandleObjectConstructed(UObject* Object)

        {

            RegisterAudioComponent(Cast<UAudioComponent>(Object));

        }



        void RegisterAudioComponent(UAudioComponent* AudioComponent)

        {

            if (!AudioComponent || AudioComponent->IsTemplate())

            {

                return;

            }



            UWorld* World = AudioComponent->GetWorld();

            if (!World || (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game && World->WorldType != EWorldType::Editor))

            {

                return;

            }



            const uint64 ComponentKey = reinterpret_cast<uint64>(AudioComponent);

            if (HookedComponentKeys.Contains(ComponentKey))

            {

                return;

            }



            AudioComponent->OnAudioPlayStateChangedNative.AddSP(this, &STMAudioPlaybackTraceWidget::HandleAudioPlayStateChanged);

            AudioComponent->OnAudioFinishedNative.AddSP(this, &STMAudioPlaybackTraceWidget::HandleAudioFinished);

            HookedComponents.Add(AudioComponent);

            HookedComponentKeys.Add(ComponentKey);



            if (AudioComponent->IsPlaying() || IsAudioActiveState(AudioComponent->GetPlayState()))

            {

                AddAudioEvent(AudioComponent, AudioComponent->GetPlayState(), TEXT("AlreadyPlaying"));

            }

        }



        void HandleAudioPlayStateChanged(const UAudioComponent* AudioComponent, EAudioComponentPlayState PlayState)

        {

            UAudioComponent* MutableAudioComponent = const_cast<UAudioComponent*>(AudioComponent);

            if (!MutableAudioComponent || MutableAudioComponent->IsTemplate())

            {

                return;

            }



            const uint64 ComponentKey = reinterpret_cast<uint64>(MutableAudioComponent);

            const bool bWasActive = ActiveComponentKeys.Contains(ComponentKey);

            const bool bIsActive = IsAudioActiveState(PlayState);



            if (bIsActive)

            {

                ActiveComponentKeys.Add(ComponentKey);

                if (!bWasActive)

                {

                    AddAudioEvent(MutableAudioComponent, PlayState, TEXT("PlaybackStarted"));

                }

                return;

            }



            if (bWasActive && (PlayState == EAudioComponentPlayState::Stopped || PlayState == EAudioComponentPlayState::FadingOut || PlayState == EAudioComponentPlayState::Paused))

            {

                if (PlayState == EAudioComponentPlayState::Stopped)

                {

                    ActiveComponentKeys.Remove(ComponentKey);

                }

                AddAudioEvent(MutableAudioComponent, PlayState, TEXT("PlaybackStateChanged"));

            }

        }



        void HandleAudioFinished(UAudioComponent* AudioComponent)

        {

            if (!AudioComponent)

            {

                return;

            }



            ActiveComponentKeys.Remove(reinterpret_cast<uint64>(AudioComponent));

            AddAudioEvent(AudioComponent, EAudioComponentPlayState::Stopped, TEXT("PlaybackFinished"));

        }



        void AddAudioEvent(UAudioComponent* AudioComponent, EAudioComponentPlayState PlayState, const FString& Reason)

        {

            if (!AudioComponent)

            {

                return;

            }



            USoundBase* Sound = AudioComponent->Sound;

            AActor* OwnerActor = ResolveAudioTraceOwnerActor(AudioComponent);

            const double NowSeconds = FPlatformTime::Seconds();

            const FVector Location = AudioComponent->GetComponentLocation();

            const FString SourceSummary = BuildCurrentExecutionSummary(AudioComponent, Reason);

            const FString StateText = AudioPlayStateToText(PlayState);
            const FString SoundPath = Sound ? Sound->GetPathName() : FString(TEXT("<No Sound>"));
            const FString Signature = FString::Printf(TEXT("%p|%s|%s"), AudioComponent, *SoundPath, *StateText);

            const double RelativeNow = FMath::Max(0.0, NowSeconds - SessionStartSeconds);
            const int32 FirstCandidate = FMath::Max(0, Events.Num() - 32);
            for (int32 Index = Events.Num() - 1; Index >= FirstCandidate; --Index)
            {
                const TSharedPtr<FTMAudioTraceEvent>& ExistingEvent = Events[Index];
                if (!ExistingEvent.IsValid() || RelativeNow - ExistingEvent->RelativeSeconds > TMTraceNoise::DuplicateWindowSeconds(TEXT("Audio")))
                {
                    continue;
                }
                if (ExistingEvent->State == StateText
                    && ExistingEvent->SoundName == (Sound ? Sound->GetName() : FString(TEXT("<No Sound>")))
                    && ExistingEvent->ComponentName == AudioComponent->GetName())
                {
                    ++ExistingEvent->RepeatCount;
                    RefreshEventList();
                    return;
                }
            }

            TSharedPtr<FTMAudioTraceEvent> Event = MakeShared<FTMAudioTraceEvent>();

            Event->EventId = NextEventId++;

            Event->RelativeSeconds = FMath::Max(0.0, NowSeconds - SessionStartSeconds);

            Event->Timestamp = FDateTime::Now();

            Event->State = AudioPlayStateToText(PlayState);

            Event->SoundName = Sound ? Sound->GetName() : FString(TEXT("<No Sound>"));

            Event->ComponentName = AudioComponent->GetName();

            Event->OwnerActorName = OwnerActor ? GetAudioTraceActorLabelSafe(OwnerActor) : FString(TEXT("<No Owner Actor>"));

            Event->OwnerClassName = OwnerActor ? GetClassNameSafe(OwnerActor) : GetClassNameSafe(AudioComponent);

            Event->WorldType = GetAudioTraceWorldTypeText(AudioComponent->GetWorld());

            Event->SourceSummary = SourceSummary.IsEmpty() ? BuildFallbackSourceSummary(AudioComponent, Reason) : SourceSummary;

            Event->LocationText = FString::Printf(TEXT("X=%.1f,Y=%.1f,Z=%.1f"), Location.X, Location.Y, Location.Z);

            Events.Add(Event);

            LastEventSignature = Signature;



            while (Events.Num() > TMPerf::PIEEventHistoryLimit())

            {

                Events.RemoveAt(0);

            }



            if (LogPreviewText.IsValid())

            {

                LogPreviewText->SetText(FText::FromString(FString::Printf(TEXT("[%s] %s | Sound=%s | Actor=%s | Source=%s"),

                    *Event->Timestamp.ToString(TEXT("%H:%M:%S")),

                    *Event->State,

                    *Event->SoundName,

                    *Event->OwnerActorName,

                    *Event->SourceSummary)));

            }



            RefreshEventList();

        }



        FString BuildFallbackSourceSummary(UAudioComponent* AudioComponent, const FString& Reason) const

        {

            AActor* OwnerActor = ResolveAudioTraceOwnerActor(AudioComponent);

            USceneComponent* AttachParent = AudioComponent ? AudioComponent->GetAttachParent() : nullptr;

            return FString::Printf(TEXT("DetectedBy=%s | Actor=%s | Class=%s | AudioComponent=%s | AttachParent=%s"),

                *Reason,

                OwnerActor ? *GetAudioTraceActorLabelSafe(OwnerActor) : TEXT("<No Owner Actor>"),

                OwnerActor ? *GetClassNameSafe(OwnerActor) : *GetClassNameSafe(AudioComponent),

                AudioComponent ? *AudioComponent->GetName() : TEXT("<No Component>"),

                AttachParent ? *AttachParent->GetName() : TEXT("None"));

        }



        FString BuildCurrentExecutionSummary(UAudioComponent* AudioComponent, const FString& Reason) const

        {

#if DO_BLUEPRINT_GUARD

            if (const FBlueprintContextTracker* ContextTracker = FBlueprintContextTracker::TryGet())

            {

                FString Summary = BuildExecutionSummaryFromContext(ContextTracker, nullptr, nullptr, AudioComponent, Reason);

                if (!Summary.IsEmpty())

                {

                    return Summary;

                }

            }

#endif



            const double NowSeconds = FPlatformTime::Seconds();

            for (int32 Index = RecentScriptSources.Num() - 1; Index >= 0; --Index)

            {

                const FTMRecentAudioScriptSource& Source = RecentScriptSources[Index];

                if (NowSeconds - Source.TimestampSeconds > 0.35)

                {

                    continue;

                }



                return Source.Summary + TEXT(" | Confidence=RecentBlueprintContext");

            }



            return FString();

        }



#if DO_BLUEPRINT_GUARD

        FString BuildExecutionSummaryFromContext(

            const FBlueprintContextTracker* ContextTracker,

            const UObject* ContextObject,

            const UFunction* ContextFunction,

            const UObject* AudioObject,

            const FString& Reason) const

        {

            const UObject* SourceObject = ContextObject;

            const UFunction* SourceFunction = ContextFunction;

            const UFunction* NativeFunction = nullptr;

            TArray<FString> StackTokens;



            if (ContextTracker)

            {

                const TArrayView<const FFrame* const> ScriptStack = ContextTracker->GetCurrentScriptStack();

                for (int32 Index = ScriptStack.Num() - 1; Index >= 0; --Index)

                {

                    const FFrame* Frame = ScriptStack[Index];

                    if (!Frame)

                    {

                        continue;

                    }



                    if (!SourceObject && Frame->Object)

                    {

                        SourceObject = Frame->Object;

                    }



                    if (!SourceFunction && Frame->Node)

                    {

                        SourceFunction = Frame->Node;

                    }



                    if (Frame->CurrentNativeFunction)

                    {

                        const FString NativeName = Frame->CurrentNativeFunction->GetName();

                        if (!NativeFunction

                            || NativeName.Contains(TEXT("Sound"), ESearchCase::IgnoreCase)

                            || NativeName.Contains(TEXT("Audio"), ESearchCase::IgnoreCase)

                            || NativeName.Contains(TEXT("Play"), ESearchCase::IgnoreCase))

                        {

                            NativeFunction = Frame->CurrentNativeFunction;

                        }

                    }



                    if (Frame->Object && Frame->Node)

                    {

                        StackTokens.Add(FString::Printf(TEXT("%s.%s"), *GetObjectNameSafe(Frame->Object), *NormalizeAudioTraceBlueprintFunctionName(Frame->Node)));

                    }

                }

            }



            if (!SourceObject && !SourceFunction && !NativeFunction)

            {

                return FString();

            }



            AActor* SourceActor = ResolveAudioTraceOwnerActor(SourceObject);

            const FString ActorName = SourceActor ? GetAudioTraceActorLabelSafe(SourceActor) : GetObjectNameSafe(SourceObject);

            const FString ClassName = SourceObject ? GetClassNameSafe(SourceObject) : FString(TEXT("<No Class>"));

            const FString FunctionName = NormalizeAudioTraceBlueprintFunctionName(SourceFunction);

            const FString NativeCallName = NativeFunction ? NativeFunction->GetName() : FString(TEXT("<unknown native call>"));

            FString Summary = FString::Printf(TEXT("Execution=RuntimeBlueprint | Actor=%s | Class=%s | Function=%s | NativeCall=%s | AudioObject=%s | Reason=%s | Confidence=ActiveScriptStack"),

                *ActorName,

                *ClassName,

                FunctionName.IsEmpty() ? TEXT("<unknown function>") : *FunctionName,

                *NativeCallName,

                AudioObject ? *AudioObject->GetName() : TEXT("<unknown audio object>"),

                Reason.IsEmpty() ? TEXT("<unknown reason>") : *Reason);



            if (StackTokens.Num() > 0)

            {

                constexpr int32 MaxStackTokens = 5;

                if (StackTokens.Num() > MaxStackTokens)

                {

                    StackTokens.SetNum(MaxStackTokens);

                    StackTokens.Add(TEXT("..."));

                }



                Summary += FString::Printf(TEXT(" | ScriptStack=%s"), *FString::Join(StackTokens, TEXT(" > ")));

            }



            return Summary;

        }



        void HandleBlueprintScriptEnter(const FBlueprintContextTracker& ContextTracker, const UObject* ContextObject, const UFunction* ContextFunction)

        {

            if (!IsInGameThread())

            {

                return;

            }



            const FString Summary = BuildExecutionSummaryFromContext(&ContextTracker, ContextObject, ContextFunction, nullptr, TEXT("ScriptEnter"));

            if (Summary.IsEmpty())

            {

                return;

            }



            FTMRecentAudioScriptSource Source;

            Source.Summary = Summary;

            Source.TimestampSeconds = FPlatformTime::Seconds();

            RecentScriptSources.Add(MoveTemp(Source));



            constexpr int32 MaxRecentSources = 96;

            while (RecentScriptSources.Num() > MaxRecentSources)

            {

                RecentScriptSources.RemoveAt(0);

            }

        }



        void HandleBlueprintScriptExit(const FBlueprintContextTracker&)

        {

        }

#endif



        void HandlePreBeginPIE(bool)

        {

            bPieActive = true;

            SessionStartSeconds = FPlatformTime::Seconds();

            ActiveComponentKeys.Reset();

            RecentScriptSources.Reset();

            AddTimelineMessage(TMLoc::String(TEXT("PIE starting. Audio trace is armed."), TEXT("PIE 시작 중입니다. 오디오 추적기가 준비되었습니다.")));

        }



        void HandlePostPIEStarted(bool)

        {

            bPieActive = true;

            DiscoverExistingAudioComponents();

            AddTimelineMessage(TMLoc::String(TEXT("PIE started. Existing AudioComponents scanned."), TEXT("PIE가 시작되었습니다. 기존 AudioComponent를 스캔했습니다.")));

        }



        void HandleEndPIE(bool)

        {

            bPieActive = false;

            ActiveComponentKeys.Reset();

            RecentScriptSources.Reset();

            AddTimelineMessage(TMLoc::String(TEXT("PIE ended. Audio trace remains armed for the next play session."), TEXT("PIE가 종료되었습니다. 오디오 추적기는 다음 플레이 세션을 위해 대기합니다.")));

        }



        void AddTimelineMessage(const FString& Message)

        {

            if (LogPreviewText.IsValid())

            {

                LogPreviewText->SetText(FText::FromString(FString::Printf(TEXT("[%s] %s"), *FDateTime::Now().ToString(TEXT("%H:%M:%S")), *Message)));

            }

        }



        TSharedRef<SWidget> BuildEventCard(const TSharedPtr<FTMAudioTraceEvent>& Event) const

        {

            const FLinearColor AccentColor = Event->State == TEXT("Playing") || Event->State == TEXT("FadingIn")

                ? FLinearColor(0.18f, 0.72f, 0.36f)

                : FLinearColor(0.95f, 0.55f, 0.18f);



            return SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

                .Padding(0.0f)

                [

                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().AutoWidth()

                        [

                            SNew(SBorder)

                                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

                                .BorderBackgroundColor(AccentColor)

                                .Padding(FMargin(4.0f, 0.0f))

                        ]

                        + SHorizontalBox::Slot().FillWidth(1.0f)

                        [

                            SNew(SVerticalBox)

                                + SVerticalBox::Slot().AutoHeight().Padding(9.0f, 8.0f, 9.0f, 3.0f)

                                [

                                    SNew(STextBlock)

                                        .Text(FText::FromString(FString::Printf(TEXT("[%.2fs] %s | %s%s"),

                                            Event->RelativeSeconds,

                                            *Event->State,

                                            *Event->SoundName,

                                            Event->RepeatCount > 1 ? *FString::Printf(TEXT(" x%d"), Event->RepeatCount) : TEXT(""))))

                                        .Font(AudioTraceFont(TEXT("Bold"), 10))

                                        .ColorAndOpacity(AccentColor)

                                ]

                                + SVerticalBox::Slot().AutoHeight().Padding(9.0f, 0.0f, 9.0f, 2.0f)

                                [

                                    SNew(STextBlock)

                                        .Text(FText::FromString(FString::Printf(TEXT("Actor: %s   Class: %s   Component: %s   World: %s"),

                                            *Event->OwnerActorName,

                                            *Event->OwnerClassName,

                                            *Event->ComponentName,

                                            *Event->WorldType)))

                                        .AutoWrapText(true)

                                        .Font(AudioTraceFont(TEXT("Regular"), 8))

                                        .ColorAndOpacity(FLinearColor(0.72f, 0.76f, 0.82f))

                                ]

                                + SVerticalBox::Slot().AutoHeight().Padding(9.0f, 0.0f, 9.0f, 3.0f)

                                [

                                    SNew(STextBlock)

                                        .Text(FText::FromString(FString::Printf(TEXT("Location: %s"), *Event->LocationText)))

                                        .AutoWrapText(true)

                                        .Font(AudioTraceFont(TEXT("Regular"), 8))

                                        .ColorAndOpacity(FLinearColor(0.58f, 0.62f, 0.68f))

                                ]

                                + SVerticalBox::Slot().AutoHeight().Padding(9.0f, 0.0f, 9.0f, 9.0f)

                                [

                                    SNew(STextBlock)

                                        .Text(FText::FromString(FString::Printf(TEXT("Source: %s"), *Event->SourceSummary)))

                                        .AutoWrapText(true)

                                        .Font(AudioTraceFont(TEXT("Regular"), 8))

                                        .ColorAndOpacity(FLinearColor(0.82f, 0.84f, 0.88f))

                                ]

                        ]

                ];

        }



        void RefreshEventList()

        {

            if (!EventScrollBox.IsValid())

            {

                return;

            }



            EventScrollBox->ClearChildren();

            if (Events.IsEmpty())

            {

                EventScrollBox->AddSlot()

                [

                    SNew(STextBlock)

                        .Text(TMLoc::Text(TEXT("No audio playback captured yet. Keep this window open, then start PIE."), TEXT("아직 캡처된 오디오 재생이 없습니다. 이 창을 열어 둔 상태로 PIE를 시작하세요.")))

                        .Font(AudioTraceFont(TEXT("Regular"), 10))

                        .ColorAndOpacity(FLinearColor(0.55f, 0.58f, 0.64f))

                ];

                return;

            }



            const int32 StartIndex = FMath::Max(0, Events.Num() - TMPerf::PIEEventHistoryLimit());

            for (int32 Index = StartIndex; Index < Events.Num(); ++Index)

            {

                EventScrollBox->AddSlot().Padding(0.0f, 0.0f, 0.0f, 5.0f)

                [

                    BuildEventCard(Events[Index])

                ];

            }



            EventScrollBox->ScrollToEnd();

            if (StatusText.IsValid())

            {

                StatusText->Invalidate(EInvalidateWidgetReason::Paint);

            }

        }



        FReply OnResetClicked()

        {

            Events.Reset();

            ActiveComponentKeys.Reset();

            RecentScriptSources.Reset();

            LastEventSignature.Empty();

            NextEventId = 1;

            SessionStartSeconds = FPlatformTime::Seconds();

            DiscoverExistingAudioComponents();

            AddTimelineMessage(TMLoc::String(TEXT("Audio trace reset."), TEXT("오디오 추적 기록을 초기화했습니다.")));

            RefreshEventList();

            return FReply::Handled();

        }



        FString BuildReportText() const

        {

            TArray<FString> Lines;

            Lines.Add(TEXT("| Time | State | Sound | Actor | Class | Component | World | Source |"));

            Lines.Add(TEXT("|---:|---|---|---|---|---|---|---|"));



            for (const TSharedPtr<FTMAudioTraceEvent>& Event : Events)

            {

                if (!Event.IsValid())

                {

                    continue;

                }



                Lines.Add(FString::Printf(TEXT("| %.2fs | %s | %s | %s | %s | %s | %s | %s |"),

                    Event->RelativeSeconds,

                    *Event->State.Replace(TEXT("|"), TEXT("/")),

                    *Event->SoundName.Replace(TEXT("|"), TEXT("/")),

                    *Event->OwnerActorName.Replace(TEXT("|"), TEXT("/")),

                    *Event->OwnerClassName.Replace(TEXT("|"), TEXT("/")),

                    *Event->ComponentName.Replace(TEXT("|"), TEXT("/")),

                    *Event->WorldType.Replace(TEXT("|"), TEXT("/")),

                    *Event->SourceSummary.Replace(TEXT("|"), TEXT("/"))));

            }



            TArray<TMReportFormatter::FMetadataItem> Metadata;

            Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Events"), FString::FromInt(Events.Num())));

            Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Scope"), TEXT("PIE/runtime audio playback events")));



            return TMReportFormatter::BuildWrappedLegacyReport(

                TEXT("Audio Playback Trace"),

                FString::Printf(TEXT("- Captured audio playback events: %d\n- Confidence: source attribution is best-effort and may be heuristic."), Events.Num()),

                FString::Join(Lines, TEXT("\n")),

                Metadata);

        }



        FReply OnCopyReportClicked() const

        {

            FPlatformApplicationMisc::ClipboardCopy(*BuildReportText());

            return FReply::Handled();

        }



        TArray<TSharedPtr<FTMAudioTraceEvent>> Events;

        TArray<TWeakObjectPtr<UAudioComponent>> HookedComponents;

        TSet<uint64> HookedComponentKeys;

        TSet<uint64> ActiveComponentKeys;

        TArray<FTMRecentAudioScriptSource> RecentScriptSources;

        TSharedPtr<SScrollBox> EventScrollBox;

        TSharedPtr<STextBlock> StatusText;

        TSharedPtr<STextBlock> LogPreviewText;

        FDelegateHandle ObjectConstructedHandle;

        FDelegateHandle PreBeginPIEHandle;

        FDelegateHandle PostPIEStartedHandle;

        FDelegateHandle EndPIEHandle;

#if DO_BLUEPRINT_GUARD

        FDelegateHandle BlueprintScriptEnterHandle;

        FDelegateHandle BlueprintScriptExitHandle;

#endif

        FString LastEventSignature;

        double SessionStartSeconds = 0.0;

        int32 NextEventId = 1;

        bool bPieActive = false;

    };



    TSharedRef<SDockTab> SpawnAudioTraceTab(const FSpawnTabArgs&)

    {

        TSharedRef<SDockTab> Tab = SNew(SDockTab)

            .TabRole(ETabRole::NomadTab)

            .Label(TMLoc::Text(TEXT("Audio Playback Trace"), TEXT("Audio Playback Trace")))

            .OnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([](TSharedRef<SDockTab>)

            {

                ExistingAudioTraceTab.Reset();

            }))

            [

                SNew(STMAudioPlaybackTraceWidget)

            ];



        ExistingAudioTraceTab = Tab;

        return Tab;

    }



    void RegisterAudioTraceTabSpawner()

    {

        if (bAudioTraceTabSpawnerRegistered)

        {

            return;

        }



        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(

            AudioTraceTabId,

            FOnSpawnTab::CreateStatic(&SpawnAudioTraceTab))

            .SetDisplayName(TMLoc::Text(TEXT("Audio Playback Trace"), TEXT("Audio Playback Trace")))

            .SetIcon(FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.AudioPlaybackTrace")))

            .SetMenuType(ETabSpawnerMenuType::Hidden);



        bAudioTraceTabSpawnerRegistered = true;

    }

}



namespace TMAudioPlaybackTrace

{

    void RegisterMenus()

    {

        RegisterAudioTraceTabSpawner();



        if (UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window"))

        {

            FToolMenuSection& Section = WindowMenu->FindOrAddSection("TraceMotive");

            Section.AddMenuEntry(

                "TMOpenAudioPlaybackTrace",

                TMLoc::Text(TEXT("Audio Playback Trace"), TEXT("Audio Playback Trace")),

                TMLoc::Text(TEXT("Detect audio playback during PIE and show which actor, component, Blueprint function, or runtime source triggered it."), TEXT("Detect audio playback during PIE and show which actor, component, Blueprint function, or runtime source triggered it.")),

                FSlateIcon(TMStyle::GetStyleSetName(), "TraceMotive.AudioPlaybackTrace"),

                FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&)

                {

                    TMAudioPlaybackTrace::OpenWindow();

                })

            );

        }

    }



    void UnregisterMenus()

    {

        if (bAudioTraceTabSpawnerRegistered)

        {

            FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AudioTraceTabId);

            bAudioTraceTabSpawnerRegistered = false;

        }



        ExistingAudioTraceTab.Reset();

    }



    void OpenWindow()

    {

        RegisterAudioTraceTabSpawner();



        if (ExistingAudioTraceTab.IsValid())

        {

            FGlobalTabmanager::Get()->TryInvokeTab(AudioTraceTabId);

            return;

        }



        ExistingAudioTraceTab = FGlobalTabmanager::Get()->TryInvokeTab(AudioTraceTabId);

    }

}



#undef LOCTEXT_NAMESPACE












