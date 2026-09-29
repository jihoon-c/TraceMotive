#include "TMSupportBundle.h"

#include "TMSettings.h"

#include "Algo/Reverse.h"
#include "Interfaces/IPluginManager.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProperties.h"
#include "HAL/PlatformProcess.h"
#include "Internationalization/Regex.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    struct FBundleContent
    {
        TMap<FString, FString> Entries;
        FTMSupportBundlePreview Preview;
    };

    void AppendUInt16(TArray<uint8>& Bytes, uint16 Value)
    {
        Bytes.Add(static_cast<uint8>(Value));
        Bytes.Add(static_cast<uint8>(Value >> 8));
    }

    void AppendUInt32(TArray<uint8>& Bytes, uint32 Value)
    {
        Bytes.Add(static_cast<uint8>(Value));
        Bytes.Add(static_cast<uint8>(Value >> 8));
        Bytes.Add(static_cast<uint8>(Value >> 16));
        Bytes.Add(static_cast<uint8>(Value >> 24));
    }

    void AppendBytes(TArray<uint8>& Bytes, const TArray<uint8>& Value)
    {
        Bytes.Append(Value);
    }

    TArray<uint8> ToUtf8(const FString& Value)
    {
        FTCHARToUTF8 Converter(*Value);
        TArray<uint8> Bytes;
        Bytes.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
        return Bytes;
    }

    FString ReplaceKnownPath(FString Value, const FString& Path, const TCHAR* Replacement, int32& Count)
    {
        if (Path.IsEmpty()) return Value;
        FString Normalized = FPaths::ConvertRelativePathToFull(Path);
        FPaths::NormalizeDirectoryName(Normalized);
        const int32 BeforeLength = Value.Len();
        Value.ReplaceInline(*Normalized, Replacement, ESearchCase::IgnoreCase);
        FString BackslashPath = Normalized.Replace(TEXT("/"), TEXT("\\"));
        Value.ReplaceInline(*BackslashPath, Replacement, ESearchCase::IgnoreCase);
        if (Value.Len() != BeforeLength) ++Count;
        return Value;
    }

    FString ReplaceRegexMatches(const FString& Input, const FRegexPattern& Pattern, const FString& Replacement, int32& Count)
    {
        FRegexMatcher Matcher(Pattern, Input);
        FString Output;
        int32 Cursor = 0;
        while (Matcher.FindNext())
        {
            Output += Input.Mid(Cursor, Matcher.GetMatchBeginning() - Cursor);
            Output += Replacement;
            Cursor = Matcher.GetMatchEnding();
            ++Count;
        }
        if (Cursor == 0) return Input;
        Output += Input.Mid(Cursor);
        return Output;
    }

    FString LatestProjectLog()
    {
        TArray<FString> Files;
        IFileManager::Get().FindFiles(Files, *FPaths::Combine(FPaths::ProjectLogDir(), TEXT("*.log")), true, false);
        FString Latest;
        FDateTime LatestTime = FDateTime::MinValue();
        for (const FString& File : Files)
        {
            const FString FullPath = FPaths::Combine(FPaths::ProjectLogDir(), File);
            const FDateTime Time = IFileManager::Get().GetTimeStamp(*FullPath);
            if (Latest.IsEmpty() || Time > LatestTime)
            {
                Latest = FullPath;
                LatestTime = Time;
            }
        }
        return Latest;
    }

    FString BuildLogExcerpt(const FString& SourceLogPath, int32& OutLines)
    {
        OutLines = 0;
        if (SourceLogPath.IsEmpty()) return FString();
        FString Log;
        if (!FFileHelper::LoadFileToString(Log, *SourceLogPath)) return FString();
        TArray<FString> Lines;
        Log.ParseIntoArrayLines(Lines, false);
        TArray<FString> Selected;
        for (int32 Index = Lines.Num() - 1; Index >= 0 && Selected.Num() < 400; --Index)
        {
            const FString& Line = Lines[Index];
            if (Line.Contains(TEXT("TraceMotive"), ESearchCase::IgnoreCase) ||
                Line.Contains(TEXT("Error:"), ESearchCase::IgnoreCase) ||
                Line.Contains(TEXT("Warning:"), ESearchCase::IgnoreCase) ||
                Line.Contains(TEXT("Blueprint"), ESearchCase::IgnoreCase) ||
                Line.Contains(TEXT("Ensure condition failed"), ESearchCase::IgnoreCase))
            {
                Selected.Add(Line.Left(4096));
            }
        }
        Algo::Reverse(Selected);
        OutLines = Selected.Num();
        return FString::Join(Selected, TEXT("\n"));
    }

    FString JsonString(const TSharedRef<FJsonObject>& Object)
    {
        FString Json;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
        FJsonSerializer::Serialize(Object, Writer);
        return Json;
    }

    TArray<TSharedPtr<FJsonValue>> JsonArray(const TArray<FString>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        for (const FString& Value : Values) Result.Add(MakeShared<FJsonValueString>(Value));
        return Result;
    }

    FBundleContent BuildContent(const FTMSupportBundleInput& Input)
    {
        FBundleContent Content;
        Content.Preview.SourceLog = LatestProjectLog();

        int32 Redactions = 0;
        FString LogExcerpt = BuildLogExcerpt(Content.Preview.SourceLog, Content.Preview.LogLinesIncluded);
        LogExcerpt = TMSupportBundle::RedactSensitiveText(LogExcerpt, &Redactions);
        Content.Preview.bHasLogExcerpt = !LogExcerpt.IsEmpty();

        TSharedRef<FJsonObject> Metadata = MakeShared<FJsonObject>();
        Metadata->SetStringField(TEXT("generatedUtc"), FDateTime::UtcNow().ToIso8601());
        Metadata->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
        Metadata->SetStringField(TEXT("platform"), FPlatformProperties::IniPlatformName());
        Metadata->SetStringField(TEXT("osVersion"), FPlatformMisc::GetOSVersion());
        Metadata->SetStringField(TEXT("projectName"), FApp::GetProjectName());
        if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("TraceMotive")))
        {
            Metadata->SetStringField(TEXT("pluginVersion"), Plugin->GetDescriptor().VersionName);
        }
        Metadata->SetBoolField(TEXT("logExcerptIncluded"), Content.Preview.bHasLogExcerpt);
        Metadata->SetNumberField(TEXT("logLinesIncluded"), Content.Preview.LogLinesIncluded);

        TSharedRef<FJsonObject> SettingsJson = MakeShared<FJsonObject>();
        if (const UTraceMotiveSettings* Settings = UTraceMotiveSettings::Get())
        {
            SettingsJson->SetBoolField(TEXT("largeProjectSafeMode"), Settings->bLargeProjectSafeMode);
            SettingsJson->SetNumberField(TEXT("searchBudgetScale"), Settings->SearchBudgetScale);
            SettingsJson->SetNumberField(TEXT("maxSearchResults"), Settings->MaxSearchResults);
            SettingsJson->SetNumberField(TEXT("maxAssetUsageFindings"), Settings->MaxAssetUsageFindings);
            SettingsJson->SetNumberField(TEXT("maxSnapshotFields"), Settings->MaxSnapshotFields);
            SettingsJson->SetNumberField(TEXT("pieEventHistoryLimit"), Settings->PIEEventHistoryLimit);
            SettingsJson->SetBoolField(TEXT("autoSaveInvestigations"), Settings->bAutoSaveInvestigations);
        }

        const FString SafeName = TMSupportBundle::RedactSensitiveText(Input.InvestigationName, &Redactions);
        const FString SafeScenario = TMSupportBundle::RedactSensitiveText(Input.Scenario, &Redactions);
        const FString SafeNotes = TMSupportBundle::RedactSensitiveText(Input.Notes, &Redactions);
        const FString SafeTarget = TMSupportBundle::RedactSensitiveText(Input.Target, &Redactions);
        TArray<FString> SafeHistory;
        for (const FString& Value : Input.History) SafeHistory.Add(TMSupportBundle::RedactSensitiveText(Value, &Redactions));
        TArray<FString> SafeEvidence;
        for (const FString& Value : Input.Evidence) SafeEvidence.Add(TMSupportBundle::RedactSensitiveText(Value, &Redactions));
        TArray<FString> SafeDiff;
        for (const FString& Value : Input.SnapshotDiff) SafeDiff.Add(TMSupportBundle::RedactSensitiveText(Value, &Redactions));

        TSharedRef<FJsonObject> Investigation = MakeShared<FJsonObject>();
        Investigation->SetStringField(TEXT("id"), Input.InvestigationId);
        Investigation->SetStringField(TEXT("name"), SafeName);
        Investigation->SetStringField(TEXT("scenario"), SafeScenario);
        Investigation->SetStringField(TEXT("notes"), SafeNotes);
        Investigation->SetStringField(TEXT("target"), SafeTarget);
        Investigation->SetStringField(TEXT("createdUtc"), Input.CreatedUtc);
        Investigation->SetStringField(TEXT("updatedUtc"), Input.UpdatedUtc);
        Investigation->SetNumberField(TEXT("beforeFieldCount"), Input.BeforeFieldCount);
        Investigation->SetNumberField(TEXT("afterFieldCount"), Input.AfterFieldCount);
        Investigation->SetArrayField(TEXT("history"), JsonArray(SafeHistory));
        Investigation->SetArrayField(TEXT("evidence"), JsonArray(SafeEvidence));
        Investigation->SetArrayField(TEXT("snapshotDiff"), JsonArray(SafeDiff));

        const FString EvidenceMarkdown = SafeEvidence.IsEmpty() ? TEXT("(none)") : TEXT("- ") + FString::Join(SafeEvidence, TEXT("\n- "));
        const FString DiffMarkdown = SafeDiff.IsEmpty() ? TEXT("(none)") : FString::Join(SafeDiff, TEXT("\n"));
        FString Markdown = FString::Printf(
            TEXT("# TraceMotive Support Bundle\n\n## Investigation\n\n- Name: %s\n- Scenario: %s\n- Target: %s\n- Created: %s\n- Updated: %s\n\n## Reproduction notes\n\n%s\n\n## Evidence\n\n%s\n\n## Before / After diff\n\n%s\n"),
            *SafeName, *SafeScenario, SafeTarget.IsEmpty() ? TEXT("(not selected)") : *SafeTarget,
            *Input.CreatedUtc, *Input.UpdatedUtc, SafeNotes.IsEmpty() ? TEXT("(none)") : *SafeNotes,
            *EvidenceMarkdown, *DiffMarkdown);

        Content.Entries.Add(TEXT("README.md"),
            TEXT("TraceMotive support bundle generated locally. Review every file before sharing.\n")
            TEXT("Known project/user paths, email addresses, and common secret assignments are redacted.\n")
            TEXT("No file is uploaded automatically.\n"));
        Content.Entries.Add(TEXT("investigation.md"), Markdown);
        Content.Entries.Add(TEXT("investigation.json"), JsonString(Investigation));
        Content.Entries.Add(TEXT("metadata.json"), JsonString(Metadata));
        Content.Entries.Add(TEXT("settings.json"), JsonString(SettingsJson));
        if (Content.Preview.bHasLogExcerpt) Content.Entries.Add(TEXT("editor-log-excerpt.txt"), LogExcerpt);

        Content.Preview.RedactionCount = Redactions;
        TArray<FString> Names;
        Content.Entries.GetKeys(Names);
        Names.Sort();
        Content.Preview.Text = FString::Printf(
            TEXT("Files to include (%d):\n• %s\n\nLog excerpt: %d relevant line(s)\nSensitive values redacted: %d\nOutput: <Project>/Saved/TraceMotive/SupportBundles\n\nNothing is uploaded automatically. Review the ZIP before sharing."),
            Names.Num(), *FString::Join(Names, TEXT("\n• ")), Content.Preview.LogLinesIncluded, Content.Preview.RedactionCount);
        return Content;
    }
}

