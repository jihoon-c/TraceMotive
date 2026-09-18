# TraceMotive — Debug Pathfinder for Unreal

TraceMotive — Debug Pathfinder for Unreal is an editor-only Unreal Engine plugin for inspecting
Blueprint and level-instance relationships. It does not add runtime code to a
packaged game.

## Main tools

Start with the four core tools in **TraceMotiveTools → Core tools / Start here**:

1. **Visual Reference Search**: right-click a Blueprint variable/function/dispatcher,
   inspect references, then navigate to the source node. Nested graphs are included;
   assets with the same name in different packages remain separate.
2. **Asset Usage Locator**: select an asset in the Content Browser and inspect
   verified object references separately from text/registry candidates. Progress
   reports load failures and result limits. Cancel preserves partial results.
3. **Variable Value Trace**: choose a class and properties during PIE. Filter by
   full instance/world path and choose a 0.05, 0.1, or 0.5 second sample interval.
   Changes between samples can be missed; this is not a complete write-event log.
   Values exceeding the recursive export budget are explicitly reported as skipped.
4. **Collision Pair Analyzer**: capture two actors, analyze component pairs, and
   distinguish settings from contact/sweep evidence. Analysis runs across frames
   and can be cancelled. A scan observes state over time, not one atomic snapshot.

## User guide

For detailed Korean instructions covering installation, all four active core tools,
result interpretation, limitations, troubleshooting, and combined workflows, see
the [TraceMotive v1 active core features guide](https://app.notion.com/p/3dd69d0f1bf281288b09f5272c997b7b?pvs=204).

Advanced runtime and scenario tools are collapsed in the launcher. Favorites,
reports, and investigation sessions remain supporting workflow features.

## Release surface

The Fab v1 build exposes only the four core tools above. Other experimental and
future-update tools remain compiled in the source tree but do not register editor
menus, context actions, or tabs. This keeps unfinished features out of the customer
workflow without discarding their implementation.

## Installation

1. Place the plugin under the project's `Plugins/TraceMotive` folder,
   or install it through Fab when the listing is available.
2. Enable **TraceMotive — Debug Pathfinder for Unreal** in the Unreal Editor plugin browser.
3. Restart the editor when prompted.

The plugin module type is `Editor`; it is not loaded by game or server targets.

## Engine compatibility

- Unreal Engine 5.8.2 on Win64: source compilation and `BuildPlugin` packaging verified
- Packaged binary build ID: `55116800`

Binary packages are engine-build-specific. Rebuild the plugin from source when
using a different Unreal Engine patch or custom engine build.

## Local data handling

The plugin operates on project data locally. It does not contain networking,
analytics, advertising, telemetry, account, or payment code. See [PRIVACY.md](PRIVACY.md)
for the project data it reads and the local files it may create.

Support bundles are created only when the user presses **Export ZIP**. Before
export, **Preview contents** lists the included files, log-line count, and
redaction count. Nothing is uploaded automatically.

## Important limitations

- Trace results are debugging evidence, not a formal proof that every possible
  runtime caller has been identified.
- Blueprint caller attribution depends on the runtime context exposed by the
  active Unreal Engine build and may be reported as unresolved.
- Audio tracing focuses on `UAudioComponent` state and Blueprint execution context.
- Engine-version compatibility must be verified for each version offered on Fab.

## Trademark notice

This product is independently developed and is not affiliated with, sponsored
by, or endorsed by Epic Games, Inc. Unreal Engine, Epic Games, and their related
marks are trademarks or registered trademarks of Epic Games, Inc. in the United
States and elsewhere.

Copyright (c) 2026 CJH. All rights reserved. Customer use is governed by the
license selected for the product on Fab and the applicable Fab terms.
