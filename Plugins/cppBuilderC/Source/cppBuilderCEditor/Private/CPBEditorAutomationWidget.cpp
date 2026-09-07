// Copyright Epic Games, Inc. All Rights Reserved.

#include "CPBEditorAutomationWidget.h"

#include "CPBEditorAutomationBFL.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

UCPBEditorAutomationWidget::UCPBEditorAutomationWidget()
{
	TabDisplayName = FText::FromString(TEXT("CPB Automation"));
	HelpText = TEXT("Drop the master Excel file into the configured folder, then run CPB automation from this editor-only widget.");
	bAlwaysReregisterWithWindowsMenu = true;
	bAutoRunDefaultAction = false;

	ResetToDefaultPaths();
	SetStatus(false, TEXT("Ready. Put the master Excel file in the folder, then run automation."));
}

TSharedRef<SWidget> UCPBEditorAutomationWidget::RebuildWidget()
{
	PushValuesToTextBoxes();

	return SNew(SBorder)
		.Padding(12.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("CPB Excel Automation")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
			]
			+ SScrollBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 12.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(FText::FromString(TEXT("Reads the master .xlsx, creates CPB DataTables, cleans previous generated actors, then places level actors and sequences.")))
			]
			+ SScrollBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Excel Folder Path")))
			]
			+ SScrollBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SAssignNew(ExcelFolderTextBox, SEditableTextBox)
				.Text(FText::FromString(ExcelFolderPath))
			]
			+ SScrollBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Generated DataTable Asset Folder")))
			]
			+ SScrollBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SAssignNew(DestinationFolderTextBox, SEditableTextBox)
				.Text(FText::FromString(DestinationAssetFolder))
			]
			+ SScrollBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Narration/Sound Asset Root")))
			]
			+ SScrollBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 14.0f)
			[
				SAssignNew(NarrationRootTextBox, SEditableTextBox)
				.Text(FText::FromString(NarrationAssetRootPath))
			]
			+ SScrollBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 8.0f, 8.0f)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Run Excel Placement")))
					.OnClicked_UObject(this, &UCPBEditorAutomationWidget::HandleRunAutomationClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 8.0f, 8.0f)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Regenerate DataTables Only")))
					.OnClicked_UObject(this, &UCPBEditorAutomationWidget::HandleGenerateTablesClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 8.0f, 8.0f)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Clear Generated Actors")))
					.OnClicked_UObject(this, &UCPBEditorAutomationWidget::HandleClearGeneratedActorsClicked)
				]
			]
			+ SScrollBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 12.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Open Excel Folder")))
					.OnClicked_UObject(this, &UCPBEditorAutomationWidget::HandleOpenFolderClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Reset Paths")))
					.OnClicked_UObject(this, &UCPBEditorAutomationWidget::HandleResetClicked)
				]
			]
			+ SScrollBox::Slot()
			[
				SNew(SBorder)
				.Padding(8.0f)
				[
					SAssignNew(StatusTextBlock, STextBlock)
					.AutoWrapText(true)
					.Text(StatusText)
				]
			]
		];
}

bool UCPBEditorAutomationWidget::RunExcelFolderAutomation()
{
	PullValuesFromTextBoxes();

	const bool bSucceeded = UCPBEditorAutomationBFL::RunMasterAutomationFromExcelFolder(
		ExcelFolderPath,
		DestinationAssetFolder,
		NarrationAssetRootPath);

	SetStatus(
		bSucceeded,
		bSucceeded
			? TEXT("Done. Excel placement automation completed.")
			: TEXT("Failed. Check the Output Log and on-screen warnings."));

	return bSucceeded;
}

