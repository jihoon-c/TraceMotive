// Source/TraceMotive/Public/SCallChainViewer.h



#pragma once



#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

#include "FunctionCallChainTracer.h" // Call chain result/node definitions



class SCallChainViewer : public SCompoundWidget

{

public:

    SLATE_BEGIN_ARGS(SCallChainViewer) {}

    SLATE_END_ARGS()



    void Construct(const FArguments& InArgs, const FCallChainResult& InResult);



private:

    // UI actions

    void JumpToNode(const struct FCallChainNode& NodeInfo);

    TSharedRef<SWidget> CreatePathWidget(const FCallChainPath& Path, int32 PathIndex);

    TSharedRef<SWidget> CreateChainNodeWidget(const FCallChainNode& Node, bool bIsLast);

    TSharedRef<SWidget> CreateArrowWidget();



    // Event handlers

    FReply OnPathHeaderClicked(int32 PathIndex);

    FReply OnExpandAllClicked();

    FReply OnCollapseAllClicked();

    FReply OnCopyReportClicked();

    EVisibility GetPathContentVisibility(int32 PathIndex) const;

    const FSlateBrush* GetPathExpandArrowImage(int32 PathIndex) const;



    // Navigation helpers

    void OnNodeClicked(TWeakObjectPtr<class UEdGraphNode> NodeWeakPtr);

    FString BuildReportText() const;



private:

    FCallChainResult Result;



    // Per-path expansion state; true means expanded.

    TArray<bool> PathExpansionStates;

};

