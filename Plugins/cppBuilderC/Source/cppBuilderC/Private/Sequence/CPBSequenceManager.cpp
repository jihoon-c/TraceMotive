#include "Sequence/CPBSequenceManager.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Scenario/CPBScenarioSubsystem.h"
#include "Subsystems/CPBNarrationSubsystem.h"
#include "Subsystems/CPBUISubsystem.h"
#include "TimerManager.h"

ACPBSequenceManager::ACPBSequenceManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ACPBSequenceManager::BeginPlay()
{
	Super::BeginPlay();
	RebuildSequenceRegistry();
	if (UCPBNarrationSubsystem* NarrationSubsystem = GetGameInstance()->GetSubsystem<UCPBNarrationSubsystem>())
	{
		NarrationSubsystem->SetSequenceManager(this);
	}
	RebuildInitialVisibilityCache();
	PublishSequenceListToUI();
}

void ACPBSequenceManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearAutoAdvanceTimer();
	Super::EndPlay(EndPlayReason);
}

void ACPBSequenceManager::SetActiveSequence(ACPBSequence* InActiveSequence)
{
	ClearAutoAdvanceTimer();
	ActiveSequence = InActiveSequence;
	ResolveSequenceIndex(ActiveSequence, CurrentSequenceIndex);
	CurrentStepIndex = INDEX_NONE;
	RebuildInitialVisibilityCache();
	PublishSequenceListToUI();
}

void ACPBSequenceManager::RegisterSequence(ACPBSequence* Sequence)
{
	if (!Sequence)
	{
		return;
	}

	RegisteredSequences.AddUnique(Sequence);
	RebuildSequenceRegistry();
}

void ACPBSequenceManager::RebuildSequenceRegistry()
{
	SequenceIndexById.Reset();

	for (int32 SequenceIndex = RegisteredSequences.Num() - 1; SequenceIndex >= 0; --SequenceIndex)
	{
		if (!RegisteredSequences[SequenceIndex])
		{
			RegisteredSequences.RemoveAt(SequenceIndex);
		}
	}

	for (int32 SequenceIndex = 0; SequenceIndex < RegisteredSequences.Num(); ++SequenceIndex)
	{
		const ACPBSequence* Sequence = RegisteredSequences[SequenceIndex];
		if (!Sequence)
		{
			continue;
		}

		const FName SequenceId = Sequence->GetSequenceId();
		if (!SequenceId.IsNone() && !SequenceIndexById.Contains(SequenceId))
		{
			SequenceIndexById.Add(SequenceId, SequenceIndex);
		}
	}

	ResolveSequenceIndex(ActiveSequence, CurrentSequenceIndex);
	PublishSequenceListToUI();
}

void ACPBSequenceManager::SortRegisteredSequencesBySortOrder()
{
	RegisteredSequences.Sort([](const TObjectPtr<ACPBSequence>& Left, const TObjectPtr<ACPBSequence>& Right)
	{
		if (!Left)
		{
			return false;
		}

		if (!Right)
		{
			return true;
		}

		if (Left->GetSortOrder() == Right->GetSortOrder())
		{
			return Left->GetSequenceId().LexicalLess(Right->GetSequenceId());
		}

		return Left->GetSortOrder() < Right->GetSortOrder();
	});

	RebuildSequenceRegistry();
}

bool ACPBSequenceManager::SetActiveSequenceById(FName SequenceId)
{
	RebuildSequenceRegistry();

	const int32* FoundSequenceIndex = SequenceIndexById.Find(SequenceId);
	return FoundSequenceIndex ? SetActiveSequenceByIndex(*FoundSequenceIndex) : false;
}

bool ACPBSequenceManager::SetActiveSequenceByIndex(int32 SequenceIndex)
{
	if (!RegisteredSequences.IsValidIndex(SequenceIndex) || !RegisteredSequences[SequenceIndex])
	{
		return false;
	}

	ClearAutoAdvanceTimer();
	ActiveSequence = RegisteredSequences[SequenceIndex];
	CurrentSequenceIndex = SequenceIndex;
	CurrentStepIndex = INDEX_NONE;
	RebuildInitialVisibilityCache();
	PublishSequenceListToUI();
	return true;
}

bool ACPBSequenceManager::ApplyStep(int32 StepIndex)
{
	if (!ActiveSequence || !ActiveSequence->IsValidStepIndex(StepIndex))
	{
		return false;
	}

	ClearAutoAdvanceTimer();
	ApplyVisibilityUpToStep(StepIndex);
	CurrentStepIndex = StepIndex;

	FCPBSequenceStep Step;
	ActiveSequence->GetStep(StepIndex, Step);

	OnVisibilityTransactionApplied.Broadcast(StepIndex);
	OnViewTransitionRequested.Broadcast(Step.ViewTransition, Step.CameraViewId);
	PublishSequenceListToUI();

	if (UCPBScenarioSubsystem* ScenarioSubsystem = GetScenarioSubsystem())
	{
		ScenarioSubsystem->RequestTransition(Step.ViewTransition, StepIndex);
	}

	PlayNarrationForCurrentStep();
	StartAutoAdvanceIfNeeded(Step);
	return true;
}

