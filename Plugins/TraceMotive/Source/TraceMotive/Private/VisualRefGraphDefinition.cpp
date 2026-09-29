// Fill out your copyright notice in the Description page of Project Settings.







#include "VisualRefGraphDefinition.h"



#include "Editor.h"
#include "ConnectionDrawingPolicy.h"
#include "Rendering/DrawElements.h"

#include "Kismet2/KismetEditorUtilities.h"

#include "Subsystems/AssetEditorSubsystem.h"



namespace
{
    class FVisualRefConnectionDrawingPolicy final : public FConnectionDrawingPolicy
    {
    public:
        FVisualRefConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor,
            const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements)
            : FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
        {
        }

        virtual void DrawSplineWithArrow(const FVector2D& Start, const FVector2D& End, const FConnectionParams& Params) override
        {
            const FVector2D Direction = End - Start;
            if (Direction.IsNearlyZero())
            {
                return;
            }

            // A zero-tangent Hermite spline is geometrically a straight segment. This deliberately
            // avoids the graph editor's side-pin curve, which could arc across a declaration node.
            FSlateDrawElement::MakeDrawSpaceSpline(
                DrawElementsList, WireLayerID, Start, FVector2D::ZeroVector, End, FVector2D::ZeroVector,
                Params.WireThickness, ESlateDrawEffect::None, Params.WireColor);

            if (ArrowImage)
            {
                const FVector2D ArrowSize = ArrowRadius * 2.0f;
                const FVector2D ArrowTopLeft = End - ArrowRadius;
                const float ArrowAngle = FMath::Atan2(Direction.Y, Direction.X);
                FSlateDrawElement::MakeRotatedBox(
                    DrawElementsList, ArrowLayerID,
                    FPaintGeometry(ArrowTopLeft, ArrowSize, 1.0f), ArrowImage,
                    ESlateDrawEffect::None, ArrowAngle, TOptional<FVector2f>(),
                    FSlateDrawElement::RelativeToElement, Params.WireColor);
            }
        }

        virtual FVector2D ComputeSplineTangent(const FVector2D& Start, const FVector2D& End) const override
        {
            return End - Start;
        }
    };
}

FConnectionDrawingPolicy* UVisualRefSchema::CreateConnectionDrawingPolicy(
    int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect,
    FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj) const
{
    return new FVisualRefConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements);
}

void UVisualRefNode::JumpToDefinition() const

{

    if (DefinitionGraphNode)

    {

        FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(DefinitionGraphNode);

        return;

    }



    if (SourceAsset && GEditor)

    {

        GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(SourceAsset);

    }

}



void UVisualRefNode::JumpToReference(UEdGraphNode* TargetNode)

{

    if (TargetNode)

    {

        FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(TargetNode);

    }

}
