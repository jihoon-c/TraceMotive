// Source/TraceMotive/Private/SCallChainViewer.cpp

#include "SCallChainViewer.h"

#include "TMReportFormatter.h"
#include "TMPerformanceGuard.h"
#include "TMLocalization.h"

#include "Widgets/Layout/SScrollBox.h"

#include "Widgets/Text/STextBlock.h"

#include "Widgets/Layout/SBox.h"

#include "Widgets/Layout/SBorder.h"

#include "Widgets/Images/SImage.h"

#include "Widgets/Input/SButton.h"

#include "Kismet2/KismetEditorUtilities.h"

#include "Styling/AppStyle.h"

#include "EdGraph/EdGraphNode.h"

#include "Editor/EditorEngine.h"

#include "Engine/Blueprint.h"

#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"

#include "Subsystems/AssetEditorSubsystem.h"
#include "SourceCodeNavigation.h"

#include "K2Node_CallFunction.h"

#include "K2Node_Event.h"

#include "K2Node_FunctionEntry.h"

namespace

{

    FString GetRelationLabel(ECallChainRelationKind Kind)

    {

        switch (Kind)

        {

        case ECallChainRelationKind::DirectCall: return TEXT("CALL");

        case ECallChainRelationKind::DelegateBinding: return TEXT("DELEGATE");

        case ECallChainRelationKind::MacroExpansion: return TEXT("MACRO");

        case ECallChainRelationKind::EntryPoint: return TEXT("ENTRY");

        case ECallChainRelationKind::Target: return TEXT("TARGET");

        case ECallChainRelationKind::CppSourceCall: return TEXT("C++");

        default: return TEXT("MATCH");

        }

    }

    FLinearColor GetRelationColor(ECallChainRelationKind Kind)

    {

        switch (Kind)

        {

        case ECallChainRelationKind::DelegateBinding: return FLinearColor(0.85f, 0.55f, 1.0f);

        case ECallChainRelationKind::MacroExpansion: return FLinearColor(1.0f, 0.68f, 0.25f);

        case ECallChainRelationKind::EntryPoint: return FLinearColor(0.30f, 1.0f, 0.35f);

        case ECallChainRelationKind::Target: return FLinearColor(1.0f, 0.30f, 0.30f);

        case ECallChainRelationKind::CppSourceCall: return FLinearColor(0.20f, 0.90f, 1.0f);

        case ECallChainRelationKind::DirectCall: return FLinearColor(0.40f, 0.70f, 1.0f);

        default: return FLinearColor(0.72f, 0.72f, 0.72f);

        }

    }

    void AddGraphRecursive(UEdGraph* Graph, TArray<UEdGraph*>& OutGraphs)

    {

        if (!Graph || OutGraphs.Contains(Graph))

        {

            return;

        }

        OutGraphs.Add(Graph);

        TArray<UEdGraph*> ChildGraphs;

        Graph->GetAllChildrenGraphs(ChildGraphs);

        for (UEdGraph* ChildGraph : ChildGraphs)

        {

            AddGraphRecursive(ChildGraph, OutGraphs);

        }

    }

}

void SCallChainViewer::Construct(const FArguments& InArgs, const FCallChainResult& InResult)

