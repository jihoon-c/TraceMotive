#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Sound/SoundBase.h"
#include "Engine/Texture2D.h"
#include "CPBTypes.generated.h"

UENUM(BlueprintType)
enum class ECPBGameMode : uint8
{
	Practice UMETA(DisplayName = "Practice"),
	Video UMETA(DisplayName = "Video")
};

UENUM(BlueprintType)
enum class ECPBLanguage : uint8
{
	Korean UMETA(DisplayName = "Korean"),
	English UMETA(DisplayName = "English"),
	Japanese UMETA(DisplayName = "Japanese"),
	ChineseSimplified UMETA(DisplayName = "Chinese Simplified")
};

UENUM(BlueprintType)
enum class ECPBVisibilityState : uint8
{
	Visible UMETA(DisplayName = "Visible"),
	Hidden UMETA(DisplayName = "Hidden")
};

UENUM(BlueprintType)
enum class ECPBViewTransition : uint8
{
	None UMETA(DisplayName = "None"),
	Cut UMETA(DisplayName = "Cut"),
	Fade UMETA(DisplayName = "Fade"),
	Blend UMETA(DisplayName = "Blend")
};

UENUM(BlueprintType)
enum class ECPBStudyAssetType : uint8
{
	None UMETA(DisplayName = "None"),
	PDF UMETA(DisplayName = "PDF"),
	Video UMETA(DisplayName = "Video"),
	Image UMETA(DisplayName = "Image"),
	Web UMETA(DisplayName = "Web")
};

UENUM(BlueprintType)
enum class ECPBActivePlatform : uint8
{
	Tablet UMETA(DisplayName = "Tablet"),
	VR UMETA(DisplayName = "VR"),
	CBT UMETA(DisplayName = "CBT")
};

UENUM(BlueprintType)
enum class ECPBWidgetRenderMode : uint8
{
	Viewport UMETA(DisplayName = "Viewport"),
	WorldSpace UMETA(DisplayName = "World Space"),
	Material UMETA(DisplayName = "Material")
};

UENUM(BlueprintType)
enum class ECPBMenuAction : uint8
{
	Home UMETA(DisplayName = "Home"),
	ScenarioMode UMETA(DisplayName = "Scenario Mode"),
	VideoMode UMETA(DisplayName = "Video Mode"),
	FullScreen UMETA(DisplayName = "Full Screen"),
	Settings UMETA(DisplayName = "Settings"),
	Quit UMETA(DisplayName = "Quit")
};

UENUM(BlueprintType)
enum class ECPBSidePanelTab : uint8
{
	Tools UMETA(DisplayName = "Tools"),
	Parts UMETA(DisplayName = "Parts"),
	Sequence UMETA(DisplayName = "Sequence")
};

UENUM(BlueprintType)
enum class ECPBSequenceElementState : uint8
{
	Locked UMETA(DisplayName = "Locked"),
	Ready UMETA(DisplayName = "Ready"),
	Active UMETA(DisplayName = "Active"),
	Completed UMETA(DisplayName = "Completed"),
	Failed UMETA(DisplayName = "Failed")
};

UENUM(BlueprintType)
enum class ECPBPlaybackState : uint8
{
	Stopped UMETA(DisplayName = "Stopped"),
	Playing UMETA(DisplayName = "Playing"),
	Paused UMETA(DisplayName = "Paused")
};

UENUM(BlueprintType)
enum class ECPBAssemblyRuleType : uint8
{
	MatchAssemblyId UMETA(DisplayName = "Match Assembly Id"),
	EnvironmentClick UMETA(DisplayName = "Environment Click"),
	AlwaysFail UMETA(DisplayName = "Always Fail"),
	Custom UMETA(DisplayName = "Custom")
};

UENUM(BlueprintType)
enum class ECPBAssemblyRuleResult : uint8
{
	Success UMETA(DisplayName = "Success"),
	Failure UMETA(DisplayName = "Failure"),
	EnvironmentClick UMETA(DisplayName = "Environment Click"),
	Ignored UMETA(DisplayName = "Ignored")
};

USTRUCT(BlueprintType)
struct CPPBUILDERC_API FCPBScenarioDTO : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Scenario")
	FName ScenarioId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Scenario")
	ECPBGameMode GameMode = ECPBGameMode::Practice;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Scenario")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Scenario")
	TArray<FName> SequenceIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Scenario")
	ECPBLanguage DefaultLanguage = ECPBLanguage::Korean;
};

USTRUCT(BlueprintType)
struct CPPBUILDERC_API FCPBAssemblyDTO : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Assembly")
	FName AssemblyId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Assembly")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Assembly")
	ECPBAssemblyRuleType RuleType = ECPBAssemblyRuleType::MatchAssemblyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Assembly")
	FName ExpectedTargetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Assembly")
	FName StudyAssetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Assembly")
	FName SpawnTag = NAME_None;
};

USTRUCT(BlueprintType)
struct CPPBUILDERC_API FCPBStringDTO : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Localization")
	FName StringId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Localization")
	ECPBLanguage Language = ECPBLanguage::Korean;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Localization")
	FText Text;
};

USTRUCT(BlueprintType)
struct CPPBUILDERC_API FCPBSoundDTO : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sound")
	FName SoundId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sound")
	ECPBLanguage Language = ECPBLanguage::Korean;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sound")
	TSoftObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sound")
	FName SubtitleStringId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Sound", meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.0f;
};

USTRUCT(BlueprintType)
struct CPPBUILDERC_API FCPBStudyAssetDTO : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Study Asset")
	FName AssetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Study Asset")
	ECPBStudyAssetType AssetType = ECPBStudyAssetType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Study Asset")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Study Asset")
	TSoftObjectPtr<UObject> Asset;
};

USTRUCT(BlueprintType)
struct CPPBUILDERC_API FCPBSidePanelItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|UI")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|UI")
	ECPBSidePanelTab Tab = ECPBSidePanelTab::Parts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|UI")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|UI")
	TSoftObjectPtr<UTexture2D> Thumbnail;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|UI")
	bool bEnabled = true;
};

USTRUCT(BlueprintType)
struct CPPBUILDERC_API FCPBSequenceListItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|UI")
	FName SequenceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|UI")
	FName StepId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|UI")
	int32 GlobalStepIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|UI")
	int32 LocalStepIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|UI")
	FText DisplayText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|UI")
	ECPBSequenceElementState State = ECPBSequenceElementState::Locked;
};

USTRUCT(BlueprintType)
struct CPPBUILDERC_API FCPBNarrationDTO : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Narration")
	FName NarrationId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Narration")
	FName SequenceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Narration")
	int32 StepIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Narration")
	FName SubtitleStringId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Narration")
	FText SubtitleOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Narration")
	TSoftObjectPtr<USoundBase> NarrationSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Narration", meta = (ClampMin = "0.0"))
	float FallbackDuration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CPB|Narration")
	bool bAutoAdvanceSequenceOnFinished = true;
};
