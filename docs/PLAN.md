# Procedural Building Grammar — Unreal Engine 5.6 Plugin Port

## Context

`procedural_building_grammar` is a mature Blender add-on (11 Python files, ~2,250-line grammar
engine) that turns OSM building footprints + tags already present as Blender objects into detailed
procedural building meshes: walls, windows (+frames/mullions/sills/shutters), doors
(+frames/handles/canopies), ledges, balconies (+rails/bars), four roof types with tile bands,
dormers, roof windows, chimneys, gutters, parapets, antennas, and retail/industrial street-level
detail (signboards, awnings, garage doors, loading docks). It ships 31 hand-authored facade styles
and 16 full building presets (Gründerzeit, Plattenbau, Bauhaus, industrial, church/cathedral,
etc.), keyed off OSM tags via exact-match + semantic-keyword scoring. It has no OSM parser of its
own — it assumes a separate addon already imported footprints as tagged Blender mesh objects — and
its only "instancing" is Blender mesh-datablock sharing (translation-only, not real GPU instancing)
plus a mesh-merge-by-role pass.

The goal is a from-scratch Unreal Engine 5.6 C++ plugin at `/Users/schulz/ue_building_grammars/`
that reproduces this generation behavior, but:
- parses OSM data itself (built fresh inside the plugin, per user decision — not reusing the
  sibling `/Users/schulz/osm_building` C++ prototype or the Blender exporter's JSON format),
- is usable both as an **editor authoring tool** (bake a city block into level actors, mirroring
  the Blender workflow) **and** designed so its core generation logic is **runtime-callable** from
  day one (no editor-only dependencies baked into the grammar/geometry layer), and
- replaces Blender's mesh-sharing hack with genuine Unreal instancing (HISM/Nanite-ISM) so
  thousands of repeated windows/ledges/roof-tiles/antennas across a large area stay cheap.

Research basis: a full read of every source file in `procedural_building_grammar` (grammar.py in
two passes, config.py, builder.py, blender_adapter.py, exporter.py, export_config.py, operators.py,
panel.py, ui_settings.py, presets.py, __init__.py) and inspection of the sibling `osm_building` C++
prototype's headers (osm_model.h, style_rules.h, facade_elements.h, geometry2d.h, exporters.h) to
confirm it is *not* being reused, per the user's explicit choice.

## Architecture Overview

Four plugin modules, cleanly layered so the bottom two have no Editor/Slate/Actor dependencies and
can run identically in an Editor Utility tool or a packaged game:

```
BuildingGrammarCore      (Runtime, deps: Core, CoreUObject, Json, XmlParser)
  OSM ingestion, footprint/tag model, grammar config data, the pure-math grammar engine
      ↓ produces FBuildingSpec (array of FMeshSpec, mirroring MeshSpec dataclass) or, for
        instanced roles, FPlacementRecord (role, style/variant id, FTransform)
BuildingGrammarGeometry   (Runtime, deps: Core, GeometryCore, GeometryFramework, DynamicMesh)
  Turns FMeshSpec → FDynamicMesh3 (hero surfaces) and → baked Nanite StaticMesh "kit" parts
      ↓
BuildingGrammarRuntime    (Runtime, deps: Engine, above two)
  ABuildingActor, UBuildingInstancePool (HISM pooling), streaming subsystem
BuildingGrammarEditor     (Editor)
  Editor Utility Widget (import/generate/bake UI), DataAsset factories, details customization
```

`Content/` holds master materials + Material Instances for the "Grammar <Name>" material set,
`UDataAsset` instances for ported facade-style/building presets, and the baked Nanite kit meshes.

---

## 1. OSM Ingestion — `BuildingGrammarCore`

New parser (not derived from the sibling prototype), built on Unreal's `XmlParser` module
(`FXmlFile`/`FXmlNode`) rather than a hand-rolled tokenizer:

- `FOsmDocument::Parse(FilePath)` — reads `<node>`, `<way>`, `<relation>` elements + child `<tag>`s
  into `FOsmNode{Id, Lat, Lon}`, `FOsmWay{Id, NodeRefs, Tags}`, `FOsmRelation{Id, Members, Tags}`.
- `FBuildingFootprintAssembler` — for each `building=*`/`building:part=*` way, build a closed ring;
  for each `type=multipolygon` relation with a building tag, stitch `outer`-role ways by shared
  endpoint node id into one or more outer rings and assign `inner`-role ways as holes (same
  topological approach validated by the sibling prototype's README, reimplemented independently).
  Ways already consumed by a relation are not re-emitted standalone.
- `FLocalTangentPlaneProjection` — configurable origin (lat/lon), equirectangular local-meters
  projection, then scaled ×100 into UE centimeters. UE5.6's native double-precision Large World
  Coordinates removes the need for the sibling prototype's manual origin-rebasing/chunking — a
  single project-wide or per-import origin is enough; document this explicitly since it's a
  behavior Blender/Blender-adapter and the old prototype both had to work around and this plugin
  does not.
- `FBuildingFootprint{OsmId, OuterRing (CCW, FVector2D), Holes, TagMap}` — the ingestion output,
  analogous to `blender_adapter.py`'s `SourceFootprint`.
- Building-part parent/child resolution (port of `_source_volumes`/`_matching_parent` from
  `blender_adapter.py`): explicit relation-tag matching first (`building:parent`, `part_of`,
  `site`, OSM relation membership), else point-in-polygon of the part centroid against candidate
  parents (smallest-area containing parent wins), else edge-distance fallback tolerance. Reuse the
  polygon math already required for multipolygon assembly (point-in-ring, point-to-segment
  distance) — no external geometry library needed for this.
- Stretch (not v1): an Overpass API JSON fetch path behind the same `FBuildingFootprint` output, so
  the ingestion boundary is format-agnostic from the grammar engine's perspective.

**Status: implemented.** `FOsmDocument::ParseFile` (`Osm/OsmTypes.h` + `Private/Osm/OsmXmlParser.cpp`),
`FBuildingFootprintAssembler::Assemble` (`Osm/BuildingFootprintAssembler.h/.cpp`),
`FLocalTangentPlaneProjection` (`Geo/LocalTangentPlaneProjection.h/.cpp`),
`FBuildingPartResolver::Resolve` + `FGrammarPartTags` (`Osm/BuildingPartResolver.h/.cpp`,
`Grammar/GrammarPartTags.h/.cpp`). Overpass JSON fetch remains a stretch goal, not implemented.

## 2. Grammar Config Data Model — `BuildingGrammarCore`

Direct USTRUCT translation of `config.py`'s dataclasses, `UPROPERTY(EditAnywhere)` throughout so
they're editable both in a `UBuildingGrammarConfigAsset : UPrimaryDataAsset` and from C++/BP:

`FWindowStyleConfig`, `FLedgeStyleConfig`, `FBalconyStyleConfig`, `FDoorStyleConfig`,
`FAntennaStyleConfig`, `FRoofStyleConfig`, `FFacadeStyleConfig` (owns the five above),
`FBuildingGrammarConfig` (root: collection/output naming replaced by Actor/Data-Layer targets,
`bUseMeshInstancing`→drives the HISM-pool path described in §5, `BatchRoles`, level defaults,
irregular floor heights, excluded building values, style list, roof defaults). JSON
import/export (`FJsonObjectConverter`) replaces Blender's bespoke `from_dict`/`to_dict`, giving
config-file compatibility for free without hand-writing serialization.

Field-for-field cross-reference for this port is `ui_settings.py`'s `_style_from_settings` (the
~70-field flat-UI→struct mapping) and `config.py`'s `_style_from_dict`/`_roof_from_dict` — these
enumerate the authoritative full field list.

**Status: implemented.** See `Source/BuildingGrammarCore/Public/Config/`.

## 3. Grammar Engine — `BuildingGrammarCore`

`FBuildingGrammarEngine::GenerateBuildingSpec(Footprint, Tags, Config) -> FBuildingSpec` is a
straight, function-by-function port of `grammar.py`'s ~2,250 lines, pure math (no UObject/engine
mesh types), unit-testable in isolation:

