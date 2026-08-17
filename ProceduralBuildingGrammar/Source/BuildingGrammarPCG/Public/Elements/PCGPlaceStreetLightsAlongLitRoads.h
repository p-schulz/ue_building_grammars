#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGElement.h"

#include "PCGPlaceStreetLightsAlongLitRoads.generated.h"

// Layout PCG node: synthesizes evenly-spaced streetlight placements along every `lit=yes` OSM
// highway way, since real OSM data very rarely maps individual highway=street_lamp nodes -- see
// UPCGLoadOsmPointFeaturesSettings' header comment, which handles those rare explicit nodes; this
// node covers the much more common case where a road is simply tagged as lit.
//
// Takes the "Streets" pin (UPCGSplineData, e.g. UPCGGetStreetNetworkSettings' own output) as input,
// reading each entry's "Lit:true"/"Lit:false" tag (the same prefix-tag convention as "Name:<value>",
// which UPCGGetStreetNetworkSettings already adds alongside its own "StreetInfo" Lit column) -- ways
// tagged unlit are skipped entirely, no geometry emitted for them.
//
// For each lit way, walks its own polyline via FGrammarGeometry2D::PointsAlongPolyline at
// LightSpacing intervals, offsets each sample SideOffset to the side of the road centerline
// (optionally alternating sides -- see bAlternateSides) via the same PointOnSegment pattern used
// throughout this pipeline, and orients each point along the sampled tangent
// (FRotationMatrix::MakeFromXZ(Forward, FVector::UpVector) -- explicit up-vector, not MakeFromXY,
// matching UPCGExtrudeFootprintToWallsSettings' own documented sign gotcha for upright objects).
//
// Outputs a single "Lights" UPCGPointData pin, the same attribute SHAPE as
// UPCGLoadOsmPointFeaturesSettings' "Features" pin ("Category"=FString, "TagsJson"=FString (always
// empty here -- these points aren't derived from any single OSM node), "SourceId"=int64 (always -1
// here, no corresponding OSM node id), "MeshOverride"=FSoftObjectPath (resolved from
// UStreetFurnitureMeshSettings' "StreetLight" category entry, same "no override" sentinel convention
// as that node's own MeshOverride)) so the two pins can be merged with a stock PCG points-union node
// and treated uniformly downstream, plus "bSynthetic"=bool (always true here) so a consumer that DOES
// care about the distinction (e.g. to avoid a synthetic light landing right on top of an
// explicitly-mapped one) can still tell them apart. Ground alignment is left to a downstream stock
// PCG projection node, same as UPCGLoadOsmPointFeaturesSettings.
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGPlaceStreetLightsAlongLitRoadsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PlaceStreetLightsAlongLitRoads")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGPlaceStreetLightsAlongLitRoadsSettings", "NodeTitle", "Place Street Lights Along Lit Roads"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGPlaceStreetLightsAlongLitRoadsSettings", "NodeTooltip", "Synthesizes evenly-spaced streetlight placements along every lit=yes OSM road, since individual streetlight nodes are rarely mapped in real data."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	// Unreal centimeters -- this pipeline works in UE world units throughout downstream of the OSM
	// parsing nodes. A realistic residential streetlight spacing is roughly 25-35m.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin = "10.0"))
	double LightSpacing = 3000.0;

	// How far to the side of the road centerline each light sits (cm) -- positive = the sampled
	// tangent's local +Y (left-hand perpendicular) side.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double SideOffset = 250.0;

	// Alternates SideOffset's sign every consecutive sample along the SAME road, instead of every
	// light sitting on the same side -- a common real streetlight layout.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bAlternateSides = false;
};

class FPCGPlaceStreetLightsAlongLitRoadsElement : public IPCGElement
{
public:
	// Consults UStreetFurnitureMeshSettings, which synchronously loads (LoadSynchronous) a
	// TSoftObjectPtr<UStaticMesh> -- same game-thread requirement as every other node in this module
	// that resolves an asset (e.g. UPCGFacadeWindowDoorLayoutSettings' material resolution).
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
