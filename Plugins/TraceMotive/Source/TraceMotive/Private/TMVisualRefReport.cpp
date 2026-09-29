#include "TMVisualRefReport.h"
#include "TMSettings.h"

#include "TMReportFormatter.h"



#include "HAL/FileManager.h"

#include "Misc/DateTime.h"

#include "Misc/FileHelper.h"

#include "Misc/Paths.h"

#include "VisualRefGraphDefinition.h"



namespace

{

    FString CleanReportText(FString Value)

    {

        Value.ReplaceInline(TEXT("\r"), TEXT(" "));

        Value.ReplaceInline(TEXT("\n"), TEXT(" "));

        Value.TrimStartAndEndInline();

        return Value;

    }



    FString EscapeCsv(FString Value)

    {

        Value = CleanReportText(Value);

        Value.ReplaceInline(TEXT("\""), TEXT("\"\""));

        return FString::Printf(TEXT("\"%s\""), *Value);

    }



    FString EscapeJson(FString Value)

    {

        Value.ReplaceInline(TEXT("\\"), TEXT("\\\\"));

        Value.ReplaceInline(TEXT("\""), TEXT("\\\""));

        Value.ReplaceInline(TEXT("\r"), TEXT("\\r"));

        Value.ReplaceInline(TEXT("\n"), TEXT("\\n"));

        Value.ReplaceInline(TEXT("\t"), TEXT("\\t"));

        return Value;

    }



    FString EscapeMarkdown(FString Value)

    {

        Value = CleanReportText(Value);

        Value.ReplaceInline(TEXT("|"), TEXT("\\|"));

        return Value;

    }



    FString EscapeMermaidLabel(FString Value)

    {

        Value = CleanReportText(Value);

        Value.ReplaceInline(TEXT("\""), TEXT("'"));

        Value.ReplaceInline(TEXT("["), TEXT("("));

        Value.ReplaceInline(TEXT("]"), TEXT(")"));

        return Value;

    }



    TArray<UVisualRefNode*> CollectSortedVisualNodes(UEdGraph* Graph)

    {

        TArray<UVisualRefNode*> VisualNodes;

        if (Graph)

        {

            for (UEdGraphNode* Node : Graph->Nodes)

            {

                if (UVisualRefNode* VisualNode = Cast<UVisualRefNode>(Node))

                {

                    VisualNodes.Add(VisualNode);

                }

            }

        }



        VisualNodes.Sort([](const UVisualRefNode& A, const UVisualRefNode& B)

        {

            return A.GetNodeTitle(ENodeTitleType::ListView).ToString() < B.GetNodeTitle(ENodeTitleType::ListView).ToString();

        });



        return VisualNodes;

    }



    FString GetVisualRefConfidenceText()

    {

        return TEXT("Medium");

    }



    FString GetVisualRefLimitationsText()

    {

        return TEXT("Soft references, data-table paths, reflection by string, native C++ references, and skipped budget slices may be incomplete.");

    }



    FString BuildVisualRefSafetyMarkdown()

    {

        const TMReportFormatter::FDiagnosticSafetySection SafetySection = TMReportFormatter::BuildDefaultDiagnosticSafetySection(TEXT("Visual Reference Viewer"));

        return FString::Printf(TEXT("## Diagnostic Safety\n%s\n"), *TMReportFormatter::BuildDiagnosticSafetyBlock(SafetySection));

    }

}



FString TMVisualRefReport::GetExportFormatLabel(EVisualRefExportFormat Format)

{

    switch (Format)

    {

    case EVisualRefExportFormat::CSV:

        return TEXT("CSV");

    case EVisualRefExportFormat::JSON:

        return TEXT("JSON");

    case EVisualRefExportFormat::Mermaid:

        return TEXT("Mermaid");

    default:

        return TEXT("Markdown");

    }

}



FString TMVisualRefReport::GetSearchScopeLabel(EVisualRefSearchScope Scope)

{

    switch (Scope)

    {

    case EVisualRefSearchScope::CurrentBlueprint:

        return TEXT("Current Blueprint");

    case EVisualRefSearchScope::SameFolder:

        return TEXT("Same Folder");

    default:

        return TEXT("Project Content");

    }

}



FString TMVisualRefReport::GetReferenceKind(const FVisualReferenceInfo& Info, bool bIsFunction, bool bIsDispatcher)

{

    if (bIsDispatcher)

    {

        if (Info.MatchReason.Contains(TEXT("Call"))) return TEXT("Call");

        if (Info.MatchReason.Contains(TEXT("Assign"))) return TEXT("Assign");

        if (Info.MatchReason.Contains(TEXT("Unbind all"))) return TEXT("Unbind all");

        if (Info.MatchReason.Contains(TEXT("Unbind"))) return TEXT("Unbind");

        if (Info.MatchReason.Contains(TEXT("Bind"))) return TEXT("Bind");

        if (Info.MatchReason.Contains(TEXT("Event"))) return TEXT("Event");

        return TEXT("Reference");

    }



    if (bIsFunction)

    {

        if (Info.bIsFunctionDefinition) return TEXT("Definition");

        if (Info.bIsSearchSubject) return TEXT("Search Subject");

        return TEXT("Call");

    }



    return Info.bIsSetter ? TEXT("Set") : TEXT("Get");

}



