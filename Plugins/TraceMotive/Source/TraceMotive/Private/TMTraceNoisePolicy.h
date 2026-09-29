#pragma once

#include "CoreMinimal.h"

namespace TMTraceNoise
{
    inline bool IsTickLike(const FString& Text)
    {
        return Text.Contains(TEXT("ReceiveTick"), ESearchCase::IgnoreCase)
            || Text.Contains(TEXT("ActorTick"), ESearchCase::IgnoreCase)
            || Text.Contains(TEXT("ComponentTick"), ESearchCase::IgnoreCase)
            || Text.Contains(TEXT("TickFunction"), ESearchCase::IgnoreCase)
            || Text.Contains(TEXT("TickComponent"), ESearchCase::IgnoreCase)
            || Text.Equals(TEXT("Tick"), ESearchCase::IgnoreCase);
    }

    inline double DuplicateWindowSeconds(const FString& Category)
    {
        if (Category.Equals(TEXT("Created"), ESearchCase::IgnoreCase)) return 5.0;
        if (Category.Equals(TEXT("Visibility"), ESearchCase::IgnoreCase)) return 1.5;
        if (Category.Equals(TEXT("Transform"), ESearchCase::IgnoreCase)) return 0.35;
        if (Category.Equals(TEXT("Audio"), ESearchCase::IgnoreCase)) return 0.75;
        if (Category.Equals(TEXT("Click"), ESearchCase::IgnoreCase)) return 0.20;
        return 0.75;
    }

    inline FString StableWidgetSignature(const FString& Type, const FString& File, const FString& Widget, const FString& Child, const FString& Summary)
    {
        return FString::Printf(TEXT("%s|%s|%s|%s|%s"), *Type, *File, *Widget, *Child, *Summary);
    }
}
