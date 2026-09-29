#include "TMPIEErrorLogAnalyzer.h"
#include "TMLocalization.h"

#include "TMReportFormatter.h"

#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"

namespace
{
    struct FPIEErrorLogFinding
    {
        FString ErrorMessage;
        FString InstanceName;
        FString ObjectPath;
        FString BlueprintName;
        FString FunctionName;
        FString GraphName;
        FString NodeName;
        FString SourceLogFile;
        FString RawLine;
        FString Confidence;
        int32 LineNumber = 0;
        int32 RepeatCount = 1;
    };

    using FPIEErrorLogFindingPtr = TSharedPtr<FPIEErrorLogFinding>;

    FString TrimToken(FString Value)
    {
        Value.TrimStartAndEndInline();
        Value.TrimQuotesInline();
        return Value;
    }

    FString ExtractAfterToken(const FString& Text, const FString& Token)
    {
        const int32 TokenIndex = Text.Find(Token, ESearchCase::IgnoreCase);
        if (TokenIndex == INDEX_NONE) return FString();

        FString Tail = Text.Mid(TokenIndex + Token.Len());
        Tail.TrimStartInline();
        if (Tail.StartsWith(TEXT(":")))
        {
            Tail.RightChopInline(1);
            Tail.TrimStartInline();
        }

        const bool bQuoted = Tail.StartsWith(TEXT("\"")) || Tail.StartsWith(TEXT("\'"));
        const TCHAR QuoteChar = bQuoted ? Tail[0] : TEXT('\0');
        if (bQuoted)
        {
            Tail.RightChopInline(1);
        }

        int32 EndIndex = INDEX_NONE;
        if (bQuoted)
        {
            Tail.FindChar(QuoteChar, EndIndex);
        }
        else
        {
            for (int32 Index = 0; Index < Tail.Len(); ++Index)
            {
                const TCHAR Ch = Tail[Index];
                if (Ch == TEXT(',') || Ch == TEXT('|') || Ch == TEXT('"') || Ch == TEXT('\''))
                {
                    EndIndex = Index;
                    break;
                }
            }
        }

        if (EndIndex != INDEX_NONE)
        {
            Tail.LeftInline(EndIndex);
        }
        return TrimToken(Tail);
    }

    FString ExtractBetween(const FString& Text, const FString& BeginToken, const FString& EndToken)
    {
        const int32 BeginIndex = Text.Find(BeginToken, ESearchCase::IgnoreCase);
        if (BeginIndex == INDEX_NONE) return FString();
        const int32 ValueStart = BeginIndex + BeginToken.Len();
        const int32 EndIndex = Text.Find(EndToken, ESearchCase::IgnoreCase, ESearchDir::FromStart, ValueStart);
        if (EndIndex == INDEX_NONE || EndIndex <= ValueStart) return FString();
        return TrimToken(Text.Mid(ValueStart, EndIndex - ValueStart));
    }

    FString ExtractLastObjectSegment(const FString& Path)
    {
        if (Path.IsEmpty()) return FString();
        FString Segment = Path;
        int32 DotIndex = INDEX_NONE;
        if (Segment.FindLastChar(TEXT('.'), DotIndex)) Segment = Segment.Mid(DotIndex + 1);
        int32 ColonIndex = INDEX_NONE;
        if (Segment.FindLastChar(TEXT(':'), ColonIndex)) Segment = Segment.Mid(ColonIndex + 1);
        return TrimToken(Segment);
    }

    FString ExtractLikelyObjectPath(const FString& Text)
    {
        int32 PathIndex = Text.Find(TEXT("/Game/"), ESearchCase::IgnoreCase);
        if (PathIndex == INDEX_NONE) PathIndex = Text.Find(TEXT("/Temp/"), ESearchCase::IgnoreCase);
        if (PathIndex == INDEX_NONE) return FString();
        FString Tail = Text.Mid(PathIndex);
        int32 EndIndex = Tail.Len();
        for (int32 Index = 0; Index < Tail.Len(); ++Index)
        {
            const TCHAR Ch = Tail[Index];
            if (FChar::IsWhitespace(Ch) || Ch == TEXT('\"') || Ch == TEXT('\'') || Ch == TEXT(',') || Ch == TEXT(')'))
            {
                EndIndex = Index;
                break;
            }
        }
        Tail.LeftInline(EndIndex);
        return TrimToken(Tail);
    }

