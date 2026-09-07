#include "TMPackageProgress.h"
#include "TMEngineCompatibility.h"
#include "TMStyle.h"



#include "TMLocalization.h"

#include "TMReportFormatter.h"



#include "Async/Async.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "HAL/PlatformProcess.h"
#include "IContentBrowserSingleton.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PlatformInfo.h"
#include "Subsystems/AssetEditorSubsystem.h"

#include "Framework/Docking/TabManager.h"

#include "HAL/PlatformApplicationMisc.h"

#include "HAL/PlatformTime.h"

#include "Internationalization/Regex.h"

#include "CoreGlobals.h"

#include "Misc/OutputDevice.h"

#include "Misc/ScopeLock.h"

#include "Styling/AppStyle.h"

#include "ToolMenus.h"
#include "UObject/SoftObjectPath.h"

#include "Widgets/Docking/SDockTab.h"

#include "Widgets/Images/SThrobber.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"

#include "Widgets/Input/SButton.h"

#include "Widgets/Layout/SBorder.h"

#include "Widgets/Layout/SBox.h"

#include "Widgets/Layout/SScrollBox.h"

#include "Widgets/Layout/SSeparator.h"

#include "Widgets/Notifications/SProgressBar.h"

#include "Widgets/SBoxPanel.h"

#include "Widgets/SCompoundWidget.h"

#include "Widgets/Text/STextBlock.h"



namespace

{

    const FName PackageProgressTabId(TEXT("TraceMotive.PackageProgress"));



    enum class EPackageStage : uint8

    {

        Idle,

        Prepare,

        Build,

        Cook,

        Stage,

        Package,

        Archive,

        Complete,

        Canceled,

        Failed

    };



    struct FPackageProgressSnapshot

    {

        EPackageStage Stage = EPackageStage::Idle;

        EPackageStage ActiveStage = EPackageStage::Idle;

        bool bRunning = false;

        bool bSucceeded = false;

        bool bCanceled = false;

        float Progress = 0.0f;

        float StageProgress = 0.0f;

        double ElapsedSeconds = 0.0;

        double EstimatedRemainingSeconds = -1.0;

        FString Platform = TEXT("-");

        FString Configuration = TEXT("-");

        FString CurrentWork = TEXT("Waiting for a packaging task.");

        FString RecentLog;

        FString FailureReason;

    };



    float GetStageStart(EPackageStage Stage)

    {

        switch (Stage)

        {

        case EPackageStage::Prepare: return 0.01f;

        case EPackageStage::Build: return 0.05f;

        case EPackageStage::Cook: return 0.25f;

        case EPackageStage::Stage: return 0.70f;

        case EPackageStage::Package: return 0.80f;

        case EPackageStage::Archive: return 0.95f;

        case EPackageStage::Complete: return 1.0f;

        case EPackageStage::Canceled: return 0.0f;

        case EPackageStage::Failed: return 0.0f;

        default: return 0.0f;

        }

    }



    float GetStageEnd(EPackageStage Stage)

    {

        switch (Stage)

        {

        case EPackageStage::Prepare: return 0.05f;

        case EPackageStage::Build: return 0.25f;

        case EPackageStage::Cook: return 0.70f;

        case EPackageStage::Stage: return 0.80f;

        case EPackageStage::Package: return 0.95f;

        case EPackageStage::Archive: return 0.995f;

        case EPackageStage::Complete: return 1.0f;

        case EPackageStage::Canceled: return 0.0f;

        default: return 0.0f;

        }

    }



    float GetRunningStageCap(EPackageStage Stage)

    {

        if (Stage == EPackageStage::Complete)

        {

            return 1.0f;

        }

        return FMath::Max(GetStageStart(Stage), GetStageEnd(Stage) - 0.001f);

    }



    double GetStageExpectedSeconds(EPackageStage Stage)

    {

        switch (Stage)

        {

        case EPackageStage::Prepare: return 20.0;

        case EPackageStage::Build: return 180.0;

        case EPackageStage::Cook: return 600.0;

        case EPackageStage::Stage: return 90.0;

        case EPackageStage::Package: return 240.0;

        case EPackageStage::Archive: return 60.0;

        default: return 1.0;

        }

    }



    double GetExpectedPackageSeconds()

    {

        return GetStageExpectedSeconds(EPackageStage::Prepare)

            + GetStageExpectedSeconds(EPackageStage::Build)

            + GetStageExpectedSeconds(EPackageStage::Cook)

            + GetStageExpectedSeconds(EPackageStage::Stage)

            + GetStageExpectedSeconds(EPackageStage::Package)

            + GetStageExpectedSeconds(EPackageStage::Archive);

    }



    double GetExpectedRemainingSeconds(EPackageStage Stage, float StageProgress)

    {

        const double RemainingStageSeconds = GetStageExpectedSeconds(Stage) * (1.0 - FMath::Clamp(StageProgress, 0.0f, 1.0f));

        switch (Stage)

        {

        case EPackageStage::Prepare:

            return RemainingStageSeconds + GetStageExpectedSeconds(EPackageStage::Build) + GetStageExpectedSeconds(EPackageStage::Cook)

                + GetStageExpectedSeconds(EPackageStage::Stage) + GetStageExpectedSeconds(EPackageStage::Package) + GetStageExpectedSeconds(EPackageStage::Archive);

        case EPackageStage::Build:

            return RemainingStageSeconds + GetStageExpectedSeconds(EPackageStage::Cook) + GetStageExpectedSeconds(EPackageStage::Stage)

                + GetStageExpectedSeconds(EPackageStage::Package) + GetStageExpectedSeconds(EPackageStage::Archive);

        case EPackageStage::Cook:

            return RemainingStageSeconds + GetStageExpectedSeconds(EPackageStage::Stage) + GetStageExpectedSeconds(EPackageStage::Package)

                + GetStageExpectedSeconds(EPackageStage::Archive);

        case EPackageStage::Stage:

            return RemainingStageSeconds + GetStageExpectedSeconds(EPackageStage::Package) + GetStageExpectedSeconds(EPackageStage::Archive);

        case EPackageStage::Package:

            return RemainingStageSeconds + GetStageExpectedSeconds(EPackageStage::Archive);

        case EPackageStage::Archive:

            return RemainingStageSeconds;

        default:

            return 0.0;

        }

    }



    FString GetStageEnglish(EPackageStage Stage)

    {

        switch (Stage)

        {

        case EPackageStage::Prepare: return TEXT("Preparing");

        case EPackageStage::Build: return TEXT("Building code");

        case EPackageStage::Cook: return TEXT("Cooking assets");

        case EPackageStage::Stage: return TEXT("Staging files");

        case EPackageStage::Package: return TEXT("Pak / IoStore");

        case EPackageStage::Archive: return TEXT("Archiving");

        case EPackageStage::Complete: return TEXT("Complete");

        case EPackageStage::Canceled: return TEXT("Canceled");

        case EPackageStage::Failed: return TEXT("Failed");

        default: return TEXT("Waiting");

        }

    }



    FString GetStageKorean(EPackageStage Stage)

    {

        return GetStageEnglish(Stage);

    }

    FString FormatDuration(double Seconds)

    {

        if (Seconds < 0.0)

        {

            return TEXT("-");

        }

        const int32 Total = FMath::Max(0, FMath::RoundToInt(Seconds));

        const int32 Hours = Total / 3600;

        const int32 Minutes = (Total % 3600) / 60;

        const int32 Remainder = Total % 60;

        return Hours > 0

            ? FString::Printf(TEXT("%d:%02d:%02d"), Hours, Minutes, Remainder)

            : FString::Printf(TEXT("%02d:%02d"), Minutes, Remainder);

    }

    struct FPackageFailureFinding
    {
        FString Summary;
        FString Detail;
        FString TargetLabel;
        FString AssetPath;
        FString FilePath;
        int32 LineNumber = 0;
    };

    FString TrimFailureLine(FString Line)
    {
        Line.TrimStartAndEndInline();
        Line.RemoveFromStart(TEXT("UATHelper: Packaging: "));
        Line.RemoveFromStart(TEXT("PackagingResults: Error: "));
        Line.RemoveFromStart(TEXT("LogInit: Error: "));
        return Line.Left(900);
    }

    bool IsGenericFailureLine(const FString& Line)
    {
        return Line.Contains(TEXT("AutomationTool exiting"), ESearchCase::IgnoreCase)
            || Line.Contains(TEXT("BUILD FAILED"), ESearchCase::IgnoreCase)
            || Line.Contains(TEXT("Packaging failed"), ESearchCase::IgnoreCase)
            || Line.Contains(TEXT("ExitCode="), ESearchCase::IgnoreCase);
    }


    bool IsPackageErrorLogLine(const FString& Line)
    {
        return Line.Contains(TEXT("error"), ESearchCase::IgnoreCase)
            || Line.Contains(TEXT("fatal"), ESearchCase::IgnoreCase)
            || Line.Contains(TEXT("failed"), ESearchCase::IgnoreCase)
            || Line.Contains(TEXT("exception"), ESearchCase::IgnoreCase)
            || Line.Contains(TEXT("ensure"), ESearchCase::IgnoreCase)
            || Line.Contains(TEXT("unknown cook failure"), ESearchCase::IgnoreCase);
    }

