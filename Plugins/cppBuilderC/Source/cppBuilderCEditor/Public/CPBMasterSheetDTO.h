// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CPBMasterSheetDTO.generated.h"

USTRUCT(BlueprintType)
struct CPPBUILDERCEDITOR_API FCPBScenarioFlowDTO : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Scenario Flow")
	FName SequenceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Scenario Flow")
	FName PhaseId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Scenario Flow")
	FText PhaseName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Scenario Flow")
	int32 StepOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Scenario Flow")
	FName TargetViewTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Scenario Flow")
	FName TargetInteractorTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Scenario Flow")
	FName RequiredToolName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Scenario Flow")
	FName InteractionType = NAME_None;
};

USTRUCT(BlueprintType)
struct CPPBUILDERCEDITOR_API FCPBActionEventDTO : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Action Event")
	FName EventId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Action Event")
	FName OwnerSequence = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Action Event")
	FName Trigger = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Action Event")
	float Time = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Action Event")
	FName ActionType = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Action Event")
	FName TargetTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Action Event")
	FName AssetName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Action Event")
	FText NarrationKOR;
};

USTRUCT(BlueprintType)
struct CPPBUILDERCEDITOR_API FCPBDictionaryDTO : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Dictionary")
	FName StringId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Dictionary")
	FText KOR;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Dictionary")
	FText ENG;
};
