// Source/TraceMotive/Private/SVisualRefNode.cpp



#include "SVisualRefNode.h"
#include "TMLocalization.h"

#include "Widgets/SBoxPanel.h"

#include "Widgets/Images/SImage.h"

#include "Widgets/Text/STextBlock.h"

#include "Widgets/Layout/SBox.h"

#include "Widgets/Layout/SScrollBox.h"

#include "Widgets/Input/SButton.h"

#include "SGraphPin.h"



bool SVisualRefNode::bShowGet = true;

bool SVisualRefNode::bShowSet = true;

bool SVisualRefNode::bShowCalls = true;

bool SVisualRefNode::bShowDefinitions = true;

EVisualRefViewMode SVisualRefNode::CurrentViewMode = EVisualRefViewMode::Tree;

bool SVisualRefNode::bAllowNodeMovement = false;



class SInvisiblePin : public SGraphPin

{

public:

    SLATE_BEGIN_ARGS(SInvisiblePin) {}

    SLATE_END_ARGS()



    void Construct(const FArguments& InArgs, UEdGraphPin* InPin)

    {

        SGraphPin::Construct(SGraphPin::FArguments(), InPin);

        bShowLabel = false;

    }



protected:

    virtual const FSlateBrush* GetPinIcon() const override

    {

        return FAppStyle::GetBrush("Graph.Pin.Disconnected");

    }

    virtual FSlateColor GetPinColor() const override { return FLinearColor::Transparent; }

    virtual FSlateColor GetPinTextColor() const override { return FLinearColor::Transparent; }

};



DECLARE_DELEGATE_OneParam(FOnRowSelected, int32);

DECLARE_DELEGATE_OneParam(FOnRowDoubleClicked, int32);



class SReferenceRow : public SCompoundWidget

{

public:

    SLATE_BEGIN_ARGS(SReferenceRow)

        : _IsSelected(false), _IsSource(false) {

        }

        SLATE_DEFAULT_SLOT(FArguments, Content)

        SLATE_ATTRIBUTE(bool, IsSelected)

        SLATE_ATTRIBUTE(bool, IsSource)

        SLATE_EVENT(FOnRowSelected, OnSelected)

        SLATE_EVENT(FOnRowDoubleClicked, OnDoubleClicked)

        SLATE_ARGUMENT(int32, Index)

    SLATE_END_ARGS()



    void Construct(const FArguments& InArgs)

    {

        IsSelected = InArgs._IsSelected;

        IsSource = InArgs._IsSource;

        OnSelected = InArgs._OnSelected;

        OnDoubleClicked = InArgs._OnDoubleClicked;

        Index = InArgs._Index;



        ChildSlot

            [

                SNew(SBorder)

                    .Padding(FMargin(4.0f, 2.0f))

                    .BorderImage(FAppStyle::GetBrush("WhiteBrush"))

                    .BorderBackgroundColor(this, &SReferenceRow::GetBackgroundColor)

                    [

                        InArgs._Content.Widget

                    ]

            ];

    }



    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override

    {

        if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)

        {

            if (OnSelected.IsBound()) OnSelected.Execute(Index);

            return FReply::Handled();

        }

        return FReply::Unhandled();

    }



    virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override

    {

        if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)

        {

            if (OnDoubleClicked.IsBound()) OnDoubleClicked.Execute(Index);

            return FReply::Handled();

        }

        return FReply::Unhandled();

    }



    FSlateColor GetBackgroundColor() const

    {

        if (IsSelected.Get()) return FLinearColor(0.0f, 0.4f, 1.0f, 0.6f);

        if (IsSource.Get()) return FLinearColor(1.0f, 0.74f, 0.18f, 0.18f);

        if (IsHovered()) return FLinearColor(1.0f, 1.0f, 1.0f, 0.1f);

        return FLinearColor::Transparent;

    }



private:

    TAttribute<bool> IsSelected;

    TAttribute<bool> IsSource;

    FOnRowSelected OnSelected;

    FOnRowDoubleClicked OnDoubleClicked;

    int32 Index;

};



class SResizeHandle : public SCompoundWidget

{

public:

    SLATE_BEGIN_ARGS(SResizeHandle) {}

    SLATE_END_ARGS()



    void Construct(const FArguments& InArgs)

    {

        ChildSlot

            [

                SNew(SBox).HeightOverride(10.0f)

                    [

                        SNew(SBorder)

                            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))

                            .BorderBackgroundColor(this, &SResizeHandle::GetHandleColor)

                            .HAlign(HAlign_Center).VAlign(VAlign_Center)

                            [

                                SNew(STextBlock).Text(FText::FromString("?")).ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f, 0.8f))

                            ]

                    ]

            ];

    }



    FSlateColor GetHandleColor() const

    {

        return IsHovered() ? FLinearColor(0.3f, 0.5f, 0.8f, 0.5f) : FLinearColor(0.2f, 0.2f, 0.2f, 0.3f);

    }



    virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override

    {

        return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);

    }

};



void SVisualRefNode::Construct(const FArguments& InArgs, UVisualRefNode* InNode)

{

    GraphNode = InNode;

    SetCursor(EMouseCursor::Default);



    bIsResizing = false;

    bHasResizableContent = false;

    InitialHeight = 300.0f;

    CurrentNodeHeight = 300.0f;



    UpdateGraphNode();

}



FReply SVisualRefNode::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)

{

    if (bHasResizableContent && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)

    {

        FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        if (LocalPos.Y > MyGeometry.GetLocalSize().Y - 10.0f)

        {

            bIsResizing = true;

            ResizeStartPos = MouseEvent.GetScreenSpacePosition();

            InitialHeight = CurrentNodeHeight;

            return FReply::Handled().CaptureMouse(SharedThis(this));

        }

    }



    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)

    {

        SelectedReferenceIndex = -1;

    }



    if (!bAllowNodeMovement && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)

    {

        return FReply::Handled();

    }



    return SGraphNode::OnMouseButtonDown(MyGeometry, MouseEvent);

}



FReply SVisualRefNode::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)

{

    if (bIsResizing)

    {

        float DeltaY = MouseEvent.GetScreenSpacePosition().Y - ResizeStartPos.Y;

        CurrentNodeHeight = FMath::Clamp(InitialHeight + DeltaY, MinNodeHeight, MaxNodeHeight);

        UpdateGraphNode();

        return FReply::Handled();

    }

    return SGraphNode::OnMouseMove(MyGeometry, MouseEvent);

}



FReply SVisualRefNode::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)

{

    if (bIsResizing && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)

    {

        bIsResizing = false;

        return FReply::Handled().ReleaseMouseCapture();

    }

    return SGraphNode::OnMouseButtonUp(MyGeometry, MouseEvent);

}









FReply SVisualRefNode::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)

{

    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)

    {

        if (UVisualRefNode* VisualNode = Cast<UVisualRefNode>(GraphNode))

        {

            VisualNode->JumpToDefinition();

            return FReply::Handled();

        }

    }



    return SGraphNode::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);

}



FCursorReply SVisualRefNode::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const

{

    if (bHasResizableContent && !bIsResizing)

    {

        FVector2D LocalPos = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition());

        if (LocalPos.Y > MyGeometry.GetLocalSize().Y - 10.0f)

        {

            return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);

        }

    }

    return SGraphNode::OnCursorQuery(MyGeometry, CursorEvent);

}



void SVisualRefNode::UpdateGraphNode()

