// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "CPBEditorAutomationWidget.generated.h"

class SEditableTextBox;
class STextBlock;
class SWidget;

UCLASS(BlueprintType)
class CPPBUILDERCEDITOR_API UCPBEditorAutomationWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:
	UCPBEditorAutomationWidget();

	virtual TSharedRef<SWidget> RebuildWidget() override;

	UFUNCTION(BlueprintCallable, Category = "CPB|Automation")
	bool RunExcelFolderAutomation();

	UFUNCTION(BlueprintCallable, Category = "CPB|Automation")
	bool GenerateDataTablesOnly();

	UFUNCTION(BlueprintCallable, Category = "CPB|Automation")
	bool ClearGeneratedActors();

	UFUNCTION(BlueprintCallable, Category = "CPB|Automation")
	void ResetToDefaultPaths();

	UFUNCTION(BlueprintCallable, Category = "CPB|Automation")
	void OpenExcelFolderInExplorer();

	UFUNCTION(BlueprintPure, Category = "CPB|Automation")
	FText GetStatusText() const { return StatusText; }

	UFUNCTION(BlueprintPure, Category = "CPB|Automation")
	bool WasLastRunSuccessful() const { return bLastRunSuccessful; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Automation")
	FString ExcelFolderPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Automation")
	FString DestinationAssetFolder = TEXT("/cppBuilderC/Generated/DT");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CPB|Automation")
	FString NarrationAssetRootPath = TEXT("/cppBuilderC/Resource/Sound/Narration");

private:
	FReply HandleRunAutomationClicked();
	FReply HandleGenerateTablesClicked();
	FReply HandleClearGeneratedActorsClicked();
	FReply HandleOpenFolderClicked();
	FReply HandleResetClicked();

	void PullValuesFromTextBoxes();
	void PushValuesToTextBoxes() const;
	void SetStatus(bool bSucceeded, const FString& Message);
	static FString GetDefaultExcelFolderPath();

	TSharedPtr<SEditableTextBox> ExcelFolderTextBox;
	TSharedPtr<SEditableTextBox> DestinationFolderTextBox;
	TSharedPtr<SEditableTextBox> NarrationRootTextBox;
	TSharedPtr<STextBlock> StatusTextBlock;

	FText StatusText;
	bool bLastRunSuccessful = false;
};
