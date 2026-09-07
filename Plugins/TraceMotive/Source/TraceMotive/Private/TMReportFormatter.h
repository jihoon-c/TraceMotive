#pragma once



#include "CoreMinimal.h"



namespace TMReportFormatter

{

    struct FMetadataItem

    {

        FString Key;

        FString Value;



        FMetadataItem() = default;

        FMetadataItem(const FString& InKey, const FString& InValue)

            : Key(InKey)

            , Value(InValue)

        {

        }

    };



    struct FSection

    {

        FString Title;

        FString Body;



        FSection() = default;

        FSection(const FString& InTitle, const FString& InBody)

            : Title(InTitle)

            , Body(InBody)

        {

        }

    };



    struct FReport

    {

        FString ToolName;

        FString Summary;

        TArray<FMetadataItem> Metadata;

        TArray<FSection> Sections;

    };



    struct FDiagnosticSafetySection

    {

        FString Confidence;

        FString Scope;

        TArray<FString> Evidence;

        TArray<FString> Limitations;

        TArray<FString> NextChecks;

    };



    FString BuildDiagnosticSafetyBlock(const FDiagnosticSafetySection& Section);

    FDiagnosticSafetySection BuildDefaultDiagnosticSafetySection(const FString& ToolName);

    FString BuildMarkdownReport(const FReport& Report);

    FString BuildWrappedLegacyReport(const FString& ToolName, const FString& Summary, const FString& Details, const TArray<FMetadataItem>& Metadata = TArray<FMetadataItem>());

    FString NowText();

    FString SanitizeLine(FString Text);

}



