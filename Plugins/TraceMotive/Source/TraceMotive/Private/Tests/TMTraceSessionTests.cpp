#include "TMTraceSession.h"
#include "TMInvestigationSession.h"
#include "TMLocalization.h"
#include "TMPerformanceGuard.h"
#include "TMSettings.h"
#include "TMSupportBundle.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ToolMenu.h"
#include "ToolMenus.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMTraceSessionLifecycleTest,
    "TraceMotive.TraceSession.Lifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMTraceSessionLifecycleTest::RunTest(const FString&)
{
    FTMTraceSession Session;
    TestFalse(TEXT("A new session is inactive"), Session.IsActive());

    Session.Begin();
    const uint64 FirstGeneration = Session.GetGeneration();
    TestTrue(TEXT("Begin activates the session"), Session.IsActive());
    TestTrue(TEXT("First generation is non-zero"), FirstGeneration > 0);

    TestTrue(TEXT("First UI refresh is allowed"), Session.ShouldRefreshUi(10.0, 0.2));
    TestFalse(TEXT("Refresh is throttled inside the interval"), Session.ShouldRefreshUi(10.1, 0.2));
    TestTrue(TEXT("Refresh is allowed after the interval"), Session.ShouldRefreshUi(10.2, 0.2));

    Session.Cancel();
    TestFalse(TEXT("Cancel deactivates the session"), Session.IsActive());
    TestTrue(TEXT("Cancel invalidates queued work generation"), Session.GetGeneration() > FirstGeneration);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMSettingsSafetyDefaultsTest,
    "TraceMotive.Settings.SafetyDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMSettingsSafetyDefaultsTest::RunTest(const FString&)
{
    const UTraceMotiveSettings* Settings = UTraceMotiveSettings::Get();
    TestNotNull(TEXT("TraceMotive developer settings are available"), Settings);
    if (!Settings) return false;

    TestTrue(TEXT("Large-project safe mode is enabled by default"), Settings->bLargeProjectSafeMode);
    TestTrue(TEXT("Search result limit is bounded"), TMPerf::MaxSearchResults() >= 50 && TMPerf::MaxSearchResults() <= 10000);
    TestTrue(TEXT("Snapshot field limit is bounded"), TMPerf::MaxSnapshotFields() >= 100 && TMPerf::MaxSnapshotFields() <= 20000);

    const uint64 PreviousGeneration = TMPerf::GetCancellationGeneration();
    TMPerf::RequestCancelAll();
    TestEqual(TEXT("Stop-all invalidates running work generations"), TMPerf::GetCancellationGeneration(), PreviousGeneration + 1);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMLocalizationRuntimeCoverageTest,
    "TraceMotive.Localization.RuntimeCoverage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMLocalizationRuntimeCoverageTest::RunTest(const FString&)
{
    const bool bOriginalKorean = TMLoc::UseKorean();
    TMLoc::SetUseKorean(true);
    TestEqual(TEXT("Existing tool label is translated"), TMLoc::String(TEXT("Collision Pair Analyzer"), TEXT("")), FString(TEXT("충돌 쌍 분석기")));
    TestEqual(TEXT("New tool label is translated"), TMLoc::String(TEXT("Variable Value Trace"), TEXT("")), FString(TEXT("변수 값 추적")));
    TestEqual(TEXT("Guide chrome is translated"), TMLoc::String(TEXT("Annotated live Slate example"), TEXT("")), FString(TEXT("주석이 포함된 라이브 Slate 예시")));
    TMLoc::SetUseKorean(false);
    TestEqual(TEXT("English selection remains available"), TMLoc::String(TEXT("Collision Pair Analyzer"), TEXT("")), FString(TEXT("Collision Pair Analyzer")));
    TMLoc::SetUseKorean(bOriginalKorean);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMInvestigationSnapshotDiffTest,
    "TraceMotive.Investigation.SnapshotDiff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMInvestigationSnapshotDiffTest::RunTest(const FString&)
{
    TMap<FString, FString> Before;
    Before.Add(TEXT("Actor.Health"), TEXT("100"));
    Before.Add(TEXT("Actor.OldOnly"), TEXT("Present"));

    TMap<FString, FString> After;
    After.Add(TEXT("Actor.Health"), TEXT("75"));
    After.Add(TEXT("Actor.NewOnly"), TEXT("Present"));

    const FTMSnapshotDiff Diff = TMInvestigationSession::CompareSnapshots(Before, After);
    TestEqual(TEXT("One changed field is detected"), Diff.Changed, 1);
    TestEqual(TEXT("One added field is detected"), Diff.Added, 1);
    TestEqual(TEXT("One removed field is detected"), Diff.Removed, 1);
    TestEqual(TEXT("All differences have readable lines"), Diff.Lines.Num(), 3);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMSupportBundlePrivacyAndZipTest,
    "TraceMotive.SupportBundle.PrivacyAndZip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMSupportBundlePrivacyAndZipTest::RunTest(const FString&)
{
    const FString Sensitive = TEXT("C:\\Users\\TestPerson\\Project\\File.uasset user@example.com api_key=secret-value");
    int32 Redactions = 0;
    const FString Redacted = TMSupportBundle::RedactSensitiveText(Sensitive, &Redactions);
    TestTrue(TEXT("User home is removed"), Redacted.Contains(TEXT("<USER_HOME>")));
    TestTrue(TEXT("Email is removed"), Redacted.Contains(TEXT("<EMAIL>")));
    TestTrue(TEXT("Common secret assignment is removed"), Redacted.Contains(TEXT("<REDACTED_SECRET>")));
    TestFalse(TEXT("Original username is absent"), Redacted.Contains(TEXT("TestPerson")));
    TestFalse(TEXT("Original email is absent"), Redacted.Contains(TEXT("user@example.com")));
    TestFalse(TEXT("Original secret is absent"), Redacted.Contains(TEXT("secret-value")));
    TestTrue(TEXT("Redactions are reported"), Redactions >= 3);

    const FString ZipPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TraceMotive"), TEXT("Automation"), TEXT("SupportBundleTest.zip"));
    TMap<FString, FString> Entries;
    Entries.Add(TEXT("README.md"), TEXT("TraceMotive support test"));
    Entries.Add(TEXT("metadata.json"), TEXT("{\"ok\":true}"));
    FString Error;
    TestTrue(TEXT("Stored ZIP is written"), TMSupportBundle::WriteStoredZip(ZipPath, Entries, Error));
    TArray<uint8> Bytes;
    TestTrue(TEXT("Stored ZIP can be read back"), FFileHelper::LoadFileToArray(Bytes, *ZipPath));
    TestTrue(TEXT("ZIP has a local-file signature"), Bytes.Num() >= 4 && Bytes[0] == 0x50 && Bytes[1] == 0x4b && Bytes[2] == 0x03 && Bytes[3] == 0x04);
    TestTrue(TEXT("ZIP has an end-of-central-directory signature"), Bytes.Num() >= 22 && Bytes[Bytes.Num() - 22] == 0x50 && Bytes[Bytes.Num() - 21] == 0x4b && Bytes[Bytes.Num() - 20] == 0x05 && Bytes[Bytes.Num() - 19] == 0x06);
    IFileManager::Get().Delete(*ZipPath, false, true);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTMWindowMenuRegistrationTest,
    "TraceMotive.Registration.ConsolidatedMainMenus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTMWindowMenuRegistrationTest::RunTest(const FString&)
{
    UToolMenu* WindowMenu = UToolMenus::Get()->FindMenu(TEXT("LevelEditor.MainMenu.Window"));
    TestNotNull(TEXT("The Level Editor Window menu exists"), WindowMenu);
    if (!WindowMenu)
    {
        return false;
    }

    TestTrue(
        TEXT("Window menu exposes only the TraceMotiveTools launcher"),
        WindowMenu->ContainsEntry(TEXT("TMOpenTraceMotiveTools")));

    UToolMenu* ToolsMenu = UToolMenus::Get()->FindMenu(TEXT("LevelEditor.MainMenu.Tools"));
    TestNotNull(TEXT("The Level Editor Tools menu exists"), ToolsMenu);
    if (!ToolsMenu)
    {
        return false;
    }

    TestTrue(
        TEXT("Tools menu exposes the TraceMotive submenu"),
        ToolsMenu->ContainsEntry(TEXT("TraceMotive")));

    return true;
}

#endif
