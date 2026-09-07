#include "Player/CPBMainPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Controller.h"
#include "Interaction/CPBInteractable.h"
#include "Platform/CPBInteractionSourceComponent.h"
#include "Platform/CPBInteractionSourceInterface.h"
#include "Platform/CPBViewModeComponent.h"
#include "Platform/CPBViewModeInterface.h"
#include "Player/CPBPlayerController.h"

ACPBMainPawn::ACPBMainPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootSceneComponent);

	ScreenCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("ScreenCamera"));
	ScreenCameraComponent->SetupAttachment(RootSceneComponent);
	ScreenCameraComponent->ComponentTags.Add(TEXT("CPB_ViewCamera"));

	ViewModeComponent = CreateDefaultSubobject<UCPBViewModeComponent>(TEXT("ViewModeComponent"));
	InteractionSourceComponent = CreateDefaultSubobject<UCPBInteractionSourceComponent>(TEXT("InteractionSourceComponent"));
}

void ACPBMainPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateHoverTarget();
}

void ACPBMainPawn::BeginPlay()
{
	Super::BeginPlay();
	BindControllerDelegates(GetController());
}

void ACPBMainPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	BindControllerDelegates(NewController);
}

void ACPBMainPawn::UnPossessed()
{
	UnbindControllerDelegates(GetController());
	Super::UnPossessed();
}

FTransform ACPBMainPawn::GetCurrentViewTransform() const
{
	const ICPBViewModeInterface* ViewMode = Cast<ICPBViewModeInterface>(ViewModeComponent);
	return ViewMode ? ViewMode->GetViewTransform() : GetActorTransform();
}

void ACPBMainPawn::HandleInteractPressed()
{
	if (InteractionSourceComponent)
	{
		InteractionSourceComponent->SetInteractionTriggered(true);
	}

	AActor* HitActor = TraceInteractableActor();
	if (HitActor && HitActor->GetClass()->ImplementsInterface(UCPBInteractable::StaticClass()))
	{
		if (ICPBInteractable::Execute_CanInteract(HitActor))
		{
			ICPBInteractable::Execute_OnInteract(HitActor, GetController());
		}
	}
}

void ACPBMainPawn::HandleInteractReleased()
{
	if (InteractionSourceComponent)
	{
		InteractionSourceComponent->SetInteractionTriggered(false);
	}
}

void ACPBMainPawn::UpdateHoverTarget()
{
	AActor* HitActor = TraceInteractableActor();
	if (HitActor == LastHoveredActor)
	{
		return;
	}

	if (LastHoveredActor && LastHoveredActor->GetClass()->ImplementsInterface(UCPBInteractable::StaticClass()))
	{
		ICPBInteractable::Execute_OnHoverEnd(LastHoveredActor);
	}

	LastHoveredActor = HitActor;

	if (LastHoveredActor && LastHoveredActor->GetClass()->ImplementsInterface(UCPBInteractable::StaticClass()))
	{
		ICPBInteractable::Execute_OnHoverBegin(LastHoveredActor);
	}
}

AActor* ACPBMainPawn::TraceInteractableActor() const
{
	const ICPBInteractionSourceInterface* InteractionSource = Cast<ICPBInteractionSourceInterface>(InteractionSourceComponent);
	if (!InteractionSource)
	{
		return nullptr;
	}

	FVector RayStart = FVector::ZeroVector;
	FVector RayDirection = FVector::ForwardVector;
	if (!InteractionSource->GetInteractionRay(RayStart, RayDirection))
	{
		return nullptr;
	}

	const float TraceDistance = InteractionSourceComponent ? InteractionSourceComponent->GetInteractionRange() : 10000.0f;
	const FVector RayEnd = RayStart + (RayDirection.GetSafeNormal() * TraceDistance);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CPBMainPawnInteractionTrace), false, this);
	const UWorld* World = GetWorld();
	if (World && World->LineTraceSingleByChannel(HitResult, RayStart, RayEnd, InteractionTraceChannel, QueryParams))
	{
		AActor* HitActor = HitResult.GetActor();
		if (HitActor && HitActor->GetClass()->ImplementsInterface(UCPBInteractable::StaticClass()))
		{
			return HitActor;
		}
	}

	return nullptr;
}

void ACPBMainPawn::BindControllerDelegates(AController* NewController)
{
	if (ACPBPlayerController* CPBPlayerController = Cast<ACPBPlayerController>(NewController))
	{
		CPBPlayerController->OnInteractPressed.AddUniqueDynamic(this, &ACPBMainPawn::HandleInteractPressed);
		CPBPlayerController->OnInteractReleased.AddUniqueDynamic(this, &ACPBMainPawn::HandleInteractReleased);
	}
}

void ACPBMainPawn::UnbindControllerDelegates(AController* OldController)
{
	if (ACPBPlayerController* CPBPlayerController = Cast<ACPBPlayerController>(OldController))
	{
		CPBPlayerController->OnInteractPressed.RemoveDynamic(this, &ACPBMainPawn::HandleInteractPressed);
		CPBPlayerController->OnInteractReleased.RemoveDynamic(this, &ACPBMainPawn::HandleInteractReleased);
	}
}
