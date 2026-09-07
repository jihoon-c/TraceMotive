#pragma once



#include "CoreMinimal.h"

#include "HAL/PlatformTime.h"
#include "TMSettings.h"



namespace TMPerf

{

    inline uint64& CancellationGenerationStorage()

    {

        static uint64 Generation = 0;

        return Generation;

    }



    inline uint64 GetCancellationGeneration()

    {

        return CancellationGenerationStorage();

    }



    inline void RequestCancelAll()

    {

        ++CancellationGenerationStorage();

    }



    inline double ConfiguredBudgetScale()

    {

        const UTraceMotiveSettings* Settings = UTraceMotiveSettings::Get();

        return Settings ? FMath::Clamp(static_cast<double>(Settings->SearchBudgetScale), 0.25, 4.0) : 1.0;

    }



    inline int32 MaxSearchResults()

    {

        const UTraceMotiveSettings* Settings = UTraceMotiveSettings::Get();

        return Settings ? FMath::Clamp(Settings->MaxSearchResults, 50, 10000) : 500;

    }



    inline int32 MaxAssetUsageFindings()

    {

        const UTraceMotiveSettings* Settings = UTraceMotiveSettings::Get();

        return Settings ? FMath::Clamp(Settings->MaxAssetUsageFindings, 100, 50000) : 5000;

    }



    inline int32 MaxSnapshotFields()

    {

        const UTraceMotiveSettings* Settings = UTraceMotiveSettings::Get();

        return Settings ? FMath::Clamp(Settings->MaxSnapshotFields, 100, 20000) : 5000;

    }



    inline int32 PIEEventHistoryLimit()

    {

        const UTraceMotiveSettings* Settings = UTraceMotiveSettings::Get();

        return Settings ? FMath::Clamp(Settings->PIEEventHistoryLimit, 20, 5000) : 250;

    }

    inline bool& SearchBoostStorage()

    {

        static bool bSearchBoostEnabled = false;

        return bSearchBoostEnabled;

    }



    inline bool IsSearchBoostEnabled()

    {

        return SearchBoostStorage();

    }



    inline void SetSearchBoostEnabled(bool bEnabled)

    {

        SearchBoostStorage() = bEnabled;

    }



    inline void ToggleSearchBoost()

    {

        SearchBoostStorage() = !SearchBoostStorage();

    }



    inline bool IsEffectiveBoostEnabled()

    {

        const UTraceMotiveSettings* Settings = UTraceMotiveSettings::Get();

        return IsSearchBoostEnabled() && (!Settings || !Settings->bLargeProjectSafeMode);

    }



    inline double BoostMultiplier()

    {

        const UTraceMotiveSettings* Settings = UTraceMotiveSettings::Get();

        const bool bAllowBoost = !Settings || !Settings->bLargeProjectSafeMode;

        return ConfiguredBudgetScale() * (IsSearchBoostEnabled() && bAllowBoost ? 3.0 : 1.0);

    }



    inline int32 BoostInt(int32 BaseValue, int32 MaxValue)

    {

        const UTraceMotiveSettings* Settings = UTraceMotiveSettings::Get();

        const bool bAllowBoost = !Settings || !Settings->bLargeProjectSafeMode;

        const double Scale = ConfiguredBudgetScale() * (IsSearchBoostEnabled() && bAllowBoost ? 3.0 : 1.0);

        return FMath::Min(MaxValue, FMath::Max(1, FMath::RoundToInt(BaseValue * Scale)));

    }



    // Background-friendly defaults. Exhaustive scans should keep the editor responsive

    // even if that means large scans take longer to finish. Fast search can spend a bit

    // more budget because it uses a narrower candidate set.

    inline double VisualSearchTickBudgetSeconds(bool bFastSearch)

    {

        return (bFastSearch ? 0.0040 : 0.0015) * BoostMultiplier();

    }



    inline double CallChainTickBudgetSeconds()

    {

        return 0.0020 * BoostMultiplier();

    }



    inline double OutlinerSearchTickBudgetSeconds()

    {

        return 0.0015 * BoostMultiplier();

    }



    inline double AssetUsageTickBudgetSeconds()

    {

        return 0.0015 * BoostMultiplier();

    }



    inline double VisualSearchTickerIntervalSeconds(bool bFastSearch)

    {

        return IsEffectiveBoostEnabled() ? (bFastSearch ? 0.010 : 0.025) : (bFastSearch ? 0.025 : 0.06);

    }



    inline double CallChainTickerIntervalSeconds()

    {

        return IsEffectiveBoostEnabled() ? 0.025 : 0.08;

    }



    inline double OutlinerSearchTickerIntervalSeconds()

    {

        return IsEffectiveBoostEnabled() ? 0.015 : 0.05;

    }



    inline double AssetUsageTickerIntervalSeconds()

    {

        return IsEffectiveBoostEnabled() ? 0.020 : 0.06;

    }



    inline int32 MaxConcurrentAsyncBlueprintLoads(bool bFastSearch)

    {

        return IsEffectiveBoostEnabled() ? (bFastSearch ? 8 : 4) : (bFastSearch ? 3 : 1);

    }



    inline int32 VisualSearchBatchSize(bool bFastSearch)

    {

        return IsEffectiveBoostEnabled() ? (bFastSearch ? 12 : 4) : (bFastSearch ? 4 : 1);

    }



    inline int32 CallChainPackagesPerTick()

    {

        return BoostInt(1, 4);

    }



    inline int32 CallChainPathsPerTick()

    {

        return BoostInt(2, 8);

    }



    inline int32 OutlinerActorsPerTick()

    {

        return BoostInt(4, 16);

    }



    inline int32 MaxGraphNodesPerBlueprint()

    {

        return IsEffectiveBoostEnabled() ? 36000 : 12000;

    }



    inline int32 MaxMacroGraphNodesPerMatch()

    {

        return IsEffectiveBoostEnabled() ? 4096 : 1024;

    }



    inline int32 MaxCallerCacheBlueprintsPerBuild()

    {

        return IsEffectiveBoostEnabled() ? 7500 : 2500;

    }



    class FTickBudget

    {

    public:

        explicit FTickBudget(double InBudgetSeconds)

            : StartSeconds(FPlatformTime::Seconds())

            , BudgetSeconds(InBudgetSeconds)

        {

        }



        bool HasTime() const

        {

            return FPlatformTime::Seconds() - StartSeconds < BudgetSeconds;

        }



        bool ShouldYield() const

        {

            return !HasTime();

        }



        double GetElapsedSeconds() const

        {

            return FPlatformTime::Seconds() - StartSeconds;

        }



        double GetStartSeconds() const

        {

            return StartSeconds;

        }



    private:

        double StartSeconds = 0.0;

        double BudgetSeconds = 0.0;

    };

}