    FString ExtractBlueprintClassName(const FString& Text)
    {
        const int32 SuffixIndex = Text.Find(TEXT("_C"), ESearchCase::CaseSensitive);
        if (SuffixIndex == INDEX_NONE) return FString();
        int32 StartIndex = SuffixIndex;
        while (StartIndex > 0)
        {
            const TCHAR Ch = Text[StartIndex - 1];
            if (!(FChar::IsAlnum(Ch) || Ch == TEXT('_'))) break;
            --StartIndex;
        }
        return TrimToken(Text.Mid(StartIndex, SuffixIndex + 2 - StartIndex));
    }

    FString ExtractFunctionFromScriptStackLine(const FString& Text)
    {
        FString Work = Text;
        Work.TrimStartAndEndInline();
        if (Work.IsEmpty()) return FString();
        int32 LastDot = INDEX_NONE;
        if (!Work.FindLastChar(TEXT('.'), LastDot)) return FString();
        FString FunctionPart = Work.Mid(LastDot + 1);
        int32 ParenIndex = INDEX_NONE;
        if (FunctionPart.FindChar(TEXT('('), ParenIndex)) FunctionPart.LeftInline(ParenIndex);
        return TrimToken(FunctionPart);
    }

    bool IsRuntimeErrorLine(const FString& Line)
    {
        return Line.Contains(TEXT("Blueprint Runtime Error"), ESearchCase::IgnoreCase)
            || Line.Contains(TEXT("Script Msg:"), ESearchCase::IgnoreCase)
            || (Line.Contains(TEXT("Accessed None"), ESearchCase::IgnoreCase) && Line.Contains(TEXT("Blueprint"), ESearchCase::IgnoreCase));
    }

    bool IsScriptStackLine(const FString& Line)
    {
        return Line.Contains(TEXT("Script Stack"), ESearchCase::IgnoreCase)
            || Line.Contains(TEXT("Function /"), ESearchCase::IgnoreCase)
            || Line.Contains(TEXT("PersistentLevel."), ESearchCase::IgnoreCase);
    }

    void ParseErrorContextLine(FPIEErrorLogFinding& Finding, const FString& Line)
    {
        if (Finding.NodeName.IsEmpty()) Finding.NodeName = ExtractAfterToken(Line, TEXT("Node:"));
        if (Finding.GraphName.IsEmpty()) Finding.GraphName = ExtractAfterToken(Line, TEXT("Graph:"));
        if (Finding.FunctionName.IsEmpty()) Finding.FunctionName = ExtractAfterToken(Line, TEXT("Function:"));
        if (Finding.BlueprintName.IsEmpty()) Finding.BlueprintName = ExtractAfterToken(Line, TEXT("Blueprint:"));

        const FString ObjectPath = ExtractLikelyObjectPath(Line);
        if (!ObjectPath.IsEmpty())
        {
            Finding.ObjectPath = ObjectPath;
            if (Finding.InstanceName.IsEmpty()) Finding.InstanceName = ExtractLastObjectSegment(ObjectPath);
        }

        const FString BlueprintClass = ExtractBlueprintClassName(Line);
        if (!BlueprintClass.IsEmpty() && Finding.BlueprintName.IsEmpty())
        {
            Finding.BlueprintName = BlueprintClass;
        }

        const FString StackFunction = ExtractFunctionFromScriptStackLine(Line);
        if (!StackFunction.IsEmpty() && Finding.FunctionName.IsEmpty())
        {
            Finding.FunctionName = StackFunction;
        }
    }

