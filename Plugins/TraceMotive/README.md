# TraceMotive — Debug Pathfinder for Unreal

TraceMotive — Debug Pathfinder for Unreal is an editor-only Unreal Engine plugin for inspecting
Blueprint and level-instance relationships. It does not add runtime code to a
packaged game.

## Overview

TraceMotive shortens the path from a debugging question to the editor object,
Blueprint node, runtime instance, or collision setting that can answer it. The
Fab v1 release deliberately keeps the customer surface focused on four tools:

- follow a Blueprint symbol to the nodes that reference it;
- identify where a selected asset is actually used;
- observe selected property values on PIE instances; and
- explain why two actors can or cannot block each other.

Each tool presents its evidence and its limits separately. A verified object
reference is not mixed with a text-only candidate, sampled values are not
presented as a complete write history, and collision-compatible settings are not
presented as proof of the real gameplay movement path.

## Active features in Fab v1

Start with the four core tools in **TraceMotiveTools → Core tools / Start here**:

### Visual Reference Search

Open a Blueprint graph and right-click a variable, function, event, or dispatcher
to start a contextual search. Results retain their Blueprint, graph, and node
identity so you can navigate back to the exact source node. The search includes
nested graphs and keeps same-named assets from different packages separate.

Use this tool when you need to answer questions such as “where is this value read
or written?” or “which Blueprint path reaches this function?” Results are static
editor evidence; dynamically constructed calls or runtime context unavailable to
the active engine build may remain unresolved.

### Asset Usage Locator

Select an asset in the Content Browser, then open **Asset Usage Locator**. The
result view separates verified object references from Asset Registry or text
candidates that still require inspection. It checks Blueprint pins, class/default
objects, component templates, properties, and supported soft-reference evidence.

Progress, loading failures, cancellation, and result truncation are shown
explicitly. Cancelling a scan keeps the evidence already collected instead of
mislabeling it as a complete result.

### Variable Value Trace

During PIE, select a class and one or more properties to observe. Instances are
identified with their world and full object path, which helps distinguish
same-named objects in multi-world sessions. Sampling intervals of 0.05, 0.1, and
0.5 seconds are available, and the display can be filtered to a specific instance.

This is a sampled state observer, not a write-event hook: a value that changes and
changes back between samples may be missed. Recursive or very large values are
bounded, and skipped values are reported rather than silently treated as empty.

### Collision Pair Analyzer

Select two actors and run the analyzer to inspect their primitive-component pairs.
The report distinguishes collision-enabled state, channel responses, overlap/contact
evidence, and short sweep-probe evidence. It also calls out common movement-path
risks such as `Sweep=false`, teleport-style movement, or a non-blocking updated
component.

The analysis is incremental and cancellable. “Block-capable” means the inspected
settings can block; the final gameplay result still depends on the movement API,
ignore lists, physics state, and the component that actually moves.

## Performance and large-project design

TraceMotive is designed to keep editor diagnostics responsive without claiming a
hard frame-time guarantee. The current implementation uses these safeguards:

- **Time-sliced work:** reference, asset-usage, and collision scans process bounded
  slices across editor ticks instead of intentionally completing an entire project
  scan in one tick.
- **Resumable cursors:** long Blueprint and package scans retain their graph/node or
  candidate position, then continue from that point on a later tick.
- **Bounded asynchronous loading:** unloaded Blueprint and asset candidates are
  requested asynchronously with a concurrency limit rather than being loaded as
  one unbounded batch.
- **Large-project safe mode:** enabled by default, it uses conservative scan budgets
  and prevents optional search boosts from increasing work unexpectedly.
- **Explicit data limits:** reference and asset-usage result sets have clamped
  limits, while recursively exported runtime values use a fixed traversal budget.
  Hitting a limit is surfaced as a partial, truncated, or skipped result.
- **Throttled UI refresh:** progress and result widgets update at controlled
  intervals so the interface is not rebuilt for every discovered item.
- **Cooperative cancellation:** active work records a cancellation generation;
  cancelling a running tool invalidates its queued work safely.
- **Profiling hooks:** the main search, sampling, and collision paths expose Unreal
  Insights CPU scopes for project-specific measurement.

These techniques reduce long uninterrupted editor work, but individual engine
operations such as loading one package, inspecting one large property, or running
one physics query may still exceed a nominal tick budget. Measure cold and warm
runs on a representative project before making performance claims.

## User guide

For detailed Korean instructions covering installation, all four active core tools,
result interpretation, limitations, troubleshooting, and combined workflows, see
the [TraceMotive v1 active core features guide](https://app.notion.com/p/3dd69d0f1bf281288b09f5272c997b7b?pvs=204).

Experimental and future-update tools that remain in the source tree are not
registered in the Fab v1 editor UI.

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

The active tools do not upload project data. Any report or copied diagnostic text
is created only through an explicit user action and can contain project-specific
asset, actor, class, or package names; review exported material before sharing it.

## Important limitations

- Trace results are debugging evidence, not a formal proof that every possible
  runtime caller has been identified.
- Blueprint caller attribution depends on the runtime context exposed by the
  active Unreal Engine build and may be reported as unresolved.
- Tick budgets limit cooperative scan loops, not every individual engine API call.
- Result caps, failed loads, cancellation, and recursive-value limits can produce
  intentionally partial results; the tool reports these states in its progress or
  summary UI.
- Engine-version compatibility must be verified for each version offered on Fab.

## Trademark notice

This product is independently developed and is not affiliated with, sponsored
by, or endorsed by Epic Games, Inc. Unreal Engine, Epic Games, and their related
marks are trademarks or registered trademarks of Epic Games, Inc. in the United
States and elsewhere.

Copyright (c) 2026 CJH. All rights reserved. Customer use is governed by the
license selected for the product on Fab and the applicable Fab terms.
