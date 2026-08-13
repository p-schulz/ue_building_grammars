#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGElement.h"
#include "Config/RoofStyleConfig.h"

class UMaterialInterface;

#include "PCGRoofFrameGenerator.generated.h"

// Per-building layout PCG node (native reimplementation -- see the module's own header comment):
// builds a roof mesh over a closed footprint spline. Phase A implements Flat and Gabled roofs only
// (Hipped/Pyramid are documented Phase B follow-ups -- see this plugin's plan notes); an unsupported
// RoofType falls back to Flat. Reuses FGrammarRoofFrameMath (BuildingGrammarCore/Public/Geometry/
// GrammarRoofFrame.h) for the gabled ridge-frame math -- bespoke geometric computation with no stock
// PCG node equivalent, so reusing the existing, already-correct math avoids re-deriving and
// re-debugging it by hand (same carve-out as UPCGSelectFacadeStyleSettings' tag-matching reuse).
//
// Consumes every spline entry on its input pin independently, preserving tags, same as
// UPCGExtrudeFootprintToWallsSettings. Ridge direction for gabled roofs is the footprint's own
// longest edge (computed locally, self-contained -- not a call into BuildingGrammarCore's
// FGrammarGeometry2D::LongestAxisDirection).
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
// unconnected or a building's row isn't found in it.
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
