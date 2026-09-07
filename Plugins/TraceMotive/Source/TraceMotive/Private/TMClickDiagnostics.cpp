#include "TMClickDiagnostics.h"
#include "TMInvestigationSession.h"
#include "TMPerformanceGuard.h"
#include "TMTraceNoisePolicy.h"
#include "TMStyle.h"







#include "TMLocalization.h"







#include "AssetRegistry/AssetRegistryModule.h"



#include "Blueprint/UserWidget.h"



#include "Components/PrimitiveComponent.h"
#include "Components/Widget.h"



#include "ContentBrowserModule.h"



#include "IContentBrowserSingleton.h"



#include "Editor.h"



#include "Editor/EditorEngine.h"



#include "EdGraph/EdGraph.h"



#include "EdGraph/EdGraphNode.h"



#include "Engine/Blueprint.h"



#include "Engine/Engine.h"

#include "Engine/EngineTypes.h"



#include "Engine/Level.h"



#include "Engine/LevelScriptBlueprint.h"



#include "Engine/Selection.h"



#include "Engine/World.h"



#include "EngineUtils.h"



#include "EnhancedActionKeyMapping.h"



#include "EnhancedInputComponent.h"



#include "Framework/Application/IInputProcessor.h"



#include "Framework/Application/SlateApplication.h"



#include "Framework/Docking/TabManager.h"



#include "GameFramework/Actor.h"



#include "GameFramework/GameModeBase.h"



#include "GameFramework/PlayerController.h"



#include "GameFramework/WorldSettings.h"



#include "HAL/PlatformApplicationMisc.h"



#include "InputAction.h"



#include "InputCoreTypes.h"



#include "InputMappingContext.h"



#include "GameFramework/InputSettings.h"



#include "InputTriggers.h"



#include "K2Node_CallFunction.h"



#include "K2Node_ActorBoundEvent.h"



#include "K2Node_ComponentBoundEvent.h"



#include "K2Node_EnhancedInputAction.h"



#include "K2Node_InputAction.h"



#include "K2Node_InputActionEvent.h"

#include "K2Node_InputTouch.h"

#include "K2Node_InputTouchEvent.h"



#include "K2Node_Event.h"



#include "Layout/WidgetPath.h"



#include "Modules/ModuleManager.h"



#include "Misc/PackageName.h"



#include "Slate/SObjectWidget.h"



#include "Styling/AppStyle.h"



#include "ToolMenus.h"
#include "Types/ReflectionMetadata.h"



#include "WidgetBlueprint.h"



#include "Widgets/Docking/SDockTab.h"



#include "Widgets/Input/SButton.h"



#include "Widgets/Layout/SBorder.h"



#include "Widgets/Layout/SBox.h"



#include "Widgets/Layout/SScrollBox.h"



#include "Widgets/Layout/SSeparator.h"



#include "Widgets/Layout/SWidgetSwitcher.h"



#include "Widgets/SBoxPanel.h"



#include "Widgets/SCompoundWidget.h"



#include "Widgets/Text/STextBlock.h"







namespace



{



    const FName ClickDiagnosticsTabId(TEXT("TraceMotive.ClickEventDiagnostics"));







    enum class EDiagnosticMode : uint8 { Static, Armed, Live };



    enum class EDiagnosticVerdict : uint8 { ConfirmedPass, Blocked, NeedsRun, Skip, Passed, Unreached };



    enum class EDiagnosticTargetType : uint8 { None, Actor, Widget };







    struct FDiagnosticStep



    {



        int32 Number = 0;



        FString Name;



        FString Detail;



        FString Suggestion;



        EDiagnosticVerdict Verdict = EDiagnosticVerdict::NeedsRun;



        int32 CaptureCount = 0;



    };







    struct FInputAuditItem



    {



        FString System;



        FString Context;



        FString Action;



        FString Key;



        FString Status;



        FString Detail;



        bool bWarning = false;



    };







    struct FBindingItem



    {



        FString Action;



        FString Blueprint;



        FString Graph;



        FString Binding;



        bool bDead = false;



    };








    struct FClickCaptureHistoryItem

    {

        FString Summary;

        FString Target;

        FString EventClass;

        FString ClickedObject;

        FString BindingClasses;

        FVector2D ScreenPosition = FVector2D::ZeroVector;

        double TimeSeconds = 0.0;

        EDiagnosticVerdict Verdict = EDiagnosticVerdict::NeedsRun;

        bool bConsumedByUI = false;

    };

    struct FProjectInputScan



    {



        TArray<FInputAuditItem> AuditItems;



        TSet<FString> BoundEnhancedActionPaths;



        TSet<FName> BoundLegacyActions;



        TSet<FName> MappedLegacyActions;



        TArray<FString> EnhancedActionPaths;



        int32 WidgetBlueprintCount = 0;



        bool bScanned = false;



    };







    FString GetVerdictText(EDiagnosticVerdict Verdict)

    {

        switch (Verdict)

        {

        case EDiagnosticVerdict::ConfirmedPass: return TMLoc::String(TEXT("Confirmed - Normal"), TEXT("확인됨 - 정상"));

        case EDiagnosticVerdict::Blocked: return TMLoc::String(TEXT("Blocked"), TEXT("차단됨"));

        case EDiagnosticVerdict::NeedsRun: return TMLoc::String(TEXT("Run required"), TEXT("실행 필요"));

        case EDiagnosticVerdict::Skip: return TMLoc::String(TEXT("Skip"), TEXT("건너뜀"));

        case EDiagnosticVerdict::Passed: return TMLoc::String(TEXT("Passed"), TEXT("통과"));

        default: return TMLoc::String(TEXT("Not reached"), TEXT("도달하지 않음"));

        }

    }



    FString GetVerdictIcon(EDiagnosticVerdict Verdict)



    {



        switch (Verdict)



        {



        case EDiagnosticVerdict::ConfirmedPass:



        case EDiagnosticVerdict::Passed: return TEXT("OK");



        case EDiagnosticVerdict::Blocked: return TEXT("BLOCK");



        case EDiagnosticVerdict::NeedsRun: return TEXT("RUN");



        default: return TEXT("-");



        }



    }







    FLinearColor GetVerdictColor(EDiagnosticVerdict Verdict)



    {



        switch (Verdict)



        {



        case EDiagnosticVerdict::ConfirmedPass:



        case EDiagnosticVerdict::Passed: return FLinearColor(0.22f, 0.76f, 0.36f);



        case EDiagnosticVerdict::Blocked: return FLinearColor(0.94f, 0.27f, 0.22f);



        case EDiagnosticVerdict::NeedsRun: return FLinearColor(0.96f, 0.67f, 0.18f);



        default: return FLinearColor(0.48f, 0.51f, 0.56f);



        }



    }







    FString GetModeText(EDiagnosticMode Mode)



    {



        switch (Mode)



        {



        case EDiagnosticMode::Armed: return TEXT("ARMED");



        case EDiagnosticMode::Live: return TEXT("LIVE");



        default: return TEXT("STATIC");



        }



    }







    void GetBlueprintGraphs(UBlueprint* Blueprint, TArray<UEdGraph*>& OutGraphs)



    {



        OutGraphs.Reset();



        if (Blueprint)



        {



            Blueprint->GetAllGraphs(OutGraphs);



        }



    }







    FProjectInputScan ScanProjectInput()



    {



        FProjectInputScan Result;



        Result.bScanned = true;



        IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();







        TArray<FAssetData> ActionAssets;



        Registry.GetAssetsByClass(UInputAction::StaticClass()->GetClassPathName(), ActionAssets, true);



        for (const FAssetData& Asset : ActionAssets)



        {



            Result.EnhancedActionPaths.Add(Asset.GetSoftObjectPath().ToString());



        }







        TArray<FAssetData> ContextAssets;



        Registry.GetAssetsByClass(UInputMappingContext::StaticClass()->GetClassPathName(), ContextAssets, true);



        TMap<FKey, TSet<FString>> ActionsByKey;



        for (const FAssetData& Asset : ContextAssets)



        {



            UInputMappingContext* Context = Cast<UInputMappingContext>(Asset.GetAsset());



            if (!Context) continue;



            for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())



            {



                const FString ActionName = Mapping.Action ? Mapping.Action->GetName() : TEXT("<None>");



                if (Mapping.Action)



                {



                    ActionsByKey.FindOrAdd(Mapping.Key).Add(Mapping.Action->GetPathName());



                }



                FInputAuditItem& Row = Result.AuditItems.AddDefaulted_GetRef();



                Row.System = TEXT("Enhanced");



                Row.Context = Context->GetName();



                Row.Action = ActionName;



                Row.Key = Mapping.Key.GetDisplayName().ToString();



                const int32 ActionTriggerCount = Mapping.Action ? Mapping.Action->Triggers.Num() : 0;



                Row.bWarning = !Mapping.Action || (Mapping.Triggers.IsEmpty() && ActionTriggerCount == 0);



                Row.Status = !Mapping.Action ? TEXT("Invalid") : (Row.bWarning ? TEXT("Review") : TEXT("Mapped"));



                Row.Detail = !Mapping.Action



                    ? TEXT("Mapping has no InputAction.")



                    : FString::Printf(TEXT("Mapping triggers=%d, action triggers=%d, modifiers=%d"), Mapping.Triggers.Num(), ActionTriggerCount, Mapping.Modifiers.Num());



            }



        }







        for (FInputAuditItem& Row : Result.AuditItems)



        {



            for (const TPair<FKey, TSet<FString>>& Pair : ActionsByKey)



            {



                if (Pair.Key.GetDisplayName().ToString() == Row.Key && Pair.Value.Num() > 1)



                {



                    Row.bWarning = true;



                    Row.Status = TEXT("Key conflict");



                    Row.Detail += FString::Printf(TEXT("; same key is mapped to %d actions across contexts"), Pair.Value.Num());



                    break;



                }



            }



        }







        const UInputSettings* InputSettings = GetDefault<UInputSettings>();



        if (InputSettings)



        {



            for (const FInputActionKeyMapping& Mapping : InputSettings->GetActionMappings())



            {



                Result.MappedLegacyActions.Add(Mapping.ActionName);



                FInputAuditItem& Row = Result.AuditItems.AddDefaulted_GetRef();



                Row.System = TEXT("Legacy Action");



                Row.Context = TEXT("Project Settings");



                Row.Action = Mapping.ActionName.ToString();



                Row.Key = Mapping.Key.GetDisplayName().ToString();



                Row.Status = TEXT("Mapped");



                Row.Detail = TEXT("Legacy action mapping");



            }



            for (const FInputAxisKeyMapping& Mapping : InputSettings->GetAxisMappings())



            {



                FInputAuditItem& Row = Result.AuditItems.AddDefaulted_GetRef();



                Row.System = TEXT("Legacy Axis");



                Row.Context = TEXT("Project Settings");



                Row.Action = Mapping.AxisName.ToString();



                Row.Key = Mapping.Key.GetDisplayName().ToString();



                Row.Status = TEXT("Mapped");



                Row.Detail = FString::Printf(TEXT("Scale %.2f"), Mapping.Scale);



            }



        }







        TArray<FAssetData> WidgetAssets;



        Registry.GetAssetsByClass(UWidgetBlueprint::StaticClass()->GetClassPathName(), WidgetAssets, true);



        Result.WidgetBlueprintCount = WidgetAssets.Num();



