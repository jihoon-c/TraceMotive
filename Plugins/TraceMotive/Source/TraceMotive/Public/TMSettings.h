#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TMSettings.generated.h"

UENUM()
enum class ETMDefaultReportFormat : uint8
{
    Markdown,
    CSV,
    JSON,
    Mermaid
};

/** Project-local defaults for TraceMotive diagnostics and performance safety. */
UCLASS(Config=EditorPerProjectUserSettings, DefaultConfig, meta=(DisplayName="TraceMotive"))
class TRACEMOTIVE_API UTraceMotiveSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UTraceMotiveSettings();

    virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
    virtual FText GetSectionText() const override;
    virtual FText GetSectionDescription() const override;

    UPROPERTY(Config, EditAnywhere, Category="Performance", meta=(DisplayName="Large Project Safe Mode", ToolTip="Keeps background work conservative and prevents Search Boost from multiplying the configured per-tick budget."))
    bool bLargeProjectSafeMode = true;

    UPROPERTY(Config, EditAnywhere, Category="Performance", meta=(ClampMin="0.25", ClampMax="4.0", UIMin="0.25", UIMax="4.0", DisplayName="Search Work Budget Scale"))
    float SearchBudgetScale = 1.0f;

    UPROPERTY(Config, EditAnywhere, Category="Limits", meta=(ClampMin="50", ClampMax="10000", UIMin="50", UIMax="2000"))
    int32 MaxSearchResults = 500;

    UPROPERTY(Config, EditAnywhere, Category="Limits", meta=(ClampMin="100", ClampMax="50000", UIMin="100", UIMax="10000"))
    int32 MaxAssetUsageFindings = 5000;

    UPROPERTY(Config, EditAnywhere, Category="Limits", meta=(ClampMin="100", ClampMax="20000", UIMin="100", UIMax="10000"))
    int32 MaxSnapshotFields = 5000;

    UPROPERTY(Config, EditAnywhere, Category="Runtime Capture", meta=(ClampMin="20", ClampMax="5000", UIMin="20", UIMax="1000", DisplayName="PIE Event History Limit"))
    int32 PIEEventHistoryLimit = 250;

    UPROPERTY(Config, EditAnywhere, Category="Reports")
    ETMDefaultReportFormat DefaultReportFormat = ETMDefaultReportFormat::Markdown;

    UPROPERTY(Config, EditAnywhere, Category="Reports", meta=(DisplayName="Report Folder Under Project/Saved", ToolTip="A relative folder only. Invalid or absolute paths fall back to TraceMotiveReports."))
    FString ReportSubdirectory = TEXT("TraceMotiveReports");

    UPROPERTY(Config, EditAnywhere, Category="Investigation")
    bool bAutoSaveInvestigations = true;

    static const UTraceMotiveSettings* Get();
};
