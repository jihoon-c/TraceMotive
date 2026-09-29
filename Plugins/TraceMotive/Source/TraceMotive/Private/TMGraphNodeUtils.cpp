#include "TMGraphNodeUtils.h"



#include "EdGraph/EdGraphNode.h"

#include "EdGraph/EdGraphPin.h"

#include "EdGraphSchema_K2.h"

#include "Engine/Blueprint.h"

#include "K2Node_CallFunction.h"

#include "K2Node_Variable.h"



int32 TMGraphNodeUtils::CalculateExecutionOrder(UEdGraphNode* Node)

{

    if (!Node)

    {

        return 0;

    }



    int32 Order = 0;

    UEdGraphNode* CurrentNode = Node;



    while (CurrentNode)

    {

        UEdGraphPin* ExecInputPin = nullptr;

        for (UEdGraphPin* Pin : CurrentNode->Pins)

        {

            if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)

            {

                ExecInputPin = Pin;

                break;

            }

        }



        if (!ExecInputPin || ExecInputPin->LinkedTo.Num() == 0)

        {

            break;

        }



        CurrentNode = ExecInputPin->LinkedTo[0]->GetOwningNode();

        Order++;



        if (Order > 1000)

        {

            break;

        }

    }



    return Order;

}



FString TMGraphNodeUtils::NormalizeBlueprintClassName(FString ClassName)

{

    ClassName.RemoveFromStart(TEXT("SKEL_"));

    ClassName.RemoveFromStart(TEXT("REINST_"));

    ClassName.RemoveFromEnd(TEXT("_C"));



    int32 SuffixIndex = INDEX_NONE;

    if (ClassName.FindLastChar(TEXT('_'), SuffixIndex))

    {

        const FString Suffix = ClassName.Mid(SuffixIndex + 1);

        if (Suffix.IsNumeric())

        {

            ClassName.LeftInline(SuffixIndex);

            ClassName.RemoveFromEnd(TEXT("_C"));

        }

    }



    return ClassName;

}



FString TMGraphNodeUtils::GetClassDisplayName(UClass* Class)

{

    if (!Class)

    {

        return FString();

    }



    if (UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy))

    {

        return Blueprint->GetName();

    }



    FString ClassName = NormalizeBlueprintClassName(Class->GetName());

    ClassName.RemoveFromStart(TEXT("A"));

    ClassName.RemoveFromStart(TEXT("U"));

    return ClassName;

}



UClass* TMGraphNodeUtils::GetPinClass(UEdGraphPin* Pin)

{

    if (!Pin || !Pin->PinType.PinSubCategoryObject.IsValid())

    {

        return nullptr;

    }



    return Cast<UClass>(Pin->PinType.PinSubCategoryObject.Get());

}



FString TMGraphNodeUtils::GetVariableNodeName(UK2Node_Variable* VariableNode)

{

    if (!VariableNode)

    {

        return FString();

    }



    FString DisplayName = VariableNode->GetNodeTitle(ENodeTitleType::ListView).ToString();

    DisplayName.RemoveFromStart(TEXT("Get "));

    DisplayName.RemoveFromStart(TEXT("Set "));



    if (DisplayName.IsEmpty())

    {

        DisplayName = FName::NameToDisplayString(VariableNode->GetVarName().ToString(), false);

    }



    return DisplayName;

}



void TMGraphNodeUtils::ExtractFunctionTargetInfo(

    UEdGraphNode* Node,

    UClass* DefaultOwnerClass,

    FString& OutObjectName,

    FString& OutClassName)

{

    OutObjectName.Empty();

    OutClassName.Empty();



    UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);

    if (!CallNode)

    {

        return;

    }



    UClass* TargetClass = nullptr;

    UEdGraphPin* SelfPin = CallNode->FindPin(UEdGraphSchema_K2::PN_Self);

    if (SelfPin)

    {

        TargetClass = GetPinClass(SelfPin);



        for (UEdGraphPin* LinkedPin : SelfPin->LinkedTo)

        {

            if (!LinkedPin)

            {

                continue;

            }



            if (UClass* LinkedClass = GetPinClass(LinkedPin))

            {

                TargetClass = LinkedClass;

            }



            UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();

            if (UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(LinkedNode))

            {

                OutObjectName = GetVariableNodeName(VariableNode);

            }

            else if (LinkedNode)

            {

                OutObjectName = LinkedNode->GetNodeTitle(ENodeTitleType::ListView).ToString();

                OutObjectName.RemoveFromStart(TEXT("Get "));

                OutObjectName.RemoveFromStart(TEXT("Set "));

            }



            if (!OutObjectName.IsEmpty())

            {

                break;

            }

        }



        if (SelfPin->LinkedTo.Num() == 0)

        {

            OutObjectName = TEXT("Self");

        }

    }



    if (!TargetClass)

    {

        if (UFunction* TargetFunction = CallNode->GetTargetFunction())

        {

            TargetClass = TargetFunction->GetOwnerClass();

        }

    }



    if (!TargetClass)

    {

        TargetClass = CallNode->FunctionReference.GetMemberParentClass();

    }



    if (!TargetClass)

    {

        TargetClass = DefaultOwnerClass;

    }



    if (TargetClass)

    {

        OutClassName = GetClassDisplayName(TargetClass);

    }

}

