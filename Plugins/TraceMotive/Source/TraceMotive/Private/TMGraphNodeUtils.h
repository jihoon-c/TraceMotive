#pragma once



#include "CoreMinimal.h"



class UClass;

class UEdGraphNode;

class UEdGraphPin;

class UK2Node_Variable;



namespace TMGraphNodeUtils

{

    int32 CalculateExecutionOrder(UEdGraphNode* Node);

    FString NormalizeBlueprintClassName(FString ClassName);

    FString GetClassDisplayName(UClass* Class);

    UClass* GetPinClass(UEdGraphPin* Pin);

    FString GetVariableNodeName(UK2Node_Variable* VariableNode);

    void ExtractFunctionTargetInfo(UEdGraphNode* Node, UClass* DefaultOwnerClass, FString& OutObjectName, FString& OutClassName);

}