{

    UVisualRefNode* MyNode = CastChecked<UVisualRefNode>(GraphNode);



    InputPins.Empty();

    OutputPins.Empty();

    RightNodeBox.Reset();

    LeftNodeBox.Reset();

    BottomNodeBox.Reset();



    bool bIsRoot = MyNode->bIsDefinitionNode;

    bool bNodeIsFunction = (MyNode->RefType == EVisualRefType::Function);

    bool bNodeIsDispatcher = (MyNode->RefType == EVisualRefType::Dispatcher);



    // 함수는 보라색, 변수는 기존 색상 유지

    FLinearColor HeaderColor;

    if (bNodeIsFunction)

    {

        HeaderColor = bIsRoot ? FLinearColor(0.6f, 0.2f, 0.8f, 1.0f) : FLinearColor(0.4f, 0.1f, 0.6f, 1.0f);

    }

    else if (bNodeIsDispatcher)

    {

        HeaderColor = bIsRoot ? FLinearColor(0.0f, 0.55f, 0.62f, 1.0f) : FLinearColor(0.0f, 0.36f, 0.45f, 1.0f);

    }

    else

    {

        HeaderColor = bIsRoot ? FLinearColor(0.8f, 0.4f, 0.0f, 1.0f) : FLinearColor(0.0f, 0.3f, 0.6f, 1.0f);

    }



    TSharedPtr<SVerticalBox> ListBox;

    SAssignNew(ListBox, SVerticalBox);



    int32 VisibleItemCount = 0;

    bHasResizableContent = false;





    auto GetDispatcherTypeText = [](const FVisualReferenceInfo& Info) -> FString

    {

        if (Info.MatchReason.Contains(TEXT("Call"))) return TEXT("CALL");

        if (Info.MatchReason.Contains(TEXT("Assign"))) return TEXT("ASSIGN");

        if (Info.MatchReason.Contains(TEXT("Unbind all"))) return TEXT("CLEAR");

        if (Info.MatchReason.Contains(TEXT("Unbind"))) return TEXT("UNBIND");

        if (Info.MatchReason.Contains(TEXT("Bind"))) return TEXT("BIND");

        if (Info.MatchReason.Contains(TEXT("Event"))) return TEXT("EVENT");

        return TEXT("DISP");

    };

    auto BuildTargetLabel = [](const FVisualReferenceInfo& Info) -> FString

    {

        if (!Info.TargetObjectName.IsEmpty() && !Info.TargetClassName.IsEmpty())

        {

            return FString::Printf(TEXT("Target: %s (%s)"), *Info.TargetObjectName, *Info.TargetClassName);

        }

        if (!Info.TargetObjectName.IsEmpty())

        {

            return FString::Printf(TEXT("Target: %s"), *Info.TargetObjectName);

        }

        if (!Info.TargetClassName.IsEmpty())

        {

            return FString::Printf(TEXT("Target Class: %s"), *Info.TargetClassName);

        }

        return FString();

    };



    auto BuildMatchReasonLabel = [](const FVisualReferenceInfo& Info) -> FString

    {

        return Info.MatchReason.IsEmpty()

            ? FString()

            : FString::Printf(TEXT("Match: %s"), *Info.MatchReason);

    };

    // =========================================================

    // 뷰 모드 분기 처리 (List vs Tree) + 함수/변수 구분

    // =========================================================

    if (CurrentViewMode == EVisualRefViewMode::List)

    {

        auto ShouldShowReference = [bNodeIsFunction, bNodeIsDispatcher](const FVisualReferenceInfo& Info) -> bool

        {

            if (bNodeIsDispatcher)

            {

                return true;

            }



            if (bNodeIsFunction)

            {

                return Info.bIsFunctionDefinition ? SVisualRefNode::bShowDefinitions : SVisualRefNode::bShowCalls;

            }



            if (Info.bIsSetter && !SVisualRefNode::bShowSet) return false;

            if (!Info.bIsSetter && !SVisualRefNode::bShowGet) return false;

            return true;

        };



        auto AddListSection = [this, &ListBox, &VisibleItemCount, MyNode, bNodeIsFunction, bNodeIsDispatcher, &GetDispatcherTypeText, &BuildTargetLabel, &BuildMatchReasonLabel](const FString& SectionTitle, const FLinearColor& SectionColor, const TArray<int32>& Indices)

        {

            if (Indices.Num() == 0)

            {

                return;

            }



            FLinearColor SectionBackgroundColor = SectionColor;

            SectionBackgroundColor.A = 0.16f;



            ListBox->AddSlot().AutoHeight().Padding(2.0f, 4.0f, 2.0f, 2.0f)

                [

                    SNew(SBorder)

                        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))

                        .BorderBackgroundColor(SectionBackgroundColor)

                        .Padding(FMargin(7.0f, 4.0f))

                        [

                            SNew(SHorizontalBox)

                                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

                                [

                                    SNew(STextBlock)

                                        .Text(FText::FromString(SectionTitle))

                                        .ColorAndOpacity(SectionColor)

                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))

                                ]

                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                                [

                                    SNew(STextBlock)

                                        .Text(FText::AsNumber(Indices.Num()))

                                        .ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f))

                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))

                                ]

                        ]

                ];



            for (int32 i : Indices)

            {

                const FVisualReferenceInfo& Info = MyNode->References[i];

                const bool bRefIsFunction = bNodeIsFunction || bNodeIsDispatcher || Info.bIsFunctionCall;



                FString DisplayGraphName = Info.GraphName;

                DisplayGraphName.RemoveFromStart(TEXT("["));

                DisplayGraphName.RemoveFromEnd(TEXT("]"));



                FString TypeText;

                FLinearColor AccentColor;

                if (bNodeIsDispatcher)

                {

                    TypeText = GetDispatcherTypeText(Info);

                    AccentColor = FLinearColor(0.0f, 0.76f, 0.86f);

                }

                else if (bNodeIsFunction)

                {

                    TypeText = Info.bIsFunctionDefinition

                        ? TEXT("DEF")

                        : (Info.bIsSearchSubject ? TEXT("TARGET") : FString::Printf(TEXT("#%d"), FMath::Max(Info.CallOrder + 1, 1)));

                    AccentColor = Info.bIsFunctionDefinition

                        ? FLinearColor(0.75f, 0.45f, 1.0f)

                        : (Info.bIsSearchSubject ? FLinearColor(1.0f, 0.78f, 0.22f) : FLinearColor(0.35f, 0.62f, 1.0f));

                }

                else

                {

                    TypeText = Info.bIsSetter ? TEXT("SET") : TEXT("GET");

                    AccentColor = Info.bIsSetter

                        ? FLinearColor(1.0f, 0.35f, 0.35f)

                        : FLinearColor(0.35f, 0.95f, 0.45f);

                }



                FLinearColor BadgeBackgroundColor = AccentColor;

                BadgeBackgroundColor.A = 0.18f;



                const FString TargetLabel = BuildTargetLabel(Info);

                const FString MatchReasonLabel = BuildMatchReasonLabel(Info);



                FLinearColor RowBackgroundColor = (VisibleItemCount % 2 == 0)

                    ? FLinearColor(1.0f, 1.0f, 1.0f, 0.035f)

                    : FLinearColor::Transparent;



                ListBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f)

                    [

                        SNew(SBorder)

                            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))

                            .BorderBackgroundColor(RowBackgroundColor)

                            .Padding(0.0f)

                            [

                                SNew(SReferenceRow)

                                    .Index(i)

                                    .IsSource(Info.bIsSource)

                                    .IsSelected_Lambda([this, i]() { return SelectedReferenceIndex == i; })

                                    .OnSelected_Lambda([this](int32 Idx) { SelectedReferenceIndex = Idx; })

                                    .OnDoubleClicked_Lambda([MyNode, Info](int32 Idx) { MyNode->JumpToReference(Info.SourceNode); })

                                    [

                                        SNew(SVerticalBox)

                                            + SVerticalBox::Slot().AutoHeight()

                                            [

                                                SNew(SHorizontalBox)

                                                    + SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 8.0f, 0.0f).VAlign(VAlign_Center)

                                                    [

                                                        SNew(SBorder)

                                                            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))

                                                            .BorderBackgroundColor(BadgeBackgroundColor)

                                                            .Padding(FMargin(6.0f, 2.0f))

                                                            [

                                                                SNew(STextBlock)

                                                                    .Text(FText::FromString(TypeText))

                                                                    .ColorAndOpacity(AccentColor)

                                                                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))

                                                            ]

                                                    ]

                                                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

                                                    [

                                                        SNew(STextBlock)

                                                            .Text(FText::FromString(DisplayGraphName))

                                                            .ColorAndOpacity(FLinearColor(0.72f, 0.72f, 0.72f))

                                                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))

                                                    ]

                                                    + SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)

                                                    [

                                                        SNew(SBorder)

                                                            .Visibility((Info.bIsSearchSubject || Info.bIsFunctionDefinition) ? EVisibility::Visible : EVisibility::Collapsed)

                                                            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))

                                                            .BorderBackgroundColor(FLinearColor(1.0f, 0.86f, 0.2f, 0.18f))

                                                            .Padding(FMargin(5.0f, 1.0f))

                                                            [

                                                                SNew(STextBlock)

                                                                    .Text(FText::FromString(

                                                                        (Info.bIsSearchSubject && Info.bIsFunctionDefinition) ? TEXT("TARGET DEFINITION")

                                                                        : (Info.bIsSearchSubject ? TEXT("SEARCH TARGET") : TEXT("DEFINITION"))))

                                                                    .ColorAndOpacity(FLinearColor(1.0f, 0.86f, 0.2f))

                                                                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))

                                                            ]

                                                    ]

                                            ]

                                            + SVerticalBox::Slot().AutoHeight().Padding(4.0f, 2.0f, 2.0f, 0.0f)

                                                    [

                                                        SNew(SButton)

                                                            .ButtonStyle(FAppStyle::Get(), "SimpleButton")

                                                            .ToolTipText(TMLoc::Text(TEXT("Show target and match details"), TEXT("Show target and match details")))

                                                            .OnClicked_Lambda([this, i]()

                                                                {

                                                                    SelectedReferenceIndex = (SelectedReferenceIndex == i) ? -1 : i;

                                                                    return FReply::Handled();

                                                                })

                                                            [

                                                                SNew(STextBlock)

                                                                    .Text_Lambda([this, i]()

                                                                        {

                                                                            return FText::FromString(SelectedReferenceIndex == i ? TEXT("Hide") : TEXT("Details"));

                                                                        })

                                                                    .ColorAndOpacity(FLinearColor(0.8f, 0.88f, 1.0f))

                                                                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))

                                                            ]

                                                    ]

                                            + SVerticalBox::Slot().AutoHeight().Padding(4.0f, 3.0f, 2.0f, 0.0f)

                                            [

                                                SNew(STextBlock)

                                                    .Text(FText::FromString(Info.NodeName))

                                                    .ColorAndOpacity(FLinearColor::White)

                                                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))

                                                    .AutoWrapText(true)

                                            ]

                                            + SVerticalBox::Slot().AutoHeight().Padding(4.0f, 2.0f, 2.0f, 0.0f)

                                            [

                                                SNew(STextBlock)

                                                    .Visibility_Lambda([this, i, TargetLabel]() { return (SelectedReferenceIndex == i && !TargetLabel.IsEmpty()) ? EVisibility::Visible : EVisibility::Collapsed; })

                                                    .Text(FText::FromString(TargetLabel))

                                                    .ColorAndOpacity(FLinearColor(0.72f, 0.82f, 1.0f))

                                                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))

                                                    .AutoWrapText(true)

                                            ]

                                            + SVerticalBox::Slot().AutoHeight().Padding(4.0f, 2.0f, 2.0f, 0.0f)

                                            [

                                                SNew(STextBlock)

                                                    .Visibility_Lambda([this, i, MatchReasonLabel]() { return (SelectedReferenceIndex == i && !MatchReasonLabel.IsEmpty()) ? EVisibility::Visible : EVisibility::Collapsed; })

                                                    .Text(FText::FromString(MatchReasonLabel))

                                                    .ColorAndOpacity(FLinearColor(0.62f, 0.74f, 0.92f))

                                                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))

                                                    .AutoWrapText(true)

                                            ]

                                    ]

                            ]

                    ];



                VisibleItemCount++;

            }



            ListBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)

                [

                    SNew(SSpacer).Size(FVector2D(0.0f, 2.0f))

                ];

        };



        TArray<int32> FunctionDefinitionIndices;

        TArray<int32> FunctionReferenceIndices;

        TArray<int32> DispatcherIndices;

        TArray<int32> SetterIndices;

        TArray<int32> GetterIndices;



        for (int32 i = 0; i < MyNode->References.Num(); ++i)

        {

            const FVisualReferenceInfo& Info = MyNode->References[i];

            if (!ShouldShowReference(Info))

            {

                continue;

            }



            if (bNodeIsDispatcher)

            {

                DispatcherIndices.Add(i);

            }

            else if (bNodeIsFunction)

            {

                if (Info.bIsFunctionDefinition)

                {

                    FunctionDefinitionIndices.Add(i);

                }

                else

                {

                    FunctionReferenceIndices.Add(i);

                }

            }

            else if (Info.bIsSetter)

            {

                SetterIndices.Add(i);

            }

            else

            {

                GetterIndices.Add(i);

            }

        }



        if (bNodeIsDispatcher)

        {

            AddListSection(TEXT("Dispatcher Events"), FLinearColor(0.0f, 0.76f, 0.86f), DispatcherIndices);

        }

        else if (bNodeIsFunction)

        {

            AddListSection(TEXT("Function Definition"), FLinearColor(0.75f, 0.45f, 1.0f), FunctionDefinitionIndices);

            AddListSection(TEXT("Function References"), FLinearColor(0.42f, 0.66f, 1.0f), FunctionReferenceIndices);

        }

        else

        {

            AddListSection(TEXT("SET References"), FLinearColor(1.0f, 0.38f, 0.38f), SetterIndices);

            AddListSection(TEXT("GET References"), FLinearColor(0.38f, 1.0f, 0.48f), GetterIndices);

        }

    }

    else

    {

        // --- TREE MODE (GROUP BY GRAPH) ---

        TMap<FString, TArray<int32>> GroupedIndices;



        for (int32 i = 0; i < MyNode->References.Num(); ++i)

        {

            const FVisualReferenceInfo& Info = MyNode->References[i];

            bool bRefIsFunction = bNodeIsFunction || bNodeIsDispatcher || Info.bIsFunctionCall || Info.bIsFunctionDefinition;



            // 필터링

            if (bNodeIsDispatcher)

            {

            }

            else if (bNodeIsFunction)

            {

                if (!bShowDefinitions && Info.bIsFunctionDefinition) continue;

                if (!bShowCalls && !Info.bIsFunctionDefinition) continue;

            }

            else

            {

                if (Info.bIsSetter && !bShowSet) continue;

                if (!Info.bIsSetter && !bShowGet) continue;

            }



            GroupedIndices.FindOrAdd(Info.GraphName).Add(i);

        }



        TArray<FString> SortedGraphNames;

        GroupedIndices.GetKeys(SortedGraphNames);

        SortedGraphNames.Sort();



        for (const FString& GraphName : SortedGraphNames)

        {

            const TArray<int32>& Indices = GroupedIndices[GraphName];



            // 그룹 헤더

            ListBox->AddSlot().AutoHeight().Padding(2, 4, 2, 2)

                [

                    SNew(SBorder)

                        .BorderImage(FAppStyle::GetBrush("Graph.Node.Body"))

                        .BorderBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f))

                        .Padding(4.0f)

                        [

                            SNew(STextBlock)

                                .Text(FText::FromString(GraphName))

                                .ColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.4f))

                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))

                        ]

                ];



            // 그룹 아이템

            for (int32 i : Indices)

            {

                const FVisualReferenceInfo& Info = MyNode->References[i];

                bool bRefIsFunction = bNodeIsFunction || bNodeIsDispatcher || Info.bIsFunctionCall || Info.bIsFunctionDefinition;



                FLinearColor TypeColor;

                FString TypeStr;

                const FSlateBrush* IconBrush = nullptr;



                if (bNodeIsDispatcher)

                {

                    TypeColor = FLinearColor(0.0f, 0.76f, 0.86f);

                    TypeStr = GetDispatcherTypeText(Info);

                    IconBrush = nullptr;

                }

                else if (bRefIsFunction)

                {

                    TypeColor = Info.bIsFunctionDefinition

                        ? FLinearColor(0.75f, 0.45f, 1.0f)

                        : (Info.bIsSearchSubject ? FLinearColor(1.0f, 0.78f, 0.22f) : FLinearColor(0.4f, 0.7f, 1.0f));

                    TypeStr = Info.bIsFunctionDefinition

                        ? TEXT("DEF")

                        : (Info.bIsSearchSubject ? TEXT("TARGET") : TEXT("CALL"));

                    IconBrush = FAppStyle::GetBrush("GraphEditor.Function_16x");

                }

                else

                {

                    TypeColor = Info.bIsSetter ? FLinearColor(1.0f, 0.4f, 0.4f) : FLinearColor(0.4f, 1.0f, 0.4f);

                    TypeStr = Info.bIsSetter ? TEXT("SET") : TEXT("GET");

                    IconBrush = nullptr;

                }



                FLinearColor RowBackgroundColor = FLinearColor::Transparent;

                const FString TargetLabel = BuildTargetLabel(Info);

                const FString MatchReasonLabel = BuildMatchReasonLabel(Info);





                ///////////////////////////////////////////////////////////////////////////////////////////////////

                // 1. 내용을 담을 변수를 선언하고 초기화 (기본값: 빈 위젯)

                TSharedRef<SWidget> IconOrTextWidget = SNullWidget::NullWidget;



                // 2. 조건에 따라 Image 위젯 또는 TextBlock 위젯을 생성하여 변수에 저장

                if (bNodeIsDispatcher)

                {

                    IconOrTextWidget = SNew(STextBlock)

                        .Text(FText::FromString(TypeStr))

                        .ColorAndOpacity(TypeColor)

                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8));

                }

                else if (bRefIsFunction)

                {

                    IconOrTextWidget = SNew(STextBlock)

                        .Text(FText::FromString(TypeStr))

                        .ColorAndOpacity(TypeColor)

                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8));

                }

                else

                {

                    // 변수일 때: 텍스트 생성

                    IconOrTextWidget = SNew(STextBlock)

                        .Text(FText::FromString(TypeStr))

                        .ColorAndOpacity(TypeColor)

                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9));

                }

                ///////////////////////////////////////////////////////////////////////////////////////////////////







                ListBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 0.0f)

                    [

                        SNew(SBorder)

                            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))

                            .BorderBackgroundColor(RowBackgroundColor)

                            .Padding(0.0f)

                            [

                                SNew(SReferenceRow)

                                    .Index(i)

                                    .IsSource(Info.bIsSource)

                                    .IsSelected_Lambda([this, i]() { return SelectedReferenceIndex == i; })

                                    .OnSelected_Lambda([this](int32 Idx) { SelectedReferenceIndex = Idx; })

                                    .OnDoubleClicked_Lambda([MyNode, Info](int32 Idx) { MyNode->JumpToReference(Info.SourceNode); })

                                    [

                                        SNew(SHorizontalBox)

                                            + SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 8, 0).VAlign(VAlign_Center)

                                            [

                                                SNew(SBorder)

                                                    .BorderImage(FAppStyle::GetBrush("WhiteBrush"))

                                                    .BorderBackgroundColor(FLinearColor::Transparent)

                                                    .Padding(FMargin(4, 2))

                                                    [

                                                        IconOrTextWidget

                                                    ]

                                            ]

                                            // 나머지 정보

                                            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)

                                                [

                                                    SNew(SVerticalBox)

                                                        + SVerticalBox::Slot().AutoHeight()

                                                        [

                                                            SNew(STextBlock)

                                                                .Text(FText::FromString(Info.GraphName))

                                                                .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))

                                                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))

                                                        ]

                                                        + SVerticalBox::Slot().AutoHeight()

                                                        [

                                                            SNew(STextBlock)

                                                                .Text(FText::FromString(Info.NodeName))

                                                                .ColorAndOpacity(FLinearColor::White)

                                                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))

                                                        ]

                                                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)

                                                        [

                                                            SNew(SButton)

                                                                .ButtonStyle(FAppStyle::Get(), "SimpleButton")

                                                                .ToolTipText(TMLoc::Text(TEXT("Show target and match details"), TEXT("Show target and match details")))

                                                                .OnClicked_Lambda([this, i]()

                                                                    {

                                                                        SelectedReferenceIndex = (SelectedReferenceIndex == i) ? -1 : i;

                                                                        return FReply::Handled();

                                                                    })

                                                                [

                                                                    SNew(STextBlock)

                                                                        .Text_Lambda([this, i]()

                                                                            {

                                                                                return FText::FromString(SelectedReferenceIndex == i ? TEXT("Hide") : TEXT("Details"));

                                                                            })

                                                                        .ColorAndOpacity(FLinearColor(0.8f, 0.88f, 1.0f))

                                                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))

                                                                ]

                                                        ]

                                                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)

                                                        [

                                                            SNew(STextBlock)

                                                                .Visibility_Lambda([this, i, TargetLabel]() { return (SelectedReferenceIndex == i && !TargetLabel.IsEmpty()) ? EVisibility::Visible : EVisibility::Collapsed; })

                                                                .Text(FText::FromString(TargetLabel))

                                                                .ColorAndOpacity(FLinearColor(0.72f, 0.82f, 1.0f))

                                                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))

                                                                .AutoWrapText(true)

                                                        ]

                                                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)

                                                        [

                                                            SNew(STextBlock)

                                                                .Visibility_Lambda([this, i, MatchReasonLabel]() { return (SelectedReferenceIndex == i && !MatchReasonLabel.IsEmpty()) ? EVisibility::Visible : EVisibility::Collapsed; })

                                                                .Text(FText::FromString(MatchReasonLabel))

                                                                .ColorAndOpacity(FLinearColor(0.62f, 0.74f, 0.92f))

                                                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))

                                                                .AutoWrapText(true)

                                                        ]

                                                ]

                                    ]

                            ]

                    ];

                VisibleItemCount++;

            }

            ListBox->AddSlot().AutoHeight().Padding(0, 4)[SNew(SSpacer).Size(FVector2D(0, 2))];

        }

    }



    TSharedPtr<SWidget> BodyWidget;



    if (VisibleItemCount > 0)

    {

        const float EstimatedItemHeight = (CurrentViewMode == EVisualRefViewMode::List) ? 48.0f : 35.0f;

        const float EstimatedTotalHeight = VisibleItemCount * EstimatedItemHeight;

        bHasResizableContent = (EstimatedTotalHeight > MinNodeHeight);



        BodyWidget = SNew(SBorder)

            .BorderImage(FAppStyle::GetBrush("Brushes.Panel"))

            .Padding(4.0f)

            [

                SNew(SScrollBox)

                    .Orientation(Orient_Vertical)

                    .ScrollBarAlwaysVisible(false)

                    + SScrollBox::Slot()

                    [

                        ListBox.ToSharedRef()

                    ]

            ];

    }

    else

    {

        BodyWidget = SNew(SSpacer).Size(FVector2D(0, 5));

    }



    constexpr float HeaderPinVerticalOffset = 12.0f;



    this->GetOrAddSlot(ENodeZone::Center)

        .HAlign(HAlign_Fill)

        .VAlign(VAlign_Center)

        [

            SNew(SOverlay)

                + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)

                [

                    SNew(SBox)

                        .WidthOverride((CurrentViewMode == EVisualRefViewMode::List) ? 360.0f : 250.0f)

                        [

                            SNew(SBorder)

                                .BorderImage(FAppStyle::GetBrush("WhiteBrush"))

                                .BorderBackgroundColor(FLinearColor(0.12f, 0.12f, 0.12f, 1.0f))

                                .Padding(2.0f)

                                [

                                    SNew(SVerticalBox)

                                        + SVerticalBox::Slot().AutoHeight()

                                        [

                                            SNew(SBorder)

                                                .BorderImage(FAppStyle::GetBrush("WhiteBrush"))

                                                .BorderBackgroundColor(HeaderColor)

                                                .Padding(5.0f)

                                                .HAlign(HAlign_Center)

                                                [

                                                    SNew(STextBlock)

                                                        .Text(MyNode->AssetName)

                                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))

                                                        .ColorAndOpacity(FLinearColor::White)

                                                ]

                                        ]

                                    + SVerticalBox::Slot().AutoHeight().MaxHeight(CurrentNodeHeight)

                                        [

                                            BodyWidget.ToSharedRef()

                                        ]

                                        + SVerticalBox::Slot().AutoHeight()

                                        [

                                            SNew(SBox)

                                                .Visibility(bHasResizableContent ? EVisibility::Visible : EVisibility::Collapsed)

                                                [

                                                    SNew(SResizeHandle)

                                                ]

                                        ]

                                ]

                        ]

                ]

                + SOverlay::Slot()

                    .HAlign(HAlign_Center)

                    .VAlign(VAlign_Bottom)

                    .Padding(0.0f, 0.0f, 0.0f, -2.0f)

                [

                    SAssignNew(BottomNodeBox, SHorizontalBox)

                ]

                + SOverlay::Slot()

                    .HAlign(HAlign_Left)

                    .VAlign(VAlign_Top)

                    .Padding(0.0f, HeaderPinVerticalOffset, 0.0f, 0.0f)

                [

                    SAssignNew(LeftNodeBox, SVerticalBox)

                ]

                + SOverlay::Slot()

                    .HAlign(HAlign_Right)

                    .VAlign(VAlign_Top)

                    .Padding(0.0f, HeaderPinVerticalOffset, 0.0f, 0.0f)

                [

                    SAssignNew(RightNodeBox, SVerticalBox)

                ]

        ];



    CreatePinWidgets();

}