{

    Result = InResult;

    PathExpansionStates.Init(true, Result.Paths.Num());

    TSharedPtr<SVerticalBox> MainBox;

    const FString Summary = FString::Printf(

        TEXT("%d path(s) | %d direct caller(s) | %d package(s) scanned | %d cycle(s) | %d truncated"),

        Result.Paths.Num(), Result.DirectCallerCount, Result.ScannedPackageCount,

        Result.CyclicPathCount, Result.TruncatedPathCount);

    ChildSlot

    [

        SNew(SBorder)

        .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))

        .Padding(10.0f)

        [

            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)

            [

                SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush("WhiteBrush"))

                .BorderBackgroundColor(FLinearColor(0.2f, 0.3f, 0.5f))

                .Padding(10.0f)

                [

                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()

                    [

                        SNew(STextBlock)

                        .Text(FText::FromString(FString::Printf(TEXT("Call Chain: %s"), *Result.TargetFunctionName.ToString())))

                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))

                        .ColorAndOpacity(FLinearColor::White)

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)

                    [

                        SNew(STextBlock)

                        .Text(FText::FromString(Summary))

                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))

                        .ColorAndOpacity(FLinearColor(0.82f, 0.86f, 0.92f))

                    ]

                ]

            ]

            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)

            [

                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)

                [

                    SNew(SButton).Text(TMLoc::Text(TEXT("Expand All"), TEXT("Expand All"))).OnClicked(this, &SCallChainViewer::OnExpandAllClicked)

                ]

                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)

                [

                    SNew(SButton).Text(TMLoc::Text(TEXT("Collapse All"), TEXT("Collapse All"))).OnClicked(this, &SCallChainViewer::OnCollapseAllClicked)

                ]

                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)

                [

                    SNew(SButton)

                    .Text(TMLoc::Text(TEXT("Copy Report"), TEXT("Copy Report")))

                    .ToolTipText(TMLoc::Text(TEXT("Copy all paths and match reasons to the clipboard"), TEXT("Copy all paths and match reasons to the clipboard")))

                    .OnClicked(this, &SCallChainViewer::OnCopyReportClicked)

                ]

                + SHorizontalBox::Slot().AutoWidth()

                [

                    SNew(SButton)

                    .ToolTipText(TMLoc::Text(TEXT("Use more editor time per tick for the next call-chain/search operation."), TEXT("")))

                    .Text_Lambda([]()

                    {

                        return TMPerf::IsSearchBoostEnabled()
                            ? TMLoc::Text(TEXT("Search Boost: ON"), TEXT(""))
                            : TMLoc::Text(TEXT("Search Boost: OFF"), TEXT(""));

                    })

                    .ButtonColorAndOpacity_Lambda([]()

                    {

                        return TMPerf::IsSearchBoostEnabled() ? FSlateColor(FLinearColor(0.12f, 0.42f, 0.16f, 1.0f)) : FSlateColor::UseForeground();

                    })

                    .OnClicked_Lambda([]()

                    {

                        TMPerf::ToggleSearchBoost();

                        return FReply::Handled();

                    })

                ]

            ]

            + SVerticalBox::Slot().FillHeight(1.0f)

            [

                SNew(SScrollBox)

                .Orientation(Orient_Vertical)

                + SScrollBox::Slot()

                [

                    SAssignNew(MainBox, SVerticalBox)

                ]

            ]

        ]

    ];

    if (Result.Paths.Num() == 0)

    {

        MainBox->AddSlot().AutoHeight().Padding(20.0f).HAlign(HAlign_Center)

        [

            SNew(STextBlock)

            .Text(TMLoc::Text(TEXT("No Blueprint call paths found."), TEXT("No Blueprint call paths found.")))

            .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))

        ];

        return;

    }

    for (int32 Index = 0; Index < Result.Paths.Num(); ++Index)

    {

        MainBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)

        [

            CreatePathWidget(Result.Paths[Index], Index)

        ];

    }

}

TSharedRef<SWidget> SCallChainViewer::CreatePathWidget(const FCallChainPath& Path, int32 PathIndex)

{

    FString HeaderDesc = FString::Printf(TEXT("Path %d"), PathIndex + 1);

    if (Path.Nodes.Num() > 0)

    {

        const FCallChainNode& StartNode = Path.Nodes[0];

        HeaderDesc = FString::Printf(TEXT("From [%s] : %s"), *StartNode.AssetName, *StartNode.FunctionName);

    }

    const FString StatusLabel = Path.bContainsCycle ? TEXT("CYCLE") : Path.bTruncated ? TEXT("TRUNCATED") : TEXT("COMPLETE");

    const FLinearColor StatusColor = Path.bContainsCycle

        ? FLinearColor(1.0f, 0.55f, 0.20f)

        : Path.bTruncated ? FLinearColor(1.0f, 0.78f, 0.22f) : FLinearColor(0.38f, 0.90f, 0.50f);

    TSharedPtr<SVerticalBox> ContentBox = SNew(SVerticalBox);

    for (int32 Index = 0; Index < Path.Nodes.Num(); ++Index)

    {

        const bool bIsLast = Index == Path.Nodes.Num() - 1;

        ContentBox->AddSlot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)

        [

            CreateChainNodeWidget(Path.Nodes[Index], bIsLast)

        ];

        if (!bIsLast)

        {

            ContentBox->AddSlot().AutoHeight().Padding(15.0f, 0.0f, 0.0f, 0.0f)

            [

                CreateArrowWidget()

            ];

        }

    }

    if (!Path.TerminationReason.IsEmpty())

    {

        ContentBox->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)

        [

            SNew(STextBlock)

            .Text(FText::FromString(Path.TerminationReason))

            .ColorAndOpacity(StatusColor)

            .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))

            .AutoWrapText(true)

        ];

    }

    return SNew(SBorder)

        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))

        .BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.5f))

        .Padding(0.0f)

        [

            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight()

            [

                SNew(SButton)

                .ButtonStyle(FAppStyle::Get(), "NoBorder")

                .OnClicked(this, &SCallChainViewer::OnPathHeaderClicked, PathIndex)

                .ContentPadding(FMargin(10.0f, 8.0f))

                [

                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)

                    [

                        SNew(SImage)

                        .Image(this, &SCallChainViewer::GetPathExpandArrowImage, PathIndex)

                        .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))

                    ]

                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

                    [

                        SNew(STextBlock)

                        .Text(FText::FromString(HeaderDesc))

                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))

                        .ColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.4f))

                    ]

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0)

                    [

                        SNew(STextBlock)

                        .Text(FText::FromString(StatusLabel))

                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))

                        .ColorAndOpacity(StatusColor)

                    ]

                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                    [

                        SNew(STextBlock)

                        .Text(FText::FromString(FString::Printf(TEXT("Nodes: %d"), Path.Nodes.Num())))

                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))

                        .ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))

                    ]

                ]

            ]

            + SVerticalBox::Slot().AutoHeight()

            [

                SNew(SBorder)

                .BorderImage(FAppStyle::GetBrush("NoBrush"))

                .Padding(FMargin(20.0f, 0.0f, 10.0f, 10.0f))

                .Visibility(this, &SCallChainViewer::GetPathContentVisibility, PathIndex)

                [

                    ContentBox.ToSharedRef()

                ]

            ]

        ];

}

