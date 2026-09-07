#include "Core/CPBDataRegistry.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Subsystems/CPBContextSubsystem.h"

void UCPBDataRegistry::RegisterDataTables(
	const UObject* WorldContextObject,
	UDataTable* ScenarioDataTable,
	UDataTable* AssemblyDataTable,
	UDataTable* StringDataTable,
	UDataTable* SoundDataTable,
	UDataTable* StudyAssetDataTable)
{
	if (UCPBContextSubsystem* ContextSubsystem = GetContextSubsystem(WorldContextObject))
	{
		ContextSubsystem->RegisterDataTables(ScenarioDataTable, AssemblyDataTable, StringDataTable, SoundDataTable, StudyAssetDataTable);
	}
}

bool UCPBDataRegistry::GetScenarioData(const UObject* WorldContextObject, FName ScenarioId, FCPBScenarioDTO& OutScenarioData)
{
	if (const UCPBContextSubsystem* ContextSubsystem = GetContextSubsystem(WorldContextObject))
	{
		return ContextSubsystem->GetScenarioData(ScenarioId, OutScenarioData);
	}

	return false;
}

bool UCPBDataRegistry::GetAssemblyData(const UObject* WorldContextObject, FName AssemblyId, FCPBAssemblyDTO& OutAssemblyData)
{
	if (const UCPBContextSubsystem* ContextSubsystem = GetContextSubsystem(WorldContextObject))
	{
		return ContextSubsystem->GetAssemblyData(AssemblyId, OutAssemblyData);
	}

	return false;
}

bool UCPBDataRegistry::GetLocalizedText(const UObject* WorldContextObject, FName StringId, ECPBLanguage Language, FText& OutText)
{
	if (const UCPBContextSubsystem* ContextSubsystem = GetContextSubsystem(WorldContextObject))
	{
		return ContextSubsystem->GetLocalizedText(StringId, Language, OutText);
	}

	return false;
}

bool UCPBDataRegistry::GetLocalizedSound(const UObject* WorldContextObject, FName SoundId, ECPBLanguage Language, FCPBSoundDTO& OutSoundData)
{
	if (const UCPBContextSubsystem* ContextSubsystem = GetContextSubsystem(WorldContextObject))
	{
		return ContextSubsystem->GetLocalizedSound(SoundId, Language, OutSoundData);
	}

	return false;
}

bool UCPBDataRegistry::GetStudyAssetData(const UObject* WorldContextObject, FName AssetId, FCPBStudyAssetDTO& OutStudyAssetData)
{
	if (const UCPBContextSubsystem* ContextSubsystem = GetContextSubsystem(WorldContextObject))
	{
		return ContextSubsystem->GetStudyAssetData(AssetId, OutStudyAssetData);
	}

	return false;
}

UCPBContextSubsystem* UCPBDataRegistry::GetContextSubsystem(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UCPBContextSubsystem>() : nullptr;
}
