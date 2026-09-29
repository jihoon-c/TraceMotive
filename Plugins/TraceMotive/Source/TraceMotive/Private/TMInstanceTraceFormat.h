#pragma once



#include "CoreMinimal.h"



namespace TMInstanceTraceFormat

{

    FString ClassifyStateChange(const FString& StateKey, const FString& Reason);

    FString BuildConfidenceText(const FString& Reason, const FString& CallerSummary);

    FString ExtractTraceToken(const FString& Source, const FString& Token);

    FString ExtractStatePropertyName(const FString& StateKey);

    bool IsTraceValueTrue(const FString& Value);

    bool IsTraceValueFalse(const FString& Value);

    bool IsVisibilityOffValue(const FString& StateKey, const FString& NewValue);

    FString FormatTraceValueChange(const FString& OldValue, const FString& NewValue);

    FString BuildActionSummaryText(const FString& StateKey, const FString& OldValue, const FString& NewValue, const FString& Reason, const FString& CallerSummary);

    FString BuildActionSourceText(const FString& Reason, const FString& CallerSummary);

    FString BuildActionImpactText(const FString& StateKey, const FString& OldValue, const FString& NewValue);

    FString BuildDiagnosticText(const FString& StateKey, const FString& OldValue, const FString& NewValue, const FString& Reason, const FString& CallerSummary);

    FLinearColor GetEventAccentColor(const FString& Category);

    FLinearColor GetTraceSelectionAccentColor();

    bool IsUnresolvedCallerSummary(const FString& CallerSummary);

    bool HasRuntimeBlueprintExecutionSource(const FString& CallerSummary);

}

