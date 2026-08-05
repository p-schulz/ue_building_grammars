# Procedural Building Grammar

An Unreal Engine 5.6 plugin that generates procedural buildings from OpenStreetMap footprints
using facade and roof grammar rules, instanced across large areas via HISM so windows, ledges,
roof tiles, and antennas stay cheap at city scale.

## Features

- **OSM ingestion** — XML parsing, multipolygon relation assembly, `building:part` parent/child
  resolution, configurable-origin projection to world space.
- **Full grammar engine** — walls, windows (frame/mullion/sill/shutter), doors (frame/handle/
  canopy), ledges, balconies (rail/bar), flat/gabled/hipped/pyramid roofs with tiles, dormers, roof
  windows, chimneys, gutters, parapet trim, 8 antenna types, street-level retail/industrial detail,
  and facade pattern bands.
- **Large-area performance** — repeated elements resolve to instance transforms for pooled
  `HierarchicalInstancedStaticMeshComponent`s instead of unique per-instance geometry, using a
  shared Nanite-enabled kit mesh baked (and cached) automatically on first use.
- **Runtime streaming** — load an extract once, then stream buildings in and out by proximity to a
  reference point (e.g. the player).
- **Editor tool** — `Tools > Procedural Building Grammar > Generate Buildings from OSM...`.
- **Style presets** — 30 of the source add-on's 31 facade styles are available: 9 ported directly
  to C++, the rest loadable from the add-on's own bundled JSON preset file (see JSON below).
- **JSON preset import/export** — reads and writes the source Blender add-on's own config schema
  (snake_case field names, e.g. `wall_material`, `default_floor_height`) directly, so existing
  exported presets load without any conversion step.

## Requirements

- Unreal Engine 5.6
- The engine's built-in **Geometry Script** plugin (declared as a dependency; enables
  automatically with this plugin — no separate install).

## Installation

1. Copy or clone this repository into `<YourProject>/Plugins/ProceduralBuildingGrammar/`.
2. Regenerate project files (right-click your `.uproject` → *Generate Visual Studio project
   files*, or run `UnrealBuildTool`/`RunUAT` directly on macOS/Linux).
3. Open the project and build (the editor will prompt to compile missing modules on load).
4. Confirm it's enabled under **Edit > Plugins**.

## Usage

### Editor

**Tools > Procedural Building Grammar:**
- **Load Preset Config from JSON...** — load a facade/roof preset config exported from the source
  Blender add-on (or written by this plugin). Used by the command below once loaded.
- **Generate Buildings from OSM...** — pick a `.osm` file and buildings are generated into the
  currently open level, using whichever config was most recently loaded (or the built-in
  `urban_block` preset if none has been). A projection origin is derived automatically from the
  file's own bounding box.

### Blueprint / C++

```cpp
ABuildingInstancePoolActor* Pool = nullptr;
UBuildingGenerationLibrary::GenerateBuildingsFromOsmFile(
    this, TEXT("C:/data/city.osm"), OriginLatitude, OriginLongitude, Config, Pool);
```

### Loading/saving presets

```cpp
FBuildingGrammarConfig Config;
FString Error;
FGrammarConfigJson::LoadConfigFromPythonJsonFile(TEXT("C:/data/my_presets.json"), Config, Error);

// ...edit Config.Styles, Config.Roof, etc...

FGrammarConfigJson::SaveConfigToPythonJsonFile(TEXT("C:/data/my_presets.json"), Config, Error);
```

### Runtime streaming

```cpp
UBuildingStreamingSubsystem* Streaming = GetWorld()->GetSubsystem<UBuildingStreamingSubsystem>();
Streaming->LoadOsmExtract(OsmFilePath, OriginLatitude, OriginLongitude, Config);
Streaming->SetReferenceLocation(PlayerPawn->GetActorLocation()); // call again as the player moves
```

## Status & Roadmap

| Module | Status |
|---|---|
| `BuildingGrammarCore` | Complete — OSM ingestion, config model, full grammar engine |
| `BuildingGrammarGeometry` | Hero mesh → `FDynamicMesh3` builder done; Nanite kit baking (unit-box mesh + master material) done; per-style/per-dimension kit variants not yet split out (every role currently shares one mesh, scaled per instance) |
| `BuildingGrammarRuntime` | Generation + proximity streaming complete; async generation and per-actor recentering not implemented |
| `BuildingGrammarEditor` | v1 menu commands (generate + JSON preset load) done; no in-editor property picker UI yet |
| Style presets | 30 of 31 facade styles (9 native C++, 21 more via JSON import); 1 of 16 full building presets ported natively |

Every generated element now has real geometry: instanced elements (windows, doors, roof tiles,
balconies, antennas, ...) render as a shared, non-uniformly-scaled unit box tinted per material via
a dynamic material instance — not their true per-element shape yet (a window and a roof tile both
render as boxes sized to the right dimensions, not their distinct silhouettes), but no longer
invisible. Kit baking happens automatically and is cached on disk under
`/ProceduralBuildingGrammar/Kits/` the first time generation runs in-editor; **a level intended for
a packaged (non-editor) build needs generation to have run at least once in-editor first**, since
new assets can only be baked inside the editor — see `GrammarKitResolver.h`.


## Architecture

```
ProceduralBuildingGrammar.uplugin
Source/
├── BuildingGrammarCore/            OSM ingestion, grammar config, grammar engine, presets, JSON I/O
├── BuildingGrammarGeometry/        FMeshSpec -> FDynamicMesh3 / Nanite kit baking
├── BuildingGrammarRuntime/         Actors, HISM instance pools, streaming subsystem
└── BuildingGrammarEditor/          Level Editor menu tooling
Content/                            (empty at rest -- kit mesh/material are baked into
                                     /ProceduralBuildingGrammar/Kits/ the first time you generate)
```

Two independent JSON formats exist by design: `FBuildingGrammarConfig::ToJsonString`/
`FromJsonString` round-trip this plugin's own `PascalCase` `UPROPERTY` names, while
`FGrammarConfigJson` reads and writes the source Blender add-on's `snake_case` schema. Use the
latter to exchange presets with the add-on; the former is only useful for round-tripping within
this plugin itself.