    FString ExtractPackageErrorLogLines(const FString& LogText)
    {
        TArray<FString> Lines;
        LogText.ParseIntoArrayLines(Lines, false);

        TArray<FString> ErrorLines;
        for (const FString& Line : Lines)
        {
            if (IsPackageErrorLogLine(Line))
            {
                ErrorLines.Add(Line);
            }
        }
        return FString::Join(ErrorLines, TEXT("\n"));
    }

    FString ExtractAssetPathFromLine(const FString& Line)
    {
        static const FRegexPattern AssetPattern(TEXT("(/Game/[A-Za-z0-9_./-]+)"));
        FRegexMatcher Matcher(AssetPattern, Line);
        if (Matcher.FindNext())
        {
            FString Path = Matcher.GetCaptureGroup(1);
            while (Path.EndsWith(TEXT(".")) || Path.EndsWith(TEXT(",")) || Path.EndsWith(TEXT(":")) || Path.EndsWith(TEXT(")")) || Path.EndsWith(TEXT("]")))
            {
                Path.LeftChopInline(1);
            }
            return Path;
        }
        return FString();
    }

    bool ExtractFileLineFromLine(const FString& Line, FString& OutFilePath, int32& OutLineNumber)
    {
        static const FRegexPattern FileLinePattern(TEXT("([A-Za-z]:[^\r\n:]+?\\.(?:cpp|h|hpp|cs|ini|uproject|uplugin|usf|ush))\\((\\d+)\\)"));
        FRegexMatcher Matcher(FileLinePattern, Line);
        if (Matcher.FindNext())
        {
            OutFilePath = Matcher.GetCaptureGroup(1);
            OutLineNumber = FCString::Atoi(*Matcher.GetCaptureGroup(2));
            return true;
        }
        return false;
    }

    FString ExtractLikelyClassOrAssetName(const FString& Line)
    {
        static const FRegexPattern NamePattern(TEXT("\\b((?:BP|WBP|ABP|GA|GC|DA|DT|T|SK|SM|MI|M|BPI|E|ST)_[A-Za-z0-9_]+|[A-Za-z_][A-Za-z0-9_]*_C)\\b"));
        FRegexMatcher Matcher(NamePattern, Line);
        if (Matcher.FindNext())
        {
            return Matcher.GetCaptureGroup(1);
        }
        return FString();
    }

