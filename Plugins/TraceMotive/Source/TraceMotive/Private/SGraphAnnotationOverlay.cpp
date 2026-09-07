#include "SGraphAnnotationOverlay.h"
#include "TMLocalization.h"



#include "GraphEditor.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Crc.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "Framework/Application/SlateApplication.h"

#include "InputCoreTypes.h"

#include "Rendering/DrawElements.h"

#include "Styling/AppStyle.h"

#include "Types/SlateEnums.h"

#include "Widgets/Images/SImage.h"

#include "Widgets/Input/SButton.h"

#include "Widgets/Input/SCheckBox.h"

#include "Widgets/Layout/SBorder.h"

#include "Widgets/Layout/SBox.h"

#include "Widgets/SBoxPanel.h"

#include "Widgets/SLeafWidget.h"

#include "Widgets/SOverlay.h"

#include "Widgets/Text/STextBlock.h"



namespace

{

    constexpr float AnnotationLineThickness = 3.0f;

    constexpr float AnnotationEraserRadius = 14.0f;

    constexpr float MinPointDistanceSquared = 4.0f;



    FSlateColor GetToolbarTextColor(bool bEnabled)

    {

        return bEnabled ? FLinearColor::White : FLinearColor(0.72f, 0.72f, 0.72f);

    }



    bool AreAnnotationColorsEqual(const FLinearColor& A, const FLinearColor& B)

    {

        constexpr float Tolerance = 0.01f;

        return FMath::IsNearlyEqual(A.R, B.R, Tolerance)

            && FMath::IsNearlyEqual(A.G, B.G, Tolerance)

            && FMath::IsNearlyEqual(A.B, B.B, Tolerance)

            && FMath::IsNearlyEqual(A.A, B.A, Tolerance);

    }

    constexpr TCHAR AnnotationConfigSection[] = TEXT("TraceMotive.GraphAnnotations");

    FString GetAnnotationConfigKey(const FString& PersistenceKey)
    {
        return FString::Printf(TEXT("Annotation_%08X"), FCrc::StrCrc32(*PersistenceKey));
    }

    FString SerializeStroke(const FGraphAnnotationStroke& Stroke)
    {
        TArray<FString> SerializedPoints;
        SerializedPoints.Reserve(Stroke.Points.Num());
        for (const FVector2D& Point : Stroke.Points)
        {
            SerializedPoints.Add(Point.ToString());
        }

        return FString::Printf(TEXT("%s|%.3f|%s"), *Stroke.Color.ToString(), Stroke.Thickness, *FString::Join(SerializedPoints, TEXT(";")));
    }

    bool DeserializeStroke(const FString& SerializedStroke, FGraphAnnotationStroke& OutStroke)
    {
        TArray<FString> Parts;
        SerializedStroke.ParseIntoArray(Parts, TEXT("|"), false);
        if (Parts.Num() != 3 || !OutStroke.Color.InitFromString(Parts[0]))
        {
            return false;
        }

        OutStroke.Thickness = FCString::Atof(*Parts[1]);
        if (OutStroke.Thickness <= 0.0f)
        {
            return false;
        }

        TArray<FString> SerializedPoints;
        Parts[2].ParseIntoArray(SerializedPoints, TEXT(";"), false);
        for (const FString& SerializedPoint : SerializedPoints)
        {
            FVector2D Point;
            if (Point.InitFromString(SerializedPoint))
            {
                OutStroke.Points.Add(Point);
            }
        }

        return OutStroke.Points.Num() >= 2;
    }

    void SavePersistentAnnotations(const FString& PersistenceKey, const TArray<FGraphAnnotationStroke>& Strokes)
    {
        if (PersistenceKey.IsEmpty() || !GConfig)
        {
            return;
        }

        TArray<FString> SerializedStrokes;
        SerializedStrokes.Reserve(Strokes.Num());
        for (const FGraphAnnotationStroke& Stroke : Strokes)
        {
            if (Stroke.Points.Num() >= 2)
            {
                SerializedStrokes.Add(SerializeStroke(Stroke));
            }
        }

        const FString ConfigKey = GetAnnotationConfigKey(PersistenceKey);
        GConfig->SetArray(AnnotationConfigSection, *ConfigKey, SerializedStrokes, GEditorPerProjectIni);
        GConfig->SetString(AnnotationConfigSection, *(ConfigKey + TEXT("_Source")), *PersistenceKey, GEditorPerProjectIni);
        GConfig->Flush(false, GEditorPerProjectIni);
    }

