// Copyright Epic Games, Inc. All Rights Reserved.

#include "cppBuilderCEditor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "CPBEditorAutomationWidget.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#define LOCTEXT_NAMESPACE "FcppBuilderCEditorModule"

namespace
{
void CPBTryCreateDefaultAutomationWidgetAsset()
{
	if (IsRunningCommandlet())
	{
		return;
	}

	const FString PackageName = TEXT("/Game/Editor/CPB/EUW_CPB_Automation");
	const FString AssetName = TEXT("EUW_CPB_Automation");
	const FString ObjectPath = PackageName + TEXT(".") + AssetName;

	if (LoadObject<UObject>(nullptr, *ObjectPath))
	{
		return;
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		return;
	}

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		UCPBEditorAutomationWidget::StaticClass(),
		Package,
		FName(*AssetName),
		BPTYPE_Normal,
		UEditorUtilityWidgetBlueprint::StaticClass(),
		UWidgetBlueprintGeneratedClass::StaticClass(),
		FName(TEXT("CPBEditorAutomation")));

	if (!Blueprint)
	{
		return;
	}

	FAssetRegistryModule::AssetCreated(Blueprint);
	Package->MarkPackageDirty();

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	UPackage::SavePackage(Package, Blueprint, *PackageFilename, SaveArgs);
}
}

void FcppBuilderCEditorModule::StartupModule()
{
	CPBTryCreateDefaultAutomationWidgetAsset();
}

void FcppBuilderCEditorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FcppBuilderCEditorModule, cppBuilderCEditor)
