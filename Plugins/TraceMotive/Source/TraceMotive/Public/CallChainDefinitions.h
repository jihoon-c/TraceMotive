// Fill out your copyright notice in the Description page of Project Settings.







#pragma once







#include "CoreMinimal.h"



#include "EdGraph/EdGraphNode.h"



#include "CallChainDefinitions.generated.h"







/**



 * 



 */







 // 추적된 호출 노드 정보



USTRUCT()



struct FCallChainNodeInfo



{



    GENERATED_BODY()







    UPROPERTY() FString AssetName;



    UPROPERTY() FString GraphName;



    UPROPERTY() FString NodeName;







    // 이 노드가 호출하고 있는 대상(자식)의 GUID (역추적 연결 고리)



    FGuid ChildNodeGuid;







    // 이 노드 자신의 GUID



    FGuid MyGuid;







    UPROPERTY() TObjectPtr<UEdGraphNode> GraphNode;







    // 깊이 (0 = 타겟 함수, 1 = 타겟을 호출한 곳, 2 = 1을 호출한 곳...)



    int32 Depth = 0;



};







class TRACEMOTIVE_API CallChainDefinitions



{



public:



	CallChainDefinitions();



	~CallChainDefinitions();



};







DECLARE_DELEGATE_OneParam(FOnChainNodeFound, const FCallChainNodeInfo&);



DECLARE_DELEGATE(FOnChainSearchFinished);