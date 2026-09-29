#include "Core/CPBAssemblyRules.h"

ECPBAssemblyRuleResult UCPBAssemblyRules::EvaluateAssemblyRule(const FCPBAssemblyDTO& AssemblyData, FName SelectedAssemblyId)
{
	switch (AssemblyData.RuleType)
	{
	case ECPBAssemblyRuleType::EnvironmentClick:
		return ECPBAssemblyRuleResult::EnvironmentClick;

	case ECPBAssemblyRuleType::AlwaysFail:
		return ECPBAssemblyRuleResult::Failure;

	case ECPBAssemblyRuleType::Custom:
		return ECPBAssemblyRuleResult::Ignored;

	case ECPBAssemblyRuleType::MatchAssemblyId:
	default:
		return AssemblyData.ExpectedTargetId == SelectedAssemblyId
			? ECPBAssemblyRuleResult::Success
			: ECPBAssemblyRuleResult::Failure;
	}
}
