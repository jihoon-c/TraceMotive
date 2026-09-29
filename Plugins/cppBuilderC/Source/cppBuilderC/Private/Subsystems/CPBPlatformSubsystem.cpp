#include "Subsystems/CPBPlatformSubsystem.h"

#include "Core/CPBGameInstance.h"

void UCPBPlatformSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RefreshActivePlatform();
}

void UCPBPlatformSubsystem::RefreshActivePlatform()
{
	ActivePlatform = ResolveConfiguredPlatform();
	OnPlatformInitialized.Broadcast(ActivePlatform);
}

bool UCPBPlatformSubsystem::IsVRPlatform() const
{
	return IsVRPlatform(ActivePlatform);
}

bool UCPBPlatformSubsystem::IsVRPlatform(ECPBActivePlatform Platform)
{
	return Platform == ECPBActivePlatform::VR;
}

ECPBActivePlatform UCPBPlatformSubsystem::ResolveConfiguredPlatform() const
{
	if (const UCPBGameInstance* CPBGameInstance = Cast<UCPBGameInstance>(GetGameInstance()))
	{
		return CPBGameInstance->GetStartupPlatform();
	}

	return ECPBActivePlatform::CBT;
}
