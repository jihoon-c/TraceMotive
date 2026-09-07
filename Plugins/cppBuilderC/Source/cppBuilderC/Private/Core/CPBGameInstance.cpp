#include "Core/CPBGameInstance.h"

#include "Subsystems/CPBPlatformSubsystem.h"

void UCPBGameInstance::SetStartupPlatform(ECPBActivePlatform InStartupPlatform)
{
	StartupPlatform = InStartupPlatform;

	if (UCPBPlatformSubsystem* PlatformSubsystem = GetSubsystem<UCPBPlatformSubsystem>())
	{
		PlatformSubsystem->RefreshActivePlatform();
	}
}
