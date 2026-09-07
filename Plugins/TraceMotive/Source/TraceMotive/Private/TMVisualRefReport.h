#pragma once



#include "CoreMinimal.h"

#include "VisualRefSearcher.h"



class UEdGraph;

struct FVisualReferenceInfo;



enum class EVisualRefExportFormat : uint8

{

    Markdown,

    CSV,

    JSON,

    Mermaid

};



namespace TMVisualRefReport

{

    FString GetExportFormatLabel(EVisualRefExportFormat Format);

    FString GetSearchScopeLabel(EVisualRefSearchScope Scope);

    FString GetReferenceKind(const FVisualReferenceInfo& Info, bool bIsFunction, bool bIsDispatcher);

    FString BuildReport(UEdGraph* Graph, const FString& TargetName, bool bIsFunction, bool bIsDispatcher, EVisualRefSearchScope SearchScope, EVisualRefExportFormat Format);

    bool SaveReport(UEdGraph* Graph, const FString& TargetName, bool bIsFunction, bool bIsDispatcher, EVisualRefSearchScope SearchScope, EVisualRefExportFormat Format, FString& OutFilePath);

}