bool UCPBEditorAutomationWidget::GenerateDataTablesOnly()
{
	PullValuesFromTextBoxes();

	const bool bSucceeded = UCPBEditorAutomationBFL::GenerateDataTablesFromExcelFolder(
		ExcelFolderPath,
		DestinationAssetFolder,
		NarrationAssetRootPath);

	SetStatus(
		bSucceeded,
		bSucceeded
			? TEXT("Done. DataTables were regenerated from Excel.")
			: TEXT("Failed. Check the DataTable generation log."));

	return bSucceeded;
}

bool UCPBEditorAutomationWidget::ClearGeneratedActors()
{
	const bool bSucceeded = UCPBEditorAutomationBFL::ClearAutoGeneratedLevelActors();
	SetStatus(
		bSucceeded,
		bSucceeded
			? TEXT("Done. Auto-generated actors were cleared.")
			: TEXT("Failed. Generated actor cleanup did not complete."));

	return bSucceeded;
}

void UCPBEditorAutomationWidget::ResetToDefaultPaths()
{
	ExcelFolderPath = GetDefaultExcelFolderPath();
	DestinationAssetFolder = TEXT("/cppBuilderC/Generated/DT");
	NarrationAssetRootPath = TEXT("/cppBuilderC/Resource/Sound/Narration");
	PushValuesToTextBoxes();
}

void UCPBEditorAutomationWidget::OpenExcelFolderInExplorer()
{
	PullValuesFromTextBoxes();
	IFileManager::Get().MakeDirectory(*ExcelFolderPath, true);
	FPlatformProcess::ExploreFolder(*ExcelFolderPath);
	SetStatus(true, FString::Printf(TEXT("Opened Excel folder: %s"), *ExcelFolderPath));
}

FReply UCPBEditorAutomationWidget::HandleRunAutomationClicked()
{
	RunExcelFolderAutomation();
	return FReply::Handled();
}

FReply UCPBEditorAutomationWidget::HandleGenerateTablesClicked()
{
	GenerateDataTablesOnly();
	return FReply::Handled();
}

FReply UCPBEditorAutomationWidget::HandleClearGeneratedActorsClicked()
{
	ClearGeneratedActors();
	return FReply::Handled();
}

FReply UCPBEditorAutomationWidget::HandleOpenFolderClicked()
{
	OpenExcelFolderInExplorer();
	return FReply::Handled();
}

FReply UCPBEditorAutomationWidget::HandleResetClicked()
{
	ResetToDefaultPaths();
	SetStatus(true, TEXT("Paths were reset to defaults."));
	return FReply::Handled();
}

void UCPBEditorAutomationWidget::PullValuesFromTextBoxes()
{
	if (ExcelFolderTextBox.IsValid())
	{
		ExcelFolderPath = ExcelFolderTextBox->GetText().ToString();
	}

	if (DestinationFolderTextBox.IsValid())
	{
		DestinationAssetFolder = DestinationFolderTextBox->GetText().ToString();
	}

	if (NarrationRootTextBox.IsValid())
	{
		NarrationAssetRootPath = NarrationRootTextBox->GetText().ToString();
	}
}

void UCPBEditorAutomationWidget::PushValuesToTextBoxes() const
{
	if (ExcelFolderTextBox.IsValid())
	{
		ExcelFolderTextBox->SetText(FText::FromString(ExcelFolderPath));
	}

	if (DestinationFolderTextBox.IsValid())
	{
		DestinationFolderTextBox->SetText(FText::FromString(DestinationAssetFolder));
	}

	if (NarrationRootTextBox.IsValid())
	{
		NarrationRootTextBox->SetText(FText::FromString(NarrationAssetRootPath));
	}
}

void UCPBEditorAutomationWidget::SetStatus(bool bSucceeded, const FString& Message)
{
	bLastRunSuccessful = bSucceeded;
	StatusText = FText::FromString(Message);

	if (StatusTextBlock.IsValid())
	{
		StatusTextBlock->SetText(StatusText);
	}
}

FString UCPBEditorAutomationWidget::GetDefaultExcelFolderPath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CPBMasterExcel")));
}