    TArray<FGraphAnnotationStroke> LoadPersistentAnnotations(const FString& PersistenceKey)
    {
        TArray<FGraphAnnotationStroke> Result;
        if (PersistenceKey.IsEmpty() || !GConfig)
        {
            return Result;
        }

        const FString ConfigKey = GetAnnotationConfigKey(PersistenceKey);
        FString SavedSource;
        if (!GConfig->GetString(AnnotationConfigSection, *(ConfigKey + TEXT("_Source")), SavedSource, GEditorPerProjectIni) || SavedSource != PersistenceKey)
        {
            return Result;
        }

        TArray<FString> SerializedStrokes;
        GConfig->GetArray(AnnotationConfigSection, *ConfigKey, SerializedStrokes, GEditorPerProjectIni);
        for (const FString& SerializedStroke : SerializedStrokes)
        {
            FGraphAnnotationStroke Stroke;
            if (DeserializeStroke(SerializedStroke, Stroke))
            {
                Result.Add(MoveTemp(Stroke));
            }
        }

        return Result;
    }

}



class SGraphAnnotationPaintLayer : public SLeafWidget

{

public:

    SLATE_BEGIN_ARGS(SGraphAnnotationPaintLayer) {}

        SLATE_ARGUMENT(TWeakPtr<SGraphEditor>, GraphEditor)

    SLATE_END_ARGS()



    void Construct(const FArguments& InArgs)

    {

        GraphEditor = InArgs._GraphEditor;

    }



    void SetDrawingEnabled(bool bInDrawingEnabled)

    {

        bDrawingEnabled = bInDrawingEnabled;

        if (!bDrawingEnabled)

        {

            bIsDrawingStroke = false;

            bEraserEnabled = false;

        }

    }



    bool IsDrawingEnabled() const

    {

        return bDrawingEnabled;

    }



    void SetEraserEnabled(bool bInEraserEnabled)

    {

        bEraserEnabled = bInEraserEnabled;

        bDrawingEnabled = bDrawingEnabled || bEraserEnabled;

        bIsDrawingStroke = false;

    }



    bool IsEraserEnabled() const

    {

        return bEraserEnabled;

    }



    void SetDrawColor(const FLinearColor& InColor)

    {

        DrawColor = InColor;

        bEraserEnabled = false;

    }



    FLinearColor GetDrawColor() const

    {

        return DrawColor;

    }



    void ClearAnnotations()

    {

        Strokes.Reset();

        bIsDrawingStroke = false;

        bEraserEnabled = false;

        Invalidate(EInvalidateWidgetReason::Paint);

    }

    TArray<FGraphAnnotationStroke> GetAnnotations() const
    {
        return Strokes;
    }

    void SetAnnotations(const TArray<FGraphAnnotationStroke>& InAnnotations)
    {
        Strokes = InAnnotations;
        bIsDrawingStroke = false;
        Invalidate(EInvalidateWidgetReason::Paint);
    }



    EVisibility GetLayerVisibility() const

    {

        return bDrawingEnabled ? EVisibility::Visible : EVisibility::HitTestInvisible;

    }



    void ExitAnnotationMode()

    {

        bDrawingEnabled = false;

        bEraserEnabled = false;

        bIsDrawingStroke = false;

        Invalidate(EInvalidateWidgetReason::Paint);

    }



    virtual bool SupportsKeyboardFocus() const override

    {

        return true;

    }



    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override

    {

        if (InKeyEvent.GetKey() == EKeys::Escape && bDrawingEnabled)

        {

            ExitAnnotationMode();

            return FReply::Handled().ReleaseMouseCapture();

        }



        return SLeafWidget::OnKeyDown(MyGeometry, InKeyEvent);

    }



    virtual FVector2D ComputeDesiredSize(float) const override

    {

        return FVector2D::ZeroVector;

    }



    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,

        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override

