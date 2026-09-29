#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
#include "Containers/AllowShrinking.h"
#endif

namespace TMEngineCompatibility
{
    template <typename ElementType, typename AllocatorType>
    inline void RemoveAtNoShrink(TArray<ElementType, AllocatorType>& Items, int32 Index, int32 Count = 1)
    {
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
        Items.RemoveAt(Index, Count, EAllowShrinking::No);
#else
        Items.RemoveAt(Index, Count, false);
#endif
    }

    template <typename ElementType, typename AllocatorType>
    inline void RemoveAtSwapNoShrink(TArray<ElementType, AllocatorType>& Items, int32 Index, int32 Count = 1)
    {
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
        Items.RemoveAtSwap(Index, Count, EAllowShrinking::No);
#else
        Items.RemoveAtSwap(Index, Count, false);
#endif
    }
}