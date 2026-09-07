#include "TMGlobalSpeedControl.h"
#include "TMStyle.h"

#include "TMLocalization.h"

#include "Components/AudioComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Docking/TabManager.h"
#include "Rendering/DrawElements.h"
#include "Misc/OutputDevice.h"
#include "Misc/ScopeLock.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Runtime/Launch/Resources/Version.h"

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
#include "Widgets/Input/SSearchableComboBox.h"
#else
#include "SSearchableComboBox.h"
#endif
#include "UObject/Script.h"
#include "UObject/Stack.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    const FName GlobalSpeedControlTabId(TEXT("TraceMotive.GlobalSpeedControl"));
    bool bGlobalSpeedControlTabRegistered = false;

    constexpr float GlobalMinSpeed = 0.1f;
    constexpr float GlobalMaxSpeed = 20.0f;

    enum class ETMSkipTargetType : uint8
    {
        LogText,
        BlueprintClass
    };

    enum class ETMSkipTargetAction : uint8
    {
        ResetToNormal,
        PauseGame
    };

    float SnapGlobalSpeed(const float Speed)
    {
        const float ClampedSpeed = FMath::Clamp(Speed, GlobalMinSpeed, GlobalMaxSpeed);
        if (FMath::IsNearlyEqual(ClampedSpeed, 0.25f, 0.01f))
        {
            return 0.25f;
        }
        return FMath::Clamp(static_cast<float>(FMath::RoundToInt(ClampedSpeed * 10.0f)) / 10.0f, GlobalMinSpeed, GlobalMaxSpeed);
    }

    bool IsPlayableWorld(const UWorld* World)
    {
        return World
            && (World->WorldType == EWorldType::PIE
                || World->WorldType == EWorldType::Game
                || World->WorldType == EWorldType::GamePreview);
    }

    TArray<UWorld*> GetPlayableWorlds()
    {
        TArray<UWorld*> Worlds;
        if (!GEngine)
        {
            return Worlds;
        }

        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (IsPlayableWorld(World))
            {
                Worlds.AddUnique(World);
            }
        }
        return Worlds;
    }

    class FTMGlobalSpeedController;

    class FTMSkipLogOutputDevice final : public FOutputDevice
    {
    public:
        explicit FTMSkipLogOutputDevice(FTMGlobalSpeedController& InOwner)
            : Owner(InOwner)
        {
        }

        virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override;

    private:
        FTMGlobalSpeedController& Owner;
    };

    class FTMGlobalSpeedController
    {
    public:
        void SetSpeed(const float InSpeed)
        {
            const float SnappedSpeed = SnapGlobalSpeed(InSpeed);
            if (FMath::IsNearlyEqual(SnappedSpeed, 1.0f, 0.001f))
            {
                Reset();
                return;
            }

            TargetSpeed = SnappedSpeed;
            bActive = true;
            ApplyNow();
            EnsureTicker();
        }

        void Reset()
        {
            TargetSpeed = 1.0f;
            bActive = false;
            CancelPendingSkipPause();
            RestoreAudioPitch();
            ApplyWorldSpeed(1.0f);
            StopTickerIfIdle();
        }

        float GetSpeed() const
        {
            return TargetSpeed;
        }

        FString GetSpeedDisplayText() const
        {
            if (FMath::IsNearlyEqual(TargetSpeed, 0.25f, 0.001f))
            {
                return TEXT("0.25x");
            }
            const int32 Tenths = FMath::RoundToInt(TargetSpeed * 10.0f);
            if (Tenths % 10 == 0)
            {
                return FString::Printf(TEXT("%dx"), Tenths / 10);
            }
            return FString::Printf(TEXT("%.1fx"), static_cast<float>(Tenths) / 10.0f);
        }

        float GetSliderValue() const
        {
            return (TargetSpeed - GlobalMinSpeed) / (GlobalMaxSpeed - GlobalMinSpeed);
        }

        void SetSpeedFromSlider(const float SliderValue)
        {
            const float RawSpeed = FMath::Lerp(GlobalMinSpeed, GlobalMaxSpeed, FMath::Clamp(SliderValue, 0.0f, 1.0f));
            SetSpeed(RawSpeed);
        }

        bool IsActive() const
        {
            return bActive;
        }

        FString BuildStatusText() const
        {
            const TArray<UWorld*> Worlds = GetPlayableWorlds();
            return FString::Printf(TEXT("Speed %s | Worlds %d | Audio tracked %d"), *GetSpeedDisplayText(), Worlds.Num(), OriginalAudioPitch.Num());
        }

        void Stop()
        {
            Reset();
            SetSkipToTargetEnabled(false);
            if (TickerHandle.IsValid())
            {
                FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
                TickerHandle.Reset();
            }
            OriginalAudioPitch.Reset();
            RemoveLogOutputDevice();
            RemoveBlueprintEventHook();
        }

        void SetSkipToTargetEnabled(const bool bEnabled)
        {
            const bool bShouldArm = bEnabled && HasValidSkipTarget() && IsCurrentSkipTargetSupported();
            {
                FScopeLock Lock(&PendingLogTriggerMutex);
                bSkipToTargetEnabled = bShouldArm;
                ++SkipArmGeneration;
                if (!bSkipToTargetEnabled)
                {
                    PendingLogTriggerReason.Empty();
                    PendingLogTriggerGeneration = 0;
                }
            }

            if (!bShouldArm)
            {
                CancelPendingSkipPause();
            }

            if (bShouldArm)
            {
                RefreshSkipHooks();
                EnsureTicker();
            }
            else
            {
                RefreshSkipHooks();
                StopTickerIfIdle();
            }
        }

        bool IsSkipToTargetEnabled() const
        {
            return bSkipToTargetEnabled;
        }

        void SetSkipTargetType(const ETMSkipTargetType InTargetType)
        {
            if (SkipTargetType != InTargetType)
            {
                SetSkipToTargetEnabled(false);
                FScopeLock Lock(&PendingLogTriggerMutex);
                SkipTargetType = InTargetType;
            }
        }

        ETMSkipTargetType GetSkipTargetType() const
        {
            return SkipTargetType;
        }

        void SetSkipTargetText(const FString& InText)
        {
            {
                FScopeLock Lock(&PendingLogTriggerMutex);
                if (SkipTargetType == ETMSkipTargetType::LogText)
                {
                    LogNeedle = InText.TrimStartAndEnd();
                }
                else
                {
                    ClassNeedle = InText.TrimStartAndEnd();
                }
            }

            if (bSkipToTargetEnabled && !HasValidSkipTarget())
            {
                SetSkipToTargetEnabled(false);
            }
        }

        FString GetSkipTargetText() const
        {
            return SkipTargetType == ETMSkipTargetType::LogText ? LogNeedle : ClassNeedle;
        }

        bool HasValidSkipTarget() const
        {
            return SkipTargetType == ETMSkipTargetType::LogText
                ? !LogNeedle.IsEmpty()
                : BlueprintTargetClass.IsValid() && !BlueprintTargetFunction.IsNone();
        }

        void SetBlueprintTarget(UClass* InClass, const FName InFunction)
        {
            if (BlueprintTargetClass.Get() != InClass || BlueprintTargetFunction != InFunction)
            {
                SetSkipToTargetEnabled(false);
                BlueprintTargetClass = InClass;
                BlueprintTargetFunction = InFunction;
            }
        }

        UClass* GetBlueprintTargetClass() const
        {
            return BlueprintTargetClass.Get();
        }

        FName GetBlueprintTargetFunction() const
        {
            return BlueprintTargetFunction;
        }

        void SetSkipTargetAction(const ETMSkipTargetAction InAction)
        {
            SkipTargetAction = InAction;
        }

        ETMSkipTargetAction GetSkipTargetAction() const
        {
            return SkipTargetAction;
        }

        bool IsBlueprintClassTargetSupported() const
        {
#if DO_BLUEPRINT_GUARD
            return true;
#else
            return false;
#endif
        }

        FString GetSkipTargetSummary() const
        {
            if (!HasValidSkipTarget())
            {
                return SkipTargetType == ETMSkipTargetType::LogText
                    ? TEXT("Enter text from the Output Log to arm this target.")
                    : TEXT("Choose a Blueprint class and a function to arm this target.");
            }

            if (!IsCurrentSkipTargetSupported())
            {
                return TEXT("Blueprint class event targets are not available in this build configuration.");
            }

            if (SkipTargetType == ETMSkipTargetType::LogText)
            {
                return FString::Printf(TEXT("Log text: %s%s"), *LogNeedle, bSkipToTargetEnabled ? TEXT(" (armed)") : TEXT(" (not armed)"));
            }

            const FString TargetName = FString::Printf(TEXT("%s.%s"), *GetNameSafe(BlueprintTargetClass.Get()), *BlueprintTargetFunction.ToString());
            const TCHAR* ActionText = SkipTargetAction == ETMSkipTargetAction::PauseGame ? TEXT("pause PIE/Game") : TEXT("restore 1x");
            return FString::Printf(TEXT("Blueprint function: %s -> %s%s"), *TargetName, ActionText, bSkipToTargetEnabled ? TEXT(" (armed)") : TEXT(" (not armed)"));
        }

        void SetLogNeedle(const FString& InNeedle)
        {
            LogNeedle = InNeedle.TrimStartAndEnd();
        }

        FString GetLogNeedle() const
        {
            return LogNeedle;
        }

        void SetClassNeedle(const FString& InNeedle)
        {
            ClassNeedle = InNeedle.TrimStartAndEnd();
        }

        FString GetClassNeedle() const
        {
            return ClassNeedle;
        }

        FString GetLastTriggerText() const
        {
            return LastTriggerText.IsEmpty() ? TEXT("No skip target reached yet.") : LastTriggerText;
        }

        void NotifyLogLine(const FString& Line, const FName& Category, const ELogVerbosity::Type)
        {
            if (IsInGameThread())
            {
                if (!bSkipToTargetEnabled || SkipTargetType != ETMSkipTargetType::LogText || LogNeedle.IsEmpty())
                {
                    return;
                }

                if (Line.Contains(LogNeedle, ESearchCase::IgnoreCase) || Category.ToString().Contains(LogNeedle, ESearchCase::IgnoreCase))
                {
                    RequestSkipPause(FString::Printf(TEXT("Log target reached: [%s] %s"), *Category.ToString(), *Line.Left(240)));
                }
                return;
            }

            FScopeLock Lock(&PendingLogTriggerMutex);
            if (!bSkipToTargetEnabled || SkipTargetType != ETMSkipTargetType::LogText || LogNeedle.IsEmpty())
            {
                return;
            }

            if (Line.Contains(LogNeedle, ESearchCase::IgnoreCase) || Category.ToString().Contains(LogNeedle, ESearchCase::IgnoreCase))
            {
                PendingLogTriggerReason = FString::Printf(TEXT("Log target reached: [%s] %s"), *Category.ToString(), *Line.Left(240));
                PendingLogTriggerGeneration = SkipArmGeneration;
            }
        }

#if DO_BLUEPRINT_GUARD
        void HandleBlueprintScriptEnter(const FBlueprintContextTracker&, const UObject* ContextObject, const UFunction* ContextFunction)
        {
            UClass* TargetClass = BlueprintTargetClass.Get();
            if (!bSkipToTargetEnabled || SkipTargetType != ETMSkipTargetType::BlueprintClass || !TargetClass || BlueprintTargetFunction.IsNone() || !ContextObject || !ContextFunction)
            {
                return;
            }

            if (ContextFunction->GetFName() == BlueprintTargetFunction && ContextObject->IsA(TargetClass))
            {
                RequestSkipPause(FString::Printf(TEXT("Blueprint function target reached: %s.%s"), *GetNameSafe(TargetClass), *BlueprintTargetFunction.ToString()));
            }
        }
#endif

    private:

        static bool IsDefaultTickFunctionName(const FString& FunctionName)
        {
            return FunctionName.Equals(TEXT("Tick"), ESearchCase::IgnoreCase)
                || FunctionName.Equals(TEXT("ReceiveTick"), ESearchCase::IgnoreCase)
                || FunctionName.Contains(TEXT("ActorTick"), ESearchCase::IgnoreCase)
                || FunctionName.Contains(TEXT("ComponentTick"), ESearchCase::IgnoreCase)
                || FunctionName.Contains(TEXT("TickFunction"), ESearchCase::IgnoreCase);
        }

        bool IsCurrentSkipTargetSupported() const
        {
            return SkipTargetType != ETMSkipTargetType::BlueprintClass || IsBlueprintClassTargetSupported();
        }

        void RefreshSkipHooks()
        {
            if (bSkipToTargetEnabled && SkipTargetType == ETMSkipTargetType::LogText)
            {
                EnsureLogOutputDevice();
            }
            else
            {
                RemoveLogOutputDevice();
            }

            if (bSkipToTargetEnabled && SkipTargetType == ETMSkipTargetType::BlueprintClass)
            {
                EnsureBlueprintEventHook();
            }
            else
            {
                RemoveBlueprintEventHook();
            }
        }

        void CancelPendingSkipPause()
        {
            bSkipPauseRequested = false;
            PendingTriggerReason.Empty();

            FScopeLock Lock(&PendingLogTriggerMutex);
            PendingLogTriggerReason.Empty();
            PendingLogTriggerGeneration = 0;
        }

        void PromotePendingLogTrigger()
        {
            FString TriggerReason;
            {
                FScopeLock Lock(&PendingLogTriggerMutex);
                if (PendingLogTriggerReason.IsEmpty() || PendingLogTriggerGeneration != SkipArmGeneration)
                {
                    PendingLogTriggerReason.Empty();
                    return;
                }
                TriggerReason = MoveTemp(PendingLogTriggerReason);
            }

            if (bSkipToTargetEnabled && SkipTargetType == ETMSkipTargetType::LogText)
            {
                RequestSkipPause(TriggerReason);
            }
        }

        void StopTickerIfIdle()
        {
            if (!bActive && !bSkipToTargetEnabled && !bSkipPauseRequested && TickerHandle.IsValid())
            {
                FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
                TickerHandle.Reset();
            }
        }

        void EnsureLogOutputDevice()
        {
            if (!LogOutputDevice.IsValid())
            {
                LogOutputDevice = MakeUnique<FTMSkipLogOutputDevice>(*this);
                if (GLog)
                {
                    GLog->AddOutputDevice(LogOutputDevice.Get());
                }
            }
        }

        void RemoveLogOutputDevice()
        {
            if (LogOutputDevice.IsValid())
            {
                if (GLog)
                {
                    GLog->RemoveOutputDevice(LogOutputDevice.Get());
                }
                LogOutputDevice.Reset();
            }
        }

        void EnsureBlueprintEventHook()
        {
#if DO_BLUEPRINT_GUARD
            if (!BlueprintScriptEnterHandle.IsValid())
            {
                BlueprintScriptEnterHandle = FBlueprintContextTracker::OnEnterScriptContext.AddRaw(this, &FTMGlobalSpeedController::HandleBlueprintScriptEnter);
            }
#endif
        }

        void RemoveBlueprintEventHook()
        {
#if DO_BLUEPRINT_GUARD
            if (BlueprintScriptEnterHandle.IsValid())
            {
                FBlueprintContextTracker::OnEnterScriptContext.Remove(BlueprintScriptEnterHandle);
                BlueprintScriptEnterHandle.Reset();
            }
#endif
        }

        void RequestSkipPause(const FString& Reason)
        {
            if (bSkipPauseRequested)
            {
                return;
            }
            PendingTriggerReason = Reason;
            bSkipPauseRequested = true;
            EnsureTicker();
        }

        void ApplyPendingSkipPause()
        {
            if (!bSkipPauseRequested)
            {
                return;
            }

            LastTriggerText = PendingTriggerReason;
            PendingTriggerReason.Empty();
            bSkipPauseRequested = false;

            Reset();
            if (SkipTargetAction == ETMSkipTargetAction::ResetToNormal)
            {
                return;
            }

            for (UWorld* World : GetPlayableWorlds())
            {
                if (World)
                {
                    UGameplayStatics::SetGamePaused(World, true);
                }
            }
        }

        void EnsureTicker()
        {
            if (TickerHandle.IsValid())
            {
                return;
            }

            TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FTMGlobalSpeedController::Tick), 0.25f);
        }

        bool Tick(float)
        {
            PromotePendingLogTrigger();
            ApplyPendingSkipPause();
            if (bActive)
            {
                ApplyNow();
                PruneInvalidAudioComponents();
            }

            if (!bActive && !bSkipToTargetEnabled && !bSkipPauseRequested)
            {
                TickerHandle.Reset();
                return false;
            }
            return true;
        }

        void ApplyNow()
        {
            ApplyWorldSpeed(TargetSpeed);
            ApplyAudioPitch(TargetSpeed);
        }

        void ApplyWorldSpeed(const float Speed) const
        {
            for (UWorld* World : GetPlayableWorlds())
            {
                if (World)
                {
                    UGameplayStatics::SetGlobalTimeDilation(World, Speed);
                }
            }
        }

        void ApplyAudioPitch(const float Speed)
        {
            for (TObjectIterator<UAudioComponent> It; It; ++It)
            {
                UAudioComponent* AudioComponent = *It;
                if (!AudioComponent || AudioComponent->IsTemplate())
                {
                    continue;
                }

                UWorld* World = AudioComponent->GetWorld();
                if (!IsPlayableWorld(World))
                {
                    continue;
                }

                if (!OriginalAudioPitch.Contains(AudioComponent))
                {
                    OriginalAudioPitch.Add(AudioComponent, AudioComponent->PitchMultiplier);
                }

                const float OriginalPitch = OriginalAudioPitch.FindRef(AudioComponent);
                AudioComponent->SetPitchMultiplier(FMath::Clamp(OriginalPitch * Speed, 0.05f, 20.0f));
            }
        }

        void RestoreAudioPitch()
        {
            for (auto It = OriginalAudioPitch.CreateIterator(); It; ++It)
            {
                if (UAudioComponent* AudioComponent = It.Key().Get())
                {
                    AudioComponent->SetPitchMultiplier(It.Value());
                }
            }
            OriginalAudioPitch.Reset();
        }

        void PruneInvalidAudioComponents()
        {
            for (auto It = OriginalAudioPitch.CreateIterator(); It; ++It)
            {
                if (!It.Key().IsValid())
                {
                    It.RemoveCurrent();
                }
            }
        }

    private:
        float TargetSpeed = 1.0f;
        bool bActive = false;
        FTSTicker::FDelegateHandle TickerHandle;
        TMap<TWeakObjectPtr<UAudioComponent>, float> OriginalAudioPitch;
        TUniquePtr<FTMSkipLogOutputDevice> LogOutputDevice;
        FDelegateHandle BlueprintScriptEnterHandle;
        FString LogNeedle;
        FString ClassNeedle;
        TWeakObjectPtr<UClass> BlueprintTargetClass;
        FName BlueprintTargetFunction;
        FString PendingTriggerReason;
        FCriticalSection PendingLogTriggerMutex;
        FString PendingLogTriggerReason;
        uint64 PendingLogTriggerGeneration = 0;
        uint64 SkipArmGeneration = 0;
        FString LastTriggerText;
        ETMSkipTargetType SkipTargetType = ETMSkipTargetType::LogText;
        ETMSkipTargetAction SkipTargetAction = ETMSkipTargetAction::PauseGame;
        bool bSkipToTargetEnabled = false;
        bool bSkipPauseRequested = false;
    };

    void FTMSkipLogOutputDevice::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
    {
        Owner.NotifyLogLine(V ? FString(V) : FString(), Category, Verbosity);
    }

    FTMGlobalSpeedController GGlobalSpeedController;

    class STMSpeedDial : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(STMSpeedDial) {}
        SLATE_END_ARGS()

        void Construct(const FArguments&) {}
        virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(170.0f, 170.0f); }

        virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
            FSlateWindowElementList& DrawElements, int32 LayerId, const FWidgetStyle&, bool) const override
        {
            const FVector2D Size = Geometry.GetLocalSize();
            const FVector2D Center = Size * 0.5f;
            const float Radius = GetDialRadius(Size);
            const TArray<float>& DialSpeeds = GetDialSpeeds();
            const float CurrentSpeed = GGlobalSpeedController.GetSpeed();
            int32 SelectedIndex = 0;
            float ClosestDistance = TNumericLimits<float>::Max();

            for (int32 Index = 0; Index < DialSpeeds.Num(); ++Index)
            {
                const float Distance = FMath::Abs(DialSpeeds[Index] - CurrentSpeed);
                if (Distance < ClosestDistance)
                {
                    ClosestDistance = Distance;
                    SelectedIndex = Index;
                }
            }

            for (int32 Index = 0; Index < DialSpeeds.Num(); ++Index)
            {
                const float Alpha = static_cast<float>(Index) / static_cast<float>(DialSpeeds.Num() - 1);
                const float Angle = FMath::DegreesToRadians(StartDegrees + SweepDegrees * Alpha);
                const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
                const bool bSelected = Index == SelectedIndex;
                const float InnerRadius = Radius - (bSelected ? 15.0f : 10.0f);
                const TArray<FVector2D> Tick = { Center + Direction * InnerRadius, Center + Direction * Radius };
                FSlateDrawElement::MakeLines(DrawElements, LayerId + 1, Geometry.ToPaintGeometry(), Tick,
                    ESlateDrawEffect::None,
                    bSelected ? FLinearColor(0.95f, 0.22f, 0.16f, 1.0f) : FLinearColor(0.30f, 0.48f, 0.54f, 1.0f),
                    true, bSelected ? 5.0f : 3.0f);

                const FString Label = DialSpeeds[Index] < 1.0f
                    ? FString::Printf(TEXT("%g"), DialSpeeds[Index])
                    : FString::FromInt(FMath::RoundToInt(DialSpeeds[Index]));
                const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), DialSpeeds[Index] < 1.0f ? 6 : 7);
                const FVector2D LabelSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label, LabelFont);
                const FVector2D LabelCenter = Center + Direction * (Radius - 23.0f);
                FSlateDrawElement::MakeText(DrawElements, LayerId + 2,
                    Geometry.ToPaintGeometry(LabelSize, FSlateLayoutTransform(LabelCenter - LabelSize * 0.5f)), Label,
                    LabelFont, ESlateDrawEffect::None,
                    bSelected ? FLinearColor::White : FLinearColor(0.72f, 0.78f, 0.82f, 1.0f));
            }

            constexpr int32 CenterSegments = 40;
            const float CenterRadius = Radius * 0.28f;
            const float SelectedAlpha = static_cast<float>(SelectedIndex) / static_cast<float>(DialSpeeds.Num() - 1);
            const float SelectedAngle = FMath::DegreesToRadians(StartDegrees + SweepDegrees * SelectedAlpha);
            const FVector2D NeedleDirection(FMath::Cos(SelectedAngle), FMath::Sin(SelectedAngle));
            const TArray<FVector2D> Needle =
            {
                Center + NeedleDirection * (CenterRadius + 4.0f),
                Center + NeedleDirection * (Radius - 30.0f)
            };
            FSlateDrawElement::MakeLines(DrawElements, LayerId + 3, Geometry.ToPaintGeometry(), Needle,
                ESlateDrawEffect::None, FLinearColor(0.95f, 0.22f, 0.16f, 0.95f), true, 3.0f);

            TArray<FVector2D> CenterFillRing;
            TArray<FVector2D> CenterRing;
            for (int32 Index = 0; Index <= CenterSegments; ++Index)
            {
                const float Angle = 2.0f * PI * static_cast<float>(Index) / static_cast<float>(CenterSegments);
                const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
                CenterFillRing.Add(Center + Direction * (CenterRadius * 0.5f));
                CenterRing.Add(Center + Direction * CenterRadius);
            }
            FSlateDrawElement::MakeLines(DrawElements, LayerId + 4, Geometry.ToPaintGeometry(), CenterFillRing,
                ESlateDrawEffect::None, FLinearColor(0.055f, 0.075f, 0.085f, 1.0f), true, CenterRadius + 2.0f);
            FSlateDrawElement::MakeLines(DrawElements, LayerId + 5, Geometry.ToPaintGeometry(), CenterRing,
                ESlateDrawEffect::None, FLinearColor(0.20f, 0.68f, 0.82f, 1.0f), true, 3.0f);

            const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
            const FSlateFontInfo CenterFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 11);
            const FString SpeedText = GGlobalSpeedController.GetSpeedDisplayText();
            const FVector2D SpeedSize = FontMeasure->Measure(SpeedText, CenterFont);
            const FVector2D SpeedPos(Center.X - SpeedSize.X * 0.5f, Center.Y - SpeedSize.Y * 0.5f);
            FSlateDrawElement::MakeText(DrawElements, LayerId + 6,
                Geometry.ToPaintGeometry(SpeedSize, FSlateLayoutTransform(SpeedPos)), SpeedText,
                CenterFont, ESlateDrawEffect::None, FLinearColor::White);

            const FSlateFontInfo ResetFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8);
            const FString ResetText(TEXT("Reset"));
            const FVector2D ResetSize = FontMeasure->Measure(ResetText, ResetFont);
            const FSlateRect ResetRect = GetResetButtonRect(Size);
            const FVector2D ResetRectSize(ResetRect.Right - ResetRect.Left, ResetRect.Bottom - ResetRect.Top);
            FSlateDrawElement::MakeBox(DrawElements, LayerId + 7,
                Geometry.ToPaintGeometry(ResetRectSize, FSlateLayoutTransform(FVector2D(ResetRect.Left, ResetRect.Top))),
                FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
                ESlateDrawEffect::None, FLinearColor(0.10f, 0.16f, 0.18f, 1.0f));
            const TArray<FVector2D> ResetBorder =
            {
                FVector2D(ResetRect.Left, ResetRect.Top),
                FVector2D(ResetRect.Right, ResetRect.Top),
                FVector2D(ResetRect.Right, ResetRect.Bottom),
                FVector2D(ResetRect.Left, ResetRect.Bottom),
                FVector2D(ResetRect.Left, ResetRect.Top)
            };
            FSlateDrawElement::MakeLines(DrawElements, LayerId + 8, Geometry.ToPaintGeometry(), ResetBorder,
                ESlateDrawEffect::None, FLinearColor(0.20f, 0.68f, 0.82f, 0.95f), true, 1.5f);
            const FVector2D ResetPos(ResetRect.Left + (ResetRectSize.X - ResetSize.X) * 0.5f, ResetRect.Top + (ResetRectSize.Y - ResetSize.Y) * 0.5f);
            FSlateDrawElement::MakeText(DrawElements, LayerId + 9,
                Geometry.ToPaintGeometry(ResetSize, FSlateLayoutTransform(ResetPos)), ResetText,
                ResetFont, ESlateDrawEffect::None, FLinearColor::White);
            return LayerId + 9;
        }

        virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
        {
            if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
            const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
            const FVector2D Size = Geometry.GetLocalSize();
            const FSlateRect ResetRect = GetResetButtonRect(Size);
            if (ResetRect.ContainsPoint(Local))
            {
                GGlobalSpeedController.Reset();
                return FReply::Handled();
            }

            const FVector2D Center = Size * 0.5f;
            const FVector2D Delta = Local - Center;
            float Degrees = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
            if (Degrees < 0.0f) Degrees += 360.0f;
            if (Degrees < StartDegrees) Degrees += 360.0f;
            const float Alpha = FMath::Clamp((Degrees - StartDegrees) / SweepDegrees, 0.0f, 1.0f);
            const TArray<float>& DialSpeeds = GetDialSpeeds();
            const int32 Index = FMath::Clamp(FMath::RoundToInt(Alpha * static_cast<float>(DialSpeeds.Num() - 1)), 0, DialSpeeds.Num() - 1);
            GGlobalSpeedController.SetSpeed(DialSpeeds[Index]);
            return FReply::Handled();
        }

    private:
        static constexpr float StartDegrees = 135.0f;
        static constexpr float SweepDegrees = 270.0f;

        static const TArray<float>& GetDialSpeeds()
        {
            static const TArray<float> DialSpeeds = { 0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 18.0f, 19.0f, 20.0f };
            return DialSpeeds;
        }

        static float GetDialRadius(const FVector2D& Size)
        {
            return FMath::Max(28.0f, FMath::Min(Size.X, Size.Y) * 0.5f - 6.0f);
        }

        static FSlateRect GetResetButtonRect(const FVector2D& Size)
        {
            const FVector2D Center = Size * 0.5f;
            const float Radius = GetDialRadius(Size);
            const FVector2D ButtonSize(50.0f, 20.0f);
            const FVector2D ButtonCenter(Center.X, Center.Y + Radius * 0.78f);
            const FVector2D ButtonMin = ButtonCenter - ButtonSize * 0.5f;
            return FSlateRect(ButtonMin.X, ButtonMin.Y, ButtonMin.X + ButtonSize.X, ButtonMin.Y + ButtonSize.Y);
        }
    };

    class STMGlobalSpeedControlWidget : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(STMGlobalSpeedControlWidget) {}
        SLATE_END_ARGS()

        void Construct(const FArguments&)
        {
            ChildSlot
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
                .Padding(14.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
                    [
                        SNew(SBox)
                        .Visibility_Lambda([this]() { return bCompactHeight ? EVisibility::Collapsed : EVisibility::Visible; })
                        [BuildWarningBox()]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                    [
                        SNew(STextBlock)
                        .Text(TMLoc::Text(TEXT("Global Speed Control"), TEXT("Global Speed Control")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
                    [
                        SNew(STextBlock)
                        .Text(TMLoc::Text(TEXT("Applies gameplay time dilation and audio pitch speed together for PIE/Game worlds."), TEXT("Applies gameplay time dilation and audio pitch speed together for PIE/Game worlds.")))
                        .Visibility_Lambda([this]() { return bCompactHeight ? EVisibility::Collapsed : EVisibility::Visible; })
                        .AutoWrapText(true)
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
                    [
                        BuildSpeedSlider()
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 8)
                    [
                        SNew(SSeparator)
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                    [
                        SNew(SBox)
                        .Visibility_Lambda([this]() { return bCompactHeight ? EVisibility::Collapsed : EVisibility::Visible; })
                        [BuildSkipToTargetPanel()]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 8)
                    [
                        SNew(SSeparator)
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(this, &STMGlobalSpeedControlWidget::GetStatusText)
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
                    [
                        SNew(STextBlock)
                        .Text(TMLoc::Text(TEXT("Limitations: pitch-based audio speed can affect sound pitch, and some systems with custom clocks, media playback, network replication, or native timers may not follow global time dilation exactly."), TEXT("Limitations: pitch-based audio speed can affect sound pitch, and some systems with custom clocks, media playback, network replication, or native timers may not follow global time dilation exactly.")))
                        .Visibility_Lambda([this]() { return bCompactHeight ? EVisibility::Collapsed : EVisibility::Visible; })
                        .AutoWrapText(true)
                        .ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f, 1.0f))
                    ]
                ]
            ];
        }

        virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
        {
            const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
            bCompactHeight = (LocalSize.Y > 0.0f && LocalSize.Y < 390.0f) || (LocalSize.X > 0.0f && LocalSize.X < 560.0f);
            SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
        }

    private:
        TSharedRef<SWidget> BuildWarningBox() const
        {
            return SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FLinearColor(0.95f, 0.72f, 0.18f, 0.18f))
                .Padding(10.0f)
                [
                    SNew(STextBlock)
                    .Text(TMLoc::Text(TEXT("Warning: high speed can increase CPU load and make heavy projects, many Tick events, timers, physics, animation, and audio updates unstable or harder to debug."), TEXT("Warning: high speed can increase CPU load and make heavy projects, many Tick events, timers, physics, animation, and audio updates unstable or harder to debug.")))
                    .AutoWrapText(true)
                    .ColorAndOpacity(FLinearColor(1.0f, 0.78f, 0.28f, 1.0f))
                ];
        }

        TSharedRef<SWidget> BuildSpeedRiskNotice() const
        {
            return SNew(SBorder)
                .Visibility_Lambda([this]() { return GetSpeedRiskText().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
                .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
                .BorderBackgroundColor_Lambda([this]() { return GetSpeedRiskColor() * FLinearColor(1.0f, 1.0f, 1.0f, 0.16f); })
                .Padding(7.0f, 5.0f)
                [
                    SNew(STextBlock)
                    .Text(this, &STMGlobalSpeedControlWidget::GetSpeedRiskText)
                    .AutoWrapText(true)
                    .ColorAndOpacity_Lambda([this]() { return GetSpeedRiskColor(); })
                ];
        }

        FText GetSpeedRiskText() const
        {
            const float Speed = GGlobalSpeedController.GetSpeed();
            if (Speed >= 20.0f)
            {
                return TMLoc::Text(TEXT("Maximum 20x: frame skips, timer order changes, collision misses, and UI/audio desync can occur. Use only with Skip to Target."), TEXT(""));
            }
            if (Speed >= 8.0f)
            {
                return TMLoc::Text(TEXT("Skip-only speed (8x+): not recommended for normal play. Use this to reach a log or event target, then pause."), TEXT(""));
            }
            if (Speed > 6.0f)
            {
                return TMLoc::Text(TEXT("High speed (over 6x): physics, timers, complex animation, and networking can become unreliable. Prefer Skip to Target."), TEXT(""));
            }
            if (Speed > 3.0f)
            {
                return TMLoc::Text(TEXT("Caution (4x-6x): use only outside physics, dense AI, complex UI animation, and networking-heavy sections."), TEXT(""));
            }
            return FText::GetEmpty();
        }

        FLinearColor GetSpeedRiskColor() const
        {
            const float Speed = GGlobalSpeedController.GetSpeed();
            if (Speed >= 8.0f)
            {
                return FLinearColor(1.0f, 0.32f, 0.24f, 1.0f);
            }
            if (Speed > 3.0f)
            {
                return FLinearColor(1.0f, 0.74f, 0.24f, 1.0f);
            }
            return FLinearColor::Transparent;
        }

        TSharedRef<SWidget> BuildSpeedSlider()
        {
            return SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [SNew(STextBlock).Text(TMLoc::Text(TEXT("Speed"), TEXT("Speed"))).ColorAndOpacity(FSlateColor::UseSubduedForeground())]
                    + SHorizontalBox::Slot().FillWidth(1.0f).Padding(10, 0).VAlign(VAlign_Center)
                    [SNew(STextBlock).Text(this, &STMGlobalSpeedControlWidget::GetSpeedLabelText).Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                        .Visibility_Lambda([this]() { return bCompactHeight ? EVisibility::Collapsed : EVisibility::Visible; })
                        .Text(TMLoc::Text(TEXT("Reset"), TEXT("Reset"))).OnClicked_Lambda([]()
                        {
                            GGlobalSpeedController.Reset();
                            return FReply::Handled();
                        })
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SVerticalBox)
                    .Visibility_Lambda([this]() { return bCompactHeight ? EVisibility::Collapsed : EVisibility::Visible; })
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SSlider)
                        .MinValue(0.0f).MaxValue(1.0f).StepSize(1.0f / 199.0f)
                        .Value_Lambda([]() { return GGlobalSpeedController.GetSliderValue(); })
                        .OnValueChanged_Lambda([](float NewValue) { GGlobalSpeedController.SetSpeedFromSlider(NewValue); })
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)[BuildSpeedRuler()]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
                    [
                        SNew(STextBlock)
                        .Text(TMLoc::Text(TEXT("Speed snaps to 0.1x steps from 0.1x to 20x. Integer values are rounded cleanly; 1x restores normal time and audio pitch."), TEXT("Speed snaps to 0.1x steps from 0.1x to 20x. Integer values are rounded cleanly; 1x restores normal time and audio pitch.")))
                        .AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
                    [
                        BuildSpeedRiskNotice()
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SVerticalBox)
                    .Visibility_Lambda([this]() { return bCompactHeight ? EVisibility::Visible : EVisibility::Collapsed; })
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)[SNew(STMSpeedDial)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
                    [
                        BuildSpeedRiskNotice()
                    ]
                ];
        }

        TSharedRef<SWidget> BuildSpeedRuler() const
        {
            TSharedRef<SHorizontalBox> Ruler = SNew(SHorizontalBox);
            for (int32 Speed = 1; Speed <= 20; ++Speed)
            {
                Ruler->AddSlot()
                    .FillWidth(1.0f)
                    .HAlign(HAlign_Center)
                    [
                        BuildRulerMark(FString::FromInt(Speed))
                    ];
            }

            return SNew(SScaleBox)
                .Stretch(EStretch::ScaleToFitX)
                .StretchDirection(EStretchDirection::DownOnly)
                .HAlign(HAlign_Fill)
                [
                    SNew(SBox)
                    .MinDesiredWidth(560.0f)
                    [
                        Ruler
                    ]
                ];
        }

        TSharedRef<SWidget> BuildRulerMark(const FString& Label) const
        {
            return SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(TMLoc::Text(TEXT("|"), TEXT("|")))
                    .ColorAndOpacity(FLinearColor(0.55f, 0.60f, 0.68f, 1.0f))
                ]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Label))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                ];
        }

        void RefreshBlueprintClassOptions()
        {
            BlueprintClassOptions.Reset();
            BlueprintClassByOption.Reset();
            BlueprintAssetPathByOption.Reset();
            TSet<FString> AddedOptions;

            auto AddClassOption = [&AddedOptions, this](UClass* Class)
            {
                if (!Class || Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
                {
                    return;
                }

                const FString Option = FString::Printf(TEXT("%s  (%s)"), *Class->GetName(), *Class->GetPathName());
                if (!AddedOptions.Contains(Option))
                {
                    AddedOptions.Add(Option);
                    BlueprintClassOptions.Add(MakeShared<FString>(Option));
                    BlueprintClassByOption.Add(Option, Class);
                }
            };

            for (TObjectIterator<UClass> It; It; ++It)
            {
                AddClassOption(*It);
            }

            FARFilter Filter;
            Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
            Filter.bRecursiveClasses = true;
            TArray<FAssetData> BlueprintAssets;
            FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
            AssetRegistry.Get().GetAssets(Filter, BlueprintAssets);
            for (const FAssetData& Asset : BlueprintAssets)
            {
                const FString Option = FString::Printf(TEXT("%s  (%s)"), *Asset.AssetName.ToString(), *Asset.PackageName.ToString());
                if (!AddedOptions.Contains(Option))
                {
                    AddedOptions.Add(Option);
                    BlueprintClassOptions.Add(MakeShared<FString>(Option));
                    BlueprintAssetPathByOption.Add(Option, Asset.GetSoftObjectPath());
                }
            }

            BlueprintClassOptions.Sort([](const TSharedPtr<FString>& Left, const TSharedPtr<FString>& Right)
            {
                return Left.IsValid() && Right.IsValid() ? *Left < *Right : Left.IsValid();
            });
        }

        void RefreshBlueprintFunctionOptions()
        {
            BlueprintFunctionOptions.Reset();
            if (UClass* TargetClass = GGlobalSpeedController.GetBlueprintTargetClass())
            {
                TSet<FName> AddedFunctions;
                for (TFieldIterator<UFunction> It(TargetClass, EFieldIteratorFlags::IncludeSuper); It; ++It)
                {
                    UFunction* Function = *It;
                    if (!Function || Function->HasAnyFunctionFlags(FUNC_Delegate | FUNC_MulticastDelegate))
                    {
                        continue;
                    }

                    const FName FunctionName = Function->GetFName();
                    if (!AddedFunctions.Contains(FunctionName))
                    {
                        AddedFunctions.Add(FunctionName);
                        BlueprintFunctionOptions.Add(MakeShared<FString>(FunctionName.ToString()));
                    }
                }
            }

            BlueprintFunctionOptions.Sort([](const TSharedPtr<FString>& Left, const TSharedPtr<FString>& Right)
            {
                return Left.IsValid() && Right.IsValid() ? *Left < *Right : Left.IsValid();
            });
        }

        void OnBlueprintClassSelected(TSharedPtr<FString> Selection, ESelectInfo::Type)
        {
            if (!Selection.IsValid())
            {
                return;
            }

            UClass* TargetClass = BlueprintClassByOption.FindRef(*Selection).Get();
            if (!TargetClass)
            {
                if (const FSoftObjectPath* AssetPath = BlueprintAssetPathByOption.Find(*Selection))
                {
                    if (UBlueprint* Blueprint = Cast<UBlueprint>(AssetPath->TryLoad()))
                    {
                        TargetClass = Blueprint->GeneratedClass;
                    }
                }
            }

            GGlobalSpeedController.SetBlueprintTarget(TargetClass, NAME_None);
            RefreshBlueprintFunctionOptions();
            if (BlueprintFunctionCombo.IsValid())
            {
                BlueprintFunctionCombo->ClearSelection();
                BlueprintFunctionCombo->RefreshOptions();
            }
        }

        void OnBlueprintFunctionSelected(TSharedPtr<FString> Selection, ESelectInfo::Type)
        {
            if (Selection.IsValid())
            {
                GGlobalSpeedController.SetBlueprintTarget(GGlobalSpeedController.GetBlueprintTargetClass(), FName(**Selection));
            }
        }

        FText GetBlueprintClassLabel() const
        {
            return GGlobalSpeedController.GetBlueprintTargetClass()
                ? FText::FromString(GGlobalSpeedController.GetBlueprintTargetClass()->GetName())
                : TMLoc::Text(TEXT("Search and select Blueprint class..."), TEXT("Search and select Blueprint class..."));
        }

        FText GetBlueprintFunctionLabel() const
        {
            const FName Function = GGlobalSpeedController.GetBlueprintTargetFunction();
            return Function.IsNone()
                ? TMLoc::Text(TEXT("Select function..."), TEXT("Select function..."))
                : FText::FromName(Function);
        }

        TSharedRef<SWidget> BuildBlueprintTargetSelector()
        {
            RefreshBlueprintClassOptions();
            RefreshBlueprintFunctionOptions();
            return SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 3)
                [
                    SNew(STextBlock).Text(TMLoc::Text(TEXT("Blueprint class"), TEXT("Blueprint class"))).ColorAndOpacity(FSlateColor::UseSubduedForeground())
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
                [
                    SAssignNew(BlueprintClassCombo, SSearchableComboBox)
                    .OptionsSource(&BlueprintClassOptions)
                    .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
                    {
                        return SNew(STextBlock).Text(Item.IsValid() ? FText::FromString(*Item) : FText::GetEmpty());
                    })
                    .OnSelectionChanged(this, &STMGlobalSpeedControlWidget::OnBlueprintClassSelected)
                    [SNew(STextBlock).Text(this, &STMGlobalSpeedControlWidget::GetBlueprintClassLabel)]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 3)
                [
                    SNew(STextBlock).Text(TMLoc::Text(TEXT("Function"), TEXT("Function"))).ColorAndOpacity(FSlateColor::UseSubduedForeground())
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SAssignNew(BlueprintFunctionCombo, SSearchableComboBox)
                    .OptionsSource(&BlueprintFunctionOptions)
                    .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
                    {
                        return SNew(STextBlock).Text(Item.IsValid() ? FText::FromString(*Item) : FText::GetEmpty());
                    })
                    .OnSelectionChanged(this, &STMGlobalSpeedControlWidget::OnBlueprintFunctionSelected)
                    [SNew(STextBlock).Text(this, &STMGlobalSpeedControlWidget::GetBlueprintFunctionLabel)]
                ];
        }

        TSharedRef<SWidget> BuildSkipToTargetPanel()
        {
            return SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
                .Padding(10.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            SNew(SCheckBox)
                            .IsChecked_Lambda([]()
                            {
                                return GGlobalSpeedController.IsSkipToTargetEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                            })
                            .OnCheckStateChanged_Lambda([](ECheckBoxState State)
                            {
                                GGlobalSpeedController.SetSkipToTargetEnabled(State == ECheckBoxState::Checked);
                            })
                        ]
                        + SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Text(TMLoc::Text(TEXT("Arm Skip to Target"), TEXT("Arm Skip to Target")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                    [
                        SNew(STextBlock)
                        .Text(TMLoc::Text(TEXT("Choose a target and the action to run when it is reached, then arm it."), TEXT("Choose a target and the action to run when it is reached, then arm it.")))
                        .AutoWrapText(true)
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 14, 0)
                        [
                            SNew(SCheckBox)
                            .Type(ESlateCheckBoxType::ToggleButton)
                            .IsChecked_Lambda([]()
                            {
                                return GGlobalSpeedController.GetSkipTargetType() == ETMSkipTargetType::LogText ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                            })
                            .OnCheckStateChanged_Lambda([](ECheckBoxState State)
                            {
                                if (State == ECheckBoxState::Checked)
                                {
                                    GGlobalSpeedController.SetSkipTargetType(ETMSkipTargetType::LogText);
                                }
                            })
                            [SNew(STextBlock).Text(TMLoc::Text(TEXT("Output Log text"), TEXT("Output Log text")))]
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SCheckBox)
                            .Type(ESlateCheckBoxType::ToggleButton)
                            .IsChecked_Lambda([]()
                            {
                                return GGlobalSpeedController.GetSkipTargetType() == ETMSkipTargetType::BlueprintClass ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                            })
                            .OnCheckStateChanged_Lambda([](ECheckBoxState State)
                            {
                                if (State == ECheckBoxState::Checked)
                                {
                                    GGlobalSpeedController.SetSkipTargetType(ETMSkipTargetType::BlueprintClass);
                                }
                            })
                            [SNew(STextBlock).Text(TMLoc::Text(TEXT("Blueprint class event"), TEXT("Blueprint class event")))]
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                    [
                        SNew(SBox)
                        .Visibility_Lambda([]() { return GGlobalSpeedController.GetSkipTargetType() == ETMSkipTargetType::LogText ? EVisibility::Visible : EVisibility::Collapsed; })
                        [
                            SNew(SEditableTextBox)
                            .HintText(TMLoc::Text(TEXT("e.g. BossPhase2 or a log category"), TEXT("e.g. BossPhase2 or a log category")))
                            .Text_Lambda([]() { return FText::FromString(GGlobalSpeedController.GetLogNeedle()); })
                            .OnTextCommitted_Lambda([](const FText& Text, ETextCommit::Type)
                            {
                                GGlobalSpeedController.SetLogNeedle(Text.ToString());
                            })
                            .OnTextChanged_Lambda([](const FText& Text)
                            {
                                GGlobalSpeedController.SetLogNeedle(Text.ToString());
                            })
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
                    [
                        SNew(SBox)
                        .Visibility_Lambda([]() { return GGlobalSpeedController.GetSkipTargetType() == ETMSkipTargetType::BlueprintClass ? EVisibility::Visible : EVisibility::Collapsed; })
                        [BuildBlueprintTargetSelector()]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
                        [
                            SNew(SCheckBox)
                            .Type(ESlateCheckBoxType::ToggleButton)
                            .IsChecked_Lambda([]()
                            {
                                return GGlobalSpeedController.GetSkipTargetAction() == ETMSkipTargetAction::ResetToNormal ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                            })
                            .OnCheckStateChanged_Lambda([](ECheckBoxState State)
                            {
                                if (State == ECheckBoxState::Checked)
                                {
                                    GGlobalSpeedController.SetSkipTargetAction(ETMSkipTargetAction::ResetToNormal);
                                }
                            })
                            [SNew(STextBlock).Text(TMLoc::Text(TEXT("Restore 1x"), TEXT("Restore 1x")))]
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SCheckBox)
                            .Type(ESlateCheckBoxType::ToggleButton)
                            .IsChecked_Lambda([]()
                            {
                                return GGlobalSpeedController.GetSkipTargetAction() == ETMSkipTargetAction::PauseGame ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                            })
                            .OnCheckStateChanged_Lambda([](ECheckBoxState State)
                            {
                                if (State == ECheckBoxState::Checked)
                                {
                                    GGlobalSpeedController.SetSkipTargetAction(ETMSkipTargetAction::PauseGame);
                                }
                            })
                            [SNew(STextBlock).Text(TMLoc::Text(TEXT("Pause PIE/Game (restore 1x)"), TEXT("Pause PIE/Game (restore 1x)")))]
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
                    [
                        SNew(STextBlock)
                        .Text_Lambda([]()
                        {
                            return GGlobalSpeedController.GetSkipTargetType() == ETMSkipTargetType::LogText
                                ? TMLoc::Text(TEXT("Matches Output Log text or category, case-insensitively."), TEXT("Matches Output Log text or category, case-insensitively."))
                                : TMLoc::Text(TEXT("Matches the selected Blueprint class (including children) and exact function name."), TEXT("Matches the selected Blueprint class (including children) and exact function name."));
                        })
                        .AutoWrapText(true)
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(this, &STMGlobalSpeedControlWidget::GetSkipTargetStatusText)
                        .AutoWrapText(true)
                        .ColorAndOpacity(FLinearColor(0.70f, 0.82f, 1.0f, 1.0f))
                    ]
                ];
        }

        FText GetSkipTargetStatusText() const
        {
            const FString TargetState = GGlobalSpeedController.GetSkipTargetSummary();
            const FString LastTrigger = GGlobalSpeedController.GetLastTriggerText();
            return FText::FromString(FString::Printf(TEXT("%s\nLast result: %s"), *TargetState, *LastTrigger));
        }

        FText GetSpeedLabelText() const
        {
            return FText::FromString(FString::Printf(TEXT("%s / 20x"), *GGlobalSpeedController.GetSpeedDisplayText()));
        }

        FText GetStatusText() const
        {
            return FText::FromString(GGlobalSpeedController.BuildStatusText());
        }

        TArray<TSharedPtr<FString>> BlueprintClassOptions;
        TArray<TSharedPtr<FString>> BlueprintFunctionOptions;
        TMap<FString, TWeakObjectPtr<UClass>> BlueprintClassByOption;
        TMap<FString, FSoftObjectPath> BlueprintAssetPathByOption;
        TSharedPtr<SSearchableComboBox> BlueprintClassCombo;
        TSharedPtr<SSearchableComboBox> BlueprintFunctionCombo;
        bool bCompactHeight = false;
    };

    TSharedRef<SDockTab> SpawnGlobalSpeedControlTab(const FSpawnTabArgs&)
    {
        return SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            [
                SNew(STMGlobalSpeedControlWidget)
            ];
    }

    void RegisterGlobalSpeedControlTab()
    {
        if (bGlobalSpeedControlTabRegistered)
        {
            return;
        }

        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(GlobalSpeedControlTabId, FOnSpawnTab::CreateStatic(&SpawnGlobalSpeedControlTab))
            .SetDisplayName(TMLoc::Text(TEXT("Global Speed Control"), TEXT("Global Speed Control")))
            .SetTooltipText(TMLoc::Text(TEXT("Control gameplay time dilation and audio playback speed together."), TEXT("Control gameplay time dilation and audio playback speed together.")))
            .SetIcon(FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.GlobalSpeedControl")));
        bGlobalSpeedControlTabRegistered = true;
    }
}

namespace TMGlobalSpeedControl
{
    void RegisterMenus()
    {
        RegisterGlobalSpeedControlTab();

        auto AddEntry = [](UToolMenu* Menu, const FName EntryName)
        {
            if (!Menu)
            {
                return;
            }

            FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("TraceMotive"));
            Section.AddMenuEntry(
                EntryName,
                TMLoc::Text(TEXT("Global Speed Control"), TEXT("Global Speed Control")),
                TMLoc::Text(TEXT("Open a window that speeds up gameplay time dilation and audio playback together."), TEXT("Open a window that speeds up gameplay time dilation and audio playback together.")),
                FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.GlobalSpeedControl")),
                FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&) { TMGlobalSpeedControl::OpenWindow(); }));
        };

        AddEntry(UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window")), TEXT("TMOpenGlobalSpeedControl"));
        AddEntry(UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools")), TEXT("TMToolsOpenGlobalSpeedControl"));
    }

    void UnregisterMenus()
    {
        GGlobalSpeedController.Stop();
        if (bGlobalSpeedControlTabRegistered)
        {
            FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GlobalSpeedControlTabId);
            bGlobalSpeedControlTabRegistered = false;
        }
    }

    void OpenWindow()
    {
        RegisterGlobalSpeedControlTab();
        FGlobalTabmanager::Get()->TryInvokeTab(GlobalSpeedControlTabId);
    }
}