    {

        int32 AnnotationLayer = LayerId;

        FVector2D ViewLocation;

        float ZoomAmount = 1.0f;

        const bool bUseGraphSpace = GetGraphView(ViewLocation, ZoomAmount);



        for (const FGraphAnnotationStroke& Stroke : Strokes)

        {

            if (Stroke.Points.Num() < 2)

            {

                continue;

            }



            TArray<FVector2D> PaintPoints;

            PaintPoints.Reserve(Stroke.Points.Num());

            for (const FVector2D& StoredPoint : Stroke.Points)

            {

                PaintPoints.Add(bUseGraphSpace ? GraphToLocal(StoredPoint, ViewLocation, ZoomAmount) : StoredPoint);

            }



            FSlateDrawElement::MakeLines(

                OutDrawElements,

                AnnotationLayer,

                AllottedGeometry.ToPaintGeometry(),

                PaintPoints,

                ESlateDrawEffect::None,

                Stroke.Color,

                true,

                bUseGraphSpace ? FMath::Max(1.0f, Stroke.Thickness * ZoomAmount) : Stroke.Thickness);

            ++AnnotationLayer;

        }



        return AnnotationLayer;

    }



    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override

    {

        if (!bDrawingEnabled || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)

        {

            return FReply::Unhandled();

        }



        const FVector2D LocalPoint = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());



        if (bEraserEnabled)

        {

            EraseAtLocalPoint(LocalPoint);

            bIsDrawingStroke = true;

            return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));

        }



        FGraphAnnotationStroke NewStroke;

        NewStroke.Color = DrawColor;

        NewStroke.Thickness = AnnotationLineThickness;

        NewStroke.Points.Add(LocalToStored(LocalPoint));

        Strokes.Add(MoveTemp(NewStroke));

        bIsDrawingStroke = true;

        Invalidate(EInvalidateWidgetReason::Paint);

        return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));

    }



    virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override

    {

        if (!bDrawingEnabled || !bIsDrawingStroke || !HasMouseCapture())

        {

            return FReply::Unhandled();

        }



        const FVector2D NewLocalPoint = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        if (bEraserEnabled)

        {

            EraseAtLocalPoint(NewLocalPoint);

            return FReply::Handled();

        }



        if (Strokes.Num() == 0)

        {

            return FReply::Handled();

        }



        TArray<FVector2D>& Points = Strokes.Last().Points;

        const FVector2D LastLocalPoint = Points.Num() > 0 ? StoredToLocal(Points.Last()) : NewLocalPoint;

        if (Points.Num() == 0 || FVector2D::DistSquared(LastLocalPoint, NewLocalPoint) >= MinPointDistanceSquared)

        {

            Points.Add(LocalToStored(NewLocalPoint));

            Invalidate(EInvalidateWidgetReason::Paint);

        }



        return FReply::Handled();

    }



    virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override

    {

        if (bIsDrawingStroke && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)

        {

            bIsDrawingStroke = false;

            Invalidate(EInvalidateWidgetReason::Paint);

            return FReply::Handled().ReleaseMouseCapture();

        }



        return FReply::Unhandled();

    }



    virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override

    {

        if (bDrawingEnabled)

        {

            return FCursorReply::Cursor(bEraserEnabled ? EMouseCursor::CardinalCross : EMouseCursor::Crosshairs);

        }



        return FCursorReply::Unhandled();

    }



private:

    bool GetGraphView(FVector2D& OutViewLocation, float& OutZoomAmount) const

    {

        OutViewLocation = FVector2D::ZeroVector;

        OutZoomAmount = 1.0f;



        TSharedPtr<SGraphEditor> PinnedGraphEditor = GraphEditor.Pin();

        if (!PinnedGraphEditor.IsValid())

        {

            return false;

        }



        #if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
        FVector2f ViewLocation;
        PinnedGraphEditor->GetViewLocation(ViewLocation, OutZoomAmount);
        OutViewLocation = FVector2D(static_cast<double>(ViewLocation.X), static_cast<double>(ViewLocation.Y));
#else
        PinnedGraphEditor->GetViewLocation(OutViewLocation, OutZoomAmount);
#endif

        if (OutZoomAmount <= KINDA_SMALL_NUMBER)

        {

            OutZoomAmount = 1.0f;

        }

        return true;

    }



    FVector2D GraphToLocal(const FVector2D& GraphPoint, const FVector2D& ViewLocation, float ZoomAmount) const

    {

        return (GraphPoint - ViewLocation) * ZoomAmount;

    }



    FVector2D LocalToGraph(const FVector2D& LocalPoint, const FVector2D& ViewLocation, float ZoomAmount) const

    {

        return ViewLocation + (LocalPoint / ZoomAmount);

    }



    FVector2D LocalToStored(const FVector2D& LocalPoint) const

    {

        FVector2D ViewLocation;

        float ZoomAmount = 1.0f;

        return GetGraphView(ViewLocation, ZoomAmount) ? LocalToGraph(LocalPoint, ViewLocation, ZoomAmount) : LocalPoint;

    }



    FVector2D StoredToLocal(const FVector2D& StoredPoint) const

    {

        FVector2D ViewLocation;

        float ZoomAmount = 1.0f;

        return GetGraphView(ViewLocation, ZoomAmount) ? GraphToLocal(StoredPoint, ViewLocation, ZoomAmount) : StoredPoint;

    }



    static float DistanceSquaredToSegment(const FVector2D& Point, const FVector2D& SegmentStart, const FVector2D& SegmentEnd)

    {

        const FVector2D Segment = SegmentEnd - SegmentStart;

        const float SegmentLengthSquared = Segment.SizeSquared();

        if (SegmentLengthSquared <= KINDA_SMALL_NUMBER)

        {

            return FVector2D::DistSquared(Point, SegmentStart);

        }



        const float T = FMath::Clamp(FVector2D::DotProduct(Point - SegmentStart, Segment) / SegmentLengthSquared, 0.0f, 1.0f);

        const FVector2D ClosestPoint = SegmentStart + Segment * T;

        return FVector2D::DistSquared(Point, ClosestPoint);

    }



    bool StrokeHitsEraser(const FGraphAnnotationStroke& Stroke, const FVector2D& LocalPoint) const

    {

        if (Stroke.Points.Num() == 0)

        {

            return false;

        }



        const float RadiusSquared = FMath::Square(AnnotationEraserRadius);

        FVector2D PreviousPoint = StoredToLocal(Stroke.Points[0]);

        if (FVector2D::DistSquared(LocalPoint, PreviousPoint) <= RadiusSquared)

        {

            return true;

        }



        for (int32 PointIndex = 1; PointIndex < Stroke.Points.Num(); ++PointIndex)

        {

            const FVector2D CurrentPoint = StoredToLocal(Stroke.Points[PointIndex]);

            if (DistanceSquaredToSegment(LocalPoint, PreviousPoint, CurrentPoint) <= RadiusSquared)

            {

                return true;

            }

            PreviousPoint = CurrentPoint;

        }



        return false;

    }



    void EraseAtLocalPoint(const FVector2D& LocalPoint)

    {

        bool bRemovedAny = false;

        for (int32 StrokeIndex = Strokes.Num() - 1; StrokeIndex >= 0; --StrokeIndex)

        {

            if (StrokeHitsEraser(Strokes[StrokeIndex], LocalPoint))

            {

                Strokes.RemoveAt(StrokeIndex);

                bRemovedAny = true;

            }

        }



        if (bRemovedAny)

        {

            Invalidate(EInvalidateWidgetReason::Paint);

        }

    }





    bool bDrawingEnabled = false;

    bool bEraserEnabled = false;

    bool bIsDrawingStroke = false;

    FLinearColor DrawColor = FLinearColor(1.0f, 0.85f, 0.12f, 1.0f);

    TArray<FGraphAnnotationStroke> Strokes;

    TWeakPtr<SGraphEditor> GraphEditor;

};



void SGraphAnnotationOverlay::Construct(const FArguments& InArgs)

{

    GraphEditor = InArgs._GraphEditor;
    PersistenceKey = InArgs._PersistenceKey;
    const FMargin ToolbarPadding = (InArgs._ToolbarPadding.Left == 0.0f && InArgs._ToolbarPadding.Top == 0.0f && InArgs._ToolbarPadding.Right == 0.0f && InArgs._ToolbarPadding.Bottom == 0.0f) ? FMargin(10.0f) : InArgs._ToolbarPadding;



    ChildSlot

    [

        SNew(SOverlay)

        + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)

        [

            SAssignNew(AnnotationCanvas, SGraphAnnotationCanvas)

                .GraphEditor(GraphEditor)

            [

                InArgs._Content.Widget

            ]

        ]

        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(ToolbarPadding)

        [

            BuildToolbar()

        ]

    ];

    if (AnnotationCanvas.IsValid() && !PersistenceKey.IsEmpty())
    {
        AnnotationCanvas->SetAnnotations(LoadPersistentAnnotations(PersistenceKey));
    }

}

