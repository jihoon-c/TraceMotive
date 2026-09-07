// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "CPBLevelPlacementDTO.generated.h"

USTRUCT(BlueprintType)
struct CPPBUILDERCEDITOR_API FCPBLevelPlacementDTO : public FTableRowBase
{
	GENERATED_BODY()

	// Designer-authored CSV path to the CPB actor class, typically ACPBInteractorBase, ACPBSequenceManager, or ACPBViewMarker.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Level Placement")
	TSoftClassPtr<AActor> ActorClass;

	// Scenario-facing identifier injected into AActor::Tags so CPB runtime systems can resolve the actor later.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Level Placement")
	FName ActorTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Level Placement")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Level Placement")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Level Placement")
	bool InitialVisible = true;
};