- **Style selection**: exact tag/building-value matching, then semantic-keyword scoring
  (`_semantic_style_keywords`/`semantic_styles_for_tags`) — port the keyword-weight tables as-is.
- **Levels/floor heights**: `infer_levels`, `floor_height_sequence`, `effective_part_min_height`,
  `tags_for_building_part_volume`.
- **Per-edge generation**: wall (or per-floor "wall row") quads, window placement
  (`window_offsets`) + frame/mullion/sill/shutter detail, door + frame/handle/canopy, ledges,
  balconies + rail/bar detail, street-level retail/industrial/parking detail
  (signboard/awning/garage-door/loading-dock/stair-core), facade pattern bands (panel seams,
  insulation bands, ornament bands/pilasters) keyed by the same `_style_tokens` keyword-set
  mechanism.
- **Roof**: flat/gabled/hipped/pyramid roof-plane mesh, roof frame math (`RoofFrame`,
  `_roof_surface_z`'s gable-cross-section formula — see Known Behavior Decisions below), tile
  bands, dormers (+their own inset roof-window), standalone roof windows, chimneys, gutters,
  parapet/edge trim with corner caps, flat-roof service clutter (PV panels/HVAC/plant screens),
  antennas (8 types: tv/radio/satellite/lightning_rod/cellular/office_cluster/broadcast/lamp_post).
- **Shared geometry primitives** (heaviest reuse across the file, port once, use everywhere):
  `_oriented_box` (exact 8-vertex/6-face winding must be preserved for consistent normals),
  `_roof_base_vertices` (radial overhang expansion), `_segments`/`_tangent`/`_point_on_segment`,
  `signed_polygon_area`/`orient_footprint_ccw`, `_detail_positions` (evenly-spaced-with-inset
  placement reused by dormers/roof-windows/chimneys), `_stable_index` (djb2-style deterministic
  hash for per-building color-variant assignment — reimplement the exact algorithm so regenerating
  a building always yields the same variant, matching the Blender add-on's determinism guarantee).
- **Output shape — this is the key departure from the Python design**: `FMeshSpec` (vertices,
  triangle indices, role, material id, color, texture) is only actually populated for **hero,
  per-building-unique surfaces** (`facade`/`wall_row`, `roof`). For every role that today gets
  merged via `batch_roles` in the Python add-on (windows, frames, mullions, sills, ledges,
  balconies+rails+bars, door frame/handle/canopy, roof tiles/edge/windows, dormers, chimneys,
  gutters, antennas+panels, PV/HVAC/plant, shutters/signboards/awnings/garage doors, panel
  seams/insulation bands — the exact 28-role list from `config.py`'s `DEFAULT_BATCH_ROLES`), the
  engine instead emits **`FPlacementRecord{Role, VariantKey, FTransform}`** — geometry math only,
  no per-instance mesh construction. This is what makes the instancing story in §5 possible; see
  rationale there.

**Status: implemented**, including the architectural departure described above. `FBuildingGrammarEngine::GenerateBuildingSpec`
(`Public/Grammar/BuildingGrammarEngine.h` + `Private/Grammar/BuildingGrammarEngine.cpp`) is the
public entry point; everything else is module-private under `Private/Grammar/`: style selection
(`GrammarStyleSelection`, also exposed under `Public/Grammar/` since it's independently useful),
level/floor-height inference (`GrammarLevels`), shared tag-driven helpers (`GrammarEngineInternal`:
style tokens, wall-color variants, roof/facade tag overrides, street-facing side, door
applicability), wall+window (`GrammarWallWindow`, produces the only per-building-unique "facade"
hero mesh besides the roof), door (`GrammarDoor`), ledge+balcony (`GrammarLedgeBalcony`), roof
plane geometry for all four roof types (`GrammarRoof`, the other hero-mesh producer), roof-mounted
detail placements -- tiles/roof windows/dormers/chimneys/gutters/edge trim/PV+HVAC+plant
clutter/antennas (`GrammarRoofDetails`, `GrammarRoofDirection`), and street-level/pattern-band
detail -- signage/awnings/garage doors/loading docks/stair cores/panel seams/insulation
bands/ornament bands (`GrammarFacadeDepth`). The shared `FGrammarPlacementHelpers::MakeBoxPlacement`
(`Private/Grammar/GrammarPlacementHelpers.h`) is what makes this fast to have ported in full: every
non-hero role's `_oriented_box`/`_window_panel_mesh` call site became a placement-transform
computation instead of real per-instance vertex construction, since that geometry would only be
thrown away once HISM-pooled kit parts (Phase 4) take over -- see the note below.

Not yet covered by automated tests (see §10) -- ported against the Python source with care but
unverified by actually running it, since no engine was available to compile against while writing
this.

## 4. Mesh Generation & Instancing Strategy — the core technical decision

**Recommendation: hybrid — `FDynamicMesh3`-based hero surfaces (baked to Nanite `UStaticMesh` for
authored content, kept as `UDynamicMeshComponent` for pure-runtime cases) + a finite, editor-baked
Nanite StaticMesh "kit" per facade style for every repeated small element, placed purely via
per-instance `FTransform` into pooled `UHierarchicalInstancedStaticMeshComponent`s.** No external
mesh library (CGAL/OpenMesh/etc.) is needed — UE5.6's built-in `GeometryScriptingCore` /
`GeometryFramework` / `DynamicMesh` modules (shipped with the engine, no extra licensing/build
complexity) cover everything required.

Why this split, not "just use Geometry Script for everything" or "just use ISM for everything":

- **Repeated elements are the volume problem, and they're geometrically finite per style.** A
  window/frame/sill/ledge/balcony-rail/roof-tile/antenna's *shape* only varies with the facade
  style's configured dimensions, not per-instance — exactly like the Python add-on's
  `_stable_index`/cache-key logic already assumes (it dedupes by rounded relative-vertex shape).
  Unreal's `UInstancedStaticMeshComponent`/HISM is *built for* "same mesh, many transforms, GPU
  culled/LOD'd" — and Nanite meshes can be used with ISM/HISM in UE5, giving GPU-driven per-instance
  culling with zero manual LOD authoring. This is strictly better than Blender's mesh-datablock
  sharing (which only saves memory, not draw calls, and only matches on pure translation — no
  rotation sharing). §3's `FPlacementRecord` output means the grammar walk for these ~28 roles is
  pure geometry math (position/rotation/scale), never mesh construction — cheap enough to run at
  runtime for streaming without touching `DynamicMesh`/`GeometryScript` at all.
- **Hero surfaces (facade walls, roof planes) are genuinely unique per building** — no kit can
  cover arbitrary footprint polygons — so they must be constructed as real geometry, either at
  editor/import time or at runtime. `FDynamicMesh3` (the same in-memory mesh type Geometry Script
  operates on, from the engine's `GeometryCore`/`DynamicMesh` modules) is the right internal
  representation: build it directly in C++ (`AppendVertex`/`AppendTriangle`, bypassing Geometry
  Script's Blueprint-oriented function-call overhead for the hot path), then either:
  - **Editor/authoring path**: bake to `UStaticMesh` (`UGeometryScriptLibrary::CopyMeshToStaticMesh`
    or a direct `FMeshDescription` build for finer control over Nanite/collision/LOD settings) —
    gets Nanite, World Partition, HLOD, and a normal browsable content-browser asset.
  - **Pure-runtime path** (streaming, no editor present): keep it as a `UDynamicMeshComponent` and
    render directly — Geometry Script's mesh-editing functions are runtime-callable (not
    editor-only, since UE5.1+), satisfying "runtime-capable core from day one." Be explicit that
    Nanite is *not* available for a live `UDynamicMeshComponent` — that trade-off is inherent to
    UE5.6, not a plugin limitation, and only affects buildings generated purely at runtime with no
    editor bake pass.
- **Kit baking** (`BuildingGrammarGeometry`'s editor-only half, guarded by `WITH_EDITOR`): a "Bake
  Style Kit" tool walks one `FFacadeStyleConfig` and produces one Nanite `UStaticMesh` asset per
  distinct (role, parametrized shape) — e.g. one window, one window-frame side-piece, one
  balcony-rail-bar, one roof-tile-row-segment, etc. — mirroring presets.py's per-style dimension
  set. These become real, versioned content-browser assets, generated once per style rather than
  per building.
- **Non-conforming dimensions** (e.g. a facade length that doesn't evenly fit the kit's window
  spacing): default policy is snap-to-nearest-kit-variant (keeps everything instanced/cheap,
  matches how real prefabricated facade kits behave); an opt-in "high fidelity" per-instance
  `DynamicMesh` fallback is a documented future extension, not required for v1.

**Status: partially implemented.** `FGrammarPolygonTriangulator` (ear-clipping, handles the
flat-roof role's non-convex footprint-outline face correctly, unlike naive fan triangulation --
`BuildingGrammarCore/Geometry/`) and `FGrammarMeshUVs` (port of `_assign_uvs`/`_uv_axes`'s planar
projection -- also `BuildingGrammarCore/Geometry/`, since both are pure math with no engine mesh
dependency) feed `FGrammarDynamicMeshBuilder::BuildDynamicMesh`
(`BuildingGrammarGeometry/GrammarDynamicMeshBuilder.h/.cpp`), which turns one hero
`FGrammarMeshSpec` into a real `UE::Geometry::FDynamicMesh3` (vertices, triangulated faces, flat
per-triangle normals, planar UVs) -- enough to drive a `UDynamicMeshComponent` for the pure-runtime
path. **Flagged explicitly:** `FDynamicMesh3`'s attribute-overlay calls in that file
(`EnableAttributes`/`PrimaryNormals`/`PrimaryUV`/`AppendElement`/`SetTriangle`) were written from
recollection without engine headers or a compiler available to verify exact method
names/signatures against -- check this file first if `BuildingGrammarGeometry` fails to compile.
**Not yet implemented:** editor-time `UStaticMesh` baking (via `FMeshDescription` or
`UGeometryScriptLibrary::CopyMeshToStaticMesh`) and Nanite/kit-part baking -- carries the same kind
of unverified-API risk as the DynamicMesh builder, deliberately deferred rather than writing more
speculative Editor-API code in one pass; a good next increment.

## 5. Runtime Instancing — `BuildingGrammarRuntime`

- `ABuildingActor`: owns the hero-surface component (`UStaticMeshComponent` if baked,
  `UDynamicMeshComponent` if runtime-only) for one building/building-part.
- `UBuildingInstancePool` (owned by a per-cell manager, not per-building): one
  `UHierarchicalInstancedStaticMeshComponent` per distinct `(Role, StyleId, VariantKey)` bucket.
  Buildings append `FPlacementRecord`s as instance transforms into the pool that owns their cell —
  this is the standard "shared HISM pool across many actors" pattern (same shape used by Foliage
  and large PCG scatters), and is the direct fix for the Python add-on's per-object-even-if-shared
  limitation.
- A `UWorldSubsystem` (`UBuildingStreamingSubsystem`) coordinates: cell/tile determination around
  the player or a defined region, on-demand OSM ingestion + generation + actor spawn, pool
  eviction when a cell unloads. Integrate with World Partition streaming cells if the target level
  uses WP; otherwise a simple grid-cell fallback. This satisfies "runtime-capable core... from day
  one" concretely — the same `FBuildingGrammarEngine` call used by the editor tool drives this
  subsystem, just triggered by proximity instead of a button press.

**Status: partially implemented.** `ABuildingInstancePoolActor` (`BuildingInstancePoolActor.h/.cpp`)
owns one `UHierarchicalInstancedStaticMeshComponent` per `(Role, VariantKey)` bucket, created on
first use, with `AddInstance`/`ClearAllInstances`. `ABuildingActor`
(`BuildingActor.h/.cpp`) owns one `UDynamicMeshComponent` per hero mesh (built via
`FGrammarDynamicMeshBuilder` from the Geometry module) and appends its placements into a pool via
caller-supplied kit-mesh/material resolver callbacks. `UBuildingGenerationLibrary::GenerateBuildingsFromOsmFile`
(`BuildingGenerationLibrary.h/.cpp`) is the single end-to-end entry point tying every layer
together -- OSM file → parse → assemble footprints → project → resolve building-parts → generate
each volume's spec (with `grammar:part:min_height` Z-offset applied, mirroring
`blender_adapter.py`'s `_translated_mesh_specs`) → spawn actors -- as a plain
`UBlueprintFunctionLibrary` function, satisfying "runtime-capable core... callable from an editor
tool or game code" concretely (the same function either an Editor Utility Widget or gameplay code
would call). Both `.cpp` files carry the same kind of unverified-`UDynamicMeshComponent`-API risk
flagged for `GrammarDynamicMeshBuilder.cpp`.

Because kit-part meshes and per-style materials don't exist yet (sections 4 and 6), every
placement's kit mesh currently resolves to null via a stub callback and nothing is on a real
material -- only facade walls/roof planes are visible end-to-end today.

`UBuildingStreamingSubsystem` (`BuildingStreamingSubsystem.h/.cpp`) now provides proximity-based
streaming: `LoadOsmExtract` runs the expensive one-time parse/assemble/project/building-part-
resolution pipeline once and buckets the resulting volumes into a grid of `CellSize`-centimeter
cells by footprint centroid (no grammar generation or actor spawning yet at this point);
`SetReferenceLocation` then activates every cell within `StreamingRadius` of a given world point
(running the grammar engine and spawning `ABuildingActor`s + a per-cell `ABuildingInstancePoolActor`
only for those cells) and deactivates/destroys cells that fall back out of range. Deliberately
simple for a first version -- cell activation is synchronous (no async/threaded generation, so a
large `StreamingRadius` or `CellSize` can cause a frame hitch) and there's no automatic per-frame
tracking of a followed actor (something -- a pawn's `Tick`, a timer, a Blueprint event -- must call
`SetReferenceLocation` explicitly). Both are reasonable follow-ups once there's a real scene to
profile against rather than guesses about performance characteristics with no engine to test on.

Also not yet implemented: World Partition streaming-cell integration (this subsystem's own grid is
independent of and doesn't talk to WP), and per-building actor recentering (hero mesh vertices/
placements are currently absolute-world, so `ABuildingActor` must stay at the world origin -- see
the coordinate-convention note in `BuildingActor.h`).

## 6. Style/Preset Content Porting

Port `presets.py`'s reusable sub-style factories (`residential_door_style`, `office_door_style`,
`shopfront_door_style`, the four antenna-kind factories) as C++ builder functions returning
`FFacadeStyleConfig`/`FDoorStyleConfig`/`FAntennaStyleConfig`, then the 31 facade styles and 16
building presets as either equivalent C++ factories or (preferred, more designer-friendly)
`UDataAsset` instances constructed once via a one-time C++→asset bake step. Port in two waves:

- **Wave 1 (Phase 5, validates the whole pipeline)**: 5–6 representative styles spanning the
  visual range — `stone_urban_facade`, `modern_glass_facade`, `plattenbau_residential_facade`,
  `gruenderzeit_residential_facade`, `industrial_warehouse_facade`, `gothic_church_facade` — plus
  the `urban_block` and `german_office` building presets that combine multiple facade styles
  per-side.
- **Wave 2 (Phase 7)**: remaining 25 facade styles + 14 building presets, full parity with
  `presets.py`'s table (reference: the facade-style and building-preset tables already compiled
  during research, covering every style's wall material/color, window proportions, door type,
  antenna, and roof parameters).

Materials: replace Python's material-name keyword-sniffing (`_material_roughness`/
`_material_metallic` string matching in `blender_adapter.py`) with **explicit data fields** —
`FFacadeStyleConfig`/etc. get a roughness/metallic pair (or a `UMaterialInterface` reference)
directly, rather than inferring physical properties from a name substring. One parametrized master
material (`Content/Materials/M_GrammarBase`) driven by per-style Material Instance Constants
(base color, roughness, metallic, optional texture) replaces the ~60 ad-hoc "Grammar <Name>"
material names.

**Status: Wave 1 implemented.** `BuildingGrammarCore/Presets/` (pure data, no engine-mesh
dependency, same reasoning as placing it in Core rather than Editor/Runtime): `GrammarSubStyles`
ports all 8 reusable door/antenna builders; `GrammarFacadeStyles` ports 9 of the 31 facade styles,
each read from and checked against the actual `presets.py` source (not the earlier research
summary) field-by-field -- `stone_urban`, `quiet_side`, `brick_rowhouse`, `modern_glass`,
`warehouse` + `industrial_warehouse` (the clone-and-override pair), `gruenderzeit_residential`,
`plattenbau_residential`, and `gothic_church`; `GrammarBuildingPresets` ports `urban_block` (the
one Wave 1 building preset whose facade styles -- `stone_urban` + `quiet_side` -- are both already
available; the other 15 presets, including `german_office`, need facade styles not yet ported).
Zero engine-API risk (plain struct field assignment), and incidentally exercises/validates the
Config USTRUCTs from section 2 against real content for the first time. Remaining 23 facade styles
and 15 building presets are Wave 2.

## 7. Editor Tooling — `BuildingGrammarEditor`

Originally scoped as an Editor Utility Widget mirroring `panel.py`'s section layout (OSM import +
projection origin, config/preset picker, "Generate Buildings", "Bake Style Kit"). Revised in
practice: an Editor Utility Widget is a Blueprint (`.uasset`) asset, which cannot be authored as a
text source file the way everything else in this port has been -- so v1 is a plain C++
`Tools > Procedural Building Grammar > Generate Buildings from OSM...` main-menu command instead,
using `UToolMenus` (the same registration pattern used throughout the engine's own editor modules)
and `IDesktopPlatform::OpenFileDialog` for file picking. This is a real, clickable, buildable
feature rather than an asset the user would still need to construct themselves in-editor from a
written description.

**Status: v1 implemented** (`BuildingGrammarEditorModule.h/.cpp`). Flow: file-pick an `.osm` →
derive a projection origin from the file's own node bounding-box center
(`FOsmDocument::ComputeBoundsCenter`, new in `OsmTypes.h`) → generate with the `urban_block` Wave 1
preset → spawn into the currently open editor world, via the exact same
`UBuildingGenerationLibrary::GenerateBuildingsFromOsmFile` the runtime streaming subsystem uses.
No manual lat/lon entry or preset picker yet (both straightforward follow-ups -- a small custom
Slate dialog, or later, an Editor Utility Widget once one exists, could front the same library
call). Kit meshes/materials still resolve to null, so only facade walls/roof planes render after
running it, same caveat as section 5's generation path. Not yet implemented: config/preset
load-save-from-JSON UI, and the "Bake Style Kit" command (depends on section 4's kit baking, which
doesn't exist yet).

## 8. Repository Layout

```
/Users/schulz/ue_building_grammars/
├── ProceduralBuildingGrammar.uplugin
├── docs/PLAN.md               (this file)
├── Source/
│   ├── BuildingGrammarCore/        (Runtime)
│   ├── BuildingGrammarGeometry/    (Runtime, WITH_EDITOR-guarded baking functions)
│   ├── BuildingGrammarRuntime/     (Runtime)
│   └── BuildingGrammarEditor/      (Editor)
├── Content/
│   ├── Materials/
│   ├── Presets/
│   └── Kits/
└── Resources/
```

## 9. Known Behavior Decisions (flag explicitly, don't silently fix or silently replicate)

- `_roof_surface_z` in `grammar.py` is a pure gable cross-section (function of lateral offset
  only) used for tile/dormer/chimney *placement* regardless of `roof_type` — so hipped/pyramid
  roofs place details as if gabled, even though the roof *plane* mesh itself is correctly
  hipped/pyramidal. Default: **replicate for parity in Wave 1/2**, track as a Phase-7-follow-up fix
  once the visual difference is confirmed to matter. (Replicated as-is in `FGrammarRoofFrameMath::RoofSurfaceZ`.)
- Coordinate conversion: OSM lon/lat → local meters → UE centimeters, right-handed→left-handed
  axis handling, must be done correctly from scratch in `FLocalTangentPlaneProjection` (the old
  Blender exporter's `blender_to_unreal_transform` did a bare Y/Z swap with no handedness flip or
  unit scale and is *not* being reused) — cover with explicit unit tests (§10). Not yet implemented.
- `PRESET_ITEMS` vs `example_building_configs()` mismatch (`church_cathedral` missing from the
  Blender UI enum) — **fix** in the UE5 preset list rather than reproduce the omission.
- The grammar engine's placement-record `VariantKey` (`Spec/PlacementRecord.h`) is currently just
  `(Role, Material name)` -- sufficient to prove the pipeline out, but not yet the real
  dimension-aware signature a Phase 4 kit baker needs to decide which distinct unit-box variant an
  instance should bind to when a style's element dimensions could vary (they mostly don't within a
  single style today, but this should be revisited once Phase 4 actually authors kit assets).
- `FGrammarBoxPlacementParams`/`MakeBoxPlacement` assume every non-hero element is representable as
  one (possibly non-uniformly scaled) unit box. This holds for every role grammar.py builds via
  `_oriented_box`, and is a deliberate reinterpretation for roles it built via the flat,
  zero-thickness `_window_panel_mesh`/ledge quads (window panes, frame pieces, signboards, garage
  doors, panel seams, ornament bands, the ledge) -- those become thin boxes instead of true
  zero-thickness planes, which is visually near-identical and avoids a degenerate zero-scale
  transform axis. Flagged in-code at each call site; not expected to need revisiting.
- `DoorStyleConfig.placement`'s Python values `"first_facade"` and `"street_facing"` are behaviorally
  identical in `_door_applies` (both just mean "only the street-facing side gets a door") — merged
  into a single `EGrammarDoorPlacement::StreetFacing` enum value in the port rather than reproducing
  the redundant pair.
- `FBuildingGrammarConfig::ToJsonString`/`FromJsonString` currently use `FJsonObjectConverter`
  as-is, which round-trips against its own `PascalCase` field names — it does **not** read the
  add-on's existing `snake_case` JSON files (e.g. the bundled
  `german_building_grammar_config.json`, which `ui_settings.py`'s `bundled_german_building_config()`
  loads as the Blender UI's actual out-of-the-box defaults and contains real, detailed preset
  values worth reusing). Loading that file directly would need a manual `snake_case` from/to-JSON
  mapping (mirroring `config.py`'s `from_dict`/`to_dict`) instead of relying on
  `FJsonObjectConverter`'s name-for-name matching. Not implemented; flagged as a scoped follow-up
  for whenever preset content porting (§6) wants to pull real values from that bundled file rather
  than hand-authoring them.

## 10. Verification Plan

- **Unit tests** (Automation Spec, `BuildingGrammarCore`): footprint CCW cleanup, `window_offsets`
  spacing math, roof-frame projection math, `_oriented_box` vertex/face winding, `_stable_index`
  hash determinism, style tag-matching + semantic scoring, OSM XML parsing against small bundled
  `.osm` fixtures, multipolygon way-stitching, building-part parent matching, and the
  lon/lat→cm projection (round-trip + known-offset checks). **Not yet written** — should be added
  as each corresponding piece of the grammar engine/ingestion is implemented, not deferred to the
  end.
- **Editor smoke test**: import a small real extract, run "Generate Buildings," visually confirm
  walls/windows/doors/roof/roof-details render correctly and match the intended facade style;
  confirm HISM instance counts are sane via `stat instancedmesh`.
- **Performance validation**: generate a "big area" extract (several hundred buildings), confirm
  draw calls stay low (HISM/Nanite-ISM batching working — check `stat unit`/`stat gpu` and Nanite
  visualization), no per-frame allocation stalls when exercising the runtime streaming subsystem.
