// Source/TraceMotive/Public/VisualRefGraphDefinition.h

#pragma once

#include "CoreMinimal.h"

#include "EdGraph/EdGraph.h"

#include "EdGraph/EdGraphNode.h"

#include "EdGraph/EdGraphSchema.h"

#include "VisualRefGraphDefinition.generated.h"

class FSlateRect;
class FSlateWindowElementList;



UENUM()

enum class EVisualRefType : uint8

{

    Variable,

    Function,

    Dispatcher

};



USTRUCT()

struct FVisualReferenceInfo

{

    GENERATED_BODY()



    UPROPERTY()

    FString GraphName;



    UPROPERTY()

    FString NodeName;



    UPROPERTY()

    TObjectPtr<UEdGraphNode> SourceNode;



    UPROPERTY()

    bool bIsSetter = false;



    UPROPERTY()

    bool bIsSource = false;



    UPROPERTY()

    bool bIsSearchSubject = false;



    // 함수 관련 추가 정보

    UPROPERTY()

    bool bIsFunctionCall = false;



    UPROPERTY()

    bool bIsFunctionDefinition = false;



    UPROPERTY()

    int32 CallOrder = 0; // 호출 순서 (실행 핀 기준)



    UPROPERTY()

    FString TargetObjectName;



    UPROPERTY()

    FString TargetClassName;



    UPROPERTY()

    FString MatchReason;

};



UCLASS()

class UVisualRefSchema : public UEdGraphSchema

{

    GENERATED_BODY()

public:

    // Reference links enter declarations from below with an upward-facing arrowhead.
    virtual class FConnectionDrawingPolicy* CreateConnectionDrawingPolicy(
        int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect,
        class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const override;

};


UCLASS()

class UVisualRefNode : public UEdGraphNode

{

    GENERATED_BODY()

public:

    UPROPERTY()

    TObjectPtr<UObject> SourceAsset;



    UPROPERTY()

    FText AssetName;



    UPROPERTY()

    bool bIsDefinitionNode = false;



    UPROPERTY()

    TObjectPtr<UEdGraphNode> DefinitionGraphNode;



    UPROPERTY()

    TArray<FVisualReferenceInfo> References;



    UPROPERTY()

    EVisualRefType RefType = EVisualRefType::Variable;



    void CreateVisualPin(EEdGraphPinDirection Direction, FName PinName)

    {

        FEdGraphPinType PinType;

        PinType.PinCategory = "VisualReference";

        CreatePin(Direction, PinType, PinName);

    }



    virtual void JumpToDefinition() const override;

    void JumpToReference(UEdGraphNode* TargetNode);



    virtual bool CanUserDeleteNode() const override { return false; }

    virtual bool CanDuplicateNode() const override { return false; }

    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override { return AssetName; }

};
