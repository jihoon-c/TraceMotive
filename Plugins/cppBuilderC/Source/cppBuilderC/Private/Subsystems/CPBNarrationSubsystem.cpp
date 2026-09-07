#include "Subsystems/CPBNarrationSubsystem.h"

#include "Components/AudioComponent.h"
#include "Core/CPBDataRegistry.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sequence/CPBSequenceManager.h"
#include "Subsystems/CPBUISubsystem.h"
#include "TimerManager.h"

void UCPBNarrationSubsystem::Deinitialize()
{
	StopNarration();
	NarrationById.Reset();
	NarrationIdByStepKey.Reset();
	Super::Deinitialize();
}

void UCPBNarrationSubsystem::RegisterNarrationDataTable(UDataTable* InNarrationDataTable)
{
	NarrationDataTable = InNarrationDataTable;
	RebuildNarrationCache();
}

void UCPBNarrationSubsystem::SetSequenceManager(ACPBSequenceManager* InSequenceManager)
{
	SequenceManager = InSequenceManager;
}

bool UCPBNarrationSubsystem::PlayNarration(FName NarrationId)
{
	const FCPBNarrationDTO* NarrationData = NarrationById.Find(NarrationId);
	return NarrationData ? PlayNarrationData(*NarrationData) : false;
}

bool UCPBNarrationSubsystem::PlayNarrationForStep(FName SequenceId, int32 StepIndex)
{
	const FName* NarrationId = NarrationIdByStepKey.Find(MakeStepKey(SequenceId, StepIndex));
	return NarrationId ? PlayNarration(*NarrationId) : false;
}

void UCPBNarrationSubsystem::StopNarration()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FallbackFinishTimerHandle);
	}

	if (ActiveAudioComponent)
	{
		ActiveAudioComponent->OnAudioFinished.RemoveDynamic(this, &UCPBNarrationSubsystem::HandleNarrationFinished);
		ActiveAudioComponent->Stop();
		ActiveAudioComponent = nullptr;
	}

	ActiveNarrationId = NAME_None;
	bActiveNarrationAutoAdvance = false;
}

void UCPBNarrationSubsystem::RebuildNarrationCache()
{
	NarrationById.Reset();
	NarrationIdByStepKey.Reset();

	if (!NarrationDataTable)
	{
		return;
	}

	for (const TPair<FName, uint8*>& RowPair : NarrationDataTable->GetRowMap())
	{
		const FCPBNarrationDTO* Row = reinterpret_cast<const FCPBNarrationDTO*>(RowPair.Value);
		FCPBNarrationDTO RowCopy = *Row;
		if (RowCopy.NarrationId.IsNone())
		{
			RowCopy.NarrationId = RowPair.Key;
		}

		NarrationById.Add(RowCopy.NarrationId, RowCopy);
		if (!RowCopy.SequenceId.IsNone() && RowCopy.StepIndex != INDEX_NONE)
		{
			NarrationIdByStepKey.Add(MakeStepKey(RowCopy.SequenceId, RowCopy.StepIndex), RowCopy.NarrationId);
		}
	}
}

bool UCPBNarrationSubsystem::PlayNarrationData(const FCPBNarrationDTO& NarrationData)
{
	StopNarration();

	ActiveNarrationId = NarrationData.NarrationId;
	bActiveNarrationAutoAdvance = NarrationData.bAutoAdvanceSequenceOnFinished;

	const FText SubtitleText = ResolveSubtitleText(NarrationData);
	if (UCPBUISubsystem* UISubsystem = GetGameInstance()->GetSubsystem<UCPBUISubsystem>())
	{
		UISubsystem->SetSubtitle(SubtitleText);
	}
	OnNarrationChanged.Broadcast(ActiveNarrationId, SubtitleText);

	USoundBase* Sound = NarrationData.NarrationSound.LoadSynchronous();
	if (Sound)
	{
		ActiveAudioComponent = UGameplayStatics::SpawnSound2D(GetWorld(), Sound);
		if (ActiveAudioComponent)
		{
			ActiveAudioComponent->OnAudioFinished.AddDynamic(this, &UCPBNarrationSubsystem::HandleNarrationFinished);
			return true;
		}
	}

	const float FallbackDuration = FMath::Max(NarrationData.FallbackDuration, 0.01f);
	GetWorld()->GetTimerManager().SetTimer(
		FallbackFinishTimerHandle,
		this,
		&UCPBNarrationSubsystem::HandleNarrationFinished,
		FallbackDuration,
		false);

	return true;
}

FText UCPBNarrationSubsystem::ResolveSubtitleText(const FCPBNarrationDTO& NarrationData) const
{
	if (!NarrationData.SubtitleOverride.IsEmpty())
	{
		return NarrationData.SubtitleOverride;
	}

	FText LocalizedText;
	if (!NarrationData.SubtitleStringId.IsNone() &&
		UCPBDataRegistry::GetLocalizedText(GetGameInstance(), NarrationData.SubtitleStringId, ECPBLanguage::Korean, LocalizedText))
	{
		return LocalizedText;
	}

	return FText::GetEmpty();
}

FName UCPBNarrationSubsystem::MakeStepKey(FName SequenceId, int32 StepIndex)
{
	return FName(*FString::Printf(TEXT("%s_%d"), *SequenceId.ToString(), StepIndex));
}

void UCPBNarrationSubsystem::HandleNarrationFinished()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FallbackFinishTimerHandle);
	}

	const FName FinishedNarrationId = ActiveNarrationId;
	if (ActiveAudioComponent)
	{
		ActiveAudioComponent->OnAudioFinished.RemoveDynamic(this, &UCPBNarrationSubsystem::HandleNarrationFinished);
		ActiveAudioComponent = nullptr;
	}

	OnNarrationFinished.Broadcast(FinishedNarrationId);

	const bool bShouldAutoAdvance = bActiveNarrationAutoAdvance;
	ActiveNarrationId = NAME_None;
	bActiveNarrationAutoAdvance = false;

	if (bShouldAutoAdvance && SequenceManager.IsValid())
	{
		SequenceManager->AdvanceStep();
	}
}
