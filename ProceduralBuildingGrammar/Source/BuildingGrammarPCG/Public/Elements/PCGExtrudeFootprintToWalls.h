#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGElement.h"

class UMaterialInterface;

#include "PCGExtrudeFootprintToWalls.generated.h"

// Per-building layout PCG node (native reimplementation, not a wrapper around
// BuildingGrammarCore/BuildingGrammarGeometry -- see the module's own header comment): takes a
// closed footprint spline (e.g. UPCGLoadOsmBuildingVolumesSettings' "Footprints" output) and
// extrudes it to a vertical wall shell, one quad per footprint edge.
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
// Optional "BuildingInfo" input pin (UPCGLoadOsmBuildingVolumesSettings' output): if connected, each
// building's own row supplies its own TotalHeight (derived from OSM building:levels/levels/height
// tags -- see that node's header comment), used as this wall's actual extrusion height instead of
// this node's own flat Height property, so buildings of different real heights don't all render at
// one uniform height. Height remains the fallback whenever BuildingInfo is unconnected or a
// building's row isn't found in it (or has no positive TotalHeight).
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
