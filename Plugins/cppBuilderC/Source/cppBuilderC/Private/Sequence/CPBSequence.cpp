#include "Sequence/CPBSequence.h"

ACPBSequence::ACPBSequence()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool ACPBSequence::IsValidStepIndex(int32 StepIndex) const
{
	return Steps.IsValidIndex(StepIndex);
}

bool ACPBSequence::GetStep(int32 StepIndex, FCPBSequenceStep& OutStep) const
{
	if (!IsValidStepIndex(StepIndex))
	{
		return false;
	}

	OutStep = Steps[StepIndex];
	return true;
}