    bool ResolveAssetData(const FString& AssetPathOrName, FAssetData& OutAssetData)
    {
        if (AssetPathOrName.IsEmpty())
        {
            return false;
        }

        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

        FString Path = AssetPathOrName;
        if (Path.StartsWith(TEXT("/Game/")))
        {
            if (Path.Contains(TEXT(".")))
            {
                OutAssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(Path));
                if (OutAssetData.IsValid())
                {
                    return true;
                }
            }

            FString PackagePath = Path;
            FString ObjectSuffix;
            if (PackagePath.Split(TEXT("."), &PackagePath, &ObjectSuffix))
            {
            }

            TArray<FAssetData> Assets;
            AssetRegistry.GetAssetsByPackageName(FName(*PackagePath), Assets);
            if (Assets.Num() > 0)
            {
                OutAssetData = Assets[0];
                return true;
            }

            const FString ShortName = FPackageName::GetShortName(PackagePath);
            const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackagePath, *ShortName);
            OutAssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
            return OutAssetData.IsValid();
        }

        FARFilter Filter;
        Filter.bRecursivePaths = true;
        Filter.PackagePaths.Add(TEXT("/Game"));
        TArray<FAssetData> Assets;
        AssetRegistry.GetAssets(Filter, Assets);
        for (const FAssetData& Asset : Assets)
        {
            if (Asset.AssetName.ToString().Equals(AssetPathOrName, ESearchCase::IgnoreCase)
                || Asset.AssetClassPath.GetAssetName().ToString().Equals(AssetPathOrName, ESearchCase::IgnoreCase))
            {
                OutAssetData = Asset;
                return true;
            }
        }
        return false;
    }

    FReply OpenPackageFailureTarget(const FPackageFailureFinding Finding)
    {
        if (!Finding.FilePath.IsEmpty())
        {
            FString Path = Finding.FilePath;
            FPaths::NormalizeFilename(Path);
            if (FPaths::FileExists(Path))
            {
                FPlatformProcess::LaunchFileInDefaultExternalApplication(*Path);
                return FReply::Handled();
            }
        }

        FAssetData AssetData;
        if (ResolveAssetData(!Finding.AssetPath.IsEmpty() ? Finding.AssetPath : Finding.TargetLabel, AssetData))
        {
            TArray<FAssetData> AssetsToSync;
            AssetsToSync.Add(AssetData);
            FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
            ContentBrowserModule.Get().SyncBrowserToAssets(AssetsToSync, false, true);

            if (UObject* AssetObject = AssetData.GetAsset())
            {
                if (GEditor)
                {
                    if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
                    {
                        AssetEditorSubsystem->OpenEditorForAsset(AssetObject);
                    }
                }
            }
            return FReply::Handled();
        }
        return FReply::Unhandled();
    }

    TArray<FPackageFailureFinding> AnalyzePackageFailure(const FPackageProgressSnapshot& State)
    {
        TArray<FPackageFailureFinding> Findings;
        TSet<FString> Seen;
        TArray<FString> Lines;
        (State.RecentLog + TEXT("\n") + State.FailureReason).ParseIntoArrayLines(Lines, false);

        for (int32 Index = Lines.Num() - 1; Index >= 0 && Findings.Num() < 8; --Index)
        {
            const FString RawLine = Lines[Index];
            const bool bLooksLikeError = RawLine.Contains(TEXT("error"), ESearchCase::IgnoreCase)
                || RawLine.Contains(TEXT("fatal"), ESearchCase::IgnoreCase)
                || RawLine.Contains(TEXT("failed"), ESearchCase::IgnoreCase)
                || RawLine.Contains(TEXT("ensure"), ESearchCase::IgnoreCase)
                || RawLine.Contains(TEXT("unknown cook failure"), ESearchCase::IgnoreCase);
            if (!bLooksLikeError || IsGenericFailureLine(RawLine))
            {
                continue;
            }

            FPackageFailureFinding Finding;
            Finding.Detail = TrimFailureLine(RawLine);
            FString FilePath;
            int32 LineNumber = 0;
            if (ExtractFileLineFromLine(RawLine, FilePath, LineNumber))
            {
                Finding.FilePath = FilePath;
                Finding.LineNumber = LineNumber;
                Finding.TargetLabel = FString::Printf(TEXT("%s:%d"), *FPaths::GetCleanFilename(FilePath), LineNumber);
                Finding.Summary = TMLoc::String(TEXT("Source compile error"), TEXT("Source compile error"));
            }
            else
            {
                Finding.AssetPath = ExtractAssetPathFromLine(RawLine);
                Finding.TargetLabel = !Finding.AssetPath.IsEmpty() ? Finding.AssetPath : ExtractLikelyClassOrAssetName(RawLine);
                Finding.Summary = !Finding.TargetLabel.IsEmpty()
                    ? TMLoc::String(TEXT("Asset or Blueprint related failure"), TEXT("Asset or Blueprint related failure"))
                    : TMLoc::String(TEXT("Packaging error log"), TEXT("Packaging error log"));
            }

            const FString Key = Finding.TargetLabel + TEXT("|") + Finding.Detail;
            if (Seen.Contains(Key))
            {
                continue;
            }
            Seen.Add(Key);
            Findings.Add(Finding);
        }

        if (Findings.IsEmpty() && !State.FailureReason.IsEmpty())
        {
            FPackageFailureFinding Fallback;
            Fallback.Summary = TMLoc::String(TEXT("Packaging failed"), TEXT("Packaging failed"));
            Fallback.Detail = TrimFailureLine(State.FailureReason);
            Findings.Add(Fallback);
        }
        return Findings;
    }



    class FPackageProgressMonitor final : public FOutputDevice

    {

    public:

        void Start()

        {

            if (!bRegistered && GLog)

            {

                GLog->AddOutputDevice(this);

                bRegistered = true;

            }

        }



        void Stop()

        {

            if (bRegistered && GLog)

            {

                GLog->RemoveOutputDevice(this);

            }

            bRegistered = false;

        }



        virtual void Serialize(const TCHAR* V, ELogVerbosity::Type /*Verbosity*/, const FName& Category) override

        {

            if (!V)

            {

                return;

            }



            const FString Message(V);

            const FString CategoryText = Category.ToString();

            const bool bStartMarker = Message.Contains(TEXT("Packaging ("), ESearchCase::IgnoreCase)

                || (Message.Contains(TEXT("BuildCookRun"), ESearchCase::IgnoreCase)

                    && Message.Contains(TEXT("-package"), ESearchCase::IgnoreCase));



            FScopeLock Lock(&Mutex);

            if (bStartMarker && !bRunning)

            {

                BeginRun(Message);

            }



            if (!bRunning)

            {

                return;

            }



            const bool bRelevantCategory = CategoryText.Contains(TEXT("UAT"), ESearchCase::IgnoreCase)

                || CategoryText.Contains(TEXT("Cook"), ESearchCase::IgnoreCase)

                || CategoryText.Contains(TEXT("Automation"), ESearchCase::IgnoreCase)

                || CategoryText.Contains(TEXT("Packaging"), ESearchCase::IgnoreCase);

            const bool bRelevantLine = bRelevantCategory

                || Message.Contains(TEXT("COMMAND"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("AutomationTool exiting"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("BUILD SUCCESSFUL"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("BUILD FAILED"), ESearchCase::IgnoreCase)

                || (bRelevantCategory && IsCancelMessage(Message));

            if (bRelevantLine)

            {

                AddRecentLine(Message);

                UpdatePlatformFromMessage(Message);

                UpdateStageFromMessage(Message);

                UpdateConfigurationFromMessage(Message);

                UpdateCountProgress(Message);

            }



            if (bRelevantCategory && IsCancelMessage(Message))

            {

                FinishCanceled(Message);

            }

            // UBT can emit "BUILD SUCCESSFUL" while UAT still has cook, stage, package, or archive work left.
            // Only AutomationTool's zero exit code completes the overall packaging run.
            else if (Message.Contains(TEXT("AutomationTool exiting with ExitCode=0"), ESearchCase::IgnoreCase))

            {

                FinishRun(true, TEXT("Packaging completed successfully."));

            }

            else if (Message.Contains(TEXT("BUILD FAILED"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("AutomationTool exiting with ExitCode="), ESearchCase::IgnoreCase))

            {

                FinishRun(false, Message);

            }

        }



        FPackageProgressSnapshot GetSnapshot() const

        {

            FScopeLock Lock(&Mutex);

            FPackageProgressSnapshot Result;

            Result.Stage = Stage;

            Result.ActiveStage = (Stage == EPackageStage::Failed || Stage == EPackageStage::Canceled) ? LastActiveStage : Stage;

            Result.bRunning = bRunning;

            Result.bSucceeded = bSucceeded;

            Result.bCanceled = bCanceled;

            Result.Platform = Platform;

            Result.Configuration = Configuration;

            Result.CurrentWork = CurrentWork;

            Result.FailureReason = FailureReason;

            Result.ElapsedSeconds = bRunning

                ? FMath::Max(0.0, FPlatformTime::Seconds() - StartSeconds)

                : FinalElapsedSeconds;



            float TargetOverallProgress = Progress;

            float TargetStageProgress = 0.0f;

            if (bRunning)

            {

                const float StageStart = GetStageStart(Stage);

                const float StageEnd = GetStageEnd(Stage);

                const float StageCap = GetRunningStageCap(Stage);

                const double PhaseElapsed = FPlatformTime::Seconds() - StageStartSeconds;

                const float TimeFraction = FMath::Clamp(static_cast<float>(PhaseElapsed / GetStageExpectedSeconds(Stage)), 0.0f, 0.88f);

                TargetOverallProgress = FMath::Clamp(TargetOverallProgress, StageStart, StageCap);

                TargetOverallProgress = FMath::Max(TargetOverallProgress, FMath::Lerp(StageStart, StageEnd, TimeFraction));

                TargetOverallProgress = FMath::Min(TargetOverallProgress, StageCap);

                const float StageRange = FMath::Max(0.001f, StageEnd - StageStart);

                TargetStageProgress = FMath::Clamp((TargetOverallProgress - StageStart) / StageRange, 0.0f, 1.0f);

            }

            TargetOverallProgress = FMath::Clamp(TargetOverallProgress, 0.0f, 1.0f);



            const double NowSeconds = FPlatformTime::Seconds();

            if (Stage == EPackageStage::Complete)

            {

                DisplayedOverallProgress = 1.0f;

                TargetStageProgress = 1.0f;

            }

            else if (bRunning)

            {

                if (LastDisplayedProgressUpdateSeconds <= 0.0)

                {

                    LastDisplayedProgressUpdateSeconds = NowSeconds;

                }

                const double DeltaSeconds = FMath::Max(0.0, NowSeconds - LastDisplayedProgressUpdateSeconds);

                constexpr float RunningProgressCap = 0.995f;

                const double SafeEstimatedTotalSeconds = FMath::Max(1.0, EstimatedTotalSeconds);

                const float LinearStep = static_cast<float>(DeltaSeconds / SafeEstimatedTotalSeconds) * RunningProgressCap;
                const float LinearProgress = DisplayedOverallProgress + FMath::Max(0.0f, LinearStep);

                // Packaging's final container build is often much faster than its initial estimate.
                // Once it is confirmed, do not leave the total bar behind the weighted Package / Archive stage.
                const bool bUseStageProgressFloor = Stage == EPackageStage::Package || Stage == EPackageStage::Archive;
                const float StageProgressFloor = bUseStageProgressFloor
                    ? FMath::Min(TargetOverallProgress, RunningProgressCap)
                    : 0.0f;

                DisplayedOverallProgress = FMath::Clamp(

                    FMath::Max(LinearProgress, StageProgressFloor),

                    DisplayedOverallProgress,

                    RunningProgressCap);

                LastDisplayedProgressUpdateSeconds = NowSeconds;

            }

            else

            {

                DisplayedOverallProgress = FMath::Max(DisplayedOverallProgress, TargetOverallProgress);

            }



            Result.Progress = FMath::Clamp(DisplayedOverallProgress, 0.0f, 1.0f);

            if (bRunning)

            {

                constexpr double EstimateSafetyMargin = 1.30;

                const double ScheduledRemaining = FMath::Max(0.0, EstimatedTotalSeconds - Result.ElapsedSeconds);

                double CandidateRemaining = ScheduledRemaining;

                const double StageBasedRemaining = GetExpectedRemainingSeconds(Stage, TargetStageProgress) * EstimateSafetyMargin;

                CandidateRemaining = FMath::Min(CandidateRemaining, StageBasedRemaining);

                DisplayedEstimatedRemainingSeconds = DisplayedEstimatedRemainingSeconds < 0.0

                    ? CandidateRemaining

                    : FMath::Min(DisplayedEstimatedRemainingSeconds, CandidateRemaining);

                Result.EstimatedRemainingSeconds = DisplayedEstimatedRemainingSeconds;

            }

            else if (Stage == EPackageStage::Complete)

            {

                Result.EstimatedRemainingSeconds = 0.0;

            }

            if (Stage == EPackageStage::Complete)

            {

                Result.StageProgress = 1.0f;

            }

            else if (bRunning && Stage != EPackageStage::Idle && Stage != EPackageStage::Failed && Stage != EPackageStage::Canceled)

            {

                Result.StageProgress = FMath::Clamp(TargetStageProgress, 0.0f, 1.0f);

            }

            else

            {

                Result.StageProgress = 0.0f;

            }

            Result.RecentLog = FString::Join(RecentLines, TEXT("\n"));

            return Result;

        }



    private:

        void BeginRun(const FString& Message)

        {

            bRunning = true;

            bSucceeded = false;

            bCanceled = false;

            Stage = EPackageStage::Prepare;

            Progress = GetStageStart(Stage);

            DisplayedOverallProgress = 0.0f;

            LastDisplayedProgressUpdateSeconds = 0.0;

            EstimatedTotalSeconds = GetExpectedPackageSeconds() * 1.30;

            DisplayedEstimatedRemainingSeconds = EstimatedTotalSeconds;

            StartSeconds = FPlatformTime::Seconds();

            StageStartSeconds = StartSeconds;

            EndSeconds = 0.0;

            FinalElapsedSeconds = 0.0;

            LastActiveStage = EPackageStage::Prepare;

            FailureReason.Empty();

            RecentLines.Reset();

            CurrentWork = TEXT("Starting Unreal Automation Tool...");

            Platform = TEXT("-");

            Configuration = TEXT("-");

            bPlatformLocked = false;

            bConfigurationLocked = false;

            const int32 StartIndex = Message.Find(TEXT("Packaging ("), ESearchCase::IgnoreCase);

            if (StartIndex != INDEX_NONE)

            {

                const int32 NameStart = StartIndex + 11;

                int32 NameEnd = INDEX_NONE;

                int32 ParenthesisDepth = 1;

                for (int32 Index = NameStart; Index < Message.Len(); ++Index)

                {

                    if (Message[Index] == TEXT('('))

                    {

                        ++ParenthesisDepth;

                    }

                    else if (Message[Index] == TEXT(')'))

                    {

                        --ParenthesisDepth;

                        if (ParenthesisDepth == 0)

                        {

                            NameEnd = Index;

                            break;

                        }

                    }

                }

                if (NameEnd > NameStart)

                {

                    Platform = Message.Mid(NameStart, NameEnd - NameStart).TrimStartAndEnd();

                }

            }

            if (Platform != TEXT("-"))
            {
                bPlatformLocked = true;
            }

            AddRecentLine(Message);

            UpdateConfigurationFromMessage(Message);

            AsyncTask(ENamedThreads::GameThread, []()

            {

                TMPackageProgress::OpenWindow();

            });

        }



        void FinishRun(bool bSuccess, const FString& Message)

        {

            bRunning = false;

            bSucceeded = bSuccess;

            bCanceled = false;

            Stage = bSuccess ? EPackageStage::Complete : EPackageStage::Failed;

            Progress = bSuccess ? 1.0f : Progress;

            DisplayedOverallProgress = bSuccess ? 1.0f : FMath::Max(DisplayedOverallProgress, Progress);

            EndSeconds = FPlatformTime::Seconds();

            FinalElapsedSeconds = FMath::Max(0.0, EndSeconds - StartSeconds);

            CurrentWork = bSuccess ? TEXT("Packaging completed successfully.") : TEXT("Packaging failed. Check the final log lines.");

            FailureReason = bSuccess ? FString() : Message;

            AddRecentLine(Message);

        }



        bool IsCancelMessage(const FString& Message) const

        {

            return Message.Contains(TEXT("Packaging canceled"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("Packaging cancelled"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("Canceled by user"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("Cancelled by user"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("User canceled"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("User cancelled"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("Operation canceled"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("Operation cancelled"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("Cancel requested"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("Cancellation requested"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("Canceling packaging"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("Cancelling packaging"), ESearchCase::IgnoreCase);

        }



        void FinishCanceled(const FString& Message)

        {

            bRunning = false;

            bSucceeded = false;

            bCanceled = true;

            Stage = EPackageStage::Canceled;

            DisplayedOverallProgress = FMath::Max(DisplayedOverallProgress, Progress);

            EndSeconds = FPlatformTime::Seconds();

            FinalElapsedSeconds = FMath::Max(0.0, EndSeconds - StartSeconds);

            CurrentWork = TEXT("Packaging was canceled.");

            FailureReason = Message;

            AddRecentLine(Message);

        }



        void SetStage(EPackageStage NewStage, const FString& Work)

        {

            if (NewStage == EPackageStage::Failed || NewStage == EPackageStage::Canceled || NewStage == EPackageStage::Complete

                || static_cast<uint8>(NewStage) >= static_cast<uint8>(Stage))

            {

                Stage = NewStage;

                if (NewStage != EPackageStage::Complete && NewStage != EPackageStage::Failed && NewStage != EPackageStage::Canceled)

                {

                    LastActiveStage = NewStage;

                }

                Progress = NewStage == EPackageStage::Complete

                    ? 1.0f

                    : FMath::Clamp(FMath::Max(Progress, GetStageStart(NewStage)), GetStageStart(NewStage), GetRunningStageCap(NewStage));

                StageStartSeconds = FPlatformTime::Seconds();

            }

            CurrentWork = Work.Left(260);

        }



        void ApplyProgressWithinStage(EPackageStage ProgressStage, float Fraction)

        {

            if (ProgressStage == EPackageStage::Idle || ProgressStage == EPackageStage::Complete

                || ProgressStage == EPackageStage::Failed || ProgressStage == EPackageStage::Canceled)

            {

                return;

            }



            if (ProgressStage != Stage)

            {

                return;

            }



            const float ClampedFraction = FMath::Clamp(Fraction, 0.0f, 1.0f);

            const float StageProgress = FMath::Lerp(GetStageStart(ProgressStage), GetStageEnd(ProgressStage), ClampedFraction);

            Progress = FMath::Max(Progress, FMath::Min(StageProgress, GetRunningStageCap(ProgressStage)));

        }



        void UpdateStageFromMessage(const FString& Message)

        {

            if (Message.Contains(TEXT("BUILD COMMAND STARTED"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("Building UnrealEditor"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("Building "), ESearchCase::IgnoreCase) && Message.Contains(TEXT("Target"), ESearchCase::IgnoreCase))

            {

                SetStage(EPackageStage::Build, Message);

            }

            else if (Message.Contains(TEXT("COOK COMMAND STARTED"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("Cooked packages"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("CookOnTheFly"), ESearchCase::IgnoreCase))

            {

                SetStage(EPackageStage::Cook, Message);

            }

            else if (Message.Contains(TEXT("STAGE COMMAND STARTED"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("Creating Staging Manifest"), ESearchCase::IgnoreCase))

            {

                SetStage(EPackageStage::Stage, Message);

            }

            else if (Message.Contains(TEXT("PACKAGE COMMAND STARTED"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("IoStore"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("UnrealPak"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("Creating container"), ESearchCase::IgnoreCase))

            {

                SetStage(EPackageStage::Package, Message);

            }

            else if (Message.Contains(TEXT("ARCHIVE COMMAND STARTED"), ESearchCase::IgnoreCase)

                || Message.Contains(TEXT("ARCHIVE SUCCESSFUL"), ESearchCase::IgnoreCase))

            {

                SetStage(EPackageStage::Archive, Message);

            }

            else if (!Message.IsEmpty())

            {

                CurrentWork = Message.Left(260);

            }

        }



        void UpdateCountProgress(const FString& Message)

        {

            static const FRegexPattern ActionPattern(TEXT("\\[(\\d+)\\/(\\d+)\\]"));

            FRegexMatcher ActionMatcher(ActionPattern, Message);

            if (ActionMatcher.FindNext())

            {

                const int32 Done = FCString::Atoi(*ActionMatcher.GetCaptureGroup(1));

                const int32 Total = FCString::Atoi(*ActionMatcher.GetCaptureGroup(2));

                if (Total > 0)

                {

                    const float Fraction = FMath::Clamp(static_cast<float>(Done) / static_cast<float>(Total), 0.0f, 1.0f);

                    ApplyProgressWithinStage(Stage, Fraction);

                }

            }



            // UnrealPak and IoStore frequently report work as "50%" or "123 of 456"
            // instead of UBT's bracketed action count. Restrict this to packaging lines so
            // unrelated percentages cannot advance the Pak / IoStore stage.
            if (Stage == EPackageStage::Package)
            {
                const bool bPackageProgressLine = Message.Contains(TEXT("UnrealPak"), ESearchCase::IgnoreCase)
                    || Message.Contains(TEXT("IoStore"), ESearchCase::IgnoreCase)
                    || Message.Contains(TEXT("PakFile"), ESearchCase::IgnoreCase)
                    || Message.Contains(TEXT("container"), ESearchCase::IgnoreCase)
                    || Message.Contains(TEXT("compression"), ESearchCase::IgnoreCase);

                if (bPackageProgressLine)
                {
                    static const FRegexPattern PercentPattern(TEXT("\\b(100(?:\\.0+)?|[0-9]{1,2}(?:\\.[0-9]+)?)\\s*%"));
                    FRegexMatcher PercentMatcher(PercentPattern, Message);
                    if (PercentMatcher.FindNext())
                    {
                        const float Fraction = FMath::Clamp(FCString::Atof(*PercentMatcher.GetCaptureGroup(1)) / 100.0f, 0.0f, 1.0f);
                        ApplyProgressWithinStage(EPackageStage::Package, Fraction);
                    }

                    static const FRegexPattern OfPattern(TEXT("\\b(\\d+)\\s*(?:of|/)\\s*(\\d+)\\b"));
                    FRegexMatcher OfMatcher(OfPattern, Message);
                    if (OfMatcher.FindNext())
                    {
                        const int32 Done = FCString::Atoi(*OfMatcher.GetCaptureGroup(1));
                        const int32 Total = FCString::Atoi(*OfMatcher.GetCaptureGroup(2));
                        if (Total > 0)
                        {
                            ApplyProgressWithinStage(EPackageStage::Package, static_cast<float>(Done) / static_cast<float>(Total));
                        }
                    }
                }
            }
            static const FRegexPattern CookPattern(TEXT("Cooked packages (\\d+).*Packages Remain (\\d+).*Total (\\d+)"));

            FRegexMatcher CookMatcher(CookPattern, Message);

            if (CookMatcher.FindNext())

            {

                const int32 Done = FCString::Atoi(*CookMatcher.GetCaptureGroup(1));

                const int32 Total = FCString::Atoi(*CookMatcher.GetCaptureGroup(3));

                if (Total > 0)

                {

                    const float Fraction = FMath::Clamp(static_cast<float>(Done) / static_cast<float>(Total), 0.0f, 1.0f);

                    ApplyProgressWithinStage(EPackageStage::Cook, Fraction);

                }

            }

        }



        static FString NormalizeConfigurationToken(FString Token)
        {
            Token.TrimStartAndEndInline();
            if (Token.StartsWith(TEXT("\"")) && Token.EndsWith(TEXT("\"")) && Token.Len() >= 2)
            {
                Token = Token.Mid(1, Token.Len() - 2);
                Token.TrimStartAndEndInline();
            }
            if (Token.Equals(TEXT("shipping"), ESearchCase::IgnoreCase)) return TEXT("Shipping");
            if (Token.Equals(TEXT("development"), ESearchCase::IgnoreCase)) return TEXT("Development");
            if (Token.Equals(TEXT("test"), ESearchCase::IgnoreCase)) return TEXT("Test");
            if (Token.Equals(TEXT("debuggame"), ESearchCase::IgnoreCase) || Token.Equals(TEXT("debug game"), ESearchCase::IgnoreCase)) return TEXT("DebugGame");
            if (Token.Equals(TEXT("debug"), ESearchCase::IgnoreCase)) return TEXT("Debug");
            return FString();
        }

        void UpdatePlatformFromMessage(const FString& Message)
        {
            if (Message.IsEmpty() || bPlatformLocked)
            {
                return;
            }

            FString DetectedPlatform;
            const int32 StartIndex = Message.Find(TEXT("Packaging ("), ESearchCase::IgnoreCase);
            if (StartIndex != INDEX_NONE)
            {
                const int32 NameStart = StartIndex + 11;
                int32 ParenthesisDepth = 1;
                for (int32 Index = NameStart; Index < Message.Len(); ++Index)
                {
                    if (Message[Index] == TEXT('('))
                    {
                        ++ParenthesisDepth;
                    }
                    else if (Message[Index] == TEXT(')') && --ParenthesisDepth == 0)
                    {
                        DetectedPlatform = Message.Mid(NameStart, Index - NameStart).TrimStartAndEnd();
                        break;
                    }
                }
            }

            if (DetectedPlatform.IsEmpty())
            {
                static const FRegexPattern PlatformPattern(TEXT("-(?:targetplatform|platform|cookplatform)\\s*=\\s*\"?([A-Za-z0-9_]+)"));
                FRegexMatcher PlatformMatcher(PlatformPattern, Message);
                if (PlatformMatcher.FindNext())
                {
                    DetectedPlatform = PlatformMatcher.GetCaptureGroup(1);
                }
            }

            if (!DetectedPlatform.IsEmpty())
            {
                Platform = DetectedPlatform;
                bPlatformLocked = true;
            }
        }

        void SetConfiguration(const FString& Value, bool bAuthoritative)
        {
            if (Value.IsEmpty() || bConfigurationLocked)
            {
                return;
            }

            Configuration = Value;
            bConfigurationLocked |= bAuthoritative;
        }
        void UpdateConfigurationFromMessage(const FString& Message)
        {
            if (Message.IsEmpty())
            {
                return;
            }

            static const FRegexPattern ExplicitConfigPattern(TEXT("-(?:clientconfig|serverconfig)\\s*=\\s*\"?([A-Za-z]+(?:Game)?)\"?"));
            FRegexMatcher ExplicitMatcher(ExplicitConfigPattern, Message);
            if (ExplicitMatcher.FindNext())
            {
                const FString Found = NormalizeConfigurationToken(ExplicitMatcher.GetCaptureGroup(1));
                if (!Found.IsEmpty())
                {
                    SetConfiguration(Found, true);
                    return;
                }
            }

            static const FRegexPattern SpaceConfigPattern(TEXT("\\b(?:clientconfig|serverconfig)\\s+\"?([A-Za-z]+(?:Game)?)\"?"));
            FRegexMatcher SpaceMatcher(SpaceConfigPattern, Message);
            if (SpaceMatcher.FindNext())
            {
                const FString Found = NormalizeConfigurationToken(SpaceMatcher.GetCaptureGroup(1));
                if (!Found.IsEmpty())
                {
                    SetConfiguration(Found, true);
                    return;
                }
            }

            static const FRegexPattern TargetConfigPattern(TEXT("\\b(?:Shipping|Development|DebugGame|Debug|Test)\\b"));
            FRegexMatcher TargetMatcher(TargetConfigPattern, Message);
            if (TargetMatcher.FindNext()
                && (Message.Contains(TEXT("BuildCookRun"), ESearchCase::IgnoreCase)
                    || Message.Contains(TEXT("Target"), ESearchCase::IgnoreCase)
                    || Message.Contains(TEXT("ProjectParams"), ESearchCase::IgnoreCase)
                    || Message.Contains(TEXT("Configuration"), ESearchCase::IgnoreCase)
                    || Message.Contains(TEXT("Packaging"), ESearchCase::IgnoreCase)))
            {
                const FString Found = NormalizeConfigurationToken(TargetMatcher.GetCaptureGroup(0));
                if (!Found.IsEmpty() && Configuration == TEXT("-"))
                {
                    SetConfiguration(Found, false);
                }
            }
        }


        void AddRecentLine(const FString& Message)

        {

            FString Clean = Message;

            Clean.TrimStartAndEndInline();

            if (Clean.IsEmpty())

            {

                return;

            }

            Clean = Clean.Left(500);
            if (RecentLines.Num() > 0 && RecentLines.Last().Equals(Clean, ESearchCase::CaseSensitive))
            {
                return;
            }
            RecentLines.Add(Clean);

            if (RecentLines.Num() > 60)

            {

                TMEngineCompatibility::RemoveAtNoShrink(RecentLines, 0, RecentLines.Num() - 60);

            }

        }



        mutable FCriticalSection Mutex;

        TArray<FString> RecentLines;

        FString Platform = TEXT("-");

        FString Configuration = TEXT("-");

        bool bPlatformLocked = false;

        bool bConfigurationLocked = false;

        FString CurrentWork = TEXT("Waiting for a packaging task.");

        FString FailureReason;

        EPackageStage Stage = EPackageStage::Idle;

        float Progress = 0.0f;

        mutable float DisplayedOverallProgress = 0.0f;

        mutable double LastDisplayedProgressUpdateSeconds = 0.0;

        double EstimatedTotalSeconds = 0.0;

        mutable double DisplayedEstimatedRemainingSeconds = -1.0;

        double StartSeconds = 0.0;

        double StageStartSeconds = 0.0;

        double EndSeconds = 0.0;

        double FinalElapsedSeconds = 0.0;

        EPackageStage LastActiveStage = EPackageStage::Idle;

        bool bRunning = false;

        bool bSucceeded = false;

        bool bCanceled = false;

        bool bRegistered = false;

    };



    FPackageProgressMonitor PackageMonitor;



    class SPackageProgressWidget final : public SCompoundWidget

    {

    public:

        SLATE_BEGIN_ARGS(SPackageProgressWidget) {}

        SLATE_END_ARGS()



        void Construct(const FArguments&)

        {

            ChildSlot

            [

                SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

                .Padding(12.0f)

                [

                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()

                    [

                        BuildHeader()

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)

                    [

                        BuildProgressPanel()

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)

                    [

                        BuildMetricRow()

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)

                    [

                        BuildCurrentWorkPanel()

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)

                    [

                        BuildFailureAnalysisPanel()

                    ]

                    + SVerticalBox::Slot().FillHeight(1.0f).Padding(0, 10, 0, 0)

                    [

                        BuildLogPanel()

                    ]

                ]

            ];

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

                        SNew(STextBlock)

                        .Text(TMLoc::Text(TEXT("Package Progress"), TEXT("Package Progress")))

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16))

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)

                    [

                        SNew(STextBlock)

                        .Text(TMLoc::Text(TEXT("Automatically detects editor packaging and estimates progress from UAT stages and work counts."), TEXT("Automatically detects editor packaging and estimates progress from UAT stages and work counts.")))

                        .ColorAndOpacity(FLinearColor(0.62f, 0.66f, 0.72f))

                    ]

                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                [

                    SNew(SButton)

                    .Text(TMLoc::Text(TEXT("Copy Status"), TEXT("Copy Status")))

                    .OnClicked_Lambda([]()

                    {

                        const FPackageProgressSnapshot State = PackageMonitor.GetSnapshot();

                        const FString StatusText = State.bRunning

                            ? TMLoc::String(TEXT("Running"), TEXT("Running"))

                            : (State.bSucceeded ? TMLoc::String(TEXT("Succeeded"), TEXT("Succeeded")) : (State.bCanceled ? TMLoc::String(TEXT("Canceled"), TEXT("Canceled")) : (State.Stage == EPackageStage::Failed ? TMLoc::String(TEXT("Failed"), TEXT("Failed")) : TMLoc::String(TEXT("Waiting"), TEXT("Waiting")))));



                        TArray<TMReportFormatter::FMetadataItem> Metadata;

                        Metadata.Add(TMReportFormatter::FMetadataItem(TMLoc::String(TEXT("Platform"), TEXT("Platform")), State.Platform));

                        Metadata.Add(TMReportFormatter::FMetadataItem(TMLoc::String(TEXT("Configuration"), TEXT("Configuration")), State.Configuration));

                        Metadata.Add(TMReportFormatter::FMetadataItem(TMLoc::String(TEXT("Status"), TEXT("Status")), StatusText));

                        Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Stage"), GetStageEnglish(State.Stage)));

                        Metadata.Add(TMReportFormatter::FMetadataItem(TMLoc::String(TEXT("Elapsed"), TEXT("Elapsed")), FormatDuration(State.ElapsedSeconds)));



                        const FString Summary = FString::Printf(TEXT("- Progress: %.1f%%\n- Stage: %s\n- %s: %s\n- %s: %s\n- %s: %s"),

                            State.Progress * 100.0f,

                            *GetStageEnglish(State.Stage),

                            *TMLoc::String(TEXT("Configuration"), TEXT("Configuration")),

                            *State.Configuration,

                            *TMLoc::String(TEXT("Elapsed"), TEXT("Elapsed")),

                            *FormatDuration(State.ElapsedSeconds),

                            *TMLoc::String(TEXT("Status"), TEXT("Status")),

                            *StatusText);



                        FString Details;

                        Details += FString::Printf(TEXT("- %s: %s\n"), *TMLoc::String(TEXT("Current work"), TEXT("Current work")), *State.CurrentWork);

                        if (!State.RecentLog.IsEmpty())

                        {

                            Details += FString::Printf(TEXT("- %s: %s\n"), *TMLoc::String(TEXT("Recent packaging log"), TEXT("Recent packaging log")), *State.RecentLog);

                        }

                        if (!State.FailureReason.IsEmpty())

                        {

                            Details += FString::Printf(TEXT("- Failure: %s\n"), *State.FailureReason);

                        }

                        Details += TEXT("- Estimated progress: use the percent together with stage and recent log; Unreal packaging does not expose a single exact universal percent.\n");



                        const FString Text = TMReportFormatter::BuildWrappedLegacyReport(TEXT("Package Progress"), Summary, Details, Metadata);

                        FPlatformApplicationMisc::ClipboardCopy(*Text);

                        return FReply::Handled();

                    })

                ];

        }



        TSharedRef<SWidget> BuildProgressPanel()
        {
            return SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
                .Padding(12.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
                        [
                            SNew(SThrobber)
                            .Visibility_Lambda([]() { return PackageMonitor.GetSnapshot().bRunning ? EVisibility::Visible : EVisibility::Collapsed; })
                        ]
                        + SHorizontalBox::Slot().FillWidth(1.0f)
                        [
                            SNew(STextBlock)
                            .Text_Lambda([]()
                            {
                                const FPackageProgressSnapshot State = PackageMonitor.GetSnapshot();
                                return FText::FromString(TMLoc::IsKoreanEditor() ? GetStageKorean(State.Stage) : GetStageEnglish(State.Stage));
                            })
                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 12))
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
                    [
                        BuildStageStrip()
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f)
                        [
                            SNew(STextBlock)
                            .Text(TMLoc::Text(TEXT("Current stage progress"), TEXT("Current stage progress")))
                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))
                            .ColorAndOpacity(FLinearColor(0.58f, 0.62f, 0.68f))
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(STextBlock)
                            .Text_Lambda([]() { return FText::FromString(FString::Printf(TEXT("%.1f%%"), PackageMonitor.GetSnapshot().StageProgress * 100.0f)); })
                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 11))
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(2, 4, 2, 0)
                    [
                        SNew(SProgressBar)
                        .Percent_Lambda([]() { return TOptional<float>(PackageMonitor.GetSnapshot().StageProgress); })
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f)
                        [
                            SNew(STextBlock)
                            .Text(TMLoc::Text(TEXT("Overall progress"), TEXT("Overall progress")))
                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))
                            .ColorAndOpacity(FLinearColor(0.58f, 0.62f, 0.68f))
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(STextBlock)
                            .Text_Lambda([]() { return FText::FromString(FString::Printf(TEXT("%.1f%%"), PackageMonitor.GetSnapshot().Progress * 100.0f)); })
                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18))
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(2, 4, 2, 0)
                    [
                        SNew(SProgressBar)
                        .Percent_Lambda([]() { return TOptional<float>(PackageMonitor.GetSnapshot().Progress); })
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
                    [
                        SNew(STextBlock)
                        .Text(TMLoc::Text(TEXT("Estimated progress - the upper bar is the current stage, the lower bar is total packaging progress."), TEXT("Estimated progress - the upper bar is the current stage, the lower bar is total packaging progress.")))
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
                        .ColorAndOpacity(FLinearColor(0.55f, 0.59f, 0.65f))
                    ]
                ];
        }

        TSharedRef<SWidget> BuildStageStrip()

        {

            TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);

            const TArray<EPackageStage> Stages = {

                EPackageStage::Prepare, EPackageStage::Build, EPackageStage::Cook,

                EPackageStage::Stage, EPackageStage::Package, EPackageStage::Archive

            };

            for (EPackageStage StageValue : Stages)

            {

                Box->AddSlot().FillWidth(1.0f).Padding(2, 0)

                [

                    SNew(SBorder)

                    .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

                    .BorderBackgroundColor_Lambda([StageValue]()

                    {

                        const FPackageProgressSnapshot State = PackageMonitor.GetSnapshot();

                        if (State.Stage == EPackageStage::Failed && State.ActiveStage == StageValue) return FLinearColor(0.28f, 0.06f, 0.05f, 1.0f);

                        if (State.Stage == EPackageStage::Canceled && State.ActiveStage == StageValue) return FLinearColor(0.30f, 0.20f, 0.06f, 1.0f);

                        if (State.Stage == EPackageStage::Complete || static_cast<uint8>(State.ActiveStage) > static_cast<uint8>(StageValue)) return FLinearColor(0.05f, 0.23f, 0.10f, 1.0f);

                        if (State.ActiveStage == StageValue) return FLinearColor(0.05f, 0.18f, 0.34f, 1.0f);

                        return FLinearColor(0.08f, 0.085f, 0.095f, 1.0f);

                    })

                    .Padding(FMargin(5.0f, 7.0f))

                    [

                        SNew(STextBlock)

                        .Justification(ETextJustify::Center)

                        .Text_Lambda([StageValue]()

                        {

                            const FPackageProgressSnapshot State = PackageMonitor.GetSnapshot();

                            const bool bDone = State.Stage == EPackageStage::Complete

                                || static_cast<uint8>(State.ActiveStage) > static_cast<uint8>(StageValue);

                            const bool bCurrent = State.ActiveStage == StageValue && State.bRunning;

                            const bool bFailedHere = State.Stage == EPackageStage::Failed && State.ActiveStage == StageValue;

                            const bool bCanceledHere = State.Stage == EPackageStage::Canceled && State.ActiveStage == StageValue;

                            const FString Prefix = bDone
                                ? TEXT("\u2713 ")
                                : ((bFailedHere || bCanceledHere) ? TEXT("\u00D7 ") : (bCurrent ? TEXT("\u25CF ") : TEXT("\u25CB ")));

                            const FString Label = TMLoc::IsKoreanEditor() ? GetStageKorean(StageValue) : GetStageEnglish(StageValue);

                            return FText::FromString(Prefix + Label);

                        })

                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                    ]

                ];

            }

            return Box;

        }



        TSharedRef<SWidget> BuildMetricCardWidget(const FText& Label, TSharedRef<SWidget> ValueWidget)

        {

            return SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

                .Padding(9.0f)

                [

                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()

                    [

                        SNew(STextBlock).Text(Label).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8)).ColorAndOpacity(FLinearColor(0.58f, 0.62f, 0.68f))

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)

                    [

                        ValueWidget

                    ]

                ];

        }



        TSharedRef<SWidget> BuildMetricCard(const FText& Label, TAttribute<FText> Value)

        {

            return BuildMetricCardWidget(

                Label,

                SNew(STextBlock).Text(Value).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 11)));

        }


        TSharedRef<SWidget> BuildMetricRow()

        {

            return SNew(SHorizontalBox)

                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)

                [

                    BuildMetricCardWidget(

                        TMLoc::Text(TEXT("Platform"), TEXT("Platform")),

                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                        [

                            SNew(SBox).WidthOverride(16.0f).HeightOverride(16.0f)

                            [

                                SNew(SImage)

                                .Image_Lambda([]()

                                {

                                    const FString PlatformLabel = PackageMonitor.GetSnapshot().Platform;

                                    FString PlatformName = PlatformLabel;

                                    if (PlatformLabel.StartsWith(TEXT("Android"), ESearchCase::IgnoreCase))

                                    {

                                        if (PlatformLabel.Contains(TEXT("ASTC"), ESearchCase::IgnoreCase)) PlatformName = TEXT("Android_ASTC");

                                        else if (PlatformLabel.Contains(TEXT("ETC2"), ESearchCase::IgnoreCase)) PlatformName = TEXT("Android_ETC2");

                                        else if (PlatformLabel.Contains(TEXT("DXT"), ESearchCase::IgnoreCase)) PlatformName = TEXT("Android_DXT");

                                        else PlatformName = TEXT("Android");

                                    }

                                    const PlatformInfo::FTargetPlatformInfo* Info = PlatformInfo::FindPlatformInfo(FName(*PlatformName));

                                    return Info

                                        ? FAppStyle::GetBrush(Info->GetIconStyleName(EPlatformIconSize::Normal))

                                        : FAppStyle::GetBrush(TEXT("Icons.Package"));

                                })

                            ]

                        ]

                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(6, 0, 0, 0)

                        [

                            SNew(STextBlock)

                            .Text_Lambda([]() { return FText::FromString(PackageMonitor.GetSnapshot().Platform); })

                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 11))

                        ])

                ]

                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(4, 0)

                [

                    BuildMetricCard(TMLoc::Text(TEXT("Configuration"), TEXT("Configuration")), TAttribute<FText>::CreateLambda([]() { return FText::FromString(PackageMonitor.GetSnapshot().Configuration); }))

                ]

                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(4, 0)

                [

                    BuildMetricCard(TMLoc::Text(TEXT("Elapsed"), TEXT("Elapsed")), TAttribute<FText>::CreateLambda([]() { return FText::FromString(FormatDuration(PackageMonitor.GetSnapshot().ElapsedSeconds)); }))

                ]

                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(4, 0)

                [

                    BuildMetricCard(TMLoc::Text(TEXT("Estimated remaining"), TEXT("Estimated remaining")), TAttribute<FText>::CreateLambda([]()

                    {

                        const FPackageProgressSnapshot State = PackageMonitor.GetSnapshot();

                        if (State.Stage == EPackageStage::Complete)

                        {

                            return TMLoc::Text(TEXT("00:00"), TEXT("00:00"));

                        }

                        if (State.Stage == EPackageStage::Canceled)

                        {

                            return TMLoc::Text(TEXT("-"), TEXT("-"));

                        }

                        return FText::FromString(FormatDuration(State.EstimatedRemainingSeconds));

                    }))

                ]

                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(4, 0, 0, 0)

                [

                    BuildMetricCardWidget(

                        TMLoc::Text(TEXT("Status"), TEXT("Status")),

                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                        [

                            SNew(STextBlock)

                            .Text_Lambda([]()

                            {

                                const FPackageProgressSnapshot State = PackageMonitor.GetSnapshot();

                                if (State.bRunning) return TMLoc::Text(TEXT("\u25CF"), TEXT("\u25CF"));

                                if (State.Stage == EPackageStage::Complete) return TMLoc::Text(TEXT("\u2713"), TEXT("\u2713"));

                                if (State.Stage == EPackageStage::Failed) return TMLoc::Text(TEXT("!"), TEXT("!"));

                                if (State.Stage == EPackageStage::Canceled) return TMLoc::Text(TEXT("\u00D7"), TEXT("\u00D7"));

                                return TMLoc::Text(TEXT("\u25CB"), TEXT("\u25CB"));

                            })

                            .ColorAndOpacity_Lambda([]()

                            {

                                const FPackageProgressSnapshot State = PackageMonitor.GetSnapshot();

                                if (State.bRunning || State.Stage == EPackageStage::Complete) return FSlateColor(FLinearColor(0.18f, 0.78f, 0.38f));

                                if (State.Stage == EPackageStage::Failed) return FSlateColor(FLinearColor(0.95f, 0.25f, 0.22f));

                                return FSlateColor(FLinearColor(0.58f, 0.62f, 0.68f));

                            })

                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 13))

                        ]

                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(6, 0, 0, 0)

                        [

                            SNew(STextBlock)

                            .Text_Lambda([]()

                            {

                                const FPackageProgressSnapshot State = PackageMonitor.GetSnapshot();

                                if (State.bRunning) return TMLoc::Text(TEXT("Running"), TEXT("Running"));

                                if (State.Stage == EPackageStage::Complete) return TMLoc::Text(TEXT("Succeeded"), TEXT("Succeeded"));

                                if (State.Stage == EPackageStage::Canceled) return TMLoc::Text(TEXT("Canceled"), TEXT("Canceled"));

                                if (State.Stage == EPackageStage::Failed) return TMLoc::Text(TEXT("Failed"), TEXT("Failed"));

                                return TMLoc::Text(TEXT("Waiting"), TEXT("Waiting"));

                            })

                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 11))

                        ])

                ];

        }



        TSharedRef<SWidget> BuildCurrentWorkPanel()

        {

            return SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))

                .Padding(10.0f)

                [

                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()

                    [

                        SNew(STextBlock).Text(TMLoc::Text(TEXT("Current work"), TEXT("Current work"))).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)

                    [

                        SNew(STextBlock)

                        .Text_Lambda([]() { return FText::FromString(PackageMonitor.GetSnapshot().CurrentWork); })

                        .AutoWrapText(true)

                    ]

                ];

        }



        TSharedRef<SWidget> BuildFailureFindingRow(const FPackageFailureFinding& Finding) const
        {
            const bool bCanOpen = !Finding.FilePath.IsEmpty() || !Finding.AssetPath.IsEmpty() || !Finding.TargetLabel.IsEmpty();
            return SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
                .BorderBackgroundColor(FLinearColor(0.18f, 0.07f, 0.06f, 1.0f))
                .Padding(9.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(Finding.Summary))
                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))
                            .ColorAndOpacity(FLinearColor(1.0f, 0.72f, 0.66f))
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            SNew(SButton)
                            .Visibility(bCanOpen ? EVisibility::Visible : EVisibility::Collapsed)
                            .Text(TMLoc::Text(TEXT("Open target"), TEXT("Open target")))
                            .ToolTipText(TMLoc::Text(TEXT("Open the related asset, Blueprint, or source file when it can be inferred."), TEXT("Open the related asset, Blueprint, or source file when it can be inferred.")))
                            .OnClicked_Lambda([Finding]() { return OpenPackageFailureTarget(Finding); })
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(Finding.Detail))
                        .AutoWrapText(true)
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
                        .ColorAndOpacity(FLinearColor(0.86f, 0.86f, 0.84f))
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)
                    [
                        SNew(STextBlock)
                        .Visibility(!Finding.TargetLabel.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed)
                        .Text(FText::Format(TMLoc::Text(TEXT("Target: {0}"), TEXT("Target: {0}")), FText::FromString(Finding.TargetLabel)))
                        .AutoWrapText(true)
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
                        .ColorAndOpacity(FLinearColor(0.64f, 0.72f, 0.86f))
                    ]
                ];
        }

        TSharedRef<SWidget> BuildDynamicFailureFindingRow(const int32 FindingIndex) const
        {
            auto GetFinding = [FindingIndex](FPackageFailureFinding& OutFinding) -> bool
            {
                const TArray<FPackageFailureFinding> Findings = AnalyzePackageFailure(PackageMonitor.GetSnapshot());
                if (Findings.IsValidIndex(FindingIndex))
                {
                    OutFinding = Findings[FindingIndex];
                    return true;
                }
                return false;
            };

            return SNew(SBorder)
                .Visibility_Lambda([GetFinding]()
                {
                    FPackageFailureFinding Finding;
                    return GetFinding(Finding) ? EVisibility::Visible : EVisibility::Collapsed;
                })
                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
                .BorderBackgroundColor(FLinearColor(0.18f, 0.07f, 0.06f, 1.0f))
                .Padding(9.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Text_Lambda([GetFinding]()
                            {
                                FPackageFailureFinding Finding;
                                return FText::FromString(GetFinding(Finding) ? Finding.Summary : FString());
                            })
                            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))
                            .ColorAndOpacity(FLinearColor(1.0f, 0.72f, 0.66f))
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            SNew(SButton)
                            .Visibility_Lambda([GetFinding]()
                            {
                                FPackageFailureFinding Finding;
                                return GetFinding(Finding) && (!Finding.FilePath.IsEmpty() || !Finding.AssetPath.IsEmpty() || !Finding.TargetLabel.IsEmpty())
                                    ? EVisibility::Visible
                                    : EVisibility::Collapsed;
                            })
                            .Text(TMLoc::Text(TEXT("Open target"), TEXT("Open target")))
                            .ToolTipText(TMLoc::Text(TEXT("Open the related asset, Blueprint, or source file when it can be inferred."), TEXT("Open the related asset, Blueprint, or source file when it can be inferred.")))
                            .OnClicked_Lambda([GetFinding]()
                            {
                                FPackageFailureFinding Finding;
                                return GetFinding(Finding) ? OpenPackageFailureTarget(Finding) : FReply::Unhandled();
                            })
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
                    [
                        SNew(STextBlock)
                        .Text_Lambda([GetFinding]()
                        {
                            FPackageFailureFinding Finding;
                            return FText::FromString(GetFinding(Finding) ? Finding.Detail : FString());
                        })
                        .AutoWrapText(true)
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
                        .ColorAndOpacity(FLinearColor(0.86f, 0.86f, 0.84f))
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)
                    [
                        SNew(STextBlock)
                        .Visibility_Lambda([GetFinding]()
                        {
                            FPackageFailureFinding Finding;
                            return GetFinding(Finding) && !Finding.TargetLabel.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
                        })
                        .Text_Lambda([GetFinding]()
                        {
                            FPackageFailureFinding Finding;
                            return GetFinding(Finding)
                                ? FText::Format(TMLoc::Text(TEXT("Target: {0}"), TEXT("Target: {0}")), FText::FromString(Finding.TargetLabel))
                                : FText::GetEmpty();
                        })
                        .AutoWrapText(true)
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
                        .ColorAndOpacity(FLinearColor(0.64f, 0.72f, 0.86f))
                    ]
                ];
        }

        TSharedRef<SWidget> BuildFailureAnalysisPanel() const
        {
            return SNew(SBorder)
                .Visibility_Lambda([]()
                {
                    const FPackageProgressSnapshot State = PackageMonitor.GetSnapshot();
                    return State.Stage == EPackageStage::Failed ? EVisibility::Visible : EVisibility::Collapsed;
                })
                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
                .BorderBackgroundColor(FLinearColor(0.12f, 0.045f, 0.04f, 1.0f))
                .Padding(10.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(TMLoc::Text(TEXT("Failure analysis"), TEXT("Failure analysis")))
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))
                        .ColorAndOpacity(FLinearColor(1.0f, 0.54f, 0.48f))
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 8)
                    [
                        SNew(STextBlock)
                        .Text(TMLoc::Text(TEXT("Likely causes are extracted from the final UAT/cook/build log lines. Use Open target when an asset, Blueprint, class, or source file can be inferred."), TEXT("Likely causes are extracted from the final UAT/cook/build log lines. Use Open target when an asset, Blueprint, class, or source file can be inferred.")))
                        .AutoWrapText(true)
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
                        .ColorAndOpacity(FLinearColor(0.78f, 0.74f, 0.72f))
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SBox)
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(STextBlock)
                                    .Text_Lambda([]()
                                    {
                                        const FPackageProgressSnapshot State = PackageMonitor.GetSnapshot();
                                        const TArray<FPackageFailureFinding> Findings = AnalyzePackageFailure(State);
                                        return FText::Format(TMLoc::Text(TEXT("{0} likely issue(s) found"), TEXT("{0} likely issue(s) found")), FText::AsNumber(Findings.Num()));
                                    })
                                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
                                    .ColorAndOpacity(FLinearColor(0.70f, 0.70f, 0.70f))
                                ]
                            ]
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SBox)
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[BuildDynamicFailureFindingRow(0)]
                                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[BuildDynamicFailureFindingRow(1)]
                                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[BuildDynamicFailureFindingRow(2)]
                                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[BuildDynamicFailureFindingRow(3)]
                                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[BuildDynamicFailureFindingRow(4)]
                                + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(STextBlock)
                                    .Visibility_Lambda([]()
                                    {
                                        const TArray<FPackageFailureFinding> Findings = AnalyzePackageFailure(PackageMonitor.GetSnapshot());
                                        return Findings.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
                                    })
                                    .Text(TMLoc::Text(TEXT("No specific error line was detected. Check the recent packaging log below."), TEXT("No specific error line was detected. Check the recent packaging log below.")))
                                    .AutoWrapText(true)
                                    .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
                                    .ColorAndOpacity(FLinearColor(0.82f, 0.82f, 0.82f))
                                ]
                            ]
                        ]
                    ]
                ];
        }

        TSharedRef<SWidget> BuildFailureFindingList() const
        {
            const FPackageProgressSnapshot State = PackageMonitor.GetSnapshot();
            const TArray<FPackageFailureFinding> Findings = AnalyzePackageFailure(State);
            TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
            for (const FPackageFailureFinding& Finding : Findings)
            {
                Box->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
                [
                    BuildFailureFindingRow(Finding)
                ];
            }
            if (Findings.IsEmpty())
            {
                Box->AddSlot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(TMLoc::Text(TEXT("No specific error line was detected. Check the recent packaging log below."), TEXT("No specific error line was detected. Check the recent packaging log below.")))
                    .AutoWrapText(true)
                    .ColorAndOpacity(FLinearColor(0.82f, 0.82f, 0.82f))
                ];
            }
            return Box;
        }

        TSharedRef<SWidget> BuildLogLineRow(const int32 LineIndex) const
        {
            auto GetLine = [LineIndex]() -> FString
            {
                TArray<FString> Lines;
                PackageMonitor.GetSnapshot().RecentLog.ParseIntoArrayLines(Lines, false);
                return Lines.IsValidIndex(LineIndex) ? Lines[LineIndex] : FString();
            };

            return SNew(STextBlock)
                .Visibility_Lambda([GetLine]() { return GetLine().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
                .Text_Lambda([GetLine]() { return FText::FromString(GetLine()); })
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), 8))
                .ColorAndOpacity_Lambda([GetLine]()
                {
                    const FString Line = GetLine();
                    return IsPackageErrorLogLine(Line)
                        ? FSlateColor(FLinearColor(1.0f, 0.22f, 0.18f))
                        : FSlateColor(FLinearColor(0.68f, 0.71f, 0.76f));
                });
        }

        TSharedRef<SWidget> BuildLogLineList() const
        {
            TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
            constexpr int32 MaxVisibleRecentLines = 60;
            for (int32 Index = 0; Index < MaxVisibleRecentLines; ++Index)
            {
                Box->AddSlot().AutoHeight().Padding(0, 0, 0, 2)
                [
                    BuildLogLineRow(Index)
                ];
            }
            return Box;
        }

        TSharedRef<SWidget> BuildLogPanel()

        {

            return SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))

                .Padding(10.0f)

                [

                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()

                    [

                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

                        [

                            SNew(STextBlock).Text(TMLoc::Text(TEXT("Recent packaging log"), TEXT("Recent packaging log"))).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0, 0, 0)

                        [

                            SNew(SButton)

                            .Text(TMLoc::Text(TEXT("Copy all"), TEXT("Copy all")))

                            .ToolTipText(TMLoc::Text(TEXT("Copy all recent packaging log lines."), TEXT("Copy all recent packaging log lines.")))

                            .OnClicked_Lambda([]()

                            {

                                FPlatformApplicationMisc::ClipboardCopy(*PackageMonitor.GetSnapshot().RecentLog);

                                return FReply::Handled();

                            })

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0, 0, 0)

                        [

                            SNew(SButton)

                            .Text(TMLoc::Text(TEXT("Copy errors"), TEXT("Copy errors")))

                            .ToolTipText(TMLoc::Text(TEXT("Copy only error-like packaging log lines."), TEXT("Copy only error-like packaging log lines.")))

                            .OnClicked_Lambda([]()

                            {

                                const FString ErrorLog = ExtractPackageErrorLogLines(PackageMonitor.GetSnapshot().RecentLog);

                                FPlatformApplicationMisc::ClipboardCopy(*ErrorLog);

                                return FReply::Handled();

                            })

                        ]

                    ]

                    + SVerticalBox::Slot().FillHeight(1.0f).Padding(0, 6, 0, 0)

                    [

                        SNew(SScrollBox)

                        + SScrollBox::Slot()

                        [

                            BuildLogLineList()

                        ]

                    ]

                ];

        }


    };



    bool bPackageProgressTabRegistered = false;



    TSharedRef<SDockTab> SpawnPackageProgressTab(const FSpawnTabArgs&)

    {

        return SNew(SDockTab)

            .TabRole(ETabRole::NomadTab)

            .Label(TMLoc::Text(TEXT("Package Progress"), TEXT("Package Progress")))

            [

                SNew(SPackageProgressWidget)

            ];

    }



    void RegisterPackageProgressTab()

    {

        if (bPackageProgressTabRegistered)

        {

            return;

        }

        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PackageProgressTabId, FOnSpawnTab::CreateStatic(&SpawnPackageProgressTab))

            .SetDisplayName(TMLoc::Text(TEXT("Package Progress"), TEXT("Package Progress")))

            .SetTooltipText(TMLoc::Text(TEXT("Monitor packaging stages and estimated overall progress."), TEXT("Monitor packaging stages and estimated overall progress.")))

            .SetIcon(FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.PackageProgress")));

        bPackageProgressTabRegistered = true;

    }

}