void SVisualRefNode::CreatePinWidgets()

{

    UVisualRefNode* MyNode = CastChecked<UVisualRefNode>(GraphNode);

    for (UEdGraphPin* Pin : MyNode->Pins)

    {

        if (!Pin->bHidden)

        {

            TSharedPtr<SGraphPin> NewPin = SNew(SInvisiblePin, Pin);

            NewPin->SetOwner(SharedThis(this));

            NewPin->SetShowLabel(false);

            if (Pin->Direction == EGPD_Input)

            {

                // Give each incoming relationship a separate association-end port along the declaration bottom.
                if (MyNode->bIsDefinitionNode && BottomNodeBox.IsValid())
                {
                    BottomNodeBox->AddSlot().AutoWidth().Padding(1.0f, 0.0f)
                    [
                        SNew(SBox).WidthOverride(12.0f).HeightOverride(10.0f)
                        [
                            NewPin.ToSharedRef()
                        ]
                    ];
                }
                else if (LeftNodeBox.IsValid())
                {
                    LeftNodeBox->AddSlot().AutoHeight()[NewPin.ToSharedRef()];
                }

                InputPins.Add(NewPin.ToSharedRef());

            }

            else

            {

                RightNodeBox->AddSlot().AutoHeight()[NewPin.ToSharedRef()];

                OutputPins.Add(NewPin.ToSharedRef());

            }

        }

    }

}
