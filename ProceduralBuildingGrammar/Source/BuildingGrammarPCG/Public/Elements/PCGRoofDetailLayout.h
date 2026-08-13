#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGElement.h"
#include "Config/RoofStyleConfig.h"

class UMaterialInterface;

#include "PCGRoofDetailLayout.generated.h"

// Per-building layout PCG node (native reimplementation -- see the module's own header comment):
// places roof edge/corner trim, gutters, tile bands, roof windows, dormers (+ companion window), and
// chimneys over a closed footprint spline -- port of GrammarRoofDetails.cpp's RoofEdgePlacements/
// GutterPlacements/RoofTilePlacements/RoofWindowPlacements/DormerPlacements/ChimneyPlacements.
//
// Mirrors UPCGRoofFrameGeneratorSettings' own design rather than UPCGFacadeWindowDoorLayoutSettings':
// roof SHAPE/geometry parameters (RoofType, EaveHeight, RidgeHeight, Overhang, and every role's own
// dimensions: EdgeWidth/Height, TileRows, DormerCount, etc.) are flat node Settings shared by the
// whole graph run -- same simplification UPCGRoofFrameGeneratorSettings already has (no per-style
// roof-shape variation is a known Phase-A gap, not something this node fixes). EaveHeight/RidgeHeight/
// Overhang/RoofType should match whatever UPCGRoofFrameGeneratorSettings node built this building's
// actual roof mesh, or detail placements won't sit on the roof surface correctly.
//
// Reuses FGrammarRoofFrameMath (BuildingGrammarCore/Public/Geometry/GrammarRoofFrame.h) for the same
// roof-local frame math GrammarRoofDetails.cpp itself uses -- bespoke geometric computation with no
// stock PCG node equivalent and already exported/tested, so reusing it avoids re-deriving detail
// placement math by hand (same carve-out as UPCGRoofFrameGeneratorSettings' own reuse of this class
// for the roof-plane mesh). RoofSurfaceZ is a pure gable cross-section regardless of roof type (see
// FGrammarRoofFrame's own header comment) -- carried over deliberately for parity with the classic
// engine, not a bug introduced here.
//
// RoofType gating (matches GrammarRoofDetails.cpp exactly): edge/corner trim only on Flat; tile
// bands/standalone roof windows only on non-Flat; dormers (+ companion window) only on Gabled or
// Hipped specifically; chimneys and gutters on any roof type. Phase A's roof-plane mesh (see
// UPCGRoofFrameGeneratorSettings) only actually builds Flat or Gabled shapes (falling back to Flat
// for anything else) -- selecting Hipped/Pyramid here only affects THIS node's own gating logic, not
// what shape the paired roof mesh renders as, until Roof Frame Generator gains real Hipped/Pyramid
// support.
//
// Optional "BuildingInfo" input pin (UPCGLoadOsmBuildingVolumesSettings' output): if connected, each
// building's own row supplies its own TotalHeight, used as this roof's EaveHeight instead of this
// node's own flat EaveHeight property -- same mechanism as UPCGRoofFrameGeneratorSettings' own
// BuildingInfo pin (should resolve to the same value that node used for this building's actual
// roof). EaveHeight remains the fallback whenever BuildingInfo is unconnected or a building's row
// isn't found in it.
//
// Optional "StyleInfo" input pin (UPCGSelectFacadeStyleSettings' output): if connected, each
// building's own row supplies its own RoofType (used for this node's own Flat/Gabled/Hipped gating,
// same as UPCGRoofFrameGeneratorSettings -- see that node's header comment) plus EdgeMaterial/
// EdgeColor, TileMaterial/TileColor, RoofWindowMaterial/RoofWindowColor, DormerMaterial/
// DormerColor, ChimneyMaterial/ChimneyColor,
// resolved via FGrammarKitResolver::ResolveMaterial(StyleName, Role, MaterialName, Color) into the
// same persistent per-style asset the classic engine uses -- same mechanism as every other node in
// this module. Each role's own Settings Material/Color below is the fallback whenever StyleInfo is
// unconnected or a building's row isn't found in it. Gutters are the one role classic never makes
// style-driven (hardcoded "Grammar Roof Gutters" name/color in GrammarRoofDetails.cpp) -- ported the
// same way here, still resolved through ResolveMaterial using whatever StyleName is available so the
// asset still lands in the right per-style folder.
//
// Outputs one "Placements" UPCGPointData per building, same box-CENTER + (Width,Depth,Height) Scale
// convention and "MaterialOverride" FSoftObjectPath attribute as UPCGFacadeWindowDoorLayoutSettings
// (see its own header comment) -- "Role" is one of "roof_edge", "roof_gutter", "roof_tile",
// "roof_window", "dormer", "chimney".
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGRoofDetailLayoutSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("RoofDetailLayout")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGRoofDetailLayoutSettings", "NodeTitle", "Roof Detail Layout"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGRoofDetailLayoutSettings", "NodeTooltip", "Places roof edge/corner trim, gutters, tile bands, roof windows, dormers, and chimneys over a closed footprint spline."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	// Should match the paired UPCGRoofFrameGeneratorSettings node's own RoofType/EaveHeight/
	// RidgeHeight/Overhang -- see this class's header comment. Fallback whenever StyleInfo is
	// unconnected or has no usable RoofType for a building.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof", meta = (PCG_Overridable))
	EGrammarRoofType RoofType = EGrammarRoofType::Gabled;

	// Fallback whenever BuildingInfo is unconnected or has no usable TotalHeight for a building --
	// see this class's header comment.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof", meta = (PCG_Overridable))
	double EaveHeight = 900.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof", meta = (PCG_Overridable))
	double RidgeHeight = 200.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof", meta = (PCG_Overridable))
	double Overhang = 25.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Edge", meta = (PCG_Overridable))
	bool bEdgeEnabled = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Edge", meta = (PCG_Overridable))
	double EdgeWidth = 28.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Edge", meta = (PCG_Overridable))
	double EdgeHeight = 35.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Edge", meta = (PCG_Overridable))
	double SurfaceInset = 8.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Edge", meta = (PCG_Overridable))
	double CornerCapSize = 42.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Edge", meta = (PCG_Overridable))
	TSoftObjectPtr<UMaterialInterface> EdgeMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Edge", meta = (PCG_Overridable))
	FLinearColor EdgeColor = FLinearColor(0.22, 0.22, 0.2, 1.0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Tiles", meta = (PCG_Overridable, ClampMin = "0"))
	int32 TileRows = 6;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Tiles", meta = (PCG_Overridable))
	double TileDepth = 3.5;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Tiles", meta = (PCG_Overridable))
	double TileSpacing = 55.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Tiles", meta = (PCG_Overridable))
	TSoftObjectPtr<UMaterialInterface> TileMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Tiles", meta = (PCG_Overridable))
	FLinearColor TileColor = FLinearColor(0.28, 0.07, 0.045, 1.0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Roof Windows", meta = (PCG_Overridable, ClampMin = "0"))
	int32 RoofWindowCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Roof Windows", meta = (PCG_Overridable))
	double RoofWindowWidth = 75.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Roof Windows", meta = (PCG_Overridable))
	double RoofWindowHeight = 105.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Roof Windows", meta = (PCG_Overridable))
	TSoftObjectPtr<UMaterialInterface> RoofWindowMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Roof Windows", meta = (PCG_Overridable))
	FLinearColor RoofWindowColor = FLinearColor(0.08, 0.16, 0.2, 0.86);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Dormers", meta = (PCG_Overridable, ClampMin = "0"))
	int32 DormerCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Dormers", meta = (PCG_Overridable))
	double DormerWidth = 135.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Dormers", meta = (PCG_Overridable))
	double DormerDepth = 90.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Dormers", meta = (PCG_Overridable))
	double DormerHeight = 90.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Dormers", meta = (PCG_Overridable))
	TSoftObjectPtr<UMaterialInterface> DormerMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Dormers", meta = (PCG_Overridable))
	FLinearColor DormerColor = FLinearColor(0.62, 0.58, 0.5, 1.0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Chimneys", meta = (PCG_Overridable, ClampMin = "0"))
	int32 ChimneyCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Chimneys", meta = (PCG_Overridable))
	double ChimneyWidth = 45.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Chimneys", meta = (PCG_Overridable))
	double ChimneyDepth = 38.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Chimneys", meta = (PCG_Overridable))
	double ChimneyHeight = 115.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Chimneys", meta = (PCG_Overridable))
	TSoftObjectPtr<UMaterialInterface> ChimneyMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof|Chimneys", meta = (PCG_Overridable))
	FLinearColor ChimneyColor = FLinearColor(0.42, 0.16, 0.1, 1.0);
};

class FPCGRoofDetailLayoutElement : public IPCGElement
{
public:
	// See UPCGExtrudeFootprintToWallsSettings' identical FPCGExtrudeFootprintToWallsElement override
	// -- same StyleInfo-driven FGrammarKitResolver::ResolveMaterial call, same game-thread-only
	// requirement.
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