        return Result;



    }







    UBlueprint* GetBlueprintForActor(AActor* Actor)



    {



        return Actor && Actor->GetClass() ? Cast<UBlueprint>(Actor->GetClass()->ClassGeneratedBy) : nullptr;



    }







    APlayerController* GetEditorPlayerControllerCDO()



    {



        TSubclassOf<AGameModeBase> GameModeClass;



        if (GEditor)



        {



            if (UWorld* World = GEditor->GetEditorWorldContext().World())



            {



                if (AWorldSettings* Settings = World->GetWorldSettings())



                {



                    GameModeClass = Settings->DefaultGameMode;



                }



            }



        }



        const AGameModeBase* GameModeCDO = GameModeClass ? GameModeClass->GetDefaultObject<AGameModeBase>() : GetDefault<AGameModeBase>();



        UClass* ControllerClass = GameModeCDO && GameModeCDO->PlayerControllerClass ? GameModeCDO->PlayerControllerClass.Get() : APlayerController::StaticClass();



        return ControllerClass ? ControllerClass->GetDefaultObject<APlayerController>() : nullptr;



    }







    class FClickDiagnosticsSession;

    bool IsPIEViewportPath(const FWidgetPath& Path)
    {
        if (!Path.IsValid()) return false;

        for (const FArrangedWidget& ArrangedWidget : Path.Widgets.GetInternalArray())
        {
            const FName Type = ArrangedWidget.Widget->GetType();
            if (Type == FName(TEXT("SViewport"))
                || Type == FName(TEXT("SLevelViewport"))
                || Type == FName(TEXT("SGameLayerManager")))
            {
                return true;
            }
        }
        return false;
    }







    class FClickInputObserver final : public IInputProcessor



    {



    public:



        explicit FClickInputObserver(FClickDiagnosticsSession& InSession) : Session(InSession) {}



        virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}



        virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;



        virtual const TCHAR* GetDebugName() const override { return TEXT("TMClickDiagnostics"); }



    private:



        FClickDiagnosticsSession& Session;



    };







    class FClickDiagnosticsSession



    {



    public:



        void Start()



        {



            if (!BeginPIEHandle.IsValid()) BeginPIEHandle = FEditorDelegates::BeginPIE.AddRaw(this, &FClickDiagnosticsSession::OnBeginPIE);



            if (!EndPIEHandle.IsValid()) EndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &FClickDiagnosticsSession::OnEndPIE);



            ResetSteps();



        }







        void Stop()



        {



            RemoveLiveHooks();



            if (BeginPIEHandle.IsValid()) FEditorDelegates::BeginPIE.Remove(BeginPIEHandle);



            if (EndPIEHandle.IsValid()) FEditorDelegates::EndPIE.Remove(EndPIEHandle);



            BeginPIEHandle.Reset();



            EndPIEHandle.Reset();



        }







        void RefreshProjectScan() { ProjectScan = ScanProjectInput(); }







        void SelectActor(AActor* Actor)



        {



            EnsureProjectScan();



            bPickNextClickTarget = false;



            TargetType = Actor ? EDiagnosticTargetType::Actor : EDiagnosticTargetType::None;



            TargetActor = Actor;



            TargetWidget = nullptr;



            TargetName = Actor ? Actor->GetActorLabel() : TEXT("No target");



            TargetBlueprint = GetBlueprintForActor(Actor);



            Mode = Actor ? (GEditor && GEditor->PlayWorld ? EDiagnosticMode::Live : EDiagnosticMode::Armed) : EDiagnosticMode::Static;



            BuildStaticPipeline();



            if (Mode == EDiagnosticMode::Live && !InputObserver.IsValid()) InstallLiveHooks();



        }







        void SelectWidget(UWidgetBlueprint* Widget)



        {



            EnsureProjectScan();



            bPickNextClickTarget = false;



            TargetType = Widget ? EDiagnosticTargetType::Widget : EDiagnosticTargetType::None;



            TargetActor = nullptr;



            TargetWidget = Widget;



            TargetName = Widget ? Widget->GetName() : TEXT("No target");



            TargetBlueprint = Widget;



            Mode = Widget ? (GEditor && GEditor->PlayWorld ? EDiagnosticMode::Live : EDiagnosticMode::Armed) : EDiagnosticMode::Static;



            BuildStaticPipeline();



            if (Mode == EDiagnosticMode::Live && !InputObserver.IsValid()) InstallLiveHooks();



        }







        void ClearTarget()



        {



            bPickNextClickTarget = false;



            TargetType = EDiagnosticTargetType::None;



            TargetActor = nullptr;



            TargetWidget = nullptr;



            TargetBlueprint = nullptr;



            TargetName = TEXT("No target");



            Mode = EDiagnosticMode::Static;



            RemoveLiveHooks();



            TargetBindingItems.Reset();



            ResetSteps();



        }







        void ArmNextClickTarget()



        {



            EnsureProjectScan();



            bPickNextClickTarget = true;



            TargetType = EDiagnosticTargetType::None;



            TargetActor = nullptr;



            TargetWidget = nullptr;



            TargetBlueprint = nullptr;



            TargetName = TEXT("Waiting for the next PIE click...");



            TargetBindingItems.Reset();



            ResetSteps();



            Mode = GEditor && GEditor->PlayWorld ? EDiagnosticMode::Live : EDiagnosticMode::Armed;



            if (Mode == EDiagnosticMode::Live)



            {



                InstallLiveHooks();



            }



        }







        void OnRawPointer(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)



        {



            if (Mode != EDiagnosticMode::Live) return;

            const FWidgetPath Path = SlateApp.LocateWindowUnderMouse(
                MouseEvent.GetScreenSpacePosition(),
                SlateApp.GetInteractiveTopLevelWindows(),
                false,
                MouseEvent.GetUserIndex());

            // The input preprocessor also receives clicks on editor tools. Those clicks must not
            // be diagnosed as game input or reuse a stale PlayerController cursor hit.
            if (!IsPIEViewportPath(Path)) return;

            LastEventClass.Empty();
            LastClickedObject.Empty();



            const bool bIsTouch = MouseEvent.IsTouchEvent();

            const FString InputKind = bIsTouch

                ? FString::Printf(TEXT("touch pointer=%u"), MouseEvent.GetPointerIndex())

                : FString::Printf(TEXT("mouse button=%s"), *MouseEvent.GetEffectingButton().ToString());



            ++TotalClicks;



            const FString RawInputDetail = FString::Printf(TEXT("Raw %s reached Slate and the editor PIE input loop at screen=(%.1f, %.1f)."),

                *InputKind,

                MouseEvent.GetScreenSpacePosition().X,

                MouseEvent.GetScreenSpacePosition().Y);

            if (bPickNextClickTarget && !ResolveTargetFromClick(SlateApp, MouseEvent))
            {
                PassStep(0, RawInputDetail);
                BlockAt(5,
                    TEXT("The pointer reached the PIE viewport, but no hit-testable UMG widget or Visibility-blocking actor was found."),
                    TEXT("Check UMG hit-test visibility or make the intended actor component Block the Visibility channel."));
                RecordCaptureHistory(MouseEvent);
                return;
            }

            // ResolveTargetFromClick rebuilds the static pipeline for its selected target.
            // Apply the live pointer evidence after that rebuild so the Slate stage is not lost.
            PassStep(0, RawInputDetail);



            if (Steps[0].Verdict == EDiagnosticVerdict::Blocked) { RecordCaptureHistory(MouseEvent); return; }



            APlayerController* PC = GetPIEPlayerController();

            const bool bTouchActivationOK = PC && PC->bEnableTouchEvents;
            const bool bClickActivationOK = PC && PC->bEnableClickEvents;

            if (PC)

            {

                PassStep(6, FString::Printf(TEXT("Runtime PlayerController=%s | bEnableClickEvents=%s | bEnableTouchEvents=%s | bShowMouseCursor=%s. InputMode/focus can still be changed dynamically by SetInputMode calls."),

                    *PC->GetClass()->GetName(),

                    bClickActivationOK ? TEXT("true") : TEXT("false"),

                    bTouchActivationOK ? TEXT("true") : TEXT("false"),

                    PC->bShowMouseCursor ? TEXT("true") : TEXT("false")));

            }



            bool bTargetInPath = false;

            bool bAnyUMGInPath = false;

            FString Leaf = TEXT("No hit-test widget");

            FString PathSummary;

            FString UMGSummary;

            if (Path.IsValid())

            {

                Leaf = Path.GetLastWidget()->GetTypeAsString();

                TArray<FString> PathParts;

                for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)

                {

                    const TSharedRef<SWidget> Widget = Path.Widgets[Index].Widget;

                    PathParts.Add(Widget->GetTypeAsString());

                    if (const TSharedPtr<FReflectionMetaData> MetaData = Widget->GetMetaData<FReflectionMetaData>())
                    {
                        if (UObject* SourceObject = MetaData->SourceObject.Get())
                        {
                            LastClickedObject = FString::Printf(TEXT("%s (%s)"),
                                *SourceObject->GetName(),
                                SourceObject->GetClass() ? *SourceObject->GetClass()->GetName() : TEXT("<NoClass>"));

                            UUserWidget* EventOwner = Cast<UUserWidget>(SourceObject);
                            if (!EventOwner)
                            {
                                if (UWidget* UMGWidget = Cast<UWidget>(SourceObject))
                                {
                                    EventOwner = UMGWidget->GetTypedOuter<UUserWidget>();
                                }
                            }

                            if (EventOwner && EventOwner->GetClass())
                            {
                                LastEventClass = EventOwner->GetClass()->GetName();
                            }
                            else if (SourceObject->GetClass())
                            {
                                LastEventClass = SourceObject->GetClass()->GetName();
                            }
                        }
                    }

                    if (Widget->GetType() == FName(TEXT("SObjectWidget")))

                    {

                        const TSharedRef<SObjectWidget> ObjectWidget = StaticCastSharedRef<SObjectWidget>(Widget);

                        if (UUserWidget* UserWidget = ObjectWidget->GetWidgetObject())

                        {

                            bAnyUMGInPath = true;

                            const FString WidgetName = UserWidget->GetName();

                            const FString WidgetClass = UserWidget->GetClass() ? UserWidget->GetClass()->GetName() : FString(TEXT("<NoClass>"));

                            UMGSummary += FString::Printf(TEXT("%s%s(%s)"), UMGSummary.IsEmpty() ? TEXT("") : TEXT(" -> "), *WidgetName, *WidgetClass);

                            if (TargetWidget.IsValid() && UserWidget->GetClass() && UserWidget->GetClass()->ClassGeneratedBy == TargetWidget.Get())

                            {

                                bTargetInPath = true;

                            }

                        }

                    }

                }

                PathSummary = FString::Join(PathParts, TEXT(" -> "));

            }

            if (LastEventClass.IsEmpty() && bAnyUMGInPath && TargetWidget.IsValid())
            {
                LastEventClass = TargetWidget->GeneratedClass
                    ? TargetWidget->GeneratedClass->GetName()
                    : TargetWidget->GetName();
            }



            if (TargetType == EDiagnosticTargetType::Widget)

            {

                if (bTargetInPath)

                {

                    PassStep(4, FString::Printf(TEXT("Target UMG widget was present in the live %s hit-test path; leaf=%s | path=%s | UMG=%s."), *InputKind, *Leaf, *PathSummary, *UMGSummary));

                }

                else

                {

                    BlockAt(4, FString::Printf(TEXT("The %s hit-test path did not include the selected widget; leaf=%s | path=%s | UMG=%s."), *InputKind, *Leaf, *PathSummary, *UMGSummary),

                        TEXT("Check UMG Visibility, ZOrder, collapsed/hidden parents, and overlays set to Visible instead of Self Hit Test Invisible."));

                }

                RecordCaptureHistory(MouseEvent);

                return;

            }



            if (TargetType == EDiagnosticTargetType::Actor)

            {

                if (bAnyUMGInPath)

                {

                    BlockAt(4, FString::Printf(TEXT("The %s is over a UMG/Slate hit-test path before the viewport trace; leaf=%s | UMG=%s."), *InputKind, *Leaf, *UMGSummary),

                        TEXT("If the actor should receive touch/click, set decorative widgets to Self Hit Test Invisible or route the touch from the widget to gameplay."));

                    RecordCaptureHistory(MouseEvent);

                    return;

                }



                PassStep(4, FString::Printf(TEXT("No UMG widget was found in the live %s hit-test path before the viewport trace; leaf=%s."), *InputKind, *Leaf));

                if ((bIsTouch && !bTouchActivationOK) || (!bIsTouch && !bClickActivationOK))
                {
                    BlockAt(0,
                        bIsTouch
                            ? TEXT("Touch reached the PIE viewport, but PlayerController bEnableTouchEvents is false.")
                            : TEXT("Mouse click reached the PIE viewport, but PlayerController bEnableClickEvents is false."),
                        bIsTouch
                            ? TEXT("Enable Touch Events on the active PlayerController, or route touch through Enhanced Input.")
                            : TEXT("Enable Click Events on the active PlayerController, or handle the input through an InputAction."));
                    RecordCaptureHistory(MouseEvent);
                    return;
                }



                AActor* RuntimeTarget = ResolveRuntimeActor();

                FHitResult Hit;

                bool bHit = false;

                if (PC)

                {

                    if (bIsTouch)

                    {

                        const int32 FingerIndex = FMath::Clamp(static_cast<int32>(MouseEvent.GetPointerIndex()), 0, 9);

                        bHit = PC->GetHitResultUnderFingerByChannel(static_cast<ETouchIndex::Type>(FingerIndex), UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit);

                    }

                    else

                    {

                        bHit = PC->GetHitResultUnderCursor(ECC_Visibility, true, Hit);

                    }

                }

                if (bHit && Hit.GetActor())
                {
                    LastEventClass = Hit.GetActor()->GetClass()
                        ? Hit.GetActor()->GetClass()->GetName()
                        : FString(TEXT("<NoClass>"));
                    LastClickedObject = FString::Printf(TEXT("%s.%s"),
                        *Hit.GetActor()->GetName(),
                        Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("<NoComponent>"));
                }
                else if (RuntimeTarget && RuntimeTarget->GetClass())
                {
                    LastEventClass = RuntimeTarget->GetClass()->GetName();
                    LastClickedObject = RuntimeTarget->GetName();
                }



                if (bHit && RuntimeTarget && Hit.GetActor() == RuntimeTarget)

                {

                    PassStep(5, FString::Printf(TEXT("Visibility trace for %s hit target %s.%s | blocking=%s."),

                        *InputKind,

                        *RuntimeTarget->GetName(),

                        Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("<None>"),

                        Hit.bBlockingHit ? TEXT("true") : TEXT("false")));

                }

                else

                {

                    const FString HitName = bHit && Hit.GetActor() ? Hit.GetActor()->GetName() : TEXT("nothing");

                    const FString HitComponentName = bHit && Hit.GetComponent() ? Hit.GetComponent()->GetName() : TEXT("<None>");

                    BlockAt(5, FString::Printf(TEXT("Visibility trace for %s hit %s.%s instead of the target."), *InputKind, *HitName, *HitComponentName),

                        TEXT("Enable query collision, make the intended primitive Block Visibility, verify visibility/hidden state, and confirm the touch point is over the actor in PIE."));

                }

                RecordCaptureHistory(MouseEvent);

            }



        }



        const FDiagnosticStep& GetStep(int32 Index) const { return Steps[Index]; }



        const TArray<FDiagnosticStep>& GetSteps() const { return Steps; }



        const FProjectInputScan& GetProjectScan() const { return ProjectScan; }



        const TArray<FBindingItem>& GetTargetBindingItems() const { return TargetBindingItems; }



        EDiagnosticMode GetMode() const { return Mode; }



        const FString& GetTargetName() const { return TargetName; }



        EDiagnosticTargetType GetTargetType() const { return TargetType; }



        int32 GetTotalClicks() const { return TotalClicks; }



        const FString& GetLastLiveSummary() const { return LastLiveSummary; }

        const TArray<FClickCaptureHistoryItem>& GetClickHistory() const { return ClickHistory; }

        const FClickCaptureHistoryItem* GetLastCapture() const { return ClickHistory.Num() > 0 ? &ClickHistory[0] : nullptr; }

        void ClearClickHistory()

        {

            ClickHistory.Reset();
            LastCaptureSignature.Empty();
            LastCaptureTimeSeconds = -1.0;

            TotalClicks = 0;

            CaptureGroups.Reset();

            LastLiveSummary.Reset();

            ResetSteps();

        }

        FString GetLastCaptureAgeText() const

        {

            if (ClickHistory.IsEmpty()) return TEXT("No capture yet");

            const double Delta = FMath::Max(0.0, FPlatformTime::Seconds() - ClickHistory[0].TimeSeconds);

            if (Delta < 1.0) return FString::Printf(TEXT("%.1fs ago"), Delta);

            if (Delta < 60.0) return FString::Printf(TEXT("%ds ago"), FMath::RoundToInt(Delta));

            return FString::Printf(TEXT("%dm ago"), FMath::RoundToInt(Delta / 60.0));

        }


        FString BuildCopyLogText() const

        {

            TArray<FString> Lines;

            Lines.Add(TEXT("# Click/Touch Event Diagnostics"));

            Lines.Add(FString::Printf(TEXT("- Mode: %s"), *GetModeText(Mode)));

            Lines.Add(FString::Printf(TEXT("- Target: %s"), TargetName.IsEmpty() ? TEXT("<None>") : *TargetName));

            Lines.Add(FString::Printf(TEXT("- Summary: %s"), *GetSummary().Replace(TEXT("\n"), TEXT(" | "))));

            Lines.Add(FString::Printf(TEXT("- Captured pointer events: %d"), TotalClicks));

            Lines.Add(FString());

            Lines.Add(TEXT("## Click history"));

            if (ClickHistory.IsEmpty())

            {

                Lines.Add(TEXT("- <no captures>"));

            }

            else

            {

                for (int32 Index = 0; Index < ClickHistory.Num(); ++Index)

                {

                    const FClickCaptureHistoryItem& Item = ClickHistory[Index];

                    Lines.Add(FString::Printf(TEXT("- %02d. %s | EventClass=%s | BindingClasses=%s | ClickedObject=%s | screen=(%.0f, %.0f) | %s"),

                        Index + 1,

                        *Item.Summary,

                        Item.EventClass.IsEmpty() ? TEXT("<unknown>") : *Item.EventClass,

                        Item.BindingClasses.IsEmpty() ? TEXT("<none found>") : *Item.BindingClasses,

                        Item.ClickedObject.IsEmpty() ? TEXT("<unknown>") : *Item.ClickedObject,

                        Item.ScreenPosition.X,

                        Item.ScreenPosition.Y,

                        Item.bConsumedByUI ? TEXT("Consumed by UI") : *GetVerdictText(Item.Verdict)));

                }

            }

            Lines.Add(FString());

            Lines.Add(TEXT("## Diagnostic steps"));

            for (const FDiagnosticStep& Step : Steps)

            {

                Lines.Add(FString::Printf(TEXT("- [%s] %d. %s"), *GetVerdictText(Step.Verdict), Step.Number, *Step.Name));

                if (!Step.Detail.IsEmpty())

                {

                    Lines.Add(FString::Printf(TEXT("  - Detail: %s"), *Step.Detail.Replace(TEXT("\n"), TEXT(" | "))));

                }

                if (!Step.Suggestion.IsEmpty())

                {

                    Lines.Add(FString::Printf(TEXT("  - Suggestion: %s"), *Step.Suggestion.Replace(TEXT("\n"), TEXT(" | "))));

                }

            }

            return FString::Join(Lines, TEXT("\n"));

        }







        FString GetSummary() const



        {



            int32 Confirmed = 0;



            int32 NeedsRun = 0;



            const FDiagnosticStep* Blocked = nullptr;



            for (const FDiagnosticStep& Step : Steps)



            {



                if (Step.Verdict == EDiagnosticVerdict::ConfirmedPass || Step.Verdict == EDiagnosticVerdict::Passed) ++Confirmed;



                if (Step.Verdict == EDiagnosticVerdict::NeedsRun) ++NeedsRun;



                if (!Blocked && Step.Verdict == EDiagnosticVerdict::Blocked) Blocked = &Step;



            }



            if (Blocked)



            {



                const int32 Percent = TotalClicks > 0 ? FMath::RoundToInt(100.0f * Blocked->CaptureCount / TotalClicks) : 100;



                FString Result = FString::Printf(TEXT("Stage %d blocked %d%% (%d capture(s)) - %s"), Blocked->Number, Percent, Blocked->CaptureCount, *Blocked->Suggestion);



                for (const TPair<FString, int32>& Group : CaptureGroups)



                {



                    Result += FString::Printf(TEXT("\n- %dx %s"), Group.Value, *Group.Key);



                }



                return Result;



            }



            return FString::Printf(TEXT("%d confirmed, %d require execution. Captured pointer events: %d"), Confirmed, NeedsRun, TotalClicks);



        }







    private:



        void RecordCaptureHistory(const FPointerEvent& MouseEvent)

        {

            const FDiagnosticStep* Blocked = nullptr;

            for (const FDiagnosticStep& Step : Steps)

            {

                if (!Blocked && Step.Verdict == EDiagnosticVerdict::Blocked)

                {

                    Blocked = &Step;

                    break;

                }

            }

            FClickCaptureHistoryItem Item;

            Item.ScreenPosition = MouseEvent.GetScreenSpacePosition();

            Item.TimeSeconds = FPlatformTime::Seconds();

            Item.Target = TargetName;

            Item.EventClass = LastEventClass;

            Item.ClickedObject = LastClickedObject;

            TSet<FString> UniqueBindingClasses;
            for (const FBindingItem& Binding : TargetBindingItems)
            {
                if (!Binding.bDead
                    && !Binding.Blueprint.IsEmpty()
                    && Binding.Action != TEXT("No target bindings"))
                {
                    UniqueBindingClasses.Add(Binding.Blueprint);
                }
            }
            TArray<FString> SortedBindingClasses = UniqueBindingClasses.Array();
            SortedBindingClasses.Sort();
            Item.BindingClasses = FString::Join(SortedBindingClasses, TEXT(", "));

            Item.Verdict = Blocked ? EDiagnosticVerdict::Blocked : EDiagnosticVerdict::Passed;

            Item.bConsumedByUI = Blocked && Blocked->Number == 4;

            const FString PointerText = MouseEvent.IsTouchEvent() ? TEXT("touch") : TEXT("click");

            if (Blocked)

            {

                const FString Blocker = Item.bConsumedByUI && !TargetName.IsEmpty() ? TargetName : FString::Printf(TEXT("stage %d"), Blocked->Number);

                Item.Summary = FString::Printf(TEXT("%s blocked %s at (%.0f, %.0f)"), *Blocker, *PointerText, Item.ScreenPosition.X, Item.ScreenPosition.Y);

            }

            else if (TargetType != EDiagnosticTargetType::None && !TargetName.IsEmpty())

            {

                Item.Summary = FString::Printf(TEXT("%s reached bound path at (%.0f, %.0f)"), *TargetName, Item.ScreenPosition.X, Item.ScreenPosition.Y);

            }

            else

            {

                Item.Summary = FString::Printf(TEXT("No hittable target at (%.0f, %.0f)"), Item.ScreenPosition.X, Item.ScreenPosition.Y);

                Item.Verdict = EDiagnosticVerdict::NeedsRun;

            }

            const FString CaptureSignature = FString::Printf(TEXT("%s|%s|%s|%.0f|%.0f"),
                *Item.Target, *Item.EventClass, *Item.Summary, Item.ScreenPosition.X, Item.ScreenPosition.Y);
            if (CaptureSignature == LastCaptureSignature
                && Item.TimeSeconds - LastCaptureTimeSeconds < TMTraceNoise::DuplicateWindowSeconds(TEXT("Click")))
            {
                return;
            }
            LastCaptureSignature = CaptureSignature;
            LastCaptureTimeSeconds = Item.TimeSeconds;

            ClickHistory.Insert(Item, 0);

            const int32 MaxHistoryItems = TMPerf::PIEEventHistoryLimit();

            if (ClickHistory.Num() > MaxHistoryItems)

            {

                ClickHistory.SetNum(MaxHistoryItems);

            }

            LastLiveSummary = GetSummary();

            if (Mode == EDiagnosticMode::Live || Mode == EDiagnosticMode::Armed)

            {

                bPickNextClickTarget = true;

            }

        }


        void EnsureProjectScan()



        {



            if (!ProjectScan.bScanned)



            {



                RefreshProjectScan();



            }



        }







        bool ResolveTargetFromClick(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)



        {



            const FWidgetPath Path = SlateApp.LocateWindowUnderMouse(



                MouseEvent.GetScreenSpacePosition(), SlateApp.GetInteractiveTopLevelWindows(), false, MouseEvent.GetUserIndex());



            if (Path.IsValid())



            {



                for (int32 Index = Path.Widgets.Num() - 1; Index >= 0; --Index)



                {



                    const TSharedRef<SWidget> Widget = Path.Widgets[Index].Widget;



                    if (Widget->GetType() != FName(TEXT("SObjectWidget"))) continue;



                    const TSharedRef<SObjectWidget> ObjectWidget = StaticCastSharedRef<SObjectWidget>(Widget);



                    UUserWidget* UserWidget = ObjectWidget->GetWidgetObject();



                    UWidgetBlueprint* WidgetBlueprint = UserWidget && UserWidget->GetClass()



                        ? Cast<UWidgetBlueprint>(UserWidget->GetClass()->ClassGeneratedBy) : nullptr;



                    if (WidgetBlueprint)



                    {



                        SelectWidget(WidgetBlueprint);



                        Mode = EDiagnosticMode::Live;



                        return true;



                    }



                }



            }







            APlayerController* PC = GetPIEPlayerController();



            FHitResult Hit;

            bool bHit = false;

            if (PC)

            {

                if (MouseEvent.IsTouchEvent())

                {

                    const int32 FingerIndex = FMath::Clamp(static_cast<int32>(MouseEvent.GetPointerIndex()), 0, 9);

                    bHit = PC->GetHitResultUnderFingerByChannel(static_cast<ETouchIndex::Type>(FingerIndex), UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit);

                }

                else

                {

                    bHit = PC->GetHitResultUnderCursor(ECC_Visibility, true, Hit);

                }

            }



            if (bHit && Hit.GetActor())



            {



                SelectActor(Hit.GetActor());



                Mode = EDiagnosticMode::Live;



                return true;



            }







            TargetName = TEXT("No UMG widget or Visibility-blocking actor was found. Touch/click another target in the PIE viewport.");



            return false;



        }







        void ResetSteps()



        {



            Steps = {



                {0, TEXT("Input activation"), TEXT("Select a target to inspect PlayerController click/touch settings."), TEXT("Enable click/touch events and the required cursor/touch settings."), EDiagnosticVerdict::NeedsRun},



                {1, TEXT("Mapping registration"), TEXT("Select a target to scan input mappings."), TEXT("Register the required mapping context."), EDiagnosticVerdict::NeedsRun},



                {2, TEXT("Trigger / conditions"), TEXT("Trigger and modifier behavior requires PIE observation."), TEXT("Verify trigger type and timing in PIE."), EDiagnosticVerdict::NeedsRun},



                {3, TEXT("Binding connection"), TEXT("Select a target to scan Blueprint event bindings."), TEXT("Add an OnClicked, OnInputTouch, Enhanced Input, or Legacy Input binding."), EDiagnosticVerdict::NeedsRun},



                {4, TEXT("UI layer blocking"), TEXT("Runtime Slate hit-test path must be observed."), TEXT("Use Self Hit Test Invisible for decorative overlays."), EDiagnosticVerdict::NeedsRun},



                {5, TEXT("Collision / hit blocking"), TEXT("Runtime cursor/finger trace must be observed."), TEXT("Enable query collision and Visibility response."), EDiagnosticVerdict::NeedsRun},



                {6, TEXT("Input mode match"), TEXT("Runtime input mode changes require PIE observation."), TEXT("Use GameAndUI or the mode matching the intended target."), EDiagnosticVerdict::NeedsRun}



            };



            TotalClicks = 0;



        }







        void BuildStaticPipeline()



        {



            ResetSteps();



            TargetBindingItems.Reset();



            if (TargetType == EDiagnosticTargetType::None) return;







            APlayerController* PCDO = GetEditorPlayerControllerCDO();



            const bool bClickActivationOK = PCDO && PCDO->bEnableClickEvents && PCDO->bShowMouseCursor;

            const bool bTouchActivationOK = PCDO && PCDO->bEnableTouchEvents;

            const bool bActivationOK = bClickActivationOK || bTouchActivationOK;



            Steps[0].Verdict = bActivationOK ? EDiagnosticVerdict::ConfirmedPass : EDiagnosticVerdict::Blocked;



            Steps[0].Detail = PCDO



                ? FString::Printf(TEXT("%s: click=%s (bEnableClickEvents=%s, bShowMouseCursor=%s), touch=%s (bEnableTouchEvents=%s, bEnableTouchOverEvents=%s)"), *PCDO->GetClass()->GetName(),

                    bClickActivationOK ? TEXT("ready") : TEXT("not ready"),

                    PCDO->bEnableClickEvents ? TEXT("true") : TEXT("false"),

                    PCDO->bShowMouseCursor ? TEXT("true") : TEXT("false"),

                    bTouchActivationOK ? TEXT("ready") : TEXT("not ready"),

                    PCDO->bEnableTouchEvents ? TEXT("true") : TEXT("false"),

                    PCDO->bEnableTouchOverEvents ? TEXT("true") : TEXT("false"))



                : TEXT("PlayerController default object could not be resolved.");



            if (!bActivationOK)



            {



                MarkAfterUnreached(0);



            }







            bool bHasClickBinding = false;



            bool bHasInputBinding = false;



            bool bMappingExists = false;



            bool bHasTargetAddMappingContextCall = false;



            TArray<FString> TargetActions;



            if (UBlueprint* Blueprint = TargetBlueprint.Get())



            {



                TArray<UEdGraph*> Graphs;



                GetBlueprintGraphs(Blueprint, Graphs);



                for (UEdGraph* Graph : Graphs)



                {



                    if (!Graph) continue;



                    for (UEdGraphNode* Node : Graph->Nodes)



                    {



                        if (const UK2Node_ComponentBoundEvent* Bound = Cast<UK2Node_ComponentBoundEvent>(Node))



                        {



                            if (Bound->DelegatePropertyName == TEXT("OnClicked"))



                            {



                                bHasClickBinding = true;



                                TargetBindingItems.Add({ TEXT("OnClicked"), Blueprint->GetName(), Graph->GetName(), Bound->ComponentPropertyName.ToString(), false });



                            }

                            else if (Bound->DelegatePropertyName == TEXT("OnInputTouchBegin")

                                || Bound->DelegatePropertyName == TEXT("OnInputTouchEnd")

                                || Bound->DelegatePropertyName == TEXT("OnInputTouchEnter")

                                || Bound->DelegatePropertyName == TEXT("OnInputTouchLeave"))

                            {

                                bHasInputBinding = true;

                                bMappingExists = true;

                                TargetBindingItems.Add({ Bound->DelegatePropertyName.ToString(), Blueprint->GetName(), Graph->GetName(), Bound->ComponentPropertyName.ToString(), false });

                            }



                        }



                        else if (const UK2Node_ActorBoundEvent* ActorEvent = Cast<UK2Node_ActorBoundEvent>(Node))



                        {



                            const bool bMatchesTarget = ActorEvent->DelegatePropertyName == TEXT("OnClicked")



                                && (!TargetActor.IsValid() || ActorEvent->EventOwner == TargetActor.Get());



                            bHasClickBinding |= bMatchesTarget;



                            if (bMatchesTarget)



                            {



                                TargetBindingItems.Add({ TEXT("OnClicked"), Blueprint->GetName(), Graph->GetName(), TEXT("Actor bound event"), false });



                            }



                        }



                        else if (const UK2Node_EnhancedInputAction* Enhanced = Cast<UK2Node_EnhancedInputAction>(Node))



                        {



                            if (Enhanced->InputAction)



                            {



                                bHasInputBinding = true;



                                TargetActions.Add(Enhanced->InputAction->GetPathName());



                                TargetBindingItems.Add({ Enhanced->InputAction->GetName(), Blueprint->GetName(), Graph->GetName(), TEXT("Enhanced Input event"), false });



                            }



                        }



                        else if (const UK2Node_InputAction* Legacy = Cast<UK2Node_InputAction>(Node))



                        {



                            bHasInputBinding = true;



                            bMappingExists |= ProjectScan.MappedLegacyActions.Contains(Legacy->InputActionName);



                            TargetBindingItems.Add({ Legacy->InputActionName.ToString(), Blueprint->GetName(), Graph->GetName(), TEXT("Legacy InputAction"), false });



                        }



                        else if (const UK2Node_InputActionEvent* LegacyEvent = Cast<UK2Node_InputActionEvent>(Node))



                        {



                            bHasInputBinding = true;



                            bMappingExists |= ProjectScan.MappedLegacyActions.Contains(LegacyEvent->InputActionName);



                            TargetBindingItems.Add({ LegacyEvent->InputActionName.ToString(), Blueprint->GetName(), Graph->GetName(), TEXT("Legacy InputAction event"), false });



                        }

                        else if (const UK2Node_InputTouch* TouchNode = Cast<UK2Node_InputTouch>(Node))

                        {

                            bHasInputBinding = true;

                            bMappingExists = true;

                            TargetBindingItems.Add({ TEXT("InputTouch"), Blueprint->GetName(), Graph->GetName(), FString::Printf(TEXT("Legacy touch node | Consume=%s"), TouchNode->bConsumeInput ? TEXT("true") : TEXT("false")), false });

                        }

                        else if (const UK2Node_InputTouchEvent* TouchEvent = Cast<UK2Node_InputTouchEvent>(Node))

                        {

                            bHasInputBinding = true;

                            bMappingExists = true;

                            TargetBindingItems.Add({ TEXT("InputTouchEvent"), Blueprint->GetName(), Graph->GetName(), FString::Printf(TEXT("Legacy touch event | Event=%d | Consume=%s"), static_cast<int32>(TouchEvent->InputKeyEvent.GetValue()), TouchEvent->bConsumeInput ? TEXT("true") : TEXT("false")), false });

                        }



                        else if (const UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))



                        {



                            if (EventNode->EventReference.GetMemberName() == TEXT("ReceiveActorOnClicked"))



                            {



                                bHasClickBinding = true;



                                TargetBindingItems.Add({ TEXT("ActorOnClicked"), Blueprint->GetName(), Graph->GetName(), TEXT("Actor event override"), false });



                            }

                            else if (EventNode->EventReference.GetMemberName() == TEXT("ReceiveActorOnInputTouchBegin")

                                || EventNode->EventReference.GetMemberName() == TEXT("ReceiveActorOnInputTouchEnd")

                                || EventNode->EventReference.GetMemberName() == TEXT("ReceiveActorOnInputTouchEnter")

                                || EventNode->EventReference.GetMemberName() == TEXT("ReceiveActorOnInputTouchLeave"))

                            {

                                bHasInputBinding = true;

                                bMappingExists = true;

                                TargetBindingItems.Add({ EventNode->EventReference.GetMemberName().ToString(), Blueprint->GetName(), Graph->GetName(), TEXT("Actor touch event override"), false });

                            }



                        }



                        else if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))



                        {



                            bHasTargetAddMappingContextCall |= Call->FunctionReference.GetMemberName() == TEXT("AddMappingContext");



                        }



                    }



                }



            }



            if (AActor* Actor = TargetActor.Get())



            {



                if (ULevel* Level = Actor->GetLevel())



                {



                    if (UBlueprint* LevelBlueprint = Level->GetLevelScriptBlueprint(true))



                    {



                        TArray<UEdGraph*> LevelGraphs;



                        GetBlueprintGraphs(LevelBlueprint, LevelGraphs);



                        for (UEdGraph* Graph : LevelGraphs)



                        {



                            if (!Graph) continue;



                            for (UEdGraphNode* Node : Graph->Nodes)



                            {



                                if (const UK2Node_ActorBoundEvent* ActorEvent = Cast<UK2Node_ActorBoundEvent>(Node))



                                {



                                    const bool bClickMatchesTarget = ActorEvent->DelegatePropertyName == TEXT("OnClicked")

                                        && ActorEvent->EventOwner == Actor;

                                    const bool bTouchMatchesTarget = (ActorEvent->DelegatePropertyName == TEXT("OnInputTouchBegin")

                                        || ActorEvent->DelegatePropertyName == TEXT("OnInputTouchEnd")

                                        || ActorEvent->DelegatePropertyName == TEXT("OnInputTouchEnter")

                                        || ActorEvent->DelegatePropertyName == TEXT("OnInputTouchLeave"))

                                        && ActorEvent->EventOwner == Actor;



                                    bHasClickBinding |= bClickMatchesTarget;

                                    bHasInputBinding |= bTouchMatchesTarget;

                                    bMappingExists |= bTouchMatchesTarget;



                                    if (bClickMatchesTarget || bTouchMatchesTarget)



                                    {



                                        TargetBindingItems.Add({ ActorEvent->DelegatePropertyName.ToString(), LevelBlueprint->GetName(), Graph->GetName(), TEXT("Level actor bound event"), false });



                                    }



                                }



                            }



                        }



                    }



                }



            }



            if (TargetBindingItems.IsEmpty())



            {



                TargetBindingItems.Add({ TEXT("No target bindings"), TargetBlueprint.IsValid() ? TargetBlueprint->GetName() : TargetName,



                    TEXT("-"), TEXT("No OnClicked or input-action event was found in the selected target."), true });



            }



            for (const FString& Action : TargetActions)



            {



                for (const FInputAuditItem& Audit : ProjectScan.AuditItems)



                {



                    if (Audit.System == TEXT("Enhanced") && FPackageName::ObjectPathToObjectName(Action) == Audit.Action)



                    {



                        bMappingExists = true;



                        break;



                    }



                }



            }



            if (!bHasInputBinding)



            {



                bMappingExists = true;



                Steps[1].Detail = bHasClickBinding



                    ? TEXT("OnClicked dispatch does not require an InputAction mapping.")



                    : TEXT("No InputAction is referenced by the target; mapping registration is not the deciding stage.");



            }



            else



            {



                Steps[1].Detail = FString::Printf(TEXT("Referenced actions=%d, mapping found=%s, AddMappingContext in target=%s"),



                    TargetActions.Num(), bMappingExists ? TEXT("true") : TEXT("false"), bHasTargetAddMappingContextCall ? TEXT("true") : TEXT("false"));



            }



            const bool bNeedsRegistrationProof = bHasInputBinding && !TargetActions.IsEmpty() && !bHasTargetAddMappingContextCall;



            Steps[1].Verdict = !bMappingExists ? EDiagnosticVerdict::Blocked



                : (bNeedsRegistrationProof ? EDiagnosticVerdict::NeedsRun : EDiagnosticVerdict::ConfirmedPass);



            if (!bMappingExists)



            {



                MarkAfterUnreached(1);



                return;



            }







            Steps[2].Verdict = EDiagnosticVerdict::NeedsRun;



            Steps[2].Detail = TEXT("Trigger and modifier configuration found; actual trigger timing is only reliable during PIE.");







            Steps[3].Verdict = (bHasClickBinding || bHasInputBinding) ? EDiagnosticVerdict::ConfirmedPass : EDiagnosticVerdict::Blocked;



            Steps[3].Detail = FString::Printf(TEXT("OnClicked=%s, input action event=%s in %s."),



                bHasClickBinding ? TEXT("found") : TEXT("not found"), bHasInputBinding ? TEXT("found") : TEXT("not found"), *TargetName);



            if (Steps[3].Verdict == EDiagnosticVerdict::Blocked)



            {



                MarkAfterUnreached(3);



                return;



            }







            Steps[4].Verdict = ProjectScan.WidgetBlueprintCount == 0 && TargetType == EDiagnosticTargetType::Actor



                ? EDiagnosticVerdict::Skip : EDiagnosticVerdict::NeedsRun;



            Steps[4].Detail = TargetType == EDiagnosticTargetType::Widget



                ? TEXT("Widget visibility, Z-order and the live Slate hit path require PIE.")



                : (ProjectScan.WidgetBlueprintCount == 0 ? TEXT("No WidgetBlueprint assets were found in the project.") : TEXT("Project contains UI; a live hit-test is required to exclude overlay blocking."));







            Steps[5].Verdict = TargetType == EDiagnosticTargetType::Widget ? EDiagnosticVerdict::Skip : EDiagnosticVerdict::NeedsRun;



            if (TargetType == EDiagnosticTargetType::Widget)



            {



                Steps[5].Detail = TEXT("Not applicable to a UMG widget target.");



            }



            else



            {



                int32 PrimitiveCount = 0;



                int32 QueryEnabledCount = 0;



                if (AActor* Actor = TargetActor.Get())



                {



                    TArray<UPrimitiveComponent*> Components;



                    Actor->GetComponents<UPrimitiveComponent>(Components, true);



                    PrimitiveCount = Components.Num();



                    for (UPrimitiveComponent* Component : Components)



                    {



                        if (Component && CollisionEnabledHasQuery(Component->GetCollisionEnabled())



                            && Component->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Ignore) ++QueryEnabledCount;



                    }



                }



                Steps[5].Detail = FString::Printf(TEXT("Primitive components=%d, query/Visibility candidates=%d; actual cursor hit requires PIE."), PrimitiveCount, QueryEnabledCount);



            }







            Steps[6].Verdict = EDiagnosticVerdict::NeedsRun;



            Steps[6].Detail = TEXT("SetInputMode timing and focus ownership can only be confirmed in PIE.");



        }







        void MarkAfterUnreached(int32 BlockedIndex)



        {



            for (int32 Index = BlockedIndex + 1; Index < Steps.Num(); ++Index)



            {



                Steps[Index].Verdict = EDiagnosticVerdict::Unreached;



                Steps[Index].Detail = TEXT("Not evaluated because an earlier stage is blocked.");



            }



        }







        void PassStep(int32 Index, const FString& Detail)



        {



            if (!Steps.IsValidIndex(Index) || Steps[Index].Verdict == EDiagnosticVerdict::Skip) return;



            Steps[Index].Verdict = EDiagnosticVerdict::Passed;



            Steps[Index].Detail = Detail;



            ++Steps[Index].CaptureCount;



        }







        void BlockAt(int32 Index, const FString& Detail, const FString& Suggestion)



        {



            if (!Steps.IsValidIndex(Index)) return;



            Steps[Index].Verdict = EDiagnosticVerdict::Blocked;



            Steps[Index].Detail = Detail;



            Steps[Index].Suggestion = Suggestion;



            ++Steps[Index].CaptureCount;



            ++CaptureGroups.FindOrAdd(FString::Printf(TEXT("Stage %d: %s"), Index, *Detail));



            MarkAfterUnreached(Index);



        }







        void OnBeginPIE(bool)



        {



            if (TargetType == EDiagnosticTargetType::None && !bPickNextClickTarget) return;



            Mode = EDiagnosticMode::Live;



            CachedPreviousSteps = Steps;



            TotalClicks = 0;



            CaptureGroups.Reset();



            InstallLiveHooks();



            for (FDiagnosticStep& Step : Steps)



            {



                Step.CaptureCount = 0;



                if (Step.Verdict == EDiagnosticVerdict::NeedsRun) Step.Detail += TEXT(" Waiting for a click capture...");



            }



        }







        void OnEndPIE(bool)



        {



            LastLiveSteps = Steps;



            LastLiveSummary = GetSummary();



            RemoveLiveHooks();



            Mode = bPickNextClickTarget ? EDiagnosticMode::Armed : EDiagnosticMode::Static;



        }







        void InstallLiveHooks()



        {



            RemoveLiveHooks();



            InputObserver = MakeShared<FClickInputObserver>(*this);



            if (FSlateApplication::IsInitialized())



            {



                FSlateApplication::Get().RegisterInputPreProcessor(InputObserver, 0);



            }



        }







        void RemoveLiveHooks()



        {



            if (InputObserver.IsValid() && FSlateApplication::IsInitialized())



            {



                FSlateApplication::Get().UnregisterInputPreProcessor(InputObserver);



            }



            InputObserver.Reset();



        }







        APlayerController* GetPIEPlayerController() const



        {



            if (!GEngine) return nullptr;



            for (const FWorldContext& Context : GEngine->GetWorldContexts())



            {



                UWorld* World = Context.World();



                if (World && World->WorldType == EWorldType::PIE) return World->GetFirstPlayerController();



            }



            return nullptr;



        }







        AActor* ResolveRuntimeActor() const



        {



            AActor* Source = TargetActor.Get();



            if (!Source || !GEngine) return Source;



            const FGuid Guid = Source->GetActorGuid();



            for (const FWorldContext& Context : GEngine->GetWorldContexts())



            {



                UWorld* World = Context.World();



                if (!World || World->WorldType != EWorldType::PIE) continue;



                for (TActorIterator<AActor> It(World); It; ++It)



                {



                    if ((Guid.IsValid() && It->GetActorGuid() == Guid)



                        || (It->GetClass() == Source->GetClass() && It->GetActorLabel() == Source->GetActorLabel())) return *It;



                }



            }



            return nullptr;



        }







        FProjectInputScan ProjectScan;



        TArray<FBindingItem> TargetBindingItems;



        TArray<FDiagnosticStep> Steps;



        TArray<FDiagnosticStep> CachedPreviousSteps;



        TArray<FDiagnosticStep> LastLiveSteps;



        FString LastLiveSummary;

        FString LastEventClass;

        FString LastClickedObject;



        FString TargetName = TEXT("No target");



        TWeakObjectPtr<AActor> TargetActor;



        TWeakObjectPtr<UWidgetBlueprint> TargetWidget;



        TWeakObjectPtr<UBlueprint> TargetBlueprint;



        EDiagnosticTargetType TargetType = EDiagnosticTargetType::None;



        EDiagnosticMode Mode = EDiagnosticMode::Static;



        bool bPickNextClickTarget = false;



        int32 TotalClicks = 0;



        TMap<FString, int32> CaptureGroups;

        TArray<FClickCaptureHistoryItem> ClickHistory;
        FString LastCaptureSignature;
        double LastCaptureTimeSeconds = -1.0;



        TSharedPtr<FClickInputObserver> InputObserver;



        FDelegateHandle BeginPIEHandle;



        FDelegateHandle EndPIEHandle;



    };







    bool FClickInputObserver::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)



    {



        Session.OnRawPointer(SlateApp, MouseEvent);



        return false;



    }







    FClickDiagnosticsSession DiagnosticsSession;







    class SClickDiagnosticsWidget final : public SCompoundWidget



    {



    public:



        SLATE_BEGIN_ARGS(SClickDiagnosticsWidget) {}



        SLATE_END_ARGS()







        void Construct(const FArguments&)



        {



            DiagnosticsSession.ArmNextClickTarget();

            ChildSlot



            [



                SNew(SBorder)



                .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))



                .Padding(10.0f)



                [



                    BuildResolverTab()



                ]



            ];



            RefreshHistoryRows();

            RefreshAuditRows();



            RefreshMatrixRows();



            LastRenderedTargetName = DiagnosticsSession.GetTargetName();



        }







        virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override



        {



            SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);



            if (LastRenderedTargetName != DiagnosticsSession.GetTargetName())



            {



                LastRenderedTargetName = DiagnosticsSession.GetTargetName();



                RefreshMatrixRows();



            }

            if (LastRenderedHistoryCount != DiagnosticsSession.GetClickHistory().Num())

            {

                LastRenderedHistoryCount = DiagnosticsSession.GetClickHistory().Num();

                RefreshHistoryRows();

            }



        }







    private:



        TSharedRef<SWidget> BuildHeader()



        {



            return SNew(SHorizontalBox)



                + SHorizontalBox::Slot().FillWidth(1.0f)



                [



                    SNew(SVerticalBox)



                    + SVerticalBox::Slot().AutoHeight()



                    [



                        SNew(STextBlock).Text(TMLoc::Text(TEXT("Click/Touch Event Diagnostics"), TEXT("클릭/터치 이벤트 진단"))).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16))



                    ]



                    + SVerticalBox::Slot().AutoHeight().Padding(0, 3)



                    [



                        SNew(STextBlock).Text(TMLoc::Text(TEXT("Find where click/touch delivery stops without changing game input behavior."), TEXT("게임 입력 동작을 바꾸지 않고 클릭/터치 전달이 멈추는 단계를 찾습니다."))).ColorAndOpacity(FLinearColor(0.62f, 0.66f, 0.72f))



                    ]



                ];



        }







        TSharedRef<SWidget> BuildTabs()



        {



            return SNew(SHorizontalBox)



                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)[MakeTabButton(TEXT("Target Resolver"), 0)]



                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)[MakeTabButton(TEXT("Input Asset Auditor"), 1)]



                + SHorizontalBox::Slot().AutoWidth()[MakeTabButton(TEXT("Binding Matrix"), 2)];



        }







        TSharedRef<SWidget> MakeTabButton(const TCHAR* Label, int32 Index)



        {



            return SNew(SButton).Text(FText::FromString(Label)).OnClicked_Lambda([this, Index]()



            {



                if (Switcher.IsValid()) Switcher->SetActiveWidgetIndex(Index);



                return FReply::Handled();



            });



        }








        TSharedRef<SWidget> BuildStatusPill(TAttribute<FText> Text, TAttribute<FSlateColor> Background, TAttribute<FSlateColor> Foreground)

        {

            return SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

                .BorderBackgroundColor(Background)

                .Padding(FMargin(9, 3))

                [SNew(STextBlock).Text(Text).ColorAndOpacity(Foreground).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))];

        }



        EDiagnosticVerdict GetCompactStageVerdict(int32 Stage) const

        {

            if (!DiagnosticsSession.GetLastCapture()) return EDiagnosticVerdict::Unreached;

            if (Stage == 0) return EDiagnosticVerdict::Passed;

            const FDiagnosticStep& UI = DiagnosticsSession.GetStep(4);

            const FDiagnosticStep& Hit = DiagnosticsSession.GetStep(5);

            const FDiagnosticStep& Binding = DiagnosticsSession.GetStep(3);

            if (Stage == 1) return UI.Verdict;

            if (Stage == 2)

            {

                if (UI.Verdict == EDiagnosticVerdict::Blocked) return EDiagnosticVerdict::Unreached;

                return Hit.Verdict;

            }

            if (UI.Verdict == EDiagnosticVerdict::Blocked || Hit.Verdict == EDiagnosticVerdict::Blocked) return EDiagnosticVerdict::Unreached;

            return Binding.Verdict;

        }



        FString GetCompactStageTitle(int32 Stage) const

        {

            switch (Stage)

            {

            case 0: return TEXT("Slate hit test");

            case 1:

                if (DiagnosticsSession.GetStep(4).Verdict == EDiagnosticVerdict::Blocked && DiagnosticsSession.GetTargetType() == EDiagnosticTargetType::Widget) return DiagnosticsSession.GetTargetName();

                return TEXT("UMG / Slate");

            case 2: return TEXT("World actor");

            default: return TEXT("Bound event");

            }

        }



        FString GetCompactStageSubtitle(int32 Stage) const

        {

            const EDiagnosticVerdict Verdict = GetCompactStageVerdict(Stage);

            if (Stage == 0) return DiagnosticsSession.GetLastCapture() ? TEXT("Reached viewport") : TEXT("Waiting for PIE input");

            if (Verdict == EDiagnosticVerdict::Blocked)

            {

                if (Stage == 1) return TEXT("Visible, blocking");

                if (Stage == 2) return TEXT("Visibility trace blocked");

                return TEXT("Binding missing");

            }

            if (Verdict == EDiagnosticVerdict::Passed || Verdict == EDiagnosticVerdict::ConfirmedPass) return TEXT("Reached");

            if (Verdict == EDiagnosticVerdict::Skip) return TEXT("Not needed");

            return TEXT("Not reached");

        }



        TSharedRef<SWidget> BuildCompactStage(int32 Stage)

        {

            return SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)

                [

                    SNew(SBox).WidthOverride(34).HeightOverride(34)

                    [

                        SNew(SBorder)

                        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

                        .BorderBackgroundColor_Lambda([this, Stage]()

                        {

                            const FLinearColor Color = GetVerdictColor(GetCompactStageVerdict(Stage));

                            const bool bReached = GetCompactStageVerdict(Stage) != EDiagnosticVerdict::Unreached && GetCompactStageVerdict(Stage) != EDiagnosticVerdict::NeedsRun;

                            const float Alpha = bReached ? 0.22f : 0.08f;

                            return FLinearColor(Color.R, Color.G, Color.B, Alpha);

                        })

                        .HAlign(HAlign_Center)

                        .VAlign(VAlign_Center)

                        [

                            SNew(STextBlock)

                            .Text_Lambda([Stage]()

                            {

                                if (Stage == 0) return TMLoc::Text(TEXT("S"), TEXT("S"));

                                if (Stage == 1) return TMLoc::Text(TEXT("UI"), TEXT("UI"));

                                if (Stage == 2) return TMLoc::Text(TEXT("A"), TEXT("A"));

                                return TMLoc::Text(TEXT("EV"), TEXT("EV"));

                            })

                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))

                            .ColorAndOpacity_Lambda([this, Stage]() { return GetVerdictColor(GetCompactStageVerdict(Stage)); })

                        ]

                    ]

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0, 7, 0, 0).HAlign(HAlign_Center)

                [SNew(STextBlock).Text_Lambda([this, Stage]() { return FText::FromString(GetCompactStageTitle(Stage)); }).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))]

                + SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0).HAlign(HAlign_Center)

                [SNew(STextBlock).Text_Lambda([this, Stage]() { return FText::FromString(GetCompactStageSubtitle(Stage)); }).ColorAndOpacity_Lambda([this, Stage]() { return GetVerdictColor(GetCompactStageVerdict(Stage)); }).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))];

        }



        TSharedRef<SWidget> BuildConnector()

        {

            return SNew(SBox).WidthOverride(28).HeightOverride(1).VAlign(VAlign_Center)

                [SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(FLinearColor(0.78f, 0.78f, 0.78f, 1.0f))];

        }



        TSharedRef<SWidget> BuildLastCaptureCard()

        {

            return SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))

                .BorderBackgroundColor(FLinearColor(0.98f, 0.98f, 0.97f, 1.0f))

                .Padding(16.0f)

                [

                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()

                    [

                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().FillWidth(1.0f)

                        [SNew(STextBlock).Text_Lambda([]()

                        {

                            if (const FClickCaptureHistoryItem* Last = DiagnosticsSession.GetLastCapture())

                            {

                                return FText::FromString(FString::Printf(TEXT("Last click - %s - screen (%.0f, %.0f)"), *DiagnosticsSession.GetLastCaptureAgeText(), Last->ScreenPosition.X, Last->ScreenPosition.Y));

                            }

                            return TMLoc::Text(TEXT("Last click - waiting for PIE capture"), TEXT("마지막 클릭 - PIE 캡처 대기 중"));

                        }).ColorAndOpacity(FLinearColor(0.48f, 0.48f, 0.48f)).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))]

                        + SHorizontalBox::Slot().AutoWidth()

                        [BuildStatusPill(

                            TAttribute<FText>::CreateLambda([]()

                            {

                                if (const FClickCaptureHistoryItem* Last = DiagnosticsSession.GetLastCapture()) return FText::FromString(Last->bConsumedByUI ? TMLoc::String(TEXT("Consumed by UI"), TEXT("UI가 소비함")) : TMLoc::String(TEXT("Reached target"), TEXT("대상 도달")));

                                return FText::FromString(TMLoc::String(TEXT("Waiting"), TEXT("대기 중")));

                            }),

                            TAttribute<FSlateColor>::CreateLambda([]()

                            {

                                if (const FClickCaptureHistoryItem* Last = DiagnosticsSession.GetLastCapture()) return Last->bConsumedByUI ? FSlateColor(FLinearColor(1.0f, 0.78f, 0.80f, 1.0f)) : FSlateColor(FLinearColor(0.78f, 0.94f, 0.78f, 1.0f));

                                return FSlateColor(FLinearColor(0.92f, 0.92f, 0.92f, 1.0f));

                            }),

                            TAttribute<FSlateColor>::CreateLambda([]()

                            {

                                if (const FClickCaptureHistoryItem* Last = DiagnosticsSession.GetLastCapture()) return Last->bConsumedByUI ? FSlateColor(FLinearColor(0.58f, 0.12f, 0.12f, 1.0f)) : FSlateColor(FLinearColor(0.10f, 0.45f, 0.14f, 1.0f));

                                return FSlateColor(FLinearColor(0.45f, 0.45f, 0.45f, 1.0f));

                            }))]

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 18, 0, 14)

                    [

                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center)[BuildCompactStage(0)]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[BuildConnector()]

                        + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center)[BuildCompactStage(1)]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[BuildConnector()]

                        + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center)[BuildCompactStage(2)]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[BuildConnector()]

                        + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center)[BuildCompactStage(3)]

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)

                    [SNew(SSeparator)]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)

                    [SNew(STextBlock).Text_Lambda([]()

                    {

                        if (const FClickCaptureHistoryItem* Last = DiagnosticsSession.GetLastCapture())

                        {

                            if (Last->bConsumedByUI)

                            {

                                return FText::FromString(FString::Printf(TEXT("Blocked at %s - visibility is Visible and it covers the clicked pixel, so nothing below it receives the click."), *Last->Target));

                            }

                            return FText::FromString(Last->Summary);

                        }

                        return TMLoc::Text(TEXT("Click anywhere in PIE to capture the Slate, UMG, actor hit, and bound-event path."), TEXT("PIE에서 아무 곳이나 클릭하면 Slate, UMG, 액터 히트, 바운드 이벤트 경로를 캡처합니다."));

                    }).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 7, 0, 0)

                    [SNew(STextBlock).Text_Lambda([]()

                    {

                        const FClickCaptureHistoryItem* Last = DiagnosticsSession.GetLastCapture();
                        if (!Last) return FText::GetEmpty();

                        const FString HitObjectText = Last->ClickedObject.IsEmpty()
                            ? FString()
                            : FString::Printf(TEXT(" | Hit object: %s"), *Last->ClickedObject);
                        const FString BindingClassText = Last->BindingClasses.IsEmpty()
                            ? FString()
                            : FString::Printf(TEXT(" | Bound in: %s"), *Last->BindingClasses);

                        return FText::FromString(FString::Printf(
                            TEXT("Click event class: %s%s%s"),
                            Last->EventClass.IsEmpty() ? TEXT("<unknown>") : *Last->EventClass,
                            *BindingClassText,
                            *HitObjectText));

                    }).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.34f, 0.42f, 0.58f)).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))]

                ];

        }



        TSharedRef<SWidget> BuildHistoryRow(int32 Index)

        {

            return SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))

                .Padding(FMargin(11, 5))

                [

                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)

                    [SNew(STextBlock).Text_Lambda([Index]()

                    {

                        const TArray<FClickCaptureHistoryItem>& History = DiagnosticsSession.GetClickHistory();

                        if (!History.IsValidIndex(Index)) return TMLoc::Text(TEXT("*"), TEXT("*"));

                        return FText::FromString(History[Index].Verdict == EDiagnosticVerdict::Blocked ? TEXT("*") : TEXT("*"));

                    }).ColorAndOpacity_Lambda([Index]()

                    {

                        const TArray<FClickCaptureHistoryItem>& History = DiagnosticsSession.GetClickHistory();

                        if (!History.IsValidIndex(Index)) return FLinearColor(0.45f, 0.45f, 0.45f);

                        return History[Index].Verdict == EDiagnosticVerdict::Blocked ? FLinearColor(0.86f, 0.14f, 0.16f) : FLinearColor(0.04f, 0.65f, 0.08f);

                    })]

                    + SHorizontalBox::Slot().FillWidth(1.0f)

                    [SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()

                    [SNew(STextBlock).Text_Lambda([Index]()

                    {

                        const TArray<FClickCaptureHistoryItem>& History = DiagnosticsSession.GetClickHistory();

                        return FText::FromString(History.IsValidIndex(Index) ? History[Index].Summary : FString());

                    }).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)

                    [SNew(STextBlock).Text_Lambda([Index]()

                    {

                        const TArray<FClickCaptureHistoryItem>& History = DiagnosticsSession.GetClickHistory();
                        if (!History.IsValidIndex(Index) || History[Index].EventClass.IsEmpty()) return FText::GetEmpty();
                        const FString BindingText = History[Index].BindingClasses.IsEmpty()
                            ? FString()
                            : FString::Printf(TEXT(" | Bound in: %s"), *History[Index].BindingClasses);
                        return FText::FromString(FString::Printf(
                            TEXT("Class: %s%s"),
                            *History[Index].EventClass,
                            *BindingText));

                    }).ColorAndOpacity(FLinearColor(0.42f, 0.48f, 0.60f)).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 7))]

                    ]

                    + SHorizontalBox::Slot().AutoWidth()

                    [SNew(STextBlock).Text_Lambda([Index]()

                    {

                        const TArray<FClickCaptureHistoryItem>& History = DiagnosticsSession.GetClickHistory();

                        if (!History.IsValidIndex(Index)) return FText::GetEmpty();

                        const double Delta = FMath::Max(0.0, FPlatformTime::Seconds() - History[Index].TimeSeconds);

                        if (Delta < 1.0) return TMLoc::Text(TEXT("now"), TEXT("now"));

                        if (Delta < 60.0) return FText::FromString(FString::Printf(TEXT("%ds ago"), FMath::RoundToInt(Delta)));

                        return FText::FromString(FString::Printf(TEXT("%dm ago"), FMath::RoundToInt(Delta / 60.0)));

                    }).ColorAndOpacity(FLinearColor(0.46f, 0.46f, 0.46f)).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))]

                ];

        }



        void RefreshHistoryRows()

        {

            if (!HistoryRows.IsValid()) return;

            HistoryRows->ClearChildren();

            const TArray<FClickCaptureHistoryItem>& History = DiagnosticsSession.GetClickHistory();

            const int32 MaxRows = FMath::Min(History.Num(), 5);

            if (MaxRows == 0)

            {

                HistoryRows->AddSlot().AutoHeight()

                [SNew(STextBlock).Text(TMLoc::Text(TEXT("No click history yet."), TEXT("아직 클릭 기록이 없습니다."))).ColorAndOpacity(FLinearColor(0.54f, 0.54f, 0.54f))];

                return;

            }

            for (int32 Index = 0; Index < MaxRows; ++Index)

            {

                HistoryRows->AddSlot().AutoHeight().Padding(0, 0, 0, 4)[BuildHistoryRow(Index)];

            }

        }



        TSharedRef<SWidget> BuildResolverTab()

        {

            TSharedRef<SVerticalBox> StepBox = SNew(SVerticalBox);

            for (int32 Index = 0; Index < 7; ++Index)

            {

                StepBox->AddSlot().AutoHeight().Padding(0, 0, 0, 5)[BuildStepRow(Index)];

            }

            return SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight()

                [

                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)

                    [BuildStatusPill(

                        TAttribute<FText>::CreateLambda([]() { return FText::FromString(GetModeText(DiagnosticsSession.GetMode()).Equals(TEXT("STATIC")) ? TMLoc::String(TEXT("Idle"), TEXT("대기")) : TMLoc::String(TEXT("Armed"), TEXT("준비됨"))); }),

                        TAttribute<FSlateColor>::CreateLambda([]() { return DiagnosticsSession.GetMode() == EDiagnosticMode::Static ? FSlateColor(FLinearColor(0.90f, 0.90f, 0.90f, 1.0f)) : FSlateColor(FLinearColor(0.78f, 0.94f, 0.78f, 1.0f)); }),

                        TAttribute<FSlateColor>::CreateLambda([]() { return DiagnosticsSession.GetMode() == EDiagnosticMode::Static ? FSlateColor(FLinearColor(0.34f, 0.34f, 0.34f, 1.0f)) : FSlateColor(FLinearColor(0.08f, 0.45f, 0.12f, 1.0f)); }))]

                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

                    [SNew(STextBlock).Text(TMLoc::Text(TEXT("Click anywhere in PIE to capture"), TEXT("PIE에서 아무 곳이나 클릭해 캡처"))).ColorAndOpacity(FLinearColor(0.32f, 0.32f, 0.32f))]

                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)

                    [SNew(SButton).Text(TMLoc::Text(TEXT("Copy log"), TEXT("로그 복사"))).OnClicked_Lambda([]()

                    {

                        const FString LogText = DiagnosticsSession.BuildCopyLogText();

                        FPlatformApplicationMisc::ClipboardCopy(*LogText);

                        return FReply::Handled();

                    })]

                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)

                    [SNew(SButton).Text(TMLoc::Text(TEXT("Add to Investigation"), TEXT("조사에 추가"))).OnClicked_Lambda([]()

                    {

                        TMInvestigationSession::RecordEvidence(
                            TEXT("Click Event Diagnostics"),
                            FString::Printf(TEXT("Captured click history: %d event(s)"), DiagnosticsSession.GetClickHistory().Num()));

                        TMInvestigationSession::OpenWindow();

                        return FReply::Handled();

                    })]


                    + SHorizontalBox::Slot().AutoWidth()

                    [SNew(SButton).Text(TMLoc::Text(TEXT("Clear history"), TEXT("기록 지우기"))).OnClicked_Lambda([this]()

                    {

                        DiagnosticsSession.ClearClickHistory();

                        RefreshHistoryRows();

                        return FReply::Handled();

                    })]

                ]

                + SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)[BuildLastCaptureCard()]

                + SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 5)

                [SNew(STextBlock).Text_Lambda([]() { return FText::FromString(FString::Printf(TEXT("%s (%d %s)"), *TMLoc::String(TEXT("Click history"), TEXT("클릭 기록")), DiagnosticsSession.GetClickHistory().Num(), *TMLoc::String(TEXT("captured"), TEXT("캡처됨")))); }).ColorAndOpacity(FLinearColor(0.48f, 0.48f, 0.48f))]

                + SVerticalBox::Slot().AutoHeight()[SAssignNew(HistoryRows, SVerticalBox)]

                + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)

                [SNew(SButton).HAlign(HAlign_Center).Text_Lambda([this]()

                {

                    return bShowFullSteps ? TMLoc::Text(TEXT("Hide full diagnostic steps ^"), TEXT("전체 진단 단계 숨기기 ^")) : TMLoc::Text(TEXT("View full diagnostic steps for last click v"), TEXT("마지막 클릭의 전체 진단 단계 보기 v"));

                }).OnClicked_Lambda([this]()

                {

                    bShowFullSteps = !bShowFullSteps;

                    return FReply::Handled();

                })]

                + SVerticalBox::Slot().FillHeight(1.0f).Padding(0, 8, 0, 0)

                [

                    SNew(SScrollBox)

                    .Visibility_Lambda([this]() { return bShowFullSteps ? EVisibility::Visible : EVisibility::Collapsed; })

                    + SScrollBox::Slot()[StepBox]

                ];

        }



        TSharedRef<SWidget> BuildTargetBar()



        {



            return SNew(SBorder)



                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))



                .Padding(9.0f)



                [



                    SNew(SVerticalBox)



                    + SVerticalBox::Slot().AutoHeight()



                    [



                        SNew(SHorizontalBox)



                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)



                        [



                            SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel"))).Padding(FMargin(7, 3))



                            [SNew(STextBlock).Text_Lambda([]() { return FText::FromString(GetModeText(DiagnosticsSession.GetMode())); }).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))]



                        ]



                        + SHorizontalBox::Slot().FillWidth(1.0f)



                        [



                            SNew(STextBlock).Text_Lambda([]() { return FText::FromString(DiagnosticsSession.GetTargetName()); }).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 11))



                        ]



                        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)



                        [



                            SNew(SButton).Text(TMLoc::Text(TEXT("Pick Next PIE Click/Touch"), TEXT("다음 PIE 클릭/터치 대상 선택"))).OnClicked_Lambda([this]()



                            {



                                DiagnosticsSession.ArmNextClickTarget();



                                RefreshAuditRows();



                                RefreshMatrixRows();



                                return FReply::Handled();



                            })



                        ]



                        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)



                        [



                            SNew(SButton).Text(TMLoc::Text(TEXT("Use Selected Actor"), TEXT("선택 액터 사용"))).OnClicked_Lambda([this]()



                            {



                                AActor* Actor = nullptr;



                                if (GEditor && GEditor->GetSelectedActors())



                                {



                                    for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It) { Actor = Cast<AActor>(*It); if (Actor) break; }



                                }



                                DiagnosticsSession.SelectActor(Actor);



                                RefreshAuditRows();



                                RefreshMatrixRows();



                                return FReply::Handled();



                            })



                        ]



                        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)



                        [



                            SNew(SButton).Text(TMLoc::Text(TEXT("Use Selected Widget"), TEXT("선택 위젯 사용"))).OnClicked_Lambda([this]()



                            {



                                UWidgetBlueprint* Widget = nullptr;



                                FContentBrowserModule& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));



                                TArray<FAssetData> Assets;



                                ContentBrowser.Get().GetSelectedAssets(Assets);



                                for (const FAssetData& Asset : Assets) { Widget = Cast<UWidgetBlueprint>(Asset.GetAsset()); if (Widget) break; }



                                DiagnosticsSession.SelectWidget(Widget);



                                RefreshAuditRows();



                                RefreshMatrixRows();



                                return FReply::Handled();



                            })



                        ]



                        + SHorizontalBox::Slot().AutoWidth()



                        [



                            SNew(SButton).Text(TMLoc::Text(TEXT("Clear"), TEXT("지우기"))).OnClicked_Lambda([this]()



                            {



                                DiagnosticsSession.ClearTarget();



                                RefreshMatrixRows();



                                return FReply::Handled();



                            })



                        ]



                    ]



                    + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)



                    [



                        SNew(STextBlock).Text(TMLoc::Text(TEXT("Pick Next PIE Click/Touch inspects only the UMG widget or Visibility-hit actor under that pointer event. Manual actor/widget selection is also available; no project-wide Blueprint load is performed."), TEXT("다음 PIE 클릭/터치 대상 선택은 해당 포인터 이벤트 아래의 UMG 위젯 또는 Visibility에 맞은 액터만 조사합니다. 액터/위젯 직접 선택도 가능하며 프로젝트 전체 Blueprint를 로드하지 않습니다."))).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.58f, 0.62f, 0.68f))



                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)

                    [

                        SNew(STextBlock)

                        .Text(TMLoc::Text(TEXT("Confidence: Medium. This is a target-scoped pointer diagnosis; unloaded Blueprints, native code, dynamic delegates, and post-event input-mode changes may need Details/PIE confirmation."), TEXT("확신도: 중간. 이 진단은 선택한 포인터 대상 기준이며, 로드되지 않은 Blueprint, 네이티브 코드, 동적 Delegate, 이벤트 이후 Input Mode 변경은 Details/PIE 확인이 필요할 수 있습니다.")))

                        .AutoWrapText(true)

                        .ColorAndOpacity(FLinearColor(0.86f, 0.68f, 0.30f))

                    ]



                ];



        }







        TSharedRef<SWidget> BuildStepRow(int32 Index)



        {



            return SNew(SBorder)



                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))



                .BorderBackgroundColor_Lambda([Index]()



                {



                    const FDiagnosticStep& Step = DiagnosticsSession.GetStep(Index);



                    const FLinearColor Color = GetVerdictColor(Step.Verdict);



                    const float Scale = Step.Verdict == EDiagnosticVerdict::Blocked ? 0.28f : 0.10f;



                    return FLinearColor(Color.R * Scale, Color.G * Scale, Color.B * Scale, 1.0f);



                })



                .Padding_Lambda([Index]() { return DiagnosticsSession.GetStep(Index).Verdict == EDiagnosticVerdict::Blocked ? FMargin(11.0f) : FMargin(8.0f); })



                [



                    SNew(SHorizontalBox)



                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)



                    [



                        SNew(STextBlock).Text_Lambda([Index]() { return FText::FromString(GetVerdictIcon(DiagnosticsSession.GetStep(Index).Verdict)); }).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15)).ColorAndOpacity_Lambda([Index]() { return GetVerdictColor(DiagnosticsSession.GetStep(Index).Verdict); })



                    ]



                    + SHorizontalBox::Slot().FillWidth(1.0f)



                    [



                        SNew(SVerticalBox)



                        + SVerticalBox::Slot().AutoHeight()



                        [SNew(STextBlock).Text_Lambda([Index]() { const FDiagnosticStep& S = DiagnosticsSession.GetStep(Index); return FText::FromString(FString::Printf(TEXT("%d  %s"), S.Number, *S.Name)); }).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))]



                        + SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)



                        [SNew(STextBlock).Text_Lambda([Index]() { return FText::FromString(DiagnosticsSession.GetStep(Index).Detail); }).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.68f, 0.71f, 0.76f))]



                    ]



                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10, 0, 0, 0)



                    [



                        SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel"))).Padding(FMargin(7, 3))



                        [SNew(STextBlock).Text_Lambda([Index]() { return FText::FromString(GetVerdictText(DiagnosticsSession.GetStep(Index).Verdict)); }).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8)).ColorAndOpacity_Lambda([Index]() { return GetVerdictColor(DiagnosticsSession.GetStep(Index).Verdict); })]



                    ]



                ];



        }







        TSharedRef<SWidget> BuildAuditTab()



        {



            return SNew(SVerticalBox)



                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 7)



                [SNew(SButton).Text(TMLoc::Text(TEXT("Rescan Input Mappings"), TEXT("입력 매핑 다시 스캔"))).OnClicked_Lambda([this]() { DiagnosticsSession.RefreshProjectScan(); RefreshAuditRows(); RefreshMatrixRows(); return FReply::Handled(); })]



                + SVerticalBox::Slot().FillHeight(1.0f)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(AuditRows, SVerticalBox)]];



        }







        TSharedRef<SWidget> BuildMatrixTab()



        {



            return SNew(SVerticalBox)



                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 7)



                [SNew(STextBlock).Text(TMLoc::Text(TEXT("Bindings found only in the selected or pointer-resolved target Blueprint and its relevant Level Blueprint."), TEXT("선택했거나 포인터로 확인한 대상 Blueprint와 관련 Level Blueprint의 바인딩만 표시합니다."))).AutoWrapText(true)]



                + SVerticalBox::Slot().FillHeight(1.0f)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(MatrixRows, SVerticalBox)]];



        }







        void RefreshAuditRows()



        {



            if (!AuditRows.IsValid()) return;



            AuditRows->ClearChildren();



            if (!DiagnosticsSession.GetProjectScan().bScanned)



            {



                AuditRows->AddSlot().AutoHeight()



                [SNew(STextBlock).Text(TMLoc::Text(TEXT("Input mappings have not been scanned. Select a target or press Rescan Input Mappings."), TEXT("입력 매핑이 아직 스캔되지 않았습니다. 대상을 선택하거나 입력 매핑 다시 스캔을 누르세요.")))];



                return;



            }



            for (const FInputAuditItem& Row : DiagnosticsSession.GetProjectScan().AuditItems)



            {



                const FLinearColor Color = Row.bWarning ? FLinearColor(0.96f, 0.67f, 0.18f) : FLinearColor(0.22f, 0.76f, 0.36f);



                AuditRows->AddSlot().AutoHeight().Padding(0, 0, 0, 4)



                [



                    SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder"))).Padding(7.0f)



                    [



                        SNew(SHorizontalBox)



                        + SHorizontalBox::Slot().FillWidth(0.14f)[SNew(STextBlock).Text(FText::FromString(Row.System))]



                        + SHorizontalBox::Slot().FillWidth(0.20f)[SNew(STextBlock).Text(FText::FromString(Row.Context))]



                        + SHorizontalBox::Slot().FillWidth(0.18f)[SNew(STextBlock).Text(FText::FromString(Row.Action)).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))]



                        + SHorizontalBox::Slot().FillWidth(0.14f)[SNew(STextBlock).Text(FText::FromString(Row.Key))]



                        + SHorizontalBox::Slot().FillWidth(0.12f)[SNew(STextBlock).Text(FText::FromString(Row.Status)).ColorAndOpacity(Color)]



                        + SHorizontalBox::Slot().FillWidth(0.32f)[SNew(STextBlock).Text(FText::FromString(Row.Detail)).AutoWrapText(true).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))]



                    ]



                ];



            }



        }







        void RefreshMatrixRows()



        {



            if (!MatrixRows.IsValid()) return;



            MatrixRows->ClearChildren();



            const TArray<FBindingItem>& Bindings = DiagnosticsSession.GetTargetBindingItems();



            if (Bindings.IsEmpty())



            {



                MatrixRows->AddSlot().AutoHeight()



                [SNew(STextBlock).Text(TMLoc::Text(TEXT("No target selected. Use a selected asset/actor or pick the next PIE click/touch."), TEXT("선택된 대상이 없습니다. 선택한 에셋/액터를 사용하거나 다음 PIE 클릭/터치 대상을 선택하세요.")))];



                return;



            }



            for (const FBindingItem& Row : Bindings)



            {



                const FLinearColor Color = Row.bDead ? FLinearColor(0.94f, 0.27f, 0.22f) : FLinearColor(0.78f, 0.80f, 0.84f);



                MatrixRows->AddSlot().AutoHeight().Padding(0, 0, 0, 4)



                [



                    SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder"))).BorderBackgroundColor(Row.bDead ? FLinearColor(0.20f, 0.04f, 0.04f, 1.0f) : FLinearColor::White).Padding(7.0f)



                    [



                        SNew(SHorizontalBox)



                        + SHorizontalBox::Slot().FillWidth(0.25f)[SNew(STextBlock).Text(FText::FromString(Row.Action)).ColorAndOpacity(Color).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))]



                        + SHorizontalBox::Slot().FillWidth(0.28f)[SNew(STextBlock).Text(FText::FromString(Row.Blueprint))]



                        + SHorizontalBox::Slot().FillWidth(0.20f)[SNew(STextBlock).Text(FText::FromString(Row.Graph))]



                        + SHorizontalBox::Slot().FillWidth(0.27f)[SNew(STextBlock).Text(FText::FromString(Row.Binding)).ColorAndOpacity(Color)]



                    ]



                ];



            }



        }







        TSharedPtr<SWidgetSwitcher> Switcher;

        TSharedPtr<SVerticalBox> AuditRows;

        TSharedPtr<SVerticalBox> MatrixRows;

        TSharedPtr<SVerticalBox> HistoryRows;

        FString LastRenderedTargetName;

        int32 LastRenderedHistoryCount = INDEX_NONE;

        bool bShowFullSteps = false;

    };







    bool bClickDiagnosticsTabRegistered = false;







    TSharedRef<SDockTab> SpawnClickDiagnosticsTab(const FSpawnTabArgs&)



    {



        return SNew(SDockTab).TabRole(ETabRole::NomadTab).Label(TMLoc::Text(TEXT("Click/Touch Event Diagnostics"), TEXT("클릭/터치 이벤트 진단")))[SNew(SClickDiagnosticsWidget)];



    }







    void RegisterClickDiagnosticsTab()



    {



        if (bClickDiagnosticsTabRegistered) return;



        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ClickDiagnosticsTabId, FOnSpawnTab::CreateStatic(&SpawnClickDiagnosticsTab))



            .SetDisplayName(TMLoc::Text(TEXT("Click/Touch Event Diagnostics"), TEXT("클릭/터치 이벤트 진단")))



            .SetTooltipText(TMLoc::Text(TEXT("Diagnose the seven stages of click/touch delivery."), TEXT("클릭/터치 전달의 7단계를 진단합니다.")))



            .SetIcon(FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.ClickEventDiagnostics")));



        bClickDiagnosticsTabRegistered = true;



    }



}







