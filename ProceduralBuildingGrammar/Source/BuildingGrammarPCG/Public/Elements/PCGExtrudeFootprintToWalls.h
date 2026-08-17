#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGElement.h"

class UMaterialInterface;

#include "PCGExtrudeFootprintToWalls.generated.h"

// Per-building layout PCG node (native reimplementation of the mesh-building itself, not a wrapper
// around BuildingGrammarCore/BuildingGrammarGeometry -- see the module's own header comment; the one
// exception is FGrammarWallRecess::BuildSegments below, pure shared geometry math with none of the
// asset-creation/thread-safety concerns that motivate the rest of this module's native
// reimplementation): takes a closed footprint spline (e.g. UPCGLoadOsmBuildingVolumesSettings'
// "Footprints" output) and extrudes it to a vertical wall shell, one quad per footprint edge (or
// more, wherever a window/door recess cuts into it -- see the StyleInfo paragraph below).
//
// Consumes every spline entry on its input pin independently (no explicit Loop/Subgraph needed --
// standard PCG multi-data-per-pin processing), preserving each entry's tags on its corresponding
// outputs so downstream nodes can correlate a building's Footprint/Walls/Edges by tag (e.g.
// "SourceName:...").
//
// Outputs two pins, one-to-one with each input footprint:
//  - "Walls": one UPCGDynamicMeshData wall shell (all edges merged into a single mesh).
//  - "Edges": one UPCGPointData holding one point per footprint edge (Transform = edge start
//    position, rotated so local +X faces along the edge's tangent and local +Y faces outward), with
//    "Length" (double) and "EdgeIndex" (int32) attributes -- the per-wall input a window/door layout
//    node needs.
//
// Optional "StyleInfo" input pin (UPCGSelectFacadeStyleSettings' output): if connected, each
// building's own row (matched by its "SourceName:..." tag, same correlation as
// UPCGFacadeWindowDoorLayoutSettings -- see that node's header comment for why this can't go through
// PCG's generic per-node "Overrides" pin) supplies WallMaterial/WallColor plus WallColorVariants/
// WallColorVariantMode and WallRowColors/WallRowColorMode (FFacadeStyleConfig -- full port of
// GrammarEngineInternal::VariantWallColor/RowWallColor, GrammarGrammarCore/Private/Grammar/
// GrammarEngineInternal.cpp:112-153, done HERE per edge/floor rather than in
// UPCGSelectFacadeStyleSettings since that node's StyleInfo row is per-building, no per-edge
// SideIndex). When WallRowColors is set, this node builds one quad PER FLOOR per edge instead of one
// quad for the whole facade height (mirrors BuildingGrammarEngine.cpp's own WallRowMesh-vs-WallMesh
// dispatch), each floor/edge combination resolved to its own persistent per-style material via
// FGrammarKitResolver::ResolveMaterial(StyleName, "facade", MaterialName, Color) -- so wall
// appearance actually varies by style AND by building/edge/floor instead of every building of the
// same style rendering with one identical uniform color. Material/WallColor remain the fallback
// whenever StyleInfo is unconnected or a building's row isn't found in it (single material slot,
// same as before this feature existed).
//
// StyleInfo also drives wall recessing (a visible reveal/step-back around each window/door opening,
// rather than the flat unbroken wall plane this node built before this feature existed): when a
// building has a StyleInfo row, this node independently resolves the same window/door offsets
// UPCGFacadeWindowDoorLayoutSettings resolves for its own placements (WindowWidth/Height/Spacing/
// Margin/SillHeight, DoorEnabled/Width/Height/Placement -- same StyleInfo attribute names, same
// DoorApplies/WindowOverlapsDoor port, same DetermineStreetFacingSideIndex call for a StreetFacing
// door), then cuts a recessed pocket at each one via FGrammarWallRecess::BuildSegments -- a back
// quad pushed inward by WindowRecessDepth/DoorRecessDepth plus four reveal quads (jambs/sill/head)
// bridging it back out to the flush plane, the same shared algorithm GrammarWallWindow.cpp's
// WallMesh/WallRowMesh use in the classic engine. Deliberately scoped to StyleInfo-connected
// buildings only (no Settings-level fallback, and no new "Streets" pin -- an absent one behaves
// exactly like UPCGFacadeWindowDoorLayoutSettings' own unconnected Streets pin): a building with no
// StyleInfo row simply gets no recessing, same as it gets no wall-color variation today.
//
// Optional "BuildingInfo" input pin (UPCGLoadOsmBuildingVolumesSettings' output): if connected, each
// building's own row supplies its own TotalHeight (derived from OSM building:levels/levels/height
// tags -- see that node's header comment), used as this wall's actual extrusion height instead of
// this node's own flat Height property, so buildings of different real heights don't all render at
// one uniform height. Height remains the fallback whenever BuildingInfo is unconnected or a
// building's row isn't found in it (or has no positive TotalHeight). BuildingInfo's IsBuildingPart/
// ParentSourceName columns also drive wall-overlap suppression below when connected -- unconnected,
// suppression still runs (using a plain larger-footprint-wins rule for every pair) but without the
// parent/child priority that keeps a building:part's own walls from being suppressed by its parent.
//
// Wall-overlap suppression (bSuppressOverlappingWalls, default on): real OSM data very commonly has
// overlapping building footprints -- a `building` way whose `building:part` children don't exactly
// tile it, or two separate adjacent buildings (rowhouses) whose footprints touch or slightly
// overlap -- which would otherwise extrude two independent, overlapping/z-fighting wall shells for
// the same physical space. This node consumes every Footprint input TOGETHER (not independently, in
// this one respect) to detect that: each edge is sampled every WallSuppressionSampleSpacing along
// its length, and any sample point that falls inside ANOTHER footprint is treated as "already
// walled by something else" and excluded from this edge's own wall generation (and its "Edges" pin
// output), splitting the edge into just its still-uncovered sub-ranges. Priority between two
// overlapping footprints: a building:part's own walls are NEVER suppressed by its own parent (a
// part's footprint is expected to sit against/inside its parent's outline); a parent IS suppressed
// wherever a building:part of ITS OWN covers it (this is also what lets a parent whose parts don't
// fully tile it still generate walls for the leftover, uncovered portion, instead of the whole
// parent being dropped -- see UPCGLoadOsmBuildingVolumesSettings::Config's
// bSkipParentFootprintsWithParts, defaulted to false by that node specifically so this suppression
// has both volumes to work with); two unrelated/sibling footprints use a deterministic larger-area-
// wins rule (SourceName as a last-resort tiebreak for an exact area tie). Priority alone isn't
// enough to suppress, though: the outranking volume's own height range must FULLY CONTAIN the
// suppressed one's (see DoesOtherVerticallyContainSelf in the .cpp) -- suppression is all-or-
// nothing per edge sample (no vertical splitting), so a shorter building:part, or one with its own
// min_height starting above ground, only actually covers PART of what its parent would render
// there; suppressing the parent's full-height wall anyway would leave a real hole below and/or
// above the part instead of a duplicate. When the height check fails, that edge sample is left
// un-suppressed -- both volumes' walls render there, a visible double wall rather than a hole.
//
// This is a per-edge-sample approximation, not a true 2D polygon union/boolean (no such utility
// exists anywhere in this codebase to reuse, and building one was out of scope for this feature) --
// a transition between covered and uncovered is only located to within one sample spacing, and an
// O(N^2) footprint-pair bounding-box pass runs once up front to keep the per-sample point-in-
// polygon checks cheap (fine for a single generation run over a normal OSM extract; would need a
// spatial grid to scale to a very large one).
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGExtrudeFootprintToWallsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("ExtrudeFootprintToWalls")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExtrudeFootprintToWallsSettings", "NodeTitle", "Extrude Footprint To Walls"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGExtrudeFootprintToWallsSettings", "NodeTooltip", "Extrudes a closed footprint spline into a vertical wall shell mesh, plus one point per edge for downstream facade layout."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	// Wall height in Unreal centimeters (this pipeline works in UE world units throughout, unlike
	// BuildingGrammarCore's own meters convention -- see the module's own header comment). Fallback
	// whenever the optional BuildingInfo pin is unconnected or has no usable TotalHeight for a
	// building -- see this class's header comment.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Height = 900.0;

	// Assigned to material slot 0 of the output UPCGDynamicMeshData -- unset (the "Spawn Dynamic
	// Mesh" stock node's default) renders with the engine's default material (flat gray,
	// untextured), which is what an unset value here looks like.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TSoftObjectPtr<UMaterialInterface> Material;

	// World-space UV tiling size in Unreal centimeters -- each quad's UVs span (EdgeLength /
	// TextureScale) x (Height / TextureScale), so a texture repeats at a consistent physical size
	// across walls of different lengths instead of always stretching exactly one tile per quad
	// (which is what naive 0..1-per-quad UVs would do).
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double TextureScale = 100.0;

	// Flips which vertex order is actually appended per triangle -- controls visibility/culling
	// only, never the stored shading normal (see bFlipNormals). Independent toggle: try both in
	// combination with bFlipNormals in-editor if walls appear invisible from outside, or visible
	// only from inside.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bFlipWinding = false;

	// Flips the stored shading normal only -- never the triangle winding (see bFlipWinding). Toggle
	// this in-editor if walls render visible-but-black/backwards-lit.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bFlipNormals = false;

	// See this class's own header comment for the full algorithm. Disable to restore the previous
	// behavior (every footprint's walls generated independently, ignoring overlaps with others).
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Wall Overlap Suppression", meta = (PCG_Overridable))
	bool bSuppressOverlappingWalls = true;

	// Distance in Unreal centimeters between sample points along each edge when checking whether
	// it's covered by another footprint -- smaller values locate the covered/uncovered transition
	// more precisely at the cost of more point-in-polygon checks per edge.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Wall Overlap Suppression", meta = (PCG_Overridable, EditCondition = "bSuppressOverlappingWalls", ClampMin = "5.0"))
	double WallSuppressionSampleSpacing = 50.0;
};

class FPCGExtrudeFootprintToWallsElement : public IPCGElement
{
public:
	// When a StyleInfo pin is connected, ExecuteInternal calls FGrammarKitResolver::ResolveMaterial,
	// which (WITH_EDITOR) creates/loads persistent UMaterialInstanceConstant assets via
	// FGrammarKitAssetBuilder -- editor asset creation and several UMaterialInterface parameter APIs
	// assert IsInGameThread() (hit in practice: FMaterialInterface::SetVectorParameterValueEditorOnly-
	// style calls assert at MaterialInterface.cpp inside GetOrCreateRoleMaterial), so PCG must not run
	// this element on a worker thread. Matches the same override several stock nodes that touch
	// assets/actors use (e.g. UPCGStaticMeshSpawnerSettings, UPCGSpawnActorSettings).
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