TSharedRef<SWidget> SCallChainViewer::CreateChainNodeWidget(const FCallChainNode& Node, bool bIsLast)

{

    const FLinearColor NodeColor = GetRelationColor(Node.RelationKind);

    const FString TypeText = GetRelationLabel(Node.RelationKind);

    FString TargetLabel;

    if (!Node.TargetObjectName.IsEmpty() && !Node.TargetClassName.IsEmpty())

    {

        TargetLabel = FString::Printf(TEXT("Target: %s (%s)"), *Node.TargetObjectName, *Node.TargetClassName);

    }

    else if (!Node.TargetObjectName.IsEmpty())

    {

        TargetLabel = FString::Printf(TEXT("Target: %s"), *Node.TargetObjectName);

    }

    else if (!Node.TargetClassName.IsEmpty())

    {

        TargetLabel = FString::Printf(TEXT("Target Class: %s"), *Node.TargetClassName);

    }

    return SNew(SButton)

        .ButtonStyle(FAppStyle::Get(), "SimpleButton")

        .ToolTipText(FText::FromString(Node.MatchReason.IsEmpty() ? (Node.bIsCppSource ? TEXT("Click to open C++ source") : TEXT("Click to open Blueprint")) : Node.MatchReason))

        .OnClicked_Lambda([this, Node]()

        {

            JumpToNode(Node);

            return FReply::Handled();

        })

        [

            SNew(SBorder)

            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))

            .BorderBackgroundColor(NodeColor * 0.25f)

            .Padding(8.0f)

            [

                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)

                [

                    SNew(SBox).WidthOverride(4.0f)

                    [

                        SNew(SBorder).BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(NodeColor)

                    ]

                ]

                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

                [

                    SNew(SVerticalBox)

                    + SVerticalBox::Slot().AutoHeight()

                    [

                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().AutoWidth()

                        [

                            SNew(STextBlock)

                            .Text(FText::FromString(Node.AssetName))

                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))

                            .ColorAndOpacity(FLinearColor::White)

                        ]

                        + SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 0, 0)

                        [

                            SNew(STextBlock)

                            .Text(FText::FromString(FString::Printf(TEXT("(%s)"), *Node.GraphName)))

                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))

                            .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))

                        ]

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)

                    [

                        SNew(STextBlock)

                        .Text(FText::FromString(Node.FunctionName))

                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))

                        .ColorAndOpacity(NodeColor)

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)

                    [

                        SNew(STextBlock)

                        .Visibility(Node.SourceSnippet.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)

                        .Text(FText::FromString(Node.SourceSnippet))

                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))

                        .ColorAndOpacity(FLinearColor(0.72f, 0.92f, 1.0f))

                        .AutoWrapText(true)

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)

                    [

                        SNew(STextBlock)

                        .Visibility(TargetLabel.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)

                        .Text(FText::FromString(TargetLabel))

                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))

                        .ColorAndOpacity(FLinearColor(0.72f, 0.82f, 1.0f))

                        .AutoWrapText(true)

                    ]

                    + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)

                    [

                        SNew(STextBlock)

                        .Visibility(Node.MatchReason.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)

                        .Text(FText::FromString(Node.MatchReason))

                        .Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))

                        .ColorAndOpacity(FLinearColor(0.68f, 0.72f, 0.78f))

                        .AutoWrapText(true)

                    ]

                ]

                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(8, 0, 0, 0)

                [

                    SNew(STextBlock)

                    .Text(FText::FromString(TypeText))

                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))

                    .ColorAndOpacity(NodeColor)

                ]

            ]

        ];

}

