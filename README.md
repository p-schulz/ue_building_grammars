# Procedural Building Grammar (Unreal Engine 5.6 Plugin)

Generates procedural buildings from OpenStreetMap footprints using facade and roof grammar rules,
instanced across large areas via HISM/Nanite so windows, ledges, roof tiles, and antennas stay
cheap at city scale. This is a from-scratch UE5.6 C++ port of the
[`procedural_building_grammar`](../../bl_py_facade/procedural_building_grammar) Blender add-on's
generation logic — see [`docs/PLAN.md`](docs/PLAN.md) for the full architecture, the reasoning
behind the mesh-generation/instancing strategy, and section-by-section porting notes cross-
referenced against the original Python source.

## Status

`BuildingGrammarCore` is functionally complete: OSM XML ingestion, the full grammar config data
model, and the entire grammar engine (style selection, level inference, walls, windows + frame/
mullion/sill/shutter, doors + frame/handle/canopy, ledges, balconies + rail/bar, all four roof
types, roof tiles/dormers/roof windows/chimneys/gutters/edge trim/antennas/PV+HVAC+plant clutter,
street-level retail/industrial/parking detail, and facade pattern bands) is ported from
`grammar.py`/`config.py`/`blender_adapter.py`'s building-part handling. Entry point:
`FBuildingGrammarEngine::GenerateBuildingSpec`.

- Plugin skeleton: `ProceduralBuildingGrammar.uplugin` + four modules (`BuildingGrammarCore`,
  `BuildingGrammarGeometry`, `BuildingGrammarRuntime`, `BuildingGrammarEditor`) with `.Build.cs`
  and minimal `IModuleInterface` boilerplate.
- `BuildingGrammarCore`: OSM XML parsing + multipolygon footprint assembly + building-part parent/
  child resolution (`Osm/`), lat/lon → UE-centimeter projection (`Geo/`), shared geometry
  primitives (`Geometry/`), the full grammar config data model (`Config/`, one USTRUCT per
  `config.py` dataclass), generation output types (`Spec/`), and the grammar engine itself
  (`Grammar/`) — see `docs/PLAN.md` section 3 for the full breakdown.

`BuildingGrammarGeometry` has its first piece: `FGrammarDynamicMeshBuilder` turns one hero
`FGrammarMeshSpec` (facade wall or roof plane) into a real `UE::Geometry::FDynamicMesh3` — ear-clip
triangulated (`FGrammarPolygonTriangulator`, handles non-convex flat-roof outlines correctly),
with flat normals and planar UVs (`FGrammarMeshUVs`, port of `_assign_uvs`) — enough to drive a
`UDynamicMeshComponent` for the pure-runtime path. Its `FDynamicMesh3` attribute-overlay calls are
the one part of the whole port written from recollection without engine headers to check against;
see the flag in `docs/PLAN.md` section 4 and the comment at the top of
`GrammarDynamicMeshBuilder.cpp`.

`BuildingGrammarRuntime` now has a complete (if not yet kit-mesh-populated) generation path:
`ABuildingInstancePoolActor` (HISM buckets keyed by `(Role, VariantKey)`), `ABuildingActor` (hero
`UDynamicMeshComponent`s), and `UBuildingGenerationLibrary::GenerateBuildingsFromOsmFile` — a single
Blueprint-callable function that runs the entire pipeline end to end: parse an `.osm` file, project
it, resolve building-parts, generate every volume's spec, and spawn actors. Facade walls and roof
planes render today; every other element (windows, doors, roof tiles, ...) computes correct
placement transforms but has no kit mesh to instance yet (see `docs/PLAN.md` section 4).

Wave 1 of the preset content library is in `BuildingGrammarCore/Presets/`: all 8 reusable door/
antenna sub-style builders, 9 of the 31 facade styles (`stone_urban`, `quiet_side`,
`brick_rowhouse`, `modern_glass`, `warehouse`/`industrial_warehouse`, `gruenderzeit_residential`,
`plattenbau_residential`, `gothic_church`), and the `urban_block` building preset — each checked
field-by-field against the real `presets.py` source. Pure data, zero engine-API risk.

`UBuildingStreamingSubsystem` adds proximity-based streaming on top of the same generation
pipeline: `LoadOsmExtract` parses/projects/resolves an extract once and buckets its buildings into
grid cells, and `SetReferenceLocation` activates/deactivates cells (spawning/destroying actors and
pools) as that point moves — simple and synchronous for now (no async generation, no automatic
per-frame tracking of a followed actor), a reasonable base to build on once there's a real scene to
profile.

`BuildingGrammarEditor` has a real, clickable v1: **Tools > Procedural Building Grammar > Generate
Buildings from OSM...** in the Level Editor menu — file-picks an `.osm`, derives a sensible
projection origin automatically, and generates with the `urban_block` preset into the open level.
(Not an Editor Utility Widget Blueprint — those are `.uasset` files that can't be authored as text
source, so this is a plain `UToolMenus` command instead; see `docs/PLAN.md` section 7.)

Not yet implemented: editor-time `UStaticMesh`/Nanite kit baking, World Partition integration, a
config/preset picker UI, and the remaining 23 facade styles / 15 building presets (Wave 2). See
`docs/PLAN.md` for the
phase-by-phase breakdown, each section's current status, and the "Known Behavior Decisions" this
port made explicitly (including one architectural departure: non-hero elements became placement
transforms for a shared unit-box kit instead of real per-instance geometry, since that's what the
HISM instancing design in section 4 needs anyway).

**None of this has been compiled** — no UE5 engine was available in the authoring environment, so
treat it as carefully-written-but-unverified until you build it. See Building below.

## Building

No engine was available in the environment this was authored in, so **none of this has been
compiled yet** — treat it as reviewed-but-unverified until you build it. To build:

1. Clone/copy this repo into `<YourProject>/Plugins/ProceduralBuildingGrammar/`, or register it
   as an engine plugin.
2. Right-click your project's `.uproject` → **Generate Visual Studio project files** (or run
   `UnrealBuildTool` / `RunUAT` directly on macOS/Linux), which also regenerates IDE include paths
   so editor error squiggles about unresolved engine headers go away.
3. Open the project; UE5.6 will prompt to build the plugin's modules automatically, or build via
   your IDE / `UnrealBuildTool -Target=<YourProject>Editor ...`.
4. Enable the plugin in **Edit > Plugins** if it isn't auto-enabled.

The `.uplugin` declares a dependency on the engine's `GeometryScripting` and `ModelingComponents`
plugins (needed once `BuildingGrammarGeometry` is implemented) — make sure those are available in
your engine install (they ship with UE5.6 by default).

## Repository Layout

```
ProceduralBuildingGrammar.uplugin
docs/PLAN.md                        Full architecture & porting plan
Source/
├── BuildingGrammarCore/            OSM ingestion + grammar config + grammar engine (Runtime)
├── BuildingGrammarGeometry/        FMeshSpec -> FDynamicMesh3 / Nanite kit baking (Runtime)
├── BuildingGrammarRuntime/         Actors, HISM instance pools, streaming subsystem (Runtime)
└── BuildingGrammarEditor/          Editor Utility Widget, DataAsset factories (Editor)
Content/                            Materials, ported style presets, baked kit meshes
```
