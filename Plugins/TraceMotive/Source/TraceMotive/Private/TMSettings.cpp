#include "TMSettings.h"

#define LOCTEXT_NAMESPACE "TraceMotiveSettings"

UTraceMotiveSettings::UTraceMotiveSettings()
{
    CategoryName = TEXT("Plugins");
    SectionName = TEXT("TraceMotive");
}

FText UTraceMotiveSettings::GetSectionText() const
{
    return LOCTEXT("SectionText", "TraceMotive");
}

FText UTraceMotiveSettings::GetSectionDescription() const
{
    return LOCTEXT("SectionDescription", "Configure diagnostic budgets, result limits, runtime history, reports, and investigation persistence.");
}

const UTraceMotiveSettings* UTraceMotiveSettings::Get()
{
    return GetDefault<UTraceMotiveSettings>();
}

#undef LOCTEXT_NAMESPACE