bool ACPBSequenceManager::AdvanceStep()
{
	if (!ActiveSequence)
	{
		return false;
	}

	const int32 NextStepIndex = CurrentStepIndex + 1;
	if (ActiveSequence->IsValidStepIndex(NextStepIndex))
	{
		return ApplyStep(NextStepIndex);
	}

	return AdvanceSequence(true);
}

bool ACPBSequenceManager::AdvanceSequence(bool bApplyFirstStep)
{
	const int32 NextSequenceIndex = CurrentSequenceIndex + 1;
	if (!SetActiveSequenceByIndex(NextSequenceIndex))
	{
		return false;
	}

	return !bApplyFirstStep || ApplyStep(0);
}

bool ACPBSequenceManager::RewindStep()
{
	const int32 PreviousStepIndex = CurrentStepIndex - 1;
	if (ActiveSequence && ActiveSequence->IsValidStepIndex(PreviousStepIndex))
	{
		return ApplyStep(PreviousStepIndex);
	}

	return RewindSequence(true);
}

bool ACPBSequenceManager::RewindSequence(bool bApplyLastStep)
{
	const int32 PreviousSequenceIndex = CurrentSequenceIndex - 1;
	if (!SetActiveSequenceByIndex(PreviousSequenceIndex))
	{
		return false;
	}

	return !bApplyLastStep || ApplyStep(ActiveSequence->GetNumSteps() - 1);
}

FName ACPBSequenceManager::GetCurrentSequenceId() const
{
	return ActiveSequence ? ActiveSequence->GetSequenceId() : NAME_None;
}

ACPBSequence* ACPBSequenceManager::GetSequenceAt(int32 SequenceIndex) const
{
	return RegisteredSequences.IsValidIndex(SequenceIndex) ? RegisteredSequences[SequenceIndex] : nullptr;
}

TArray<ACPBSequence*> ACPBSequenceManager::GetRegisteredSequences() const
{
	TArray<ACPBSequence*> Sequences;
	Sequences.Reserve(RegisteredSequences.Num());

	for (ACPBSequence* Sequence : RegisteredSequences)
	{
		if (Sequence)
		{
			Sequences.Add(Sequence);
		}
	}

	return Sequences;
}

void ACPBSequenceManager::RebuildInitialVisibilityCache()
{
	InitialVisibilityByActor.Reset();
	TransactionHistory.Reset();

	if (!ActiveSequence)
	{
		return;
	}

	for (const FCPBSequenceStep& Step : ActiveSequence->GetSteps())
	{
		for (AActor* Actor : Step.HidingActors)
		{
			if (Actor)
			{
				InitialVisibilityByActor.FindOrAdd(Actor, IsActorVisible(Actor));
			}
		}

		for (AActor* Actor : Step.UnhidingActors)
		{
			if (Actor)
			{
				InitialVisibilityByActor.FindOrAdd(Actor, IsActorVisible(Actor));
			}
		}
	}
}

void ACPBSequenceManager::ApplyVisibilityUpToStep(int32 TargetStepIndex)
{
	if (!ActiveSequence)
	{
		return;
	}

	for (const TPair<TWeakObjectPtr<AActor>, bool>& InitialPair : InitialVisibilityByActor)
	{
		if (AActor* Actor = InitialPair.Key.Get())
		{
			SetActorVisible(Actor, InitialPair.Value);
		}
	}

	TransactionHistory.Reset();
	for (int32 StepIndex = 0; StepIndex <= TargetStepIndex; ++StepIndex)
	{
		FCPBSequenceStep Step;
		if (!ActiveSequence->GetStep(StepIndex, Step))
		{
			continue;
		}

		FCPBVisibilityTransaction Transaction;
		Transaction.SequenceId = ActiveSequence->GetSequenceId();
		Transaction.StepIndex = StepIndex;

		for (AActor* Actor : Step.HidingActors)
		{
			ApplyVisibilityChange(Actor, false, Transaction);
		}

		for (AActor* Actor : Step.UnhidingActors)
		{
			ApplyVisibilityChange(Actor, true, Transaction);
		}

		TransactionHistory.Add(Transaction);
	}
}

void ACPBSequenceManager::ApplyVisibilityChange(AActor* Actor, bool bNewVisible, FCPBVisibilityTransaction& Transaction)
{
	if (!Actor)
	{
		return;
	}

	const bool bPreviousVisible = IsActorVisible(Actor);
	SetActorVisible(Actor, bNewVisible);

	FCPBVisibilityChange Change;
	Change.Actor = Actor;
	Change.PreviousState = bPreviousVisible ? ECPBVisibilityState::Visible : ECPBVisibilityState::Hidden;
	Change.NewState = bNewVisible ? ECPBVisibilityState::Visible : ECPBVisibilityState::Hidden;
	Transaction.Changes.Add(Change);
}

void ACPBSequenceManager::SetActorVisible(AActor* Actor, bool bVisible) const
{
	if (!Actor)
	{
		return;
	}

	Actor->SetActorHiddenInGame(!bVisible);
}

