#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGElement.h"
#include "Config/RoofStyleConfig.h"

class UMaterialInterface;

#include "PCGRoofFrameGenerator.generated.h"

// Per-building layout PCG node (native reimplementation -- see the module's own header comment):
// builds a roof mesh over a closed footprint spline. All 4 EGrammarRoofType values are implemented
// (Flat/Gabled/Hipped/Pyramid), each a faithful port of its GrammarRoof.cpp counterpart
// (FlatRoofMesh/GabledRoofMesh/HippedRoofMesh/PyramidRoofMesh) -- including Hipped's own fallback to
// Pyramid for near-square footprints (long axis no more than 25% longer than the side axis, same
// threshold classic uses). Reuses FGrammarRoofFrameMath (BuildingGrammarCore/Public/Geometry/
// GrammarRoofFrame.h) for the gabled/hipped ridge-frame math, and FGrammarPolygonTriangulator
// (BuildingGrammarCore/Public/Geometry/GrammarPolygonTriangulator.h) to ear-clip the Flat roof's
// footprint outline -- both bespoke geometric computation with no stock PCG node equivalent, so
// reusing the existing, already-correct math avoids re-deriving and re-debugging it by hand (same
// carve-out as UPCGSelectFacadeStyleSettings' tag-matching reuse). The ear-clipping in particular
// matters for non-convex (L-shaped/U-shaped) OSM footprints: a plain triangle fan from vertex 0 --
// this node's own approach before it was fixed -- produces triangles that cross outside the
// footprint outline at concave corners, visible as roof mesh hanging past the building's walls.
//
// Consumes every spline entry on its input pin independently, preserving tags, same as
// UPCGExtrudeFootprintToWallsSettings. Ridge direction for gabled/hipped roofs -- port of
// GrammarRoofDirection::RidgeDirection (GrammarRoofDirection.cpp:126-142, private/non-exported --
// ported by value, not called directly, same as GrammarEngineInternal's own porting elsewhere in
// this module): (1) an explicit roof:orientation/roof:direction OSM tag (cardinal/compass, or
// along/across relative to the footprint's own longest edge) wins outright; else (2), only if the
// resolved style's RidgeAlignment is ClosestStreet, an explicit grammar:roof:ridge_direction/
// roof:ridge:direction "X,Y" tag wins; else (3), still only for ClosestStreet and only when none of
// the four tags above are present at all (matching FGrammarStreetAlignment::ApplyRidgeDirectionTags'
// own injection gate), the nearest real street's tangent (optional "Streets" pin, addr:street name
// match ignores StreetSearchRadius same as UPCGFacadePatternStreetDetailLayoutSettings' identical
// street-facing tiers); else (4) the footprint's own longest edge (computed locally, self-contained
// -- not a call into BuildingGrammarCore's FGrammarGeometry2D::LongestAxisDirection).
//
// FGrammarRoofFrameMath::RoofBaseVertices (called here for every roof type's Base) offsets the
// footprint outward by Overhang via a per-vertex miter join, not a radial-from-centroid push -- see
// its own header comment for why that distinction matters on non-convex (L-shaped) footprints: a
// concave (inward) corner's eave vertex stays a uniform distance outside its wall edge instead of
// drifting sideways off it.
//
// Outputs one "Roof" UPCGDynamicMeshData per input footprint.
//
// Optional "StyleInfo" input pin (UPCGSelectFacadeStyleSettings' output): if connected, each
// building's own row supplies RoofType (this building's actual roof shape, not just material -- see
// UPCGSelectFacadeStyleSettings' header comment for its bOverrideRoof-else-DefaultRoof resolution)
// and RoofMaterial/RoofColor, resolved via FGrammarKitResolver::ResolveMaterial(StyleName, "roof",
// RoofMaterial, RoofColor) into the same persistent per-style asset the classic engine uses -- same
// mechanism and same reasoning as UPCGExtrudeFootprintToWallsSettings' own StyleInfo pin. RoofType/
// Material remain the fallback whenever StyleInfo is unconnected or a building's row isn't found in
// it.
//
// Optional "BuildingInfo" input pin (UPCGLoadOsmBuildingVolumesSettings' output): if connected, each
// building's own row supplies its own TotalHeight, used as this roof's EaveHeight instead of this
// node's own flat EaveHeight property -- same mechanism as UPCGExtrudeFootprintToWallsSettings' own
// BuildingInfo pin (and should resolve to the same value that node used for this building's walls,
// since both fall back the same way). EaveHeight remains the fallback whenever BuildingInfo is
// unconnected or a building's row isn't found in it. BuildingInfo's HasBuildingParts column also
// makes this node skip generating a roof entirely for a parent building that has building:part
// children (each part still gets its own) -- see UPCGExtrudeFootprintToWallsSettings' own header
// comment on wall-overlap suppression for why a parent building now reaches this node at all (it
// used to be dropped upstream whenever it had parts), and this class's own ExecuteInternal comment
// on HasBuildingPartsAttr for why roofs specifically still skip the parent rather than reconciling
// the overlap the way walls now do.
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGRoofFrameGeneratorSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("RoofFrameGenerator")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGRoofFrameGeneratorSettings", "NodeTitle", "Roof Frame Generator"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGRoofFrameGeneratorSettings", "NodeTooltip", "Builds a roof mesh (Flat or Gabled) over a closed footprint spline."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EGrammarRoofType RoofType = EGrammarRoofType::Gabled;

	// Eave height above the footprint plane, in UE centimeters -- fallback whenever the optional
	// BuildingInfo pin (see this class's header comment) is unconnected or has no usable TotalHeight
	// for a building; should match whatever Height UPCGExtrudeFootprintToWallsSettings actually used
	// for this building's walls (its own TotalHeight-or-Height fallback -- see that node's header
	// comment), or the roof won't sit on top of the walls correctly.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double EaveHeight = 900.0;

	// Ridge height above EaveHeight (Gabled only; ignored for Flat).
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "RoofType == EGrammarRoofType::Gabled"))
	double RidgeHeight = 200.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Overhang = 25.0;

	// Assigned to material slot 0 of the output UPCGDynamicMeshData -- see
	// UPCGExtrudeFootprintToWallsSettings::Material's comment for what an unset value looks like.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TSoftObjectPtr<UMaterialInterface> Material;

	// World-space UV tiling size in Unreal centimeters for the flat XY-projected UVs (see this
	// node's .cpp) -- correct texel density for a Flat roof; for a Gabled roof's sloped side faces,
	// this flat projection foreshortens the texture along the slope direction (a known Phase-A
	// approximation -- proper per-face-plane UV projection is a follow-up, not implemented yet).
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double TextureScale = 100.0;

	// Flips which vertex order is actually appended per triangle -- controls visibility/culling
	// only. Independent of bFlipNormals; see UPCGExtrudeFootprintToWallsSettings::bFlipWinding's
	// comment. Try both in combination if the roof appears invisible from outside, or from inside.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bFlipWinding = false;

	// Flips the stored shading normal only -- never the triangle winding. See
	// UPCGExtrudeFootprintToWallsSettings::bFlipNormals's comment for why the two are handled
	// separately. Toggle this in-editor if the roof renders visible-but-black/backwards-lit.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bFlipNormals = false;

	// Maximum distance (UE centimeters) from a building's footprint centroid to the nearest real
	// street before that street is ignored for the ClosestStreet ridge-alignment tier -- see this
	// class's header comment; only relevant when the optional "Streets" pin is connected and a
	// building's resolved RidgeAlignment is ClosestStreet with no explicit ridge-direction tag.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double StreetSearchRadius = 8000.0;
};

class FPCGRoofFrameGeneratorElement : public IPCGElement
{
public:
	// See UPCGExtrudeFootprintToWallsSettings' identical FPCGExtrudeFootprintToWallsElement override
	// -- same StyleInfo-driven FGrammarKitResolver::ResolveMaterial call, same game-thread-only
	// requirement.
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