    void FinalizeFinding(FPIEErrorLogFinding& Finding)
    {
        if (Finding.InstanceName.IsEmpty() && !Finding.ObjectPath.IsEmpty())
        {
            Finding.InstanceName = ExtractLastObjectSegment(Finding.ObjectPath);
        }
        if (Finding.BlueprintName.IsEmpty())
        {
            Finding.BlueprintName = ExtractBlueprintClassName(Finding.RawLine);
        }
        if (!Finding.InstanceName.IsEmpty() && !Finding.FunctionName.IsEmpty())
        {
            Finding.Confidence = TEXT("Likely");
        }
        else if (!Finding.InstanceName.IsEmpty() || !Finding.FunctionName.IsEmpty())
        {
            Finding.Confidence = TEXT("Partial");
        }
        else
        {
            Finding.Confidence = TEXT("Unknown");
        }
    }

    TArray<FString> FindProjectLogFiles()
    {
        TArray<FString> Result;
        const FString LogDir = FPaths::ProjectLogDir();
        IFileManager::Get().FindFilesRecursive(Result, *LogDir, TEXT("*.log"), true, false, false);
        Result.Sort([](const FString& Left, const FString& Right)
        {
            const FDateTime LeftTime = IFileManager::Get().GetTimeStamp(*Left);
            const FDateTime RightTime = IFileManager::Get().GetTimeStamp(*Right);
            return LeftTime > RightTime;
        });
        return Result;
    }

