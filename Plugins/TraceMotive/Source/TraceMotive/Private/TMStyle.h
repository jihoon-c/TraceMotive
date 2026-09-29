#pragma once

#include "CoreMinimal.h"

/** Shared TraceMotive vector-icon style used by menus and nomad tabs. */
namespace TMStyle
{
    void Initialize();
    void Shutdown();
    FName GetStyleSetName();
    FLinearColor GetSearchColor();
    FLinearColor GetRuntimeColor();
    FLinearColor GetWorkflowColor();
}