namespace TMSupportBundle
{
    FString RedactSensitiveText(const FString& Input, int32* OutRedactionCount)
    {
        int32 Count = 0;
        FString Result = Input;
        Result = ReplaceKnownPath(MoveTemp(Result), FPaths::ProjectDir(), TEXT("<PROJECT_ROOT>"), Count);
        Result = ReplaceKnownPath(MoveTemp(Result), FPaths::ProjectSavedDir(), TEXT("<PROJECT_SAVED>"), Count);
        Result = ReplaceKnownPath(MoveTemp(Result), FPaths::EngineDir(), TEXT("<ENGINE_ROOT>"), Count);
        Result = ReplaceKnownPath(MoveTemp(Result), FPlatformProcess::UserDir(), TEXT("<USER_HOME>"), Count);
        Result = ReplaceRegexMatches(Result, FRegexPattern(TEXT("(?i)[A-Z]:[\\\\/]+Users[\\\\/]+[^\\\\/\\s]+")), TEXT("<USER_HOME>"), Count);
        Result = ReplaceRegexMatches(Result, FRegexPattern(TEXT("(?i)/(Users|home)/[^/\\s]+")), TEXT("<USER_HOME>"), Count);
        Result = ReplaceRegexMatches(Result, FRegexPattern(TEXT("[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}")), TEXT("<EMAIL>"), Count);
        Result = ReplaceRegexMatches(Result, FRegexPattern(TEXT("(?i)(password|passwd|api[_-]?key|access[_-]?token|authorization)[ \\t]*[:=][ \\t]*[^\\s,;]+")), TEXT("<REDACTED_SECRET>"), Count);
        if (OutRedactionCount) *OutRedactionCount += Count;
        return Result;
    }