namespace TMClickDiagnostics



{



    void RegisterMenus()



    {



        DiagnosticsSession.Start();



        RegisterClickDiagnosticsTab();



        auto AddEntry = [](UToolMenu* Menu, const FName Name)



        {



            if (!Menu) return;



            Menu->FindOrAddSection(TEXT("TraceMotive")).AddMenuEntry(



                Name,



                TMLoc::Text(TEXT("Click/Touch Event Diagnostics"), TEXT("클릭/터치 이벤트 진단")),



                TMLoc::Text(TEXT("Open the click/touch delivery diagnostics panel."), TEXT("클릭/터치 전달 진단 패널을 엽니다.")),



                FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.ClickEventDiagnostics")),



                FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&) { TMClickDiagnostics::OpenWindow(); }));



        };



        AddEntry(UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window")), TEXT("TMOpenClickDiagnostics"));



        AddEntry(UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools")), TEXT("TMToolsOpenClickDiagnostics"));



    }







    void UnregisterMenus()



    {



        DiagnosticsSession.Stop();



        if (bClickDiagnosticsTabRegistered)



        {



            FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ClickDiagnosticsTabId);



            bClickDiagnosticsTabRegistered = false;



        }



    }







    void OpenWindow()



    {



        RegisterClickDiagnosticsTab();



        FGlobalTabmanager::Get()->TryInvokeTab(ClickDiagnosticsTabId);



    }



}