TSharedRef<SWidget> SGraphAnnotationOverlay::BuildToolbar()

{

    return SNew(SBorder)

        .BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))

        .BorderBackgroundColor(FLinearColor(0.045f, 0.045f, 0.045f, 0.92f))

        .Padding(FMargin(5.0f, 4.0f))

        [

            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 5.0f, 0.0f)

            [

                SNew(SBorder)

                    .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))

                    .Padding(FMargin(2.0f))

                    [

                        SNew(SCheckBox)

                            .Style(FAppStyle::Get(), TEXT("ToggleButtonCheckbox"))

                            .ToolTipText(TMLoc::Text(TEXT("Toggle freehand notes. Use Save to keep them for this function."), TEXT("Toggle freehand notes. Use Save to keep them for this function.")))

                            .IsChecked_Lambda([this]()

                            {

                                return AnnotationCanvas.IsValid() && AnnotationCanvas->IsDrawingEnabled()

                                    ? ECheckBoxState::Checked

                                    : ECheckBoxState::Unchecked;

                            })

                            .OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)

                            {

                                if (AnnotationCanvas.IsValid())

                                {

                                    AnnotationCanvas->SetDrawingEnabled(NewState == ECheckBoxState::Checked);

                                }

                            })

                            [

                                SNew(SBox)

                                    .HeightOverride(24.0f)

                                    [

                                        SNew(SHorizontalBox)

                                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                                        [

                                            SNew(SImage)

                                                .Image(FAppStyle::GetBrush(TEXT("Icons.Edit")))

                                                .ColorAndOpacity_Lambda([this]()

                                                {

                                                    return AnnotationCanvas.IsValid() && AnnotationCanvas->IsDrawingEnabled()

                                                        ? FSlateColor(FLinearColor(0.32f, 0.62f, 1.0f))

                                                        : FSlateColor(FLinearColor(0.72f, 0.72f, 0.72f));

                                                })

                                        ]

                                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5.0f, 0.0f, 2.0f, 0.0f)

                                        [

                                            SNew(STextBlock)

                                                .Text(TMLoc::Text(TEXT("Notes"), TEXT("Notes")))

                                                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                                                .ColorAndOpacity_Lambda([this]()

                                                {

                                                    return GetToolbarTextColor(AnnotationCanvas.IsValid() && AnnotationCanvas->IsDrawingEnabled());

                                                })

                                        ]

                                    ]

                            ]

                    ]

            ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 5.0f, 0.0f)

            [

                SNew(SBorder)

                    .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))

                    .Padding(FMargin(4.0f, 3.0f))

                    [

                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)

                        [

                            BuildColorButton(FLinearColor(1.0f, 0.85f, 0.12f, 1.0f), TEXT("Yellow"))

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)

                        [

                            BuildColorButton(FLinearColor(0.95f, 0.20f, 0.20f, 1.0f), TEXT("Red"))

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)

                        [

                            BuildColorButton(FLinearColor(0.18f, 0.55f, 1.0f, 1.0f), TEXT("Blue"))

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)

                        [

                            BuildColorButton(FLinearColor(0.25f, 0.85f, 0.35f, 1.0f), TEXT("Green"))

                        ]

                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                        [

                            BuildColorButton(FLinearColor(0.95f, 0.95f, 0.95f, 1.0f), TEXT("White"))

                        ]

                    ]

            ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 5.0f, 0.0f)

            [

                SNew(SBorder)

                    .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))

                    .Padding(FMargin(2.0f))

                    [

                        SNew(SCheckBox)

                            .Style(FAppStyle::Get(), TEXT("ToggleButtonCheckbox"))

                            .ToolTipText(TMLoc::Text(TEXT("Erase notes by dragging over drawn strokes."), TEXT("Erase notes by dragging over drawn strokes.")))

                            .IsChecked_Lambda([this]()

                            {

                                return AnnotationCanvas.IsValid() && AnnotationCanvas->IsEraserEnabled()

                                    ? ECheckBoxState::Checked

                                    : ECheckBoxState::Unchecked;

                            })

                            .OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)

                            {

                                if (AnnotationCanvas.IsValid())

                                {

                                    AnnotationCanvas->SetEraserEnabled(NewState == ECheckBoxState::Checked);

                                }

                            })

                            [

                                SNew(SBox)

                                    .HeightOverride(24.0f)

                                    [

                                        SNew(SHorizontalBox)

                                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                                        [

                                            SNew(SImage)

                                                .Image(FAppStyle::GetBrush(TEXT("Icons.Delete")))

                                                .ColorAndOpacity_Lambda([this]()

                                                {

                                                    return AnnotationCanvas.IsValid() && AnnotationCanvas->IsEraserEnabled()

                                                        ? FSlateColor(FLinearColor(1.0f, 0.62f, 0.25f))

                                                        : FSlateColor(FLinearColor(0.72f, 0.72f, 0.72f));

                                                })

                                        ]

                                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5.0f, 0.0f, 2.0f, 0.0f)

                                        [

                                            SNew(STextBlock)

                                                .Text(TMLoc::Text(TEXT("Eraser"), TEXT("Eraser")))

                                                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 8))

                                                .ColorAndOpacity_Lambda([this]()

                                                {

                                                    return GetToolbarTextColor(AnnotationCanvas.IsValid() && AnnotationCanvas->IsEraserEnabled());

                                                })

                                        ]

                                    ]

                            ]

                    ]

            ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 5.0f, 0.0f)

            [

                SNew(SBorder)

                    .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))

                    .Padding(FMargin(2.0f))

                    [

                        SNew(SButton)

                            .ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))

                            .ContentPadding(FMargin(7.0f, 4.0f))

                            .IsEnabled_Lambda([this]() { return !PersistenceKey.IsEmpty(); })

                            .ToolTipText(TMLoc::Text(TEXT("Save notes for this function and restore them the next time it is opened."), TEXT("Save notes for this function and restore them the next time it is opened.")))

                            .OnClicked_Lambda([this]()

                            {

                                if (AnnotationCanvas.IsValid() && !PersistenceKey.IsEmpty())

                                {

                                    SavePersistentAnnotations(PersistenceKey, AnnotationCanvas->GetAnnotations());
                                    FNotificationInfo Notification(TMLoc::Text(TEXT("Notes saved for this function."), TEXT("Notes saved for this function.")));
                                    Notification.ExpireDuration = 2.0f;
                                    FSlateNotificationManager::Get().AddNotification(Notification);

                                }

                                return FReply::Handled();

                            })

                            [

                                SNew(SHorizontalBox)

                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                                [

                                    SNew(SImage)

                                        .Image(FAppStyle::GetBrush(TEXT("Icons.Save")))

                                        .ColorAndOpacity(FLinearColor(0.35f, 0.72f, 1.0f))

                                ]

                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5.0f, 0.0f, 0.0f, 0.0f)

                                [

                                    SNew(STextBlock)

                                        .Text(TMLoc::Text(TEXT("Save"), TEXT("Save")))

                                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                                ]

                            ]

                    ]

            ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

            [

                SNew(SBorder)

                    .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))

                    .Padding(FMargin(2.0f))

                    [

                        SNew(SButton)

                            .ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))

                            .ContentPadding(FMargin(7.0f, 4.0f))

                            .ToolTipText(TMLoc::Text(TEXT("Clear all notes for this function, including saved notes."), TEXT("Clear all notes for this function, including saved notes.")))

                            .OnClicked_Lambda([this]()

                            {

                                if (AnnotationCanvas.IsValid())

                                {

                                    AnnotationCanvas->ClearAnnotations();
                                    if (!PersistenceKey.IsEmpty())
                                    {
                                        SavePersistentAnnotations(PersistenceKey, TArray<FGraphAnnotationStroke>());
                                    }

                                }

                                return FReply::Handled();

                            })

                            [

                                SNew(SHorizontalBox)

                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)

                                [

                                    SNew(SImage)

                                        .Image(FAppStyle::GetBrush(TEXT("Icons.Delete")))

                                        .ColorAndOpacity(FLinearColor(0.95f, 0.45f, 0.35f))

                                ]

                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5.0f, 0.0f, 0.0f, 0.0f)

                                [

                                    SNew(STextBlock)

                                        .Text(TMLoc::Text(TEXT("Clear"), TEXT("Clear")))

                                        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))

                                ]

                            ]

                    ]

            ]

        ];

}

