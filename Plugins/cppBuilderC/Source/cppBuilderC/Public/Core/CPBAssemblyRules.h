#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/CPBTypes.h"
#include "CPBAssemblyRules.generated.h"

UCLASS()
class CPPBUILDERC_API UCPBAssemblyRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "CPB|Assembly")
	static ECPBAssemblyRuleResult EvaluateAssemblyRule(const FCPBAssemblyDTO& AssemblyData, FName SelectedAssemblyId);
};