TSharedRef<SWidget> SCallChainViewer::CreateArrowWidget()

{

    return SNew(SBox)

        .HeightOverride(24.0f)

        .Padding(FMargin(0, -5))

        .VAlign(VAlign_Center)

        [

            SNew(STextBlock)

                .Text(TMLoc::Text(TEXT("->"), TEXT("->")))

                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))

                .ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f))

        ];

}

// --------------------------------------------------------------------------

// --------------------------------------------------------------------------

FReply SCallChainViewer::OnPathHeaderClicked(int32 PathIndex)

{

    if (PathExpansionStates.IsValidIndex(PathIndex))

    {

        PathExpansionStates[PathIndex] = !PathExpansionStates[PathIndex];

    }

    return FReply::Handled();

}

FReply SCallChainViewer::OnExpandAllClicked()

{

    for (int32 Index = 0; Index < PathExpansionStates.Num(); ++Index)

    {

        PathExpansionStates[Index] = true;

    }

    return FReply::Handled();

}

FReply SCallChainViewer::OnCollapseAllClicked()

{

    for (int32 Index = 0; Index < PathExpansionStates.Num(); ++Index)

    {

        PathExpansionStates[Index] = false;

    }

    return FReply::Handled();

}

FReply SCallChainViewer::OnCopyReportClicked()

{

    FPlatformApplicationMisc::ClipboardCopy(*BuildReportText());

    return FReply::Handled();

}

FString SCallChainViewer::BuildReportText() const

{

    FString Report;

    Report += FString::Printf(TEXT("# Integrated Call Chain: %s\n\n"), *Result.TargetFunctionName.ToString());

    Report += FString::Printf(TEXT("Paths: %d | Direct callers: %d | C++ callers: %d | Packages scanned: %d | Cycles: %d | Truncated: %d\n\n"),

        Result.Paths.Num(), Result.DirectCallerCount, Result.CppSourceCallerCount, Result.ScannedPackageCount,

        Result.CyclicPathCount, Result.TruncatedPathCount);

    for (int32 PathIndex = 0; PathIndex < Result.Paths.Num(); ++PathIndex)

    {

        const FCallChainPath& Path = Result.Paths[PathIndex];

        const TCHAR* Status = Path.bContainsCycle ? TEXT("CYCLE") : Path.bTruncated ? TEXT("TRUNCATED") : TEXT("COMPLETE");

        Report += FString::Printf(TEXT("## Path %d [%s]\n"), PathIndex + 1, Status);

        for (int32 NodeIndex = 0; NodeIndex < Path.Nodes.Num(); ++NodeIndex)

        {

            const FCallChainNode& Node = Path.Nodes[NodeIndex];

            Report += FString::Printf(TEXT("%d. [%s] %s :: %s :: %s"), NodeIndex + 1,

                *GetRelationLabel(Node.RelationKind), *Node.AssetName, *Node.GraphName, *Node.FunctionName);

            if (Node.bIsCppSource)

            {

                Report += FString::Printf(TEXT(" | Source=%s:%d:%d"), *Node.SourceFilePath, Node.SourceLine, Node.SourceColumn);

                if (!Node.SourceSnippet.IsEmpty())

                {

                    Report += FString::Printf(TEXT(" | %s"), *Node.SourceSnippet);

                }

            }

            if (!Node.TargetObjectName.IsEmpty() || !Node.TargetClassName.IsEmpty())

            {

                Report += FString::Printf(TEXT(" | Target=%s (%s)"), *Node.TargetObjectName, *Node.TargetClassName);

            }

            if (!Node.MatchReason.IsEmpty())

            {

                Report += FString::Printf(TEXT(" | %s"), *Node.MatchReason);

            }

            Report += TEXT("\n");

        }

        if (!Path.TerminationReason.IsEmpty())

        {

            Report += FString::Printf(TEXT("\nReason: %s\n"), *Path.TerminationReason);

        }

        Report += TEXT("\n");

    }

    TArray<TMReportFormatter::FMetadataItem> Metadata;

    Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Target Function"), Result.TargetFunctionName.ToString()));

    Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Paths"), FString::FromInt(Result.Paths.Num())));

    Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Direct Callers"), FString::FromInt(Result.DirectCallerCount)));

    Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Packages Scanned"), FString::FromInt(Result.ScannedPackageCount)));

    Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Cycles"), FString::FromInt(Result.CyclicPathCount)));

    Metadata.Add(TMReportFormatter::FMetadataItem(TEXT("Truncated"), FString::FromInt(Result.TruncatedPathCount)));

    return TMReportFormatter::BuildWrappedLegacyReport(

        TEXT("Function Call Chain"),

        FString::Printf(TEXT("- Target function: %s\n- Paths: %d\n- Direct callers: %d\n- Cycles: %d\n- Truncated paths: %d\n- Confidence: static Blueprint graph analysis; dynamic delegates/timers/interfaces may be incomplete."),

            *Result.TargetFunctionName.ToString(),

            Result.Paths.Num(),

            Result.DirectCallerCount,

            Result.CyclicPathCount,

            Result.TruncatedPathCount),

        Report,

        Metadata);

}