TSharedRef<SWidget> SGraphAnnotationOverlay::BuildColorButton(const FLinearColor& InColor, const FString& Label)

{

    return SNew(SButton)

        .ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))

        .ContentPadding(FMargin(2.0f))

        .ToolTipText(FText::FromString(FString::Printf(TEXT("Use %s annotation color"), *Label)))

        .OnClicked_Lambda([this, InColor]()

        {

            if (AnnotationCanvas.IsValid())

            {

                AnnotationCanvas->SetDrawColor(InColor);

                AnnotationCanvas->SetDrawingEnabled(true);

            }

            return FReply::Handled();

        })

        [

            SNew(SBox)

                .WidthOverride(20.0f)

                .HeightOverride(20.0f)

                [

                    SNew(SBorder)

                        .Padding(2.0f)

                        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

                        .BorderBackgroundColor_Lambda([this, InColor]()

                        {

                            return AnnotationCanvas.IsValid() && AreAnnotationColorsEqual(AnnotationCanvas->GetDrawColor(), InColor)

                                ? FLinearColor(0.32f, 0.62f, 1.0f, 1.0f)

                                : FLinearColor(0.18f, 0.18f, 0.18f, 1.0f);

                        })

                        [

                            SNew(SBorder)

                                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))

                                .BorderBackgroundColor(InColor)

                        ]

                ]

        ];

}

