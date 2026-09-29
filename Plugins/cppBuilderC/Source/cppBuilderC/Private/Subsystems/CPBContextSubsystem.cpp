#include "Subsystems/CPBContextSubsystem.h"

#include "Engine/DataTable.h"

void UCPBContextSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RebuildCaches();
}

void UCPBContextSubsystem::Deinitialize()
{
	ScenarioCache.Reset();
	AssemblyCache.Reset();
	StringCache.Reset();
	SoundCache.Reset();
	StudyAssetCache.Reset();

	Super::Deinitialize();
}

void UCPBContextSubsystem::RegisterDataTables(
	UDataTable* InScenarioDataTable,
	UDataTable* InAssemblyDataTable,
	UDataTable* InStringDataTable,
	UDataTable* InSoundDataTable,
	UDataTable* InStudyAssetDataTable)
{
	ScenarioDataTable = InScenarioDataTable;
	AssemblyDataTable = InAssemblyDataTable;
	StringDataTable = InStringDataTable;
	SoundDataTable = InSoundDataTable;
	StudyAssetDataTable = InStudyAssetDataTable;

	RebuildCaches();
}

bool UCPBContextSubsystem::GetScenarioData(FName ScenarioId, FCPBScenarioDTO& OutScenarioData) const
{
	if (const FCPBScenarioDTO* FoundData = ScenarioCache.Find(ScenarioId))
	{
		OutScenarioData = *FoundData;
		return true;
	}

	return false;
}

bool UCPBContextSubsystem::GetAssemblyData(FName AssemblyId, FCPBAssemblyDTO& OutAssemblyData) const
{
	if (const FCPBAssemblyDTO* FoundData = AssemblyCache.Find(AssemblyId))
	{
		OutAssemblyData = *FoundData;
		return true;
	}

	return false;
}

bool UCPBContextSubsystem::GetLocalizedText(FName StringId, ECPBLanguage Language, FText& OutText) const
{
	if (const FCPBStringDTO* FoundData = StringCache.Find(MakeLocalizedKey(StringId, Language)))
	{
		OutText = FoundData->Text;
		return true;
	}

	if (Language != ECPBLanguage::Korean)
	{
		if (const FCPBStringDTO* FallbackData = StringCache.Find(MakeLocalizedKey(StringId, ECPBLanguage::Korean)))
		{
			OutText = FallbackData->Text;
			return true;
		}
	}

	return false;
}

bool UCPBContextSubsystem::GetLocalizedSound(FName SoundId, ECPBLanguage Language, FCPBSoundDTO& OutSoundData) const
{
	if (const FCPBSoundDTO* FoundData = SoundCache.Find(MakeLocalizedKey(SoundId, Language)))
	{
		OutSoundData = *FoundData;
		return true;
	}

	if (Language != ECPBLanguage::Korean)
	{
		if (const FCPBSoundDTO* FallbackData = SoundCache.Find(MakeLocalizedKey(SoundId, ECPBLanguage::Korean)))
		{
			OutSoundData = *FallbackData;
			return true;
		}
	}

	return false;
}

bool UCPBContextSubsystem::GetStudyAssetData(FName AssetId, FCPBStudyAssetDTO& OutStudyAssetData) const
{
	if (const FCPBStudyAssetDTO* FoundData = StudyAssetCache.Find(AssetId))
	{
		OutStudyAssetData = *FoundData;
		return true;
	}

	return false;
}

void UCPBContextSubsystem::RebuildCaches()
{
	ScenarioCache.Reset();
	AssemblyCache.Reset();
	StringCache.Reset();
	SoundCache.Reset();
	StudyAssetCache.Reset();

	if (ScenarioDataTable)
	{
		for (const TPair<FName, uint8*>& RowPair : ScenarioDataTable->GetRowMap())
		{
			const FCPBScenarioDTO* Row = reinterpret_cast<const FCPBScenarioDTO*>(RowPair.Value);
			FCPBScenarioDTO RowCopy = *Row;
			if (RowCopy.ScenarioId.IsNone())
			{
				RowCopy.ScenarioId = RowPair.Key;
			}
			ScenarioCache.Add(RowCopy.ScenarioId, RowCopy);
		}
	}

	if (AssemblyDataTable)
	{
		for (const TPair<FName, uint8*>& RowPair : AssemblyDataTable->GetRowMap())
		{
			const FCPBAssemblyDTO* Row = reinterpret_cast<const FCPBAssemblyDTO*>(RowPair.Value);
			FCPBAssemblyDTO RowCopy = *Row;
			if (RowCopy.AssemblyId.IsNone())
			{
				RowCopy.AssemblyId = RowPair.Key;
			}
			AssemblyCache.Add(RowCopy.AssemblyId, RowCopy);
		}
	}

	if (StringDataTable)
	{
		for (const TPair<FName, uint8*>& RowPair : StringDataTable->GetRowMap())
		{
			const FCPBStringDTO* Row = reinterpret_cast<const FCPBStringDTO*>(RowPair.Value);
			FCPBStringDTO RowCopy = *Row;
			if (RowCopy.StringId.IsNone())
			{
				RowCopy.StringId = RowPair.Key;
			}
			StringCache.Add(MakeLocalizedKey(RowCopy.StringId, RowCopy.Language), RowCopy);
		}
	}

	if (SoundDataTable)
	{
		for (const TPair<FName, uint8*>& RowPair : SoundDataTable->GetRowMap())
		{
			const FCPBSoundDTO* Row = reinterpret_cast<const FCPBSoundDTO*>(RowPair.Value);
			FCPBSoundDTO RowCopy = *Row;
			if (RowCopy.SoundId.IsNone())
			{
				RowCopy.SoundId = RowPair.Key;
			}
			SoundCache.Add(MakeLocalizedKey(RowCopy.SoundId, RowCopy.Language), RowCopy);
		}
	}

	if (StudyAssetDataTable)
	{
		for (const TPair<FName, uint8*>& RowPair : StudyAssetDataTable->GetRowMap())
		{
			const FCPBStudyAssetDTO* Row = reinterpret_cast<const FCPBStudyAssetDTO*>(RowPair.Value);
			FCPBStudyAssetDTO RowCopy = *Row;
			if (RowCopy.AssetId.IsNone())
			{
				RowCopy.AssetId = RowPair.Key;
			}
			StudyAssetCache.Add(RowCopy.AssetId, RowCopy);
		}
	}
}

FName UCPBContextSubsystem::MakeLocalizedKey(FName RowId, ECPBLanguage Language)
{
	return FName(*FString::Printf(TEXT("%s_%d"), *RowId.ToString(), static_cast<int32>(Language)));
}
