#pragma once



#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"



class SGraphAnnotationCanvas;

class SGraphAnnotationPaintLayer;

class SGraphEditor;

struct FGraphAnnotationStroke
{
    FLinearColor Color = FLinearColor::Yellow;
    float Thickness = 3.0f;
    TArray<FVector2D> Points;
};

class SGraphAnnotationOverlay : public SCompoundWidget

{

public:

    SLATE_BEGIN_ARGS(SGraphAnnotationOverlay) {}

        SLATE_DEFAULT_SLOT(FArguments, Content)

        SLATE_ARGUMENT(TWeakPtr<SGraphEditor>, GraphEditor)

        // Stable identifier for annotations that should survive closing/reopening this viewer.
        SLATE_ARGUMENT(FString, PersistenceKey)

        // Lets host widgets keep this floating toolbar clear of their own header controls.
        SLATE_ARGUMENT(FMargin, ToolbarPadding)

    SLATE_END_ARGS()



    void Construct(const FArguments& InArgs);



private:

    TSharedRef<SWidget> BuildToolbar();

    TSharedRef<SWidget> BuildColorButton(const FLinearColor& InColor, const FString& Label);



    TSharedPtr<SGraphAnnotationCanvas> AnnotationCanvas;

    TWeakPtr<SGraphEditor> GraphEditor;

    FString PersistenceKey;

};



class SGraphAnnotationCanvas : public SCompoundWidget

{

public:

    SLATE_BEGIN_ARGS(SGraphAnnotationCanvas) {}

        SLATE_DEFAULT_SLOT(FArguments, Content)

        SLATE_ARGUMENT(TWeakPtr<SGraphEditor>, GraphEditor)

    SLATE_END_ARGS()



    void Construct(const FArguments& InArgs);

    void SetDrawingEnabled(bool bInDrawingEnabled);

    bool IsDrawingEnabled() const;

    void SetEraserEnabled(bool bInEraserEnabled);

    bool IsEraserEnabled() const;

    void SetDrawColor(const FLinearColor& InColor);

    FLinearColor GetDrawColor() const;

    void ClearAnnotations();

    TArray<FGraphAnnotationStroke> GetAnnotations() const;

    void SetAnnotations(const TArray<FGraphAnnotationStroke>& InAnnotations);



private:

    TSharedPtr<SGraphAnnotationPaintLayer> PaintLayer;

};