void SGraphAnnotationCanvas::Construct(const FArguments& InArgs)

{

    ChildSlot

    [

        SNew(SOverlay)

        + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)

        [

            InArgs._Content.Widget

        ]

        + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)

        [

            SAssignNew(PaintLayer, SGraphAnnotationPaintLayer)

                .GraphEditor(InArgs._GraphEditor)

                .Visibility_Lambda([this]()

                {

                    return PaintLayer.IsValid() ? PaintLayer->GetLayerVisibility() : EVisibility::HitTestInvisible;

                })

        ]

    ];

}



void SGraphAnnotationCanvas::SetDrawingEnabled(bool bInDrawingEnabled)

{

    if (PaintLayer.IsValid())

    {

        PaintLayer->SetDrawingEnabled(bInDrawingEnabled);

        if (bInDrawingEnabled)

        {

            FSlateApplication::Get().SetKeyboardFocus(PaintLayer);

        }

    }

}



bool SGraphAnnotationCanvas::IsDrawingEnabled() const

{

    return PaintLayer.IsValid() && PaintLayer->IsDrawingEnabled();

}



void SGraphAnnotationCanvas::SetEraserEnabled(bool bInEraserEnabled)

{

    if (PaintLayer.IsValid())

    {

        PaintLayer->SetEraserEnabled(bInEraserEnabled);

        if (bInEraserEnabled)

        {

            FSlateApplication::Get().SetKeyboardFocus(PaintLayer);

        }

    }

}



bool SGraphAnnotationCanvas::IsEraserEnabled() const

{

    return PaintLayer.IsValid() && PaintLayer->IsEraserEnabled();

}



void SGraphAnnotationCanvas::SetDrawColor(const FLinearColor& InColor)

{

    if (PaintLayer.IsValid())

    {

        PaintLayer->SetDrawColor(InColor);

        PaintLayer->SetDrawingEnabled(true);

        FSlateApplication::Get().SetKeyboardFocus(PaintLayer);

    }

}



FLinearColor SGraphAnnotationCanvas::GetDrawColor() const

{

    return PaintLayer.IsValid() ? PaintLayer->GetDrawColor() : FLinearColor(1.0f, 0.85f, 0.12f, 1.0f);

}



void SGraphAnnotationCanvas::ClearAnnotations()

{

    if (PaintLayer.IsValid())

    {

        PaintLayer->ClearAnnotations();

    }

}


TArray<FGraphAnnotationStroke> SGraphAnnotationCanvas::GetAnnotations() const
{
    return PaintLayer.IsValid() ? PaintLayer->GetAnnotations() : TArray<FGraphAnnotationStroke>();
}

void SGraphAnnotationCanvas::SetAnnotations(const TArray<FGraphAnnotationStroke>& InAnnotations)
{
    if (PaintLayer.IsValid())
    {
        PaintLayer->SetAnnotations(InAnnotations);
    }
}
