// Fill out your copyright notice in the Description page of Project Settings.







#include "VisualRefGraphDefinition.h"



#include "Editor.h"
#include "ConnectionDrawingPolicy.h"
#include "Rendering/DrawElements.h"

#include "Kismet2/KismetEditorUtilities.h"

#include "Subsystems/AssetEditorSubsystem.h"
#include "SourceCodeNavigation.h"
#include "HAL/PlatformProcess.h"



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
            DrawDirectedConnection(FVector2f(Start), FVector2f(End), Params);
        }

        virtual void DrawSplineWithArrow(const FVector2f& Start, const FVector2f& End, const FConnectionParams& Params) override
        {
            DrawDirectedConnection(Start, End, Params);
        }

        virtual void DrawSplineWithArrow(const FGeometry& StartGeom, const FGeometry& EndGeom, const FConnectionParams& Params) override
        {
            // The visual-reference pins live on horizontal strips at the top/bottom of nodes.
            // The stock graph policy assumes side pins, which makes its spline curl around the
            // nodes and leaves every arrow pointing right. Use the actual port centers instead.
            DrawDirectedConnection(
                FVector2f(FGeometryHelper::CenterOf(StartGeom)),
                FVector2f(FGeometryHelper::CenterOf(EndGeom)),
                Params);
        }

        virtual FVector2D ComputeSplineTangent(const FVector2D& Start, const FVector2D& End) const override
        {
            return End - Start;
        }

        virtual FVector2f ComputeSplineTangent(const FVector2f& Start, const FVector2f& End) const override
        {
            return End - Start;
        }

    private:
        void DrawDirectedConnection(const FVector2f& Start, const FVector2f& Target, const FConnectionParams& Params)
        {
            const FVector2f Direction = Target - Start;
            if (Direction.IsNearlyZero())
            {
                return;
            }

            const FVector2f UnitDirection = Direction.GetSafeNormal();
            const float ArrowHalfLength = ArrowImage ? FVector2f(ArrowRadius).X : 0.0f;
            const FVector2f ArrowCenter = Target - (UnitDirection * ArrowHalfLength);

            // A zero-tangent Hermite spline is geometrically a straight segment. This deliberately
            // avoids the graph editor's side-pin curve, which could arc across a declaration node.
            FSlateDrawElement::MakeDrawSpaceSpline(
                DrawElementsList, WireLayerID, Start, FVector2f::ZeroVector, ArrowCenter, FVector2f::ZeroVector,
                Params.WireThickness, ESlateDrawEffect::None, Params.WireColor);

            if (ArrowImage)
            {
                const FVector2f ArrowSize = FVector2f(ArrowRadius) * 2.0f;
                const FVector2f ArrowTopLeft = ArrowCenter - FVector2f(ArrowRadius);
                const float ArrowAngle = FMath::Atan2(Direction.Y, Direction.X);
                FSlateDrawElement::MakeRotatedBox(
                    DrawElementsList, ArrowLayerID,
                    FPaintGeometry(ArrowTopLeft, ArrowSize, 1.0f), ArrowImage,
                    ESlateDrawEffect::None, ArrowAngle, TOptional<FVector2f>(),
                    FSlateDrawElement::RelativeToElement, Params.WireColor);
            }
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

void UVisualRefNode::JumpToReference(const FVisualReferenceInfo& Reference)
{
    if (Reference.bIsCppSource && !Reference.SourceFilePath.IsEmpty())
    {
        if (!FSourceCodeNavigation::OpenSourceFile(Reference.SourceFilePath, Reference.SourceLine, Reference.SourceColumn))
        {
            FPlatformProcess::LaunchFileInDefaultExternalApplication(*Reference.SourceFilePath);
        }
        return;
    }

    JumpToReference(Reference.SourceNode);
}
