#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "PCGSettings.h"
#include "PCGElement.h"
#include "Config/GrammarStringList.h"

#include "PCGLoadOsmPointFeatures.generated.h"

// One street-furniture category's tag-match rule -- same OR-across-filters shape as
// FFacadeStyleConfig::TagFilters (BuildingGrammarCore/Public/Config/FacadeStyleConfig.h), reused
// here as a plain data struct (not FFacadeStyleConfig itself, which carries a whole facade/roof/
// window/door config unrelated to point features) so adding a new furniture category is a data
// change to this node's Categories array, not a new C++ node.
USTRUCT(BlueprintType)
struct FStreetFurnitureCategoryConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Category")
	FString Name;

	// OSM tag key -> allowed value list (case-insensitive; "*" matches any non-empty value for that
	// key). A node matches this category if ANY key's allowed-value-list is satisfied (OR across
	// every entry -- same semantics as FGrammarStyleSelection::StyleMatchesTags' TagFilters loop,
	// BuildingGrammarCore/Private/Grammar/GrammarStyleSelection.cpp).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Category")
	TMap<FString, FGrammarStringList> TagFilters;
};

// Data-source PCG node: parses an .osm file's plain <node> elements (see FOsmNode::Tags -- point
// features like street lights/benches/waste bins are almost always mapped as standalone nodes, not
// building or way geometry) and filters them against Categories, a data-driven tag-match table (see
// FStreetFurnitureCategoryConfig above) -- one node handles every furniture category instead of one
// hardcoded C++ node per category, since the category list is open-ended (the constructor
// pre-populates 8 common ones, but Categories is fully user-editable/extendable, matching
// UPCGLoadOsmBuildingVolumesSettings::Config's and UPCGSelectFacadeStyleSettings::Styles' own
// "pre-populated but editable" convention).
//
// Outputs a single "Features" UPCGPointData pin (not one pin per category -- a node whose output pin
// set has to change shape every time a category is added doesn't compose well; use PCG's own stock
// Filter/Split-by-attribute nodes downstream for per-category branches): one point per matched node,
// Transform = position only (identity rotation, unit scale -- actual orientation is a separate,
// composable step, see the planned UPCGOrientToNearestStreetSettings), with "Category" (FString),
// "TagsJson" (FString, every OSM tag on that node as a serialized JSON object -- same convention
// UPCGLoadOsmBuildingVolumesSettings' BuildingInfo pin already uses), "SourceId" (int64, the raw OSM
// node id, for stable identity across regenerations), "bSynthetic" (bool, always false here --
// present so this pin's attribute schema matches UPCGPlaceStreetLightsAlongLitRoadsSettings' own
// output exactly, letting the two be merged with a stock PCG points-union node), and "MeshOverride"
// (FSoftObjectPath, resolved from UStreetFurnitureMeshSettings' project-settings mesh table by
// Category -- left as an invalid/empty path, the same "no override" sentinel
// UPCGFacadeWindowDoorLayoutSettings' MaterialOverride attribute already uses, for a category with no
// meshes configured there) attributes.
//
// Ground alignment is NOT done here -- chain a stock PCG ground-projection node downstream, the same
// compositional-graph philosophy the rest of this pipeline already follows.
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGLoadOsmPointFeaturesSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	// Pre-populates Categories with sensible tag filters for 8 common OSM point-feature types.
	UPCGLoadOsmPointFeaturesSettings();

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("LoadOsmPointFeatures")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGLoadOsmPointFeaturesSettings", "NodeTitle", "Load OSM Point Features"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGLoadOsmPointFeaturesSettings", "NodeTooltip", "Parses an .osm file's point features (street lights, signs, benches, ...) filtered by a data-driven category/tag table, output as one point per matched node."); }
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FStreetFurnitureCategoryConfig> Categories;
};

class FPCGLoadOsmPointFeaturesElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