namespace TMPackageProgress

{

    void RegisterMenus()

    {

        PackageMonitor.Start();

        RegisterPackageProgressTab();



        auto AddEntry = [](UToolMenu* Menu, const FName EntryName)

        {

            if (!Menu)

            {

                return;

            }

            FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("TraceMotive"));

            Section.AddMenuEntry(

                EntryName,

                TMLoc::Text(TEXT("Package Progress"), TEXT("Package Progress")),

                TMLoc::Text(TEXT("Open the packaging progress monitor."), TEXT("Open the packaging progress monitor.")),

                FSlateIcon(TMStyle::GetStyleSetName(), TEXT("TraceMotive.PackageProgress")),

                FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&) { TMPackageProgress::OpenWindow(); }));

        };



        AddEntry(UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window")), TEXT("TMOpenPackageProgress"));

        AddEntry(UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools")), TEXT("TMToolsOpenPackageProgress"));

    }



    void UnregisterMenus()

    {

        PackageMonitor.Stop();

        if (bPackageProgressTabRegistered)

        {

            FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PackageProgressTabId);

            bPackageProgressTabRegistered = false;

        }

    }



    void OpenWindow()

    {

        RegisterPackageProgressTab();

        FGlobalTabmanager::Get()->TryInvokeTab(PackageProgressTabId);

    }

}
































