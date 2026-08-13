#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGElement.h"

#include "PCGFacadePatternStreetDetailLayout.generated.h"

// Per-building layout PCG node (native reimplementation -- see the module's own header comment):
// places facade texture bands (panel seams, insulation shadow bands, ornament bands + pilasters) on
// every wall edge, and street-level retail/industrial detail (signboards, awnings, garage doors,
// loading docks, stair cores) on whichever edge is street-facing -- port of GrammarWallWindow.cpp-
// adjacent GrammarFacadeDepth.cpp's FacadePatternPlacements and FacadeDepthPlacements.
//
// Takes "Edges" (UPCGExtrudeFootprintToWallsSettings' output, same convention as
// UPCGFacadeWindowDoorLayoutSettings), required "BuildingInfo" (UPCGLoadOsmBuildingVolumesSettings'
// output, for TagsJson -- both the street-facing-tag lookup below and token classification), optional
// "StyleInfo" (for StyleName's own token contribution), and optional "Streets"
// (UPCGGetStreetNetworkSettings' output, real street geometry for the street-facing-detection tiers
// below -- entirely unused if unconnected, falling back to the explicit-tag-only stub).
//
// None of this category's materials are style-config-driven in classic -- every role here is a
// hardcoded material name/color gated purely by OSM/style-name tokens, reimplemented the same
// narrow-substring-blob way UPCGRoofServiceAntennaLayoutSettings' PV/HVAC/plant gating is (see that
// class's header comment for why this isn't exact GrammarEngineInternal::StyleTokens tokenization).
// FloorCount/FloorHeight are resolved per building from BuildingInfo's Levels/TotalHeight (uniform
// FloorHeight = TotalHeight/Levels) when a usable row exists, else fall back to this node's own flat
// FloorCount/FloorHeight Settings -- same mechanism and uniform-floor-height simplification
// UPCGFacadeWindowDoorLayoutSettings already has (no variable per-floor heights yet).
//
// Street-facing detection starts with GrammarEngineInternal::StreetFacingSideIndex's own explicit-tag
// tiers, ported exactly: an OSM "grammar:street:point" (nearest-edge-to-point) or
// "grammar:street_facing_side" (explicit index) tag on BuildingInfo wins if present -- classic's own
// street-facing detection is already this same stub (these tags are never actually written anywhere
// in this codebase today). This node goes further with two more tiers using REAL street geometry from
// the optional "Streets" input pin (UPCGGetStreetNetworkSettings' output, same coordinate space as
// long as both nodes are fed the same OSM file/origin): an addr:street tag match against a named
// street (regardless of StreetSearchRadius, an explicit address being a stronger signal than raw
// proximity -- same reasoning BuildingGrammarCore's FGrammarStreetAlignment::ApplyRidgeDirectionTags
// uses for its own, different, roof-ridge-direction purpose), else the edge nearest any street within
// StreetSearchRadius. Falls back to edge 0 only if none of these four tiers resolve anything (no tags,
// Streets unconnected or empty, or nothing within range).
//
// Outputs one "Placements" UPCGPointData per building, same box-CENTER + (Width,Depth,Height) Scale
// convention and "MaterialOverride" attribute as every other layout node in this module -- "Role" is
// one of "panel_seam", "insulation_band", "facade_ornament", "signboard", "awning", "garage_door",
// "loading_dock", "stair_core".
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGFacadePatternStreetDetailLayoutSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("FacadePatternStreetDetailLayout")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGFacadePatternStreetDetailLayoutSettings", "NodeTitle", "Facade Pattern & Street Detail Layout"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGFacadePatternStreetDetailLayoutSettings", "NodeTooltip", "Places facade texture bands on every edge and street-level retail/industrial detail on the street-facing edge."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	// Should match the paired UPCGFacadeWindowDoorLayoutSettings node's own FloorHeight/FloorCount --
	// see this class's header comment.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Floors", meta = (PCG_Overridable))
	double FloorHeight = 300.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Floors", meta = (PCG_Overridable, ClampMin = "1"))
	int32 FloorCount = 3;

	// Maximum distance (UE centimeters) from a building edge to the nearest real street before that
	// street is ignored for street-facing detection (the optional "Streets" pin) -- mirrors
	// BuildingGrammarCore's own FBuildingGrammarConfig::RoofStreetAlignmentSearchRadius (80m default,
	// used for a different purpose, roof ridge alignment, but the same "don't match a street that's
	// implausibly far away" reasoning). Not consulted for an addr:street name match, which is a
	// stronger signal than raw proximity regardless of distance -- see this class's header comment.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Street", meta = (PCG_Overridable))
	double StreetSearchRadius = 8000.0;
};

class FPCGFacadePatternStreetDetailLayoutElement : public IPCGElement
{
public:
	// See UPCGExtrudeFootprintToWallsSettings' identical FPCGExtrudeFootprintToWallsElement override
	// -- same StyleInfo-driven FGrammarKitResolver::ResolveMaterial call, same game-thread-only
	// requirement.
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
