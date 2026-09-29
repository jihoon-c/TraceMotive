#pragma once

#include "CoreMinimal.h"

// Shared lifecycle state for time-sliced editor traces.
class FTMTraceSession
{
public:
    void Begin()
    {
        bActive = true;
        ++Generation;
        LastUiRefreshSeconds = 0.0;
    }

    void Cancel()
    {
        bActive = false;
        ++Generation;
    }

    void Complete()
    {
        bActive = false;
    }

    bool IsActive() const
    {
        return bActive;
    }

    uint64 GetGeneration() const
    {
        return Generation;
    }

    bool ShouldRefreshUi(const double NowSeconds, const double IntervalSeconds)
    {
        // Treat the exact interval boundary as ready even when binary floating-point
        // subtraction produces a value a few ulps below the requested interval.
        constexpr double RefreshBoundaryTolerance = 1.0e-9;
        if ((NowSeconds - LastUiRefreshSeconds) + RefreshBoundaryTolerance < IntervalSeconds)
        {
            return false;
        }

        LastUiRefreshSeconds = NowSeconds;
        return true;
    }

private:
    bool bActive = false;
    uint64 Generation = 0;
    double LastUiRefreshSeconds = 0.0;
};