FString TMVisualRefReport::BuildReport(

    UEdGraph* Graph,

    const FString& TargetName,

    bool bIsFunction,

    bool bIsDispatcher,

    EVisualRefSearchScope SearchScope,

    EVisualRefExportFormat Format)

{

    const FString TargetType = bIsDispatcher ? TEXT("Dispatcher") : (bIsFunction ? TEXT("Function") : TEXT("Variable"));

    const FString ScopeLabel = GetSearchScopeLabel(SearchScope);

    const FString GeneratedAt = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));

    const TArray<UVisualRefNode*> VisualNodes = CollectSortedVisualNodes(Graph);



    if (Format == EVisualRefExportFormat::CSV)

    {

        FString Output = TEXT("Target,Type,Scope,Asset,Graph,Node,Kind,CallOrder,TargetObject,TargetClass,MatchReason,IsSource,Confidence,Limitations\n");

        for (const UVisualRefNode* VisualNode : VisualNodes)

        {

            const FString AssetName = CleanReportText(VisualNode->AssetName.ToString());

            for (const FVisualReferenceInfo& Ref : VisualNode->References)

            {

                TArray<FString> Columns;

                Columns.Add(EscapeCsv(TargetName));

                Columns.Add(EscapeCsv(TargetType));

                Columns.Add(EscapeCsv(ScopeLabel));

                Columns.Add(EscapeCsv(AssetName));

                Columns.Add(EscapeCsv(Ref.GraphName));

                Columns.Add(EscapeCsv(Ref.NodeName));

                Columns.Add(EscapeCsv(GetReferenceKind(Ref, bIsFunction, bIsDispatcher)));

                Columns.Add(FString::FromInt(Ref.CallOrder + 1));

                Columns.Add(EscapeCsv(Ref.TargetObjectName));

                Columns.Add(EscapeCsv(Ref.TargetClassName));

                Columns.Add(EscapeCsv(Ref.MatchReason));

                Columns.Add(Ref.bIsSource ? TEXT("true") : TEXT("false"));

                Columns.Add(EscapeCsv(GetVisualRefConfidenceText()));

                Columns.Add(EscapeCsv(GetVisualRefLimitationsText()));

                Output += FString::Join(Columns, TEXT(",")) + TEXT("\n");

            }

        }

        return Output;

    }



    if (Format == EVisualRefExportFormat::JSON)

    {

        FString Output;

        Output += TEXT("{\n");

        Output += FString::Printf(TEXT("  \"target\": { \"name\": \"%s\", \"type\": \"%s\", \"scope\": \"%s\", \"generatedAt\": \"%s\" },\n"),

            *EscapeJson(TargetName), *EscapeJson(TargetType), *EscapeJson(ScopeLabel), *EscapeJson(GeneratedAt));

        Output += FString::Printf(TEXT("  \"diagnosticSafety\": { \"confidence\": \"%s\", \"limitations\": \"%s\" },\n"),

            *EscapeJson(GetVisualRefConfidenceText()), *EscapeJson(GetVisualRefLimitationsText()));

        Output += TEXT("  \"references\": [\n");



        bool bFirst = true;

        for (const UVisualRefNode* VisualNode : VisualNodes)

        {

            const FString AssetName = CleanReportText(VisualNode->AssetName.ToString());

            for (const FVisualReferenceInfo& Ref : VisualNode->References)

            {

                if (!bFirst)

                {

                    Output += TEXT(",\n");

                }

                bFirst = false;



                Output += FString::Printf(TEXT("    { \"asset\": \"%s\", \"graph\": \"%s\", \"node\": \"%s\", \"kind\": \"%s\", \"callOrder\": %d, \"targetObject\": \"%s\", \"targetClass\": \"%s\", \"matchReason\": \"%s\", \"isSource\": %s }"),

                    *EscapeJson(AssetName),

                    *EscapeJson(Ref.GraphName),

                    *EscapeJson(Ref.NodeName),

                    *EscapeJson(GetReferenceKind(Ref, bIsFunction, bIsDispatcher)),

                    Ref.CallOrder + 1,

                    *EscapeJson(Ref.TargetObjectName),

                    *EscapeJson(Ref.TargetClassName),

                    *EscapeJson(Ref.MatchReason),

                    Ref.bIsSource ? TEXT("true") : TEXT("false"));

            }

        }



        Output += TEXT("\n  ]\n}");

        return Output;

    }



    if (Format == EVisualRefExportFormat::Mermaid)

    {

        FString Output = TEXT("%% Diagnostic Safety: Confidence=Medium. Limitations: Soft references, data-table paths, reflection by string, native C++ references, and budget-skipped scan slices may be incomplete.\n");

        Output += TEXT("graph LR\n");

        Output += FString::Printf(TEXT("  Target[\"%s: %s\"]\n"), *EscapeMermaidLabel(TargetType), *EscapeMermaidLabel(TargetName));



        int32 AssetIndex = 0;

        int32 RefIndex = 0;

        for (const UVisualRefNode* VisualNode : VisualNodes)

        {

            const FString AssetId = FString::Printf(TEXT("Asset%d"), AssetIndex++);

            const FString AssetName = CleanReportText(VisualNode->AssetName.ToString());

            Output += FString::Printf(TEXT("  %s[\"%s\"]\n"), *AssetId, *EscapeMermaidLabel(AssetName));

            Output += FString::Printf(TEXT("  Target --> %s\n"), *AssetId);



            for (const FVisualReferenceInfo& Ref : VisualNode->References)

            {

                const FString RefId = FString::Printf(TEXT("Ref%d"), RefIndex++);

                const FString RefLabel = FString::Printf(TEXT("%s | %s | %s"), *Ref.GraphName, *GetReferenceKind(Ref, bIsFunction, bIsDispatcher), *Ref.NodeName);

                Output += FString::Printf(TEXT("  %s[\"%s\"]\n"), *RefId, *EscapeMermaidLabel(RefLabel));

                Output += FString::Printf(TEXT("  %s --> %s\n"), *AssetId, *RefId);

            }

        }

        return Output;

    }



    FString Output;

    Output += TEXT("# TraceMotive — Debug Pathfinder for Unreal Report\n\n");

    Output += FString::Printf(TEXT("- Target: `%s`\n"), *TargetName);

    Output += FString::Printf(TEXT("- Type: `%s`\n"), *TargetType);

    Output += FString::Printf(TEXT("- Scope: `%s`\n"), *ScopeLabel);

    Output += FString::Printf(TEXT("- Generated: `%s`\n\n"), *GeneratedAt);



    Output += BuildVisualRefSafetyMarkdown();

    Output += TEXT("## Summary\n\n");

    Output += TEXT("| Asset | References |\n| --- | ---: |\n");

    for (const UVisualRefNode* VisualNode : VisualNodes)

    {

        Output += FString::Printf(TEXT("| %s | %d |\n"), *EscapeMarkdown(VisualNode->AssetName.ToString()), VisualNode->References.Num());

    }



    Output += TEXT("\n## References\n\n");

    Output += TEXT("| Asset | Graph | Node | Kind | Target | Match Reason |\n| --- | --- | --- | --- | --- | --- |\n");

    for (const UVisualRefNode* VisualNode : VisualNodes)

    {

        const FString AssetName = EscapeMarkdown(VisualNode->AssetName.ToString());

        for (const FVisualReferenceInfo& Ref : VisualNode->References)

        {

            const FString TargetLabel = !Ref.TargetObjectName.IsEmpty() && !Ref.TargetClassName.IsEmpty()

                ? FString::Printf(TEXT("%s (%s)"), *Ref.TargetObjectName, *Ref.TargetClassName)

                : (!Ref.TargetObjectName.IsEmpty() ? Ref.TargetObjectName : Ref.TargetClassName);

            Output += FString::Printf(TEXT("| %s | %s | %s | %s | %s | %s |\n"),

                *AssetName,

                *EscapeMarkdown(Ref.GraphName),

                *EscapeMarkdown(Ref.NodeName),

                *EscapeMarkdown(GetReferenceKind(Ref, bIsFunction, bIsDispatcher)),

                *EscapeMarkdown(TargetLabel),

                *EscapeMarkdown(Ref.MatchReason));

        }

    }



    return Output;

}



bool TMVisualRefReport::SaveReport(

    UEdGraph* Graph,

    const FString& TargetName,

    bool bIsFunction,

    bool bIsDispatcher,

    EVisualRefSearchScope SearchScope,

    EVisualRefExportFormat Format,

    FString& OutFilePath)

{

    const FString Extension = Format == EVisualRefExportFormat::Markdown ? TEXT("md")

        : Format == EVisualRefExportFormat::CSV ? TEXT("csv")

        : Format == EVisualRefExportFormat::JSON ? TEXT("json")

        : TEXT("mmd");



    FString SafeTargetName = FPaths::MakeValidFileName(TargetName);

    FString ReportSubdirectory = UTraceMotiveSettings::Get()->ReportSubdirectory;
    ReportSubdirectory.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (ReportSubdirectory.IsEmpty() || !FPaths::IsRelative(ReportSubdirectory) || ReportSubdirectory.Contains(TEXT("..")))
    {
        ReportSubdirectory = TEXT("TraceMotiveReports");
    }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), ReportSubdirectory);

    IFileManager::Get().MakeDirectory(*Directory, true);



    OutFilePath = FPaths::Combine(Directory, FString::Printf(TEXT("%s_%s.%s"),

        *SafeTargetName,

        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")),

        *Extension));



    return FFileHelper::SaveStringToFile(

        BuildReport(Graph, TargetName, bIsFunction, bIsDispatcher, SearchScope, Format),

        *OutFilePath,

        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

}