EVisibility SCallChainViewer::GetPathContentVisibility(int32 PathIndex) const

{

    if (PathExpansionStates.IsValidIndex(PathIndex) && PathExpansionStates[PathIndex]) return EVisibility::Visible;

    return EVisibility::Collapsed;

}

const FSlateBrush* SCallChainViewer::GetPathExpandArrowImage(int32 PathIndex) const

{

    if (PathExpansionStates.IsValidIndex(PathIndex) && PathExpansionStates[PathIndex]) return FAppStyle::Get().GetBrush("TreeArrow_Expanded");

    return FAppStyle::Get().GetBrush("TreeArrow_Collapsed");

}

void SCallChainViewer::JumpToNode(const FCallChainNode& NodeInfo)

{

    if (NodeInfo.bIsCppSource && !NodeInfo.SourceFilePath.IsEmpty())

    {

        if (!FSourceCodeNavigation::OpenSourceFile(NodeInfo.SourceFilePath, NodeInfo.SourceLine, NodeInfo.SourceColumn))

        {

            FPlatformProcess::LaunchFileInDefaultExternalApplication(*NodeInfo.SourceFilePath);

        }

        return;

    }

    if (UEdGraphNode* LiveNode = NodeInfo.SourceNode.Get())

    {

        FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(LiveNode);

        return;

    }

    if (NodeInfo.AssetPath.IsEmpty()) return;

    UPackage* Package = LoadPackage(nullptr, *NodeInfo.AssetPath, LOAD_None);

    if (!Package) return;

    UBlueprint* BP = nullptr;

    UObject* AssetObj = Package->FindAssetInPackage();

    BP = Cast<UBlueprint>(AssetObj);

    if (!BP)

    {

        ForEachObjectWithPackage(Package, [&BP](UObject* Object) {

            if (UBlueprint* FoundBP = Cast<UBlueprint>(Object)) {

                BP = FoundBP;

                return false;

            }

            return true;

            });

    }

    if (BP)

    {

        GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(BP);

        UEdGraphNode* TargetNode = nullptr;

        TArray<UEdGraph*> AllGraphs;

        for (UEdGraph* Graph : BP->UbergraphPages) AddGraphRecursive(Graph, AllGraphs);

        for (UEdGraph* Graph : BP->FunctionGraphs) AddGraphRecursive(Graph, AllGraphs);

        for (UEdGraph* Graph : BP->MacroGraphs) AddGraphRecursive(Graph, AllGraphs);

        UEdGraph* TargetGraph = nullptr;

        for (UEdGraph* Graph : AllGraphs)

        {

            if (!Graph) continue;

            for (UEdGraphNode* Node : Graph->Nodes)

            {

                if (Node && Node->NodeGuid == NodeInfo.NodeGuid)

                {

                    TargetNode = Node;

                    break;

                }

            }

            if (!TargetGraph && NodeInfo.NodeGuid.IsValid() && Graph->GraphGuid == NodeInfo.NodeGuid)

            {

                TargetGraph = Graph;

            }

            if (!TargetGraph && !NodeInfo.GraphName.IsEmpty() && Graph->GetName() == NodeInfo.GraphName)

            {

                TargetGraph = Graph;

            }

            if (TargetNode) break;

        }

        if (TargetNode)

        {

            FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(TargetNode);

        }

        else if (TargetGraph)

        {

            FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(TargetGraph);

        }

    }

}

void SCallChainViewer::OnNodeClicked(TWeakObjectPtr<UEdGraphNode> NodeWeakPtr)

{

    if (UEdGraphNode* Node = NodeWeakPtr.Get())

    {

        FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Node);

    }

}
