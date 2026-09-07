// Source/TraceMotive/Public/SVisualRefNode.h



#pragma once



#include "CoreMinimal.h"

#include "SGraphNode.h"

#include "VisualRefGraphDefinition.h"

class SVerticalBox;
class SHorizontalBox;



#ifndef EVisualRefViewMode_DEFINED

#define EVisualRefViewMode_DEFINED



enum class EVisualRefViewMode : uint8

{

    List,

    Tree

};

#endif



class SVisualRefNode : public SGraphNode

{

public:

    SLATE_BEGIN_ARGS(SVisualRefNode) {}

    SLATE_END_ARGS()



    void Construct(const FArguments& InArgs, UVisualRefNode* InNode);

    virtual void UpdateGraphNode() override;

    virtual void CreatePinWidgets() override;



    // Mouse event overrides used by resizable graph nodes.

    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

    virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

    virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

    virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;



    // Variable reference filters.

    static bool bShowGet;

    static bool bShowSet;



    // Function reference filters.

    static bool bShowCalls;

    static bool bShowDefinitions;
    static bool bAllowNodeMovement;



    static EVisualRefViewMode CurrentViewMode;



private:

    // Definition input pins are distributed across this port strip to keep incoming UML arrows distinct.
    TSharedPtr<SHorizontalBox> BottomNodeBox;

    int32 SelectedReferenceIndex = -1;



    // Resizable node state.

    bool bIsResizing = false;

    bool bHasResizableContent = false;

    float InitialHeight = 300.0f;

    float CurrentNodeHeight = 300.0f;

    FVector2D ResizeStartPos;



    const float MinNodeHeight = 100.0f;

    const float MaxNodeHeight = 1200.0f;

    const float ResizeHandleHeight = 10.0f;

};
