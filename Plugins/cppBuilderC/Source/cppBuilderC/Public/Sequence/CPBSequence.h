#pragma once

#include "CoreMinimal.h"
#include "Core/CPBTypes.h"
#include "GameFramework/Actor.h"
#include "CPBSequence.generated.h"

USTRUCT(BlueprintType)
struct CPPBUILDERC_API FCPBSequenceStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sequence")
	FName StepId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sequence")
	TArray<TObjectPtr<AActor>> HidingActors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sequence")
	TArray<TObjectPtr<AActor>> UnhidingActors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sequence", meta = (ClampMin = "0.0"))
	float AnimationLength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sequence", meta = (ClampMin = "0.0"))
	float AutoAdvanceDelayOverride = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sequence")
	ECPBViewTransition ViewTransition = ECPBViewTransition::Fade;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sequence")
	FName CameraViewId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sequence", meta = (ClampMin = "0.0"))
	float FadeDuration = 0.25f;
};

UCLASS(Blueprintable)
class CPPBUILDERC_API ACPBSequence : public AActor
{
	GENERATED_BODY()

public:
	ACPBSequence();

	UFUNCTION(BlueprintPure, Category = "CPB|Sequence")
	FName GetSequenceId() const { return SequenceId; }

	UFUNCTION(BlueprintPure, Category = "CPB|Sequence")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "CPB|Sequence")
	int32 GetSortOrder() const { return SortOrder; }

	UFUNCTION(BlueprintPure, Category = "CPB|Sequence")
	int32 GetNumSteps() const { return Steps.Num(); }

	UFUNCTION(BlueprintPure, Category = "CPB|Sequence")
	bool IsValidStepIndex(int32 StepIndex) const;

	UFUNCTION(BlueprintPure, Category = "CPB|Sequence")
	bool GetStep(int32 StepIndex, FCPBSequenceStep& OutStep) const;

	const TArray<FCPBSequenceStep>& GetSteps() const { return Steps; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sequence", meta = (AllowPrivateAccess = "true"))
	FName SequenceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sequence", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sequence", meta = (AllowPrivateAccess = "true"))
	int32 SortOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sequence", meta = (AllowPrivateAccess = "true"))
	TArray<FCPBSequenceStep> Steps;
};
