#pragma once

#include "CoreMinimal.h"
#include "Core/CPBDelegates.h"
#include "GameFramework/Actor.h"
#include "Sequence/CPBSequence.h"
#include "CPBSequenceManager.generated.h"

USTRUCT(BlueprintType)
struct CPPBUILDERC_API FCPBVisibilityChange
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "CPB|Sequence")
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "CPB|Sequence")
	ECPBVisibilityState PreviousState = ECPBVisibilityState::Visible;

	UPROPERTY(BlueprintReadOnly, Category = "CPB|Sequence")
	ECPBVisibilityState NewState = ECPBVisibilityState::Visible;
};

USTRUCT(BlueprintType)
struct CPPBUILDERC_API FCPBVisibilityTransaction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "CPB|Sequence")
	FName SequenceId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "CPB|Sequence")
	int32 StepIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "CPB|Sequence")
	TArray<FCPBVisibilityChange> Changes;
};

class UCPBScenarioSubsystem;

UCLASS(Blueprintable)
class CPPBUILDERC_API ACPBSequenceManager : public AActor
{
	GENERATED_BODY()

public:
	ACPBSequenceManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(BlueprintAssignable, Category = "CPB|Sequence")
	FCPBOnVisibilityTransactionApplied OnVisibilityTransactionApplied;

	UPROPERTY(BlueprintAssignable, Category = "CPB|Sequence")
	FCPBOnViewTransitionRequested OnViewTransitionRequested;

	UFUNCTION(BlueprintCallable, Category = "CPB|Sequence")
	void SetActiveSequence(ACPBSequence* InActiveSequence);

	UFUNCTION(BlueprintCallable, Category = "CPB|Sequence")
	void RegisterSequence(ACPBSequence* Sequence);

	UFUNCTION(BlueprintCallable, Category = "CPB|Sequence")
	void RebuildSequenceRegistry();

	UFUNCTION(BlueprintCallable, Category = "CPB|Sequence")
	void SortRegisteredSequencesBySortOrder();

	UFUNCTION(BlueprintCallable, Category = "CPB|Sequence")
	bool SetActiveSequenceById(FName SequenceId);

	UFUNCTION(BlueprintCallable, Category = "CPB|Sequence")
	bool SetActiveSequenceByIndex(int32 SequenceIndex);

	UFUNCTION(BlueprintCallable, Category = "CPB|Sequence")
	bool ApplyStep(int32 StepIndex);

	UFUNCTION(BlueprintCallable, Category = "CPB|Sequence")
	bool AdvanceStep();

	UFUNCTION(BlueprintCallable, Category = "CPB|Sequence")
	bool AdvanceSequence(bool bApplyFirstStep = true);

	UFUNCTION(BlueprintCallable, Category = "CPB|Sequence")
	bool RewindStep();

	UFUNCTION(BlueprintCallable, Category = "CPB|Sequence")
	bool RewindSequence(bool bApplyLastStep = true);

	UFUNCTION(BlueprintPure, Category = "CPB|Sequence")
	int32 GetCurrentStepIndex() const { return CurrentStepIndex; }

	UFUNCTION(BlueprintPure, Category = "CPB|Sequence")
	int32 GetCurrentSequenceIndex() const { return CurrentSequenceIndex; }

	UFUNCTION(BlueprintPure, Category = "CPB|Sequence")
	FName GetCurrentSequenceId() const;

	UFUNCTION(BlueprintPure, Category = "CPB|Sequence")
	int32 GetSequenceCount() const { return RegisteredSequences.Num(); }

	UFUNCTION(BlueprintPure, Category = "CPB|Sequence")
	ACPBSequence* GetSequenceAt(int32 SequenceIndex) const;

	UFUNCTION(BlueprintPure, Category = "CPB|Sequence")
	TArray<ACPBSequence*> GetRegisteredSequences() const;

	UFUNCTION(BlueprintPure, Category = "CPB|Sequence")
	TArray<FCPBVisibilityTransaction> GetTransactionHistory() const { return TransactionHistory; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sequence", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<ACPBSequence>> RegisteredSequences;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sequence", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ACPBSequence> ActiveSequence;

	UPROPERTY(EditDefaultsOnly, Category = "CPB|Sequence", meta = (ClampMin = "0.01"))
	float ZeroLengthAutoAdvanceDelay = 0.1f;

	UPROPERTY(EditAnywhere, Category = "CPB|Sequence|UI")
	bool bPublishSequenceListToUI = true;

	UPROPERTY(EditAnywhere, Category = "CPB|Sequence|Narration")
	bool bPlayNarrationOnStepApplied = true;

	UPROPERTY(Transient)
	int32 CurrentStepIndex = INDEX_NONE;

	UPROPERTY(Transient)
	int32 CurrentSequenceIndex = INDEX_NONE;

	TMap<FName, int32> SequenceIndexById;
	TMap<TWeakObjectPtr<AActor>, bool> InitialVisibilityByActor;
	TArray<FCPBVisibilityTransaction> TransactionHistory;
	FTimerHandle AutoAdvanceTimerHandle;

	bool ResolveSequenceIndex(ACPBSequence* Sequence, int32& OutSequenceIndex) const;
	TArray<FCPBSequenceListItem> BuildSequenceListItems() const;
	void PublishSequenceListToUI() const;
	void PlayNarrationForCurrentStep() const;
	void RebuildInitialVisibilityCache();
	void ApplyVisibilityUpToStep(int32 TargetStepIndex);
	void ApplyVisibilityChange(AActor* Actor, bool bNewVisible, FCPBVisibilityTransaction& Transaction);
	void SetActorVisible(AActor* Actor, bool bVisible) const;
	bool IsActorVisible(const AActor* Actor) const;
	void StartAutoAdvanceIfNeeded(const FCPBSequenceStep& Step);
	void ClearAutoAdvanceTimer();
	void HandleAutoAdvanceElapsed();
	UCPBScenarioSubsystem* GetScenarioSubsystem() const;
};
