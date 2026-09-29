#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/CPBUIDelegates.h"
#include "CPBNarrationSubsystem.generated.h"

class ACPBSequenceManager;
class UAudioComponent;
class UDataTable;

UCLASS()
class CPPBUILDERC_API UCPBNarrationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "CPB|Narration")
	FCPBOnNarrationChanged OnNarrationChanged;

	UPROPERTY(BlueprintAssignable, Category = "CPB|Narration")
	FCPBOnNarrationFinished OnNarrationFinished;

	UFUNCTION(BlueprintCallable, Category = "CPB|Narration")
	void RegisterNarrationDataTable(UDataTable* InNarrationDataTable);

	UFUNCTION(BlueprintCallable, Category = "CPB|Narration")
	void SetSequenceManager(ACPBSequenceManager* InSequenceManager);

	UFUNCTION(BlueprintCallable, Category = "CPB|Narration")
	bool PlayNarration(FName NarrationId);

	UFUNCTION(BlueprintCallable, Category = "CPB|Narration")
	bool PlayNarrationForStep(FName SequenceId, int32 StepIndex);

	UFUNCTION(BlueprintCallable, Category = "CPB|Narration")
	void StopNarration();

	UFUNCTION(BlueprintPure, Category = "CPB|Narration")
	FName GetActiveNarrationId() const { return ActiveNarrationId; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> NarrationDataTable;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveAudioComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<ACPBSequenceManager> SequenceManager;

	TMap<FName, FCPBNarrationDTO> NarrationById;
	TMap<FName, FName> NarrationIdByStepKey;

	FName ActiveNarrationId = NAME_None;
	bool bActiveNarrationAutoAdvance = false;
	FTimerHandle FallbackFinishTimerHandle;

	void RebuildNarrationCache();
	bool PlayNarrationData(const FCPBNarrationDTO& NarrationData);
	FText ResolveSubtitleText(const FCPBNarrationDTO& NarrationData) const;
	static FName MakeStepKey(FName SequenceId, int32 StepIndex);

	UFUNCTION()
	void HandleNarrationFinished();
};