    TArray<FPIEErrorLogFindingPtr> AnalyzeLogFile(const FString& LogFile)
    {
        TArray<FPIEErrorLogFindingPtr> Findings;
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *LogFile)) return Findings;

        TArray<FString> Lines;
        Text.ParseIntoArrayLines(Lines, false);
        TSharedPtr<FPIEErrorLogFinding> ActiveFinding;
        int32 ContextLinesRemaining = 0;

        for (int32 Index = 0; Index < Lines.Num(); ++Index)
        {
            const FString& Line = Lines[Index];
            if (IsRuntimeErrorLine(Line))
            {
                if (ActiveFinding.IsValid())
                {
                    FinalizeFinding(*ActiveFinding);
                    Findings.Add(ActiveFinding);
                }
                ActiveFinding = MakeShared<FPIEErrorLogFinding>();
                ActiveFinding->SourceLogFile = LogFile;
                ActiveFinding->LineNumber = Index + 1;
                ActiveFinding->RawLine = Line;
                ActiveFinding->ErrorMessage = Line;
                ActiveFinding->ErrorMessage.RemoveFromStart(TEXT("LogScript: Warning:"));
                ActiveFinding->ErrorMessage.TrimStartAndEndInline();
                ParseErrorContextLine(*ActiveFinding, Line);
                ContextLinesRemaining = 8;
                continue;
            }

            if (ActiveFinding.IsValid() && ContextLinesRemaining > 0)
            {
                if (IsScriptStackLine(Line) || Line.Contains(TEXT("Node:"), ESearchCase::IgnoreCase) || Line.Contains(TEXT("Function:"), ESearchCase::IgnoreCase))
                {
                    ParseErrorContextLine(*ActiveFinding, Line);
                    ActiveFinding->RawLine += LINE_TERMINATOR;
                    ActiveFinding->RawLine += Line;
                }
                --ContextLinesRemaining;
            }
        }

        if (ActiveFinding.IsValid())
        {
            FinalizeFinding(*ActiveFinding);
            Findings.Add(ActiveFinding);
        }

        TArray<FPIEErrorLogFindingPtr> GroupedFindings;
        TMap<FString, FPIEErrorLogFindingPtr> FindingBySignature;
        for (const FPIEErrorLogFindingPtr& Finding : Findings)
        {
            if (!Finding.IsValid()) continue;
            const FString Signature = FString::Printf(TEXT("%s|%s|%s|%s|%s"),
                *Finding->ErrorMessage, *Finding->InstanceName, *Finding->BlueprintName, *Finding->FunctionName, *Finding->NodeName);
            if (FPIEErrorLogFindingPtr* Existing = FindingBySignature.Find(Signature))
            {
                ++(*Existing)->RepeatCount;
                continue;
            }
            FindingBySignature.Add(Signature, Finding);
            GroupedFindings.Add(Finding);
        }
        return GroupedFindings;
    }

    class SPIEErrorLogAnalyzer : public SCompoundWidget
    {
    public:
        SLATE_BEGIN_ARGS(SPIEErrorLogAnalyzer) {}
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            RefreshAnalysis();
            ChildSlot
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
                .Padding(10.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()
                            [SNew(STextBlock).Text(TMLoc::Text(TEXT("Completed PIE Log Analysis"), TEXT("완료된 PIE 로그 분석"))).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14))]
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
                            [SNew(STextBlock).Text(this, &SPIEErrorLogAnalyzer::GetSummaryText).ColorAndOpacity(FLinearColor(0.68f, 0.72f, 0.78f))]
                        ]
                        + SHorizontalBox::Slot().AutoWidth().Padding(6, 0)
                        [SNew(SButton).Text(TMLoc::Text(TEXT("Analyze latest log"), TEXT("Analyze latest log"))).OnClicked(this, &SPIEErrorLogAnalyzer::OnAnalyzeLatestClicked)]
                        + SHorizontalBox::Slot().AutoWidth().Padding(6, 0)
                        [SNew(SButton).Text(TMLoc::Text(TEXT("Copy report"), TEXT("Copy report"))).OnClicked(this, &SPIEErrorLogAnalyzer::OnCopyReportClicked)]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8)
                    [SNew(STextBlock).Text(TMLoc::Text(TEXT("Reads project Saved/Logs after PIE and extracts instance/function candidates from Blueprint Runtime Error log blocks. Confidence is heuristic."), TEXT("Reads project Saved/Logs after PIE and extracts instance/function candidates from Blueprint Runtime Error log blocks. Confidence is heuristic."))).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f))]
                    + SVerticalBox::Slot().FillHeight(1.0f)
                    [
                        SAssignNew(ResultListView, SListView<FPIEErrorLogFindingPtr>)
                        .ListItemsSource(&Findings)
                        .OnGenerateRow(this, &SPIEErrorLogAnalyzer::GenerateRow)
                    ]
                ]
            ];
        }

        void RefreshFromHost()
        {
            RefreshAnalysis();
        }

    private:
        TArray<FPIEErrorLogFindingPtr> Findings;
        FString LastLogFile;
        TSharedPtr<SListView<FPIEErrorLogFindingPtr>> ResultListView;

        void RefreshAnalysis()
        {
            Findings.Reset();
            const TArray<FString> Logs = FindProjectLogFiles();
            if (Logs.Num() > 0)
            {
                LastLogFile = Logs[0];
                Findings = AnalyzeLogFile(LastLogFile);
            }
            if (ResultListView.IsValid()) ResultListView->RequestListRefresh();
        }

        FText GetSummaryText() const
        {
            return FText::FromString(FString::Printf(TEXT("Log: %s | Findings: %d"), LastLogFile.IsEmpty() ? TEXT("<none>") : *FPaths::GetCleanFilename(LastLogFile), Findings.Num()));
        }

        TSharedRef<ITableRow> GenerateRow(FPIEErrorLogFindingPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
        {
            const FLinearColor Accent = Item.IsValid() && Item->Confidence == TEXT("Likely") ? FLinearColor(0.35f, 0.9f, 0.45f) : FLinearColor(1.0f, 0.72f, 0.25f);
            return SNew(STableRow<FPIEErrorLogFindingPtr>, OwnerTable)
            [
                SNew(SBorder).Padding(8.0f).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed")))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(Item.IsValid() ? FString::Printf(TEXT("%s | %s | %s"), *Item->Confidence, Item->InstanceName.IsEmpty() ? TEXT("<unknown instance>") : *Item->InstanceName, Item->FunctionName.IsEmpty() ? TEXT("<unknown function>") : *Item->FunctionName) : TEXT("<invalid>")))
                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))
                        .ColorAndOpacity(Accent)
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)
                    [SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? FString::Printf(TEXT("Blueprint=%s | Graph=%s | Node=%s | Line=%d"), Item->BlueprintName.IsEmpty() ? TEXT("-") : *Item->BlueprintName, Item->GraphName.IsEmpty() ? TEXT("-") : *Item->GraphName, Item->NodeName.IsEmpty() ? TEXT("-") : *Item->NodeName, Item->LineNumber) : TEXT(""))).ColorAndOpacity(FLinearColor(0.72f, 0.78f, 0.86f))]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)
                    [SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? FString::Printf(TEXT("%s%s"), *Item->ErrorMessage, Item->RepeatCount > 1 ? *FString::Printf(TEXT("  [repeated x%d]"), Item->RepeatCount) : TEXT("")) : FString())).AutoWrapText(true)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)
                    [SNew(STextBlock).Text(FText::FromString(Item.IsValid() && !Item->ObjectPath.IsEmpty() ? FString::Printf(TEXT("Object: %s"), *Item->ObjectPath) : FString(TEXT("Object: <not found in log>")))).ColorAndOpacity(FLinearColor(0.62f, 0.66f, 0.72f)).AutoWrapText(true)]
                ]
            ];
        }

        FString BuildReportText() const
        {
            FString Body;
            for (const FPIEErrorLogFindingPtr& Finding : Findings)
            {
                if (!Finding.IsValid()) continue;
                Body += FString::Printf(TEXT("- [%s] Instance=%s | Function=%s | Blueprint=%s | Graph=%s | Node=%s | LogLine=%d\n  Object=%s\n  Error=%s\n"),
                    *Finding->Confidence,
                    Finding->InstanceName.IsEmpty() ? TEXT("<unknown>") : *Finding->InstanceName,
                    Finding->FunctionName.IsEmpty() ? TEXT("<unknown>") : *Finding->FunctionName,
                    Finding->BlueprintName.IsEmpty() ? TEXT("<unknown>") : *Finding->BlueprintName,
                    Finding->GraphName.IsEmpty() ? TEXT("<unknown>") : *Finding->GraphName,
                    Finding->NodeName.IsEmpty() ? TEXT("<unknown>") : *Finding->NodeName,
                    Finding->LineNumber,
                    Finding->ObjectPath.IsEmpty() ? TEXT("<unknown>") : *Finding->ObjectPath,
                    *Finding->ErrorMessage);
            }
            TArray<TMReportFormatter::FMetadataItem> Metadata;
            Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Log File"), LastLogFile));
            Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Findings"), FString::FromInt(Findings.Num())));
            return TMReportFormatter::BuildWrappedLegacyReport(TEXT("Runtime Error Investigation - Completed PIE Logs"), FString::Printf(TEXT("- Log: %s\n- Findings: %d\n- Note: Instance/function extraction is heuristic from completed logs."), *LastLogFile, Findings.Num()), Body.IsEmpty() ? TEXT("- No Blueprint runtime error findings.") : Body, Metadata);
        }

        FReply OnAnalyzeLatestClicked()
        {
            RefreshAnalysis();
            return FReply::Handled();
        }

        FReply OnCopyReportClicked() const
        {
            const FString Report = BuildReportText();
            FPlatformApplicationMisc::ClipboardCopy(*Report);
            return FReply::Handled();
        }
    };

    TWeakPtr<SPIEErrorLogAnalyzer> ActiveAnalyzerPanel;
}

namespace TMPIEErrorLogAnalyzer
{
    TSharedRef<SWidget> CreatePanel()
    {
        const TSharedRef<SPIEErrorLogAnalyzer> Panel = SNew(SPIEErrorLogAnalyzer);
        ActiveAnalyzerPanel = Panel;
        return Panel;
    }

    void RefreshPanel()
    {
        if (const TSharedPtr<SPIEErrorLogAnalyzer> Panel = ActiveAnalyzerPanel.Pin())
        {
            Panel->RefreshFromHost();
        }
    }
}
