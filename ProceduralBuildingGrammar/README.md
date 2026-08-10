# Procedural Building Grammar

Unreal Engine 5.8 plugin that generates procedural buildings from OpenStreetMap footprints using
configurable facade and roof grammar rules, instanced for large-area performance.

## Features

- **OSM-driven generation** — parses `.osm` XML extracts, assembles building footprints (including
  multipolygon relations and `building:part` sub-volumes), and projects them to a local metric
  tangent plane around a configurable origin.
- **Facade & roof grammar** — per-building floor counts, window/door/ledge/balcony placement, and
  flat/gabled/hipped/pyramid roofs are derived from OSM tags and a configurable style set, with
  optional tag-based overrides (`roof:shape`, `facade:material`, `facade:colour`, etc.).
- **Street-aware roof alignment** — gabled and hipped ridges align to the nearest OSM street,
  preferring the street named in a building's `addr:street` tag over pure proximity, with a
  configurable search radius; falls back to the footprint's own longest edge otherwise.
- **Instanced rendering** — every window, door, ledge, balcony, and roof detail across a whole
  district batches into shared `UHierarchicalInstancedStaticMeshComponent` buckets, and every
  building's wall/roof surfaces merge into one `UDynamicMeshComponent` per pool, instead of one
  actor per building.
- **Chunked, memory-bounded generation** — large extracts are split into grid cells, each spawning
  its own pool actor; optional periodic save-and-reload keeps peak memory bounded regardless of
  extract size, and requires World Partition.
- **Static mesh baking** — any generated cell can be flattened into a single saved `UStaticMesh`
  asset (with persistent per-color material variants), either during generation or afterward,
  trading per-instance editability for reduced runtime actor/draw count.
- **Post-import customization** — a viewport pick tool selects an individual building out of a
  merged pool and exposes a details panel for per-building tag or facade-style overrides, which
  regenerate just that building's cell in place.
- **Runtime streaming** — `UBuildingStreamingSubsystem` loads an extract once and activates/evicts
  grid cells by distance to a reference location, with optional World Partition streaming source
  integration.
- **16 built-in presets** covering urban blocks, modern midrise, retail, industrial, parking,
  church/cathedral, and several German historical/postwar building styles, plus JSON import for
  externally authored configs.

## Requirements

- Unreal Engine 5.8
- World Partition–enabled level (only required for `bSaveAndUnloadPerCell` generation and the
  runtime streaming subsystem's native WP integration; all other features work in any level)

## Installation

Copy (or clone as a submodule) the `ProceduralBuildingGrammar` folder into your project's `Plugins/`
directory, then enable it from the Unreal Editor's Plugins window (or add it to your `.uproject`'s
plugin list) and restart the editor.

## Getting Started

1. Open **Tools > Procedural Building Grammar > Load Preset Config from JSON…** to load a facade/
   roof style config (optional — a built-in urban block preset is used otherwise).
2. Open **Tools > Procedural Building Grammar > Generate Buildings from OSM…** and select an `.osm`
   extract. The projection origin is derived automatically from the extract's building footprints.
3. For World Partition levels, you'll be prompted whether to save and periodically reload the level
   to bound memory on large extracts, and whether to bake each cell to a static mesh immediately.
4. Use **Bake Generated Buildings to Static Meshes…** to flatten existing pool actors afterward, and
   **Pick Building** to select and customize individual buildings in already-generated cells.

### From code

```cpp
ABuildingInstancePoolActor* Pool = nullptr;
UBuildingGenerationLibrary::GenerateBuildingsFromOsmFile(
    this, TEXT("C:/Extracts/district.osm"),
    OriginLatitude, OriginLongitude,
    GrammarBuildingPresets::UrbanBlockConfig(),
    Pool);
```

For large extracts, use `GenerateBuildingsFromOsmFileChunked` instead, which splits generation into
grid cells and supports memory-bounded save/reload and per-cell static mesh baking. For runtime,
distance-driven streaming, use `UBuildingStreamingSubsystem::LoadOsmExtract` and
`SetReferenceLocation`.

## Configuration

`FBuildingGrammarConfig` controls building-part handling, level/floor-height inference, facade
styles, roof shape/material/alignment, and excluded `building=*` values. Configs can be authored as
native `FBuildingGrammarConfig` JSON, or loaded from the Blender add-on's own snake_case preset
schema (see the bundled `Content/german_building_grammar_config.json` for an example covering 25
facade styles).

## Module Layout

| Module | Type | Purpose |
| --- | --- | --- |
| `BuildingGrammarCore` | Runtime | OSM parsing, footprint assembly, grammar rules, config, presets — no Unreal geometry/rendering types |
| `BuildingGrammarGeometry` | Runtime | Converts grammar output into `FDynamicMesh3` geometry and resolves/bakes shared kit meshes and materials |
| `BuildingGrammarRuntime` | Runtime | Instance pool actors, generation entry points, streaming subsystem, static mesh baking, World Partition persistence |
| `BuildingGrammarEditor` | Editor | Tools-menu commands and the building pick tool |

## Packaging for a Shipped Build

Kit meshes and materials are baked into saved assets the first time generation runs in-editor.
Run generation at least once in-editor (and keep the resulting content) before packaging, since
these assets are only ever *loaded* at runtime, never created outside the editor.

## License

No license file is currently included; treat as all rights reserved unless a license is added.
