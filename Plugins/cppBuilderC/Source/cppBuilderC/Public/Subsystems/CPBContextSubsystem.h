#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/CPBTypes.h"
#include "CPBContextSubsystem.generated.h"

class UDataTable;

UCLASS()
class CPPBUILDERC_API UCPBContextSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "CPB|Data")
	void RegisterDataTables(
		UDataTable* InScenarioDataTable,
		UDataTable* InAssemblyDataTable,
		UDataTable* InStringDataTable,
		UDataTable* InSoundDataTable,
		UDataTable* InStudyAssetDataTable);

	UFUNCTION(BlueprintPure, Category = "CPB|Data")
	bool GetScenarioData(FName ScenarioId, FCPBScenarioDTO& OutScenarioData) const;

	UFUNCTION(BlueprintPure, Category = "CPB|Data")
	bool GetAssemblyData(FName AssemblyId, FCPBAssemblyDTO& OutAssemblyData) const;

	UFUNCTION(BlueprintPure, Category = "CPB|Data")
	bool GetLocalizedText(FName StringId, ECPBLanguage Language, FText& OutText) const;

	UFUNCTION(BlueprintPure, Category = "CPB|Data")
	bool GetLocalizedSound(FName SoundId, ECPBLanguage Language, FCPBSoundDTO& OutSoundData) const;

	UFUNCTION(BlueprintPure, Category = "CPB|Data")
	bool GetStudyAssetData(FName AssetId, FCPBStudyAssetDTO& OutStudyAssetData) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> ScenarioDataTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> AssemblyDataTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> StringDataTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> SoundDataTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> StudyAssetDataTable;

	TMap<FName, FCPBScenarioDTO> ScenarioCache;
	TMap<FName, FCPBAssemblyDTO> AssemblyCache;
	TMap<FName, FCPBStringDTO> StringCache;
	TMap<FName, FCPBSoundDTO> SoundCache;
	TMap<FName, FCPBStudyAssetDTO> StudyAssetCache;

	void RebuildCaches();
	static FName MakeLocalizedKey(FName RowId, ECPBLanguage Language);
};
