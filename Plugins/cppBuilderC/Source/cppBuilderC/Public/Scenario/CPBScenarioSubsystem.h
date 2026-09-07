#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/CPBDelegates.h"
#include "CPBScenarioSubsystem.generated.h"

UCLASS()
class CPPBUILDERC_API UCPBScenarioSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "CPB|Scenario")
	FCPBOnScenarioStarted OnScenarioStarted;

	UPROPERTY(BlueprintAssignable, Category = "CPB|Scenario")
	FCPBOnScenarioCompleted OnScenarioCompleted;

	UPROPERTY(BlueprintAssignable, Category = "CPB|Scenario")
	FCPBOnStepChanged OnStepChanged;

	UFUNCTION(BlueprintCallable, Category = "CPB|Scenario")
	void StartScenario(FName ScenarioId, ECPBGameMode InGameMode);

	UFUNCTION(BlueprintCallable, Category = "CPB|Scenario")
	void RequestTransition(ECPBViewTransition Transition, int32 TargetStepIndex);

	UFUNCTION(BlueprintCallable, Category = "CPB|Scenario")
	void CompleteScenario();

	UFUNCTION(BlueprintPure, Category = "CPB|Scenario")
	FName GetActiveScenarioId() const { return ActiveScenarioId; }

	UFUNCTION(BlueprintPure, Category = "CPB|Scenario")
	ECPBGameMode GetGameMode() const { return GameMode; }

	UFUNCTION(BlueprintPure, Category = "CPB|Scenario")
	int32 GetCurrentStepIndex() const { return CurrentStepIndex; }

private:
	UPROPERTY(Transient)
	FName ActiveScenarioId = NAME_None;

	UPROPERTY(Transient)
	ECPBGameMode GameMode = ECPBGameMode::Practice;

	UPROPERTY(Transient)
	int32 CurrentStepIndex = INDEX_NONE;

	bool CanTransitionToStep(int32 TargetStepIndex) const;
};
