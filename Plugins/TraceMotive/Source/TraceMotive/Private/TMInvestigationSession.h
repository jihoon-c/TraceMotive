#pragma once

#include "CoreMinimal.h"

struct FTMSnapshotDiff
{
    int32 Added = 0;
    int32 Removed = 0;
    int32 Changed = 0;
    TArray<FString> Lines;
};

namespace TMInvestigationSession
{
    void RegisterTab();
    void UnregisterTab();
    void OpenWindow();

    /** Creates a persisted investigation and opens the first recommended tool. */
    void StartScenario(FName ScenarioId);

    /** Routes to another TraceMotive tool and records the handoff in the active investigation. */
    void OpenTool(FName ToolId, const FString& Context = FString());

    /** Records a concise finding that should survive editor restarts. */
    void RecordEvidence(const FString& SourceTool, const FString& Summary, const FString& Target = FString());

    FTMSnapshotDiff CompareSnapshots(const TMap<FString, FString>& Before, const TMap<FString, FString>& After);
}
