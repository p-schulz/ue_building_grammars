#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGElement.h"
#include "Config/RoofStyleConfig.h"

class UMaterialInterface;

#include "PCGRoofServiceAntennaLayout.generated.h"

// Per-building layout PCG node (native reimplementation -- see the module's own header comment):
// places roof clutter over a closed footprint spline -- antennas (all 8 EGrammarAntennaType kinds)
// plus flat-roof-only PV panels/HVAC units/plant screens -- port of GrammarRoofDetails.cpp's
// AntennaPlacements/AntennaInstancePlacements and RoofServicePlacements.
//
// Antennas are genuinely per-style (FAntennaStyleConfig), so they need the "StyleInfo" input pin
// (UPCGSelectFacadeStyleSettings' output) connected AND that building's row having AntennaEnabled=
// true, Count>0, MastHeight>0 -- there is no flat-Settings fallback for antennas (unlike
// UPCGRoofDetailLayoutSettings' roles), since antenna Type/Count/dimensions genuinely vary by style
// in classic and a single flat default wouldn't be representative. Classic's separate stable-hash
// "DetailStyle" selection (independent from the main facade style) is NOT ported -- this node reuses
// whatever style UPCGSelectFacadeStyleSettings already resolved for the building, a documented
// simplification (variety mechanism, not needed for functional parity).
//
// PV/HVAC/plant have no dedicated config struct in classic -- hardcoded geometry/material/color,
// gated purely by OSM/style-name tokens (office/industrial/warehouse/retail/supermarket/modern/
// passivhaus/parking, each clutter kind with its own subset -- see the .cpp). This node reimplements
// just the narrow token check it needs: StyleName plus a handful of raw OSM tag values (building,
// building:use, shop, office, industrial, landuse, amenity, parking) from the "BuildingInfo" pin's
// TagsJson, concatenated and substring-matched -- NOT the exact per-token tokenization
// GrammarEngineInternal::StyleTokens does (private to BuildingGrammarCore, not exported), so a
// keyword embedded inside a longer tag value could false-positive-match here where classic's
// whitespace-split tokenization wouldn't. A documented approximation, not expected to matter in
// practice for these particular keywords.
//
// RoofType/EaveHeight/Overhang/bEdgeEnabled/EdgeHeight/SurfaceInset should match whatever
// UPCGRoofFrameGeneratorSettings/UPCGRoofDetailLayoutSettings nodes built this building's actual
// roof, same reasoning as UPCGRoofDetailLayoutSettings' own header comment. RoofType is also
// resolved per building from the "StyleInfo" pin (if connected) and EaveHeight from the
// "BuildingInfo" pin's TotalHeight (if connected and positive) -- same mechanism and fallback
// behavior as UPCGRoofFrameGeneratorSettings/UPCGRoofDetailLayoutSettings; RoofType/EaveHeight
// Settings above remain the fallback whenever the corresponding pin is unconnected or has no usable
// value for a building.
//
// Outputs one "Placements" UPCGPointData per building, same box-CENTER + (Width,Depth,Height) Scale
// convention and "MaterialOverride" attribute as every other layout node in this module -- "Role" is
// one of "antenna", "antenna_panel", "roof_lamp", "pv_panel", "hvac_unit", "roof_plant".
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGRoofServiceAntennaLayoutSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("RoofServiceAntennaLayout")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGRoofServiceAntennaLayoutSettings", "NodeTitle", "Roof Service & Antenna Layout"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGRoofServiceAntennaLayoutSettings", "NodeTooltip", "Places antennas and flat-roof PV/HVAC/plant clutter over a closed footprint spline."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	// Should match the paired UPCGRoofFrameGeneratorSettings/UPCGRoofDetailLayoutSettings nodes -- see
	// this class's header comment.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof", meta = (PCG_Overridable))
	EGrammarRoofType RoofType = EGrammarRoofType::Gabled;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof", meta = (PCG_Overridable))
	double EaveHeight = 900.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof", meta = (PCG_Overridable))
	double Overhang = 25.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof", meta = (PCG_Overridable))
	bool bEdgeEnabled = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof", meta = (PCG_Overridable))
	double EdgeHeight = 35.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roof", meta = (PCG_Overridable))
	double SurfaceInset = 8.0;
};

class FPCGRoofServiceAntennaLayoutElement : public IPCGElement
{
public:
	// See UPCGExtrudeFootprintToWallsSettings' identical FPCGExtrudeFootprintToWallsElement override
	// -- same StyleInfo-driven FGrammarKitResolver::ResolveMaterial call, same game-thread-only
	// requirement.
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