    FTMSupportBundlePreview BuildPreview(const FTMSupportBundleInput& Input)
    {
        return BuildContent(Input).Preview;
    }

    bool WriteStoredZip(const FString& OutputPath, const TMap<FString, FString>& Utf8TextEntries, FString& OutError)
    {
        struct FCentralEntry
        {
            TArray<uint8> Name;
            uint32 Crc = 0;
            uint32 Size = 0;
            uint32 Offset = 0;
        };

        TArray<FString> Names;
        Utf8TextEntries.GetKeys(Names);
        Names.Sort();
        if (Names.Num() > MAX_uint16)
        {
            OutError = TEXT("Too many files for a ZIP support bundle.");
            return false;
        }

        TArray<uint8> Archive;
        TArray<FCentralEntry> Central;
        for (const FString& NameString : Names)
        {
            const FString* TextValue = Utf8TextEntries.Find(NameString);
            if (!TextValue) continue;
            const TArray<uint8> Name = ToUtf8(NameString);
            const TArray<uint8> Data = ToUtf8(*TextValue);
            if (Name.Num() > MAX_uint16 || static_cast<uint64>(Data.Num()) > MAX_uint32 || static_cast<uint64>(Archive.Num()) > MAX_uint32)
            {
                OutError = TEXT("A support bundle entry exceeds the standard ZIP size limit.");
                return false;
            }
            FCentralEntry Entry;
            Entry.Name = Name;
            Entry.Size = Data.Num();
            Entry.Crc = FCrc::MemCrc32(Data.GetData(), Data.Num());
            Entry.Offset = Archive.Num();
            AppendUInt32(Archive, 0x04034b50);
            AppendUInt16(Archive, 20);
            AppendUInt16(Archive, 0x0800);
            AppendUInt16(Archive, 0);
            AppendUInt16(Archive, 0);
            AppendUInt16(Archive, 0);
            AppendUInt32(Archive, Entry.Crc);
            AppendUInt32(Archive, Entry.Size);
            AppendUInt32(Archive, Entry.Size);
            AppendUInt16(Archive, Name.Num());
            AppendUInt16(Archive, 0);
            AppendBytes(Archive, Name);
            AppendBytes(Archive, Data);
            Central.Add(MoveTemp(Entry));
        }

        const uint32 CentralOffset = Archive.Num();
        for (const FCentralEntry& Entry : Central)
        {
            AppendUInt32(Archive, 0x02014b50);
            AppendUInt16(Archive, 20);
            AppendUInt16(Archive, 20);
            AppendUInt16(Archive, 0x0800);
            AppendUInt16(Archive, 0);
            AppendUInt16(Archive, 0);
            AppendUInt16(Archive, 0);
            AppendUInt32(Archive, Entry.Crc);
            AppendUInt32(Archive, Entry.Size);
            AppendUInt32(Archive, Entry.Size);
            AppendUInt16(Archive, Entry.Name.Num());
            AppendUInt16(Archive, 0);
            AppendUInt16(Archive, 0);
            AppendUInt16(Archive, 0);
            AppendUInt16(Archive, 0);
            AppendUInt32(Archive, 0);
            AppendUInt32(Archive, Entry.Offset);
            AppendBytes(Archive, Entry.Name);
        }
        const uint32 CentralSize = Archive.Num() - CentralOffset;
        AppendUInt32(Archive, 0x06054b50);
        AppendUInt16(Archive, 0);
        AppendUInt16(Archive, 0);
        AppendUInt16(Archive, Central.Num());
        AppendUInt16(Archive, Central.Num());
        AppendUInt32(Archive, CentralSize);
        AppendUInt32(Archive, CentralOffset);
        AppendUInt16(Archive, 0);

        IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), true);
        if (!FFileHelper::SaveArrayToFile(Archive, *OutputPath))
        {
            OutError = FString::Printf(TEXT("Could not write %s"), *OutputPath);
            return false;
        }
        return true;
    }

    FTMSupportBundleResult Export(const FTMSupportBundleInput& Input)
    {
        FTMSupportBundleResult Result;
        FBundleContent Content = BuildContent(Input);
        Result.Preview = Content.Preview;
        FString SafeBase = Input.InvestigationName.IsEmpty() ? TEXT("Investigation") : Input.InvestigationName;
        SafeBase = FPaths::MakeValidFileName(SafeBase).Left(48);
        const FString Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"));
        Result.BundlePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TraceMotive"), TEXT("SupportBundles"),
            FString::Printf(TEXT("TraceMotive-Support-%s-%s.zip"), *SafeBase, *Timestamp));
        Result.bSuccess = WriteStoredZip(Result.BundlePath, Content.Entries, Result.Error);
        return Result;
    }
}
