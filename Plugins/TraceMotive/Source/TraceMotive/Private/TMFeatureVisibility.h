#pragma once

// Fab release builds expose only the tools listed in TMToolLauncher::CoreTiles.
// Set TRACEMOTIVE_CORE_ONLY_RELEASE to 0 in TraceMotive.Build.cs when the
// hidden tools are ready to be exposed in a later product update.
#ifndef TRACEMOTIVE_CORE_ONLY_RELEASE
#define TRACEMOTIVE_CORE_ONLY_RELEASE 1
#endif

namespace TMFeatureVisibility
{
    constexpr bool IsCoreOnlyRelease()
    {
        return TRACEMOTIVE_CORE_ONLY_RELEASE != 0;
    }
}
