# Privacy and Local Data Handling

## Network activity

TraceMotive — Debug Pathfinder for Unreal does not collect or transmit telemetry, analytics,
project assets, editor logs, personal information, or usage data. The plugin has
no HTTP, socket, cloud-service, advertising, or account integration.

## Project data read locally

Depending on the tool used, the plugin may inspect:

- Asset Registry metadata and Blueprint graph structure
- selected actors, components, properties, references, and transforms
- PIE Blueprint execution context and runtime error messages
- `UAudioComponent` playback state during PIE
- editor selection and Content Browser paths

This inspection remains inside the Unreal Editor process.

## Data written locally

The plugin may create or update:

- reference reports under `Project/Saved/TraceMotiveReports`
- instance trace snapshots under
  `Project/Saved/TraceMotive/TraceSnapshots`
- class favorites and panel layout values in the project's editor-per-project
  configuration
- investigation sessions under `Project/Saved/TraceMotive/Investigations.json`
- user-requested support ZIPs under
  `Project/Saved/TraceMotive/SupportBundles`

Exports and copied logs can contain project asset names, actor names, class
names, package paths, and debugging details. Review them before sharing with a
third party. Users can delete exported files and configuration values locally.

Support ZIPs contain the active investigation, non-sensitive TraceMotive
settings, engine/plugin metadata, and up to 400 recent log lines relevant to
errors, warnings, Blueprints, or TraceMotive. Known project and user paths,
email addresses, and common secret assignments are masked. Because automated
redaction cannot guarantee removal of every project-specific identifier, the
preview and generated files should still be reviewed before sharing.

## Changes to this statement

If a future release adds network communication, telemetry, crash reporting, or
an external service, the Fab listing and this document must be updated before
that release is distributed.
