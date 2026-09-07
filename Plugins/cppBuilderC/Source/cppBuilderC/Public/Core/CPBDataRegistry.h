#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/CPBTypes.h"
#include "CPBDataRegistry.generated.h"

class UDataTable;
class UCPBContextSubsystem;

UCLASS()
class CPPBUILDERC_API UCPBDataRegistry : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CPB|Data", meta = (WorldContext = "WorldContextObject"))
	static void RegisterDataTables(
		const UObject* WorldContextObject,
		UDataTable* ScenarioDataTable,
		UDataTable* AssemblyDataTable,
		UDataTable* StringDataTable,
		UDataTable* SoundDataTable,
		UDataTable* StudyAssetDataTable);

	UFUNCTION(BlueprintPure, Category = "CPB|Data", meta = (WorldContext = "WorldContextObject"))
	static bool GetScenarioData(const UObject* WorldContextObject, FName ScenarioId, FCPBScenarioDTO& OutScenarioData);

	UFUNCTION(BlueprintPure, Category = "CPB|Data", meta = (WorldContext = "WorldContextObject"))
	static bool GetAssemblyData(const UObject* WorldContextObject, FName AssemblyId, FCPBAssemblyDTO& OutAssemblyData);

	UFUNCTION(BlueprintPure, Category = "CPB|Data", meta = (WorldContext = "WorldContextObject"))
	static bool GetLocalizedText(const UObject* WorldContextObject, FName StringId, ECPBLanguage Language, FText& OutText);

	UFUNCTION(BlueprintPure, Category = "CPB|Data", meta = (WorldContext = "WorldContextObject"))
	static bool GetLocalizedSound(const UObject* WorldContextObject, FName SoundId, ECPBLanguage Language, FCPBSoundDTO& OutSoundData);

	UFUNCTION(BlueprintPure, Category = "CPB|Data", meta = (WorldContext = "WorldContextObject"))
	static bool GetStudyAssetData(const UObject* WorldContextObject, FName AssetId, FCPBStudyAssetDTO& OutStudyAssetData);

private:
	static UCPBContextSubsystem* GetContextSubsystem(const UObject* WorldContextObject);
};
