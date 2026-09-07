# TraceMotive — Debug Pathfinder for Unreal

TraceMotive — Debug Pathfinder for Unreal is an editor-only Unreal Engine plugin for inspecting
Blueprint and level-instance relationships. It does not add runtime code to a
packaged game.

## Main tools

- Scenario-based quick diagnosis for click, collision, audio, runtime-error, variable, and packaging problems
- Persistent investigation sessions with cross-tool handoffs and Before/After actor-state comparison
- Privacy-redacted customer support bundles with a contents preview and local ZIP export
- Project Settings integration for safe search budgets, result/history limits, report defaults, and investigation persistence
- One-click Stop All Active Work for cancelling searches and closing active diagnostic traces
- Visual reference search for variables, functions, event dispatchers, and assets
- Function call-chain visualization
- PIE instance and component state tracing
- Blueprint runtime-error grouping and instance correlation
- Audio component playback tracing during PIE
- Two-actor runtime collision diagnosis with component-pair cause reports
- Packaging progress dashboard with UAT stage detection, ETA, and recent logs
- Click event diagnostics with input auditing, seven-stage target tracing, and binding matrix
- Placeable asset, Actor class, and level favorites with editor placement shortcuts
- Markdown, CSV, JSON, and Mermaid report export

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
