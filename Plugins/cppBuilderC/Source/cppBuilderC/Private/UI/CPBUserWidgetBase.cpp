#include "UI/CPBUserWidgetBase.h"

#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Subsystems/CPBPlatformSubsystem.h"
#include "Subsystems/CPBUISubsystem.h"

bool UCPBUserWidgetBase::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();
	SetupWidgetRenderMode();
	return bSuperInitialized;
}

void UCPBUserWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (!IsViewportPresentationAllowed() && IsInViewport())
	{
		SetupWidgetRenderMode();
		RemoveFromParent();
		UE_LOG(LogTemp, Warning, TEXT("CPB removed screen-space widget from viewport for VR platform: %s"), *GetName());
	}
}

void UCPBUserWidgetBase::AddToViewport(int32 ZOrder)
{
	if (!IsViewportPresentationAllowed())
	{
		SetupWidgetRenderMode();
		UE_LOG(LogTemp, Warning, TEXT("CPB blocked screen-space AddToViewport for VR platform: %s"), *GetName());
		return;
	}

	Super::AddToViewport(ZOrder);
}

bool UCPBUserWidgetBase::AddToPlayerScreen(int32 ZOrder)
{
	if (!IsViewportPresentationAllowed())
	{
		SetupWidgetRenderMode();
		UE_LOG(LogTemp, Warning, TEXT("CPB blocked screen-space AddToPlayerScreen for VR platform: %s"), *GetName());
		return false;
	}

	return Super::AddToPlayerScreen(ZOrder);
}

void UCPBUserWidgetBase::AddToCPBViewport(int32 ZOrder)
{
	AddToViewport(ZOrder);
}

void UCPBUserWidgetBase::SetupWidgetRenderMode_Implementation()
{
	if (!IsVRPlatformActive())
	{
		WidgetRenderMode = ECPBWidgetRenderMode::Viewport;
		return;
	}

	WidgetRenderMode = TargetRenderMaterial ? ECPBWidgetRenderMode::Material : ECPBWidgetRenderMode::WorldSpace;

	if (WorldWidgetComponent)
	{
		WorldWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
		WorldWidgetComponent->SetWidget(this);
	}
}

void UCPBUserWidgetBase::SetWorldWidgetComponent(UWidgetComponent* InWidgetComponent)
{
	WorldWidgetComponent = InWidgetComponent;
	SetupWidgetRenderMode();
}

void UCPBUserWidgetBase::SetTargetRenderMaterial(UMaterialInstanceDynamic* InTargetRenderMaterial)
{
	TargetRenderMaterial = InTargetRenderMaterial;
	SetupWidgetRenderMode();
}

bool UCPBUserWidgetBase::IsViewportPresentationAllowed() const
{
	return !IsVRPlatformActive();
}

bool UCPBUserWidgetBase::IsVRPlatformActive() const
{
	const UCPBPlatformSubsystem* PlatformSubsystem = GetCPBPlatformSubsystem();
	return PlatformSubsystem && PlatformSubsystem->IsVRPlatform();
}

UCPBUISubsystem* UCPBUserWidgetBase::GetCPBUISubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UCPBUISubsystem>() : nullptr;
}

UCPBPlatformSubsystem* UCPBUserWidgetBase::GetCPBPlatformSubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UCPBPlatformSubsystem>() : nullptr;
}