bool ACPBSequenceManager::IsActorVisible(const AActor* Actor) const
{
	return Actor && !Actor->IsHidden();
}

void ACPBSequenceManager::StartAutoAdvanceIfNeeded(const FCPBSequenceStep& Step)
{
	const UCPBScenarioSubsystem* ScenarioSubsystem = GetScenarioSubsystem();
	if (!ScenarioSubsystem || ScenarioSubsystem->GetGameMode() != ECPBGameMode::Video)
	{
		return;
	}

	const float StepDelay = Step.AutoAdvanceDelayOverride > 0.0f ? Step.AutoAdvanceDelayOverride : Step.AnimationLength;
	const float SafeDelay = StepDelay > 0.0f ? StepDelay : ZeroLengthAutoAdvanceDelay;
	const float TotalDelay = SafeDelay + Step.FadeDuration;

	GetWorldTimerManager().SetTimer(
		AutoAdvanceTimerHandle,
		this,
		&ACPBSequenceManager::HandleAutoAdvanceElapsed,
		TotalDelay,
		false);
}

void ACPBSequenceManager::ClearAutoAdvanceTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
	}
}

void ACPBSequenceManager::HandleAutoAdvanceElapsed()
{
	if (!ActiveSequence)
	{
		return;
	}

	const int32 NextStepIndex = CurrentStepIndex + 1;
	if (ActiveSequence->IsValidStepIndex(NextStepIndex))
	{
		ApplyStep(NextStepIndex);
		return;
	}

	if (AdvanceSequence(true))
	{
		return;
	}

	if (UCPBScenarioSubsystem* ScenarioSubsystem = GetScenarioSubsystem())
	{
		ScenarioSubsystem->CompleteScenario();
	}
}

bool ACPBSequenceManager::ResolveSequenceIndex(ACPBSequence* Sequence, int32& OutSequenceIndex) const
{
	OutSequenceIndex = INDEX_NONE;

	if (!Sequence)
	{
		return false;
	}

	for (int32 SequenceIndex = 0; SequenceIndex < RegisteredSequences.Num(); ++SequenceIndex)
	{
		if (RegisteredSequences[SequenceIndex] == Sequence)
		{
			OutSequenceIndex = SequenceIndex;
			return true;
		}
	}

	return false;
}

TArray<FCPBSequenceListItem> ACPBSequenceManager::BuildSequenceListItems() const
{
	TArray<FCPBSequenceListItem> Items;
	int32 GlobalStepIndex = 0;

	for (int32 SequenceIndex = 0; SequenceIndex < RegisteredSequences.Num(); ++SequenceIndex)
	{
		const ACPBSequence* Sequence = RegisteredSequences[SequenceIndex];
		if (!Sequence)
		{
			continue;
		}

		for (int32 StepIndex = 0; StepIndex < Sequence->GetNumSteps(); ++StepIndex)
		{
			FCPBSequenceStep Step;
			Sequence->GetStep(StepIndex, Step);

			FCPBSequenceListItem Item;
			Item.SequenceId = Sequence->GetSequenceId();
			Item.StepId = Step.StepId;
			Item.GlobalStepIndex = GlobalStepIndex;
			Item.LocalStepIndex = StepIndex;
			Item.DisplayText = Step.StepId.IsNone()
				? FText::FromString(FString::Printf(TEXT("Step %d"), StepIndex + 1))
				: FText::FromName(Step.StepId);

			if (SequenceIndex < CurrentSequenceIndex || (SequenceIndex == CurrentSequenceIndex && StepIndex < CurrentStepIndex))
			{
				Item.State = ECPBSequenceElementState::Completed;
			}
			else if (SequenceIndex == CurrentSequenceIndex && StepIndex == CurrentStepIndex)
			{
				Item.State = ECPBSequenceElementState::Active;
			}
			else if (SequenceIndex == CurrentSequenceIndex && StepIndex == CurrentStepIndex + 1)
			{
				Item.State = ECPBSequenceElementState::Ready;
			}
			else
			{
				Item.State = ECPBSequenceElementState::Locked;
			}

			Items.Add(Item);
			++GlobalStepIndex;
		}
	}

	return Items;
}

void ACPBSequenceManager::PublishSequenceListToUI() const
{
	if (!bPublishSequenceListToUI)
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	UCPBUISubsystem* UISubsystem = GameInstance ? GameInstance->GetSubsystem<UCPBUISubsystem>() : nullptr;
	if (UISubsystem)
	{
		UISubsystem->SetSequenceListItems(BuildSequenceListItems());
	}
}

void ACPBSequenceManager::PlayNarrationForCurrentStep() const
{
	if (!bPlayNarrationOnStepApplied || !ActiveSequence)
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	UCPBNarrationSubsystem* NarrationSubsystem = GameInstance ? GameInstance->GetSubsystem<UCPBNarrationSubsystem>() : nullptr;
	if (NarrationSubsystem)
	{
		NarrationSubsystem->PlayNarrationForStep(ActiveSequence->GetSequenceId(), CurrentStepIndex);
	}
}

UCPBScenarioSubsystem* ACPBSequenceManager::GetScenarioSubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UCPBScenarioSubsystem>() : nullptr;
}
