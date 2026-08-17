#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGElement.h"

#include "PCGFacadeWindowDoorLayout.generated.h"

// Per-building layout PCG node (native reimplementation -- see the module's own header comment):
// takes per-edge points (UPCGExtrudeFootprintToWallsSettings' "Edges" output: one UPCGPointData per
// building, one point per wall edge, Transform's local +X = edge tangent/+Y = outward normal/+Z =
// up, "Length"/"EdgeIndex" attributes) and lays out evenly spaced window placements along every
// edge, plus a door wherever FDoorStyleConfig::Placement (StreetFacing/EachFacade/None -- see
// StyleInfo's own DoorPlacement attribute below) says one belongs -- port of
// GrammarEngineInternal::DoorApplies/WindowOverlapsDoor (GrammarEngineInternal.cpp:434-455): a
// StreetFacing door only goes on the real street-facing edge (DetermineStreetFacingSideIndex,
// PCGBuildingGrammarDefaults.h -- shared with UPCGFacadePatternStreetDetailLayoutSettings' own
// street-facing detection), an EachFacade door goes on every qualifying edge, and any floor-0 window
// that would overlap a placed door on its own edge is skipped (clearance =
// (Window.Width+Door.Width)/2 + max(Door.FrameWidth,0)). The `grammar:disable_ground_entrance` OSM
// tag (read from the optional "BuildingInfo" pin's TagsJson) hard-disables a door regardless of
// Placement, same as classic.
//
// Consumes every point-data entry on its Edges pin independently, preserving tags, same as
// UPCGExtrudeFootprintToWallsSettings.
//
// Optional "StyleInfo" input pin (UPCGSelectFacadeStyleSettings' output): PCG's generic per-property
// "Overrides" pin mechanism (UPCGSettings::PCG_Overridable) only ever applies ONE value per node
// EXECUTION, not per item being processed within it -- since this node lays out every building's
// windows/doors in a single ExecuteInternal call, that mechanism cannot give different buildings
// different window sizes/colors no matter how it's wired. StyleInfo sidesteps this: if connected,
// each building's own row (matched by its "SourceName:..." tag, the same tag
// UPCGLoadOsmBuildingVolumesSettings/UPCGExtrudeFootprintToWallsSettings already propagate) is read
// directly per building, overriding this node's own WindowWidth/Height/Spacing/Margin/SillHeight/
// Depth/DoorEnabled/Width/Height/Depth properties (which remain the fallback whenever StyleInfo is
// unconnected, or a building's row isn't found in it).
//
// Outputs one "Placements" UPCGPointData per building. Each point's Transform is a box CENTER +
// (Width, Depth, Height) Scale, oriented the same way as the source edge -- i.e. directly compatible
// with a centered unit-box mesh scaled per-axis (the same convention
// BuildingGrammarGeometry's shared kit box mesh already uses), so a stock Static Mesh Spawner can
// consume these points directly. "Role" (FString) is one of "window", "window_frame",
// "window_mullion", "window_sill", "door", "shutter", "ledge", "balcony", "balcony_rail",
// "balcony_bar" -- mirrors BuildingGrammarCore's own window/ledge/balcony kit
// (GrammarWallWindow.cpp's WindowDetailPlacements, GrammarLedgeBalcony.cpp) so PCG-generated
// buildings get the same frame/mullion/sill/shutter/ledge/balcony+railing assembly the classic
// engine does, not just a bare opening box. Ledge/Balcony/Shutter are entirely StyleInfo-driven (no
// Settings-level fallback -- see FResolvedStyle's own comment in the .cpp): they're left disabled
// whenever StyleInfo is unconnected or has no row for a building. Every role still shares one plain
// box mesh (no distinct shapes needed, matching BuildingGrammarGeometry's kit), so a single Static
// Mesh Spawner can consume every role directly.
//
// Each point also carries a "MaterialOverride" FSoftObjectPath attribute: when StyleInfo is
// connected and has a row for this building, it points at the same persistent per-style
// UMaterialInstanceConstant FGrammarKitResolver::ResolveMaterial(StyleName, Role, MaterialName,
// Color) resolves for the classic engine -- so window glass/frame/sill/door actually render with
// their style's own material and color instead of whatever single material a Static Mesh Spawner
// happens to be configured with. Wire the Static Mesh Spawner's Mesh Selector to "By Attribute
// Override" with "MaterialOverride" in its material-override-attribute list to use this. Left as an
// invalid (empty) path -- no override -- for any placement StyleInfo couldn't resolve a material
// for (StyleInfo unconnected, or no row for this building), so the node still works, just without
// per-style material variation, same as the dimension fallbacks below.
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGFacadeWindowDoorLayoutSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("FacadeWindowDoorLayout")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGFacadeWindowDoorLayoutSettings", "NodeTitle", "Facade Window/Door Layout"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGFacadeWindowDoorLayoutSettings", "NodeTooltip", "Lays out evenly spaced window placements along each wall edge, plus a door on the longest edge."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	// All dimensions in Unreal centimeters -- this pipeline works in UE world units throughout.
	// Fallback values, used whenever StyleInfo is unconnected or doesn't have a row for a given
	// building -- see this class's header comment for why per-building variation needs the
	// StyleInfo pin rather than PCG's generic per-node "Overrides" mechanism. FloorCount/FloorHeight
	// specifically are ALSO overridden per building by the optional "BuildingInfo" input pin
	// (UPCGLoadOsmBuildingVolumesSettings' output) if connected: FloorCount comes from its Levels
	// column, FloorHeight from TotalHeight/Levels (uniform -- see that node's own header comment for
	// why irregular per-floor heights aren't separately exposed). BuildingInfo takes priority over
	// these Settings when both are available; these remain the fallback whenever BuildingInfo is
	// unconnected or has no usable row.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Floors", meta = (PCG_Overridable))
	double FloorHeight = 300.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Floors", meta = (PCG_Overridable, ClampMin = "1"))
	int32 FloorCount = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Window", meta = (PCG_Overridable))
	double WindowWidth = 120.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Window", meta = (PCG_Overridable))
	double WindowHeight = 150.0;

	// Center-to-center spacing between windows along an edge.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Window", meta = (PCG_Overridable))
	double WindowSpacing = 200.0;

	// Minimum clearance kept from each end of an edge.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Window", meta = (PCG_Overridable))
	double WindowMargin = 40.0;

	// Height of a window's bottom edge above the floor.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Window", meta = (PCG_Overridable))
	double WindowSillHeight = 90.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Window", meta = (PCG_Overridable))
	double WindowDepth = 10.0;

	// Frame/mullion/sill fallback dimensions -- used (alongside the Window properties above)
	// whenever StyleInfo is unconnected or a building's row isn't found in it. Defaults mirror
	// FWindowStyleConfig's own defaults (converted to centimeters).
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Window|Frame", meta = (PCG_Overridable))
	double WindowFrameWidth = 8.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Window|Frame", meta = (PCG_Overridable))
	double WindowFrameDepth = 3.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Window|Frame", meta = (PCG_Overridable, ClampMin = "0"))
	int32 WindowVerticalMullions = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Window|Frame", meta = (PCG_Overridable, ClampMin = "0"))
	int32 WindowHorizontalMullions = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Window|Sill", meta = (PCG_Overridable))
	double WindowSillDepth = 16.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Window|Sill", meta = (PCG_Overridable))
	double WindowSillThickness = 6.0;

	// Fallback whenever StyleInfo is unconnected or has no row for a building (no Settings-level way
	// to express EachFacade -- see this class's header comment for why that's an acceptable
	// limitation, same as Ledge/Balcony/Shutter having no Settings fallback either): true resolves to
	// DoorPlacement=StreetFacing (a door on the real street-facing edge only), false to
	// DoorPlacement=None.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Door", meta = (PCG_Overridable))
	bool bAddDoorOnLongestEdge = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Door", meta = (PCG_Overridable, EditCondition = "bAddDoorOnLongestEdge"))
	double DoorWidth = 100.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Door", meta = (PCG_Overridable, EditCondition = "bAddDoorOnLongestEdge"))
	double DoorHeight = 210.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Door", meta = (PCG_Overridable, EditCondition = "bAddDoorOnLongestEdge"))
	double DoorDepth = 10.0;

	// Maximum distance (UE centimeters) from a building edge to the nearest real street before that
	// street is ignored for street-facing detection -- see UPCGFacadePatternStreetDetailLayoutSettings'
	// identical property for the full explanation; only relevant when the optional "Streets" pin is
	// connected.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Door", meta = (PCG_Overridable))
	double StreetSearchRadius = 8000.0;
};

class FPCGFacadeWindowDoorLayoutElement : public IPCGElement
{
public:
	// See UPCGExtrudeFootprintToWallsSettings' identical FPCGExtrudeFootprintToWallsElement override
	// -- same StyleInfo-driven FGrammarKitResolver::ResolveMaterial call (this node resolves
	// window/frame/sill/door materials), same game-thread-only requirement. Not observed to crash in
	// practice yet, but relies on the exact same non-thread-safe asset-creation path, so it needs the
	// same guard rather than waiting for a scheduling-dependent crash to prove it.
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
