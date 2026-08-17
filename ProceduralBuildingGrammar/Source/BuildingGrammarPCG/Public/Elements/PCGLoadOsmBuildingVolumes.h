#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "PCGSettings.h"
#include "PCGElement.h"
#include "Config/BuildingGrammarConfig.h"

#include "PCGLoadOsmBuildingVolumes.generated.h"

// Data-source PCG node: the PCG-pipeline equivalent of BuildingGrammarRuntime's
// UBuildingGenerationLibrary::LoadResolvedVolumesFromOsmFile -- parses an .osm file, projects every
// footprint to local-tangent-plane meters around (OriginLatitude, OriginLongitude), and resolves
// building-part parent/child relationships, reusing that exact function rather than re-implementing
// OSM parsing. This is data ingestion, not generation layout, so reusing the existing engine here is
// intentional (see the module's own header comment).
//
// Outputs two pins, index-aligned by building volume:
//  - "Footprints": one closed UPCGSplineData per building volume (footprint ring, in UE centimeters,
//    same X=local-North/Y=local-East convention as the rest of this plugin -- see
//    FLocalTangentPlaneProjection's header comment), tagged with "SourceName:<value>" for downstream
//    identification/filtering. Every point's Z is Volume.MinHeight (converted to cm), not 0 -- this
//    is the single source of truth this pipeline uses for a building PART's min_height/min_level
//    offset (port of FBuildingGrammarEngine::ApplyMinHeightOffset -- see
//    UPCGExtrudeFootprintToWallsSettings' header comment for how this propagates downstream).
//  - "BuildingInfo": a single UPCGParamData attribute set, one row per building volume (keyed by
//    SourceName), with SourceName/MinHeight/IsBuildingPart/ParentSourceName/HasBuildingParts/
//    Building/AddrStreet columns plus a TagsJson column holding every OSM tag as a serialized JSON
//    object, for anything not covered by the convenience columns. HasBuildingParts is true for a
//    volume that is itself the PARENT of one or more building:part children (the converse of
//    IsBuildingPart) -- consumed by the roof-generating nodes to skip a parent's own roof (see
//    UPCGRoofFrameGeneratorSettings' header comment) now that Config.bSkipParentFootprintsWithParts
//    defaults to false here (see this class's own constructor), so a parent whose parts don't fully
//    tile it still emits a footprint at all, instead of the whole thing being silently dropped. Also
//    Levels/TotalHeight (UE centimeters), computed via
//    FGrammarLevels::InferLevels/FloorHeightSequence (BuildingGrammarCore/Public/Grammar/
//    GrammarLevels.h) -- the SAME function the classic engine calls, reused rather than
//    reimplemented, called with Style=nullptr (matches its own "Style may be null" contract; this
//    node runs before any style is resolved, so only the OSM tags' own building:levels/levels/height
//    values and this node's Config.DefaultLevels/DefaultFloorHeight factor in, not a style's own
//    DefaultLevels/DefaultFloorHeight override -- a per-style contribution would need re-deriving
//    Levels/TotalHeight downstream of UPCGSelectFacadeStyleSettings instead, not implemented here).
//    TotalHeight is FloorHeightSequence's returned per-floor heights summed (and, per that function's
//    own contract, uniformly rescaled to match an explicit OSM height tag if present) -- irregular
//    per-floor heights themselves (Config.IrregularFloorHeights) are computed correctly for this sum
//    but not separately exposed; downstream PCG nodes needing a single per-floor height derive
//    TotalHeight/Levels (uniform), a known simplification (see UPCGFacadeWindowDoorLayoutSettings'
//    own header comment for its pre-existing uniform-floor-height assumption, unchanged by this).
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGLoadOsmBuildingVolumesSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	// Defaults Config to the plugin's bundled Content/german_building_grammar_config.json (see
	// PCGBuildingGrammarDefaults.h) so a newly placed node starts fully configured -- falls back to
	// a plain default-constructed FBuildingGrammarConfig if that file can't be found/parsed.
	UPCGLoadOsmBuildingVolumesSettings();

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("LoadOsmBuildingVolumes")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGLoadOsmBuildingVolumesSettings", "NodeTitle", "Load OSM Building Volumes"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGLoadOsmBuildingVolumesSettings", "NodeTooltip", "Parses an .osm file into building footprint volumes (building-part parent/child resolved), output as spline geometry plus a parallel attribute set of OSM tags/metadata."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return TArray<FPCGPinProperties>(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FFilePath OsmFilePath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double OriginLatitude = 0.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double OriginLongitude = 0.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ShowOnlyInnerProperties))
	FBuildingGrammarConfig Config;
};

class FPCGLoadOsmBuildingVolumesElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
