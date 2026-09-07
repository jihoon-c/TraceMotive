#include "Scenario/CPBScenarioSubsystem.h"

void UCPBScenarioSubsystem::StartScenario(FName ScenarioId, ECPBGameMode InGameMode)
{
	ActiveScenarioId = ScenarioId;
	GameMode = InGameMode;
	CurrentStepIndex = INDEX_NONE;

	OnScenarioStarted.Broadcast(ActiveScenarioId);
	RequestTransition(ECPBViewTransition::None, 0);
}

void UCPBScenarioSubsystem::RequestTransition(ECPBViewTransition Transition, int32 TargetStepIndex)
{
	if (!CanTransitionToStep(TargetStepIndex))
	{
		return;
	}

	CurrentStepIndex = TargetStepIndex;
	OnStepChanged.Broadcast(CurrentStepIndex);
}

void UCPBScenarioSubsystem::CompleteScenario()
{
	OnScenarioCompleted.Broadcast(ActiveScenarioId);
	ActiveScenarioId = NAME_None;
	CurrentStepIndex = INDEX_NONE;
}

bool UCPBScenarioSubsystem::CanTransitionToStep(int32 TargetStepIndex) const
{
	return !ActiveScenarioId.IsNone() && TargetStepIndex >= 0;
}
