#pragma once

#include "CoreMinimal.h"

struct FTMSupportBundleInput
{
    FString InvestigationId;
    FString InvestigationName;
    FString Scenario;
    FString Notes;
    FString Target;
    FString CreatedUtc;
    FString UpdatedUtc;
    TArray<FString> History;
    TArray<FString> Evidence;
    TArray<FString> SnapshotDiff;
    int32 BeforeFieldCount = 0;
    int32 AfterFieldCount = 0;
};

struct FTMSupportBundlePreview
{
    FString Text;
    FString SourceLog;
    int32 LogLinesIncluded = 0;
    int32 RedactionCount = 0;
    bool bHasLogExcerpt = false;
};

struct FTMSupportBundleResult
{
    bool bSuccess = false;
    FString BundlePath;
    FString Error;
    FTMSupportBundlePreview Preview;
};

namespace TMSupportBundle
{
    /** Removes known local roots, user-home paths, email addresses, and common secret assignments. */
    FString RedactSensitiveText(const FString& Input, int32* OutRedactionCount = nullptr);

    /** Describes exactly what an export would contain without writing any files. */
    FTMSupportBundlePreview BuildPreview(const FTMSupportBundleInput& Input);

    /** Writes a self-contained, standard ZIP under Project/Saved/TraceMotive/SupportBundles. */
    FTMSupportBundleResult Export(const FTMSupportBundleInput& Input);

    /** Low-level stored ZIP writer exposed for deterministic automation coverage. */
    bool WriteStoredZip(const FString& OutputPath, const TMap<FString, FString>& Utf8TextEntries, FString& OutError);
}
