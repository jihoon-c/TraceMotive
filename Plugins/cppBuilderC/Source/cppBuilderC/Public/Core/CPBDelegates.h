#pragma once

#include "CoreMinimal.h"
#include "Core/CPBTypes.h"
#include "CPBDelegates.generated.h"

class AActor;
class AController;
class UMaterialInstanceDynamic;

UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPBOnPlatformInitialized, ECPBActivePlatform, ActivePlatform);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPBOnScenarioStarted, FName, ScenarioId);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPBOnScenarioCompleted, FName, ScenarioId);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPBOnStepChanged, int32, StepIndex);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCPBOnViewTransitionRequested, ECPBViewTransition, Transition, FName, CameraViewId);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCPBOnInteractionDispatched, AActor*, InteractableActor, AController*, InstigatorController);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCPBOnHoverChanged, AActor*, InteractableActor, bool, bIsHovered);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCPBOnStudyAssetOpened, FName, AssetId, ECPBStudyAssetType, AssetType);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPBOnStudyAssetClosed, FName, AssetId);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPBOnStudyAssetRenderRequested, UMaterialInstanceDynamic*, TargetMaterial);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCPBOnInteractInput);
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCPBOnVisibilityTransactionApplied, int32, StepIndex);
