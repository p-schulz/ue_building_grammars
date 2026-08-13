#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGElement.h"
#include "Config/FacadeStyleConfig.h"

#include "PCGSelectFacadeStyle.generated.h"

// Per-building layout PCG node: resolves one facade style per building from its OSM tags (the
// UPCGLoadOsmBuildingVolumesSettings "BuildingInfo" TagsJson column) against a configurable style
// set, reusing FGrammarStyleSelection::SelectableStylesForTags
// (BuildingGrammarCore/Public/Grammar/GrammarStyleSelection.h) -- tag matching is data-driven rule
// evaluation, not geometry, so reusing the existing rule evaluator here (rather than hand-building it
// from generic Branch/Filter nodes) is a deliberate, documented exception to this pipeline's "native
// layout nodes" design -- see the module's own header comment. Phase A picks a single highest-
// priority match per building (no per-side style rotation yet -- see this plugin's plan notes).
//
// Outputs one "StyleInfo" UPCGParamData attribute set, one row per building (same SourceName-keyed
// rows as the input), with the style parameters the rest of this pipeline's nodes need: StyleName,
// WallMaterial, WallColor, RoofType/RoofMaterial/RoofColor (the style's RoofOverride if
// bOverrideRoof, else DefaultRoof below -- mirrors BuildingGrammarEngine.cpp's own
// RoofStyleOverride-else-PrimaryStyle selection; RoofType is the enum-name string, e.g. "Gabled",
// consumed by UPCGRoofFrameGeneratorSettings/UPCGRoofDetailLayoutSettings/
// UPCGRoofServiceAntennaLayoutSettings so a building's actual roof SHAPE -- not just its material --
// follows its resolved style instead of always being whatever the node's own flat Settings say),
// WindowWidth/Height/Spacing/Margin/SillHeight/Depth/Material/Color,
// WindowFrameWidth/Depth/Material/Color, WindowVerticalMullions/HorizontalMullions,
// WindowSillDepth/Thickness/Material/Color, DoorEnabled/Width/Height/Depth/Material/Color -- see
// FWindowStyleConfig/FDoorStyleConfig/FRoofStyleConfig for where each value comes from. Also:
// LedgeEnabled/Depth/Height/EveryNFloors/Material/Color (FLedgeStyleConfig), BalconyEnabled/Width/
// Depth/SlabHeight/RailingHeight/EveryNFloors/Material/Color/RailingMaterial/RailingColor/
// RailingBarCount/RailingBarWidth/RailingBarDepth (FBalconyStyleConfig), ShutterEnabled (a
// reimplementation of GrammarEngineInternal::StyleHasShutters' style-Name keyword check --
// BuildingGrammarCore's own function is private/non-exported, see the .cpp), roof-detail
// EdgeMaterial/EdgeColor/TileMaterial/TileColor/RoofWindowMaterial/RoofWindowColor/DormerMaterial/
// DormerColor/ChimneyMaterial/ChimneyColor (FRoofStyleConfig, same RoofOverride-else-DefaultRoof
// resolution as RoofMaterial/RoofColor above), and Antenna* (FAntennaStyleConfig: Enabled/Type/
// Count/MastHeight/MastRadius/BaseWidth/BaseDepth/BaseHeight/PanelWidth/PanelHeight/PanelDepth/
// Material/Color/AccentMaterial/AccentColor -- Type stored as its enum-name string, e.g. "Satellite").
// Also: WallColorVariants/WallColorVariantMode and WallRowColors/WallRowColorMode (FFacadeStyleConfig
// -- color ARRAYS, PCG metadata has no array attribute type, so each is serialized as a semicolon-
// delimited "r,g,b,a;r,g,b,a;..." string; Mode is the enum-name string, same convention as RoofType/
// AntennaType). Full resolution (which variant/row color a given building/edge/floor actually gets)
// happens in UPCGExtrudeFootprintToWallsSettings, not here -- see that node's own header comment for
// why (it needs a per-edge SideIndex this per-building StyleInfo row doesn't have). Also:
// DoorPlacement (FDoorStyleConfig::Placement, enum-name string, consumed by
// UPCGFacadeWindowDoorLayoutSettings) and RidgeAlignment (FRoofStyleConfig::RidgeAlignment --
// EffectiveRoof's, same bOverrideRoof-else-DefaultRoof resolution as RoofType above -- enum-name
// string, consumed by UPCGRoofFrameGeneratorSettings).
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGSelectFacadeStyleSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	// Defaults Styles (and DefaultRoof) to the plugin's bundled
	// Content/german_building_grammar_config.json's style set/root roof config (see
	// PCGBuildingGrammarDefaults.h) so a newly placed node starts fully configured -- falls back to
	// an empty Styles array (matching FGrammarStyleSelection's own "no styles" handling) and a
	// plain default-constructed FRoofStyleConfig if that file can't be found/parsed.
	UPCGSelectFacadeStyleSettings();

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SelectFacadeStyle")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGSelectFacadeStyleSettings", "NodeTitle", "Select Facade Style"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGSelectFacadeStyleSettings", "NodeTooltip", "Resolves one facade style per building from its OSM tags against a configurable style set."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FFacadeStyleConfig> Styles;

	// Root-level roof config (FBuildingGrammarConfig::Roof), used for any building whose selected
	// style doesn't set bOverrideRoof -- matches BuildingGrammarEngine.cpp's own
	// RoofStyleOverride-else-PrimaryConfig.Roof fallback.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FRoofStyleConfig DefaultRoof;
};

class FPCGSelectFacadeStyleElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
