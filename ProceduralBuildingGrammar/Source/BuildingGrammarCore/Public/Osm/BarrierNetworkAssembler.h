#pragma once

#include "CoreMinimal.h"
#include "Osm/OsmTypes.h"

// One OSM barrier=* way, kept as a raw (Lon, Lat) polyline exactly as read -- not yet projected to
// local meters (same convention/reasoning as FGrammarStreetSegment; project via the same
// FLocalTangentPlaneProjection used for building footprints/streets so coordinates line up). Plain
// struct, not USTRUCT/reflected -- an internal parse intermediate, same as FOsmWay/FOsmNode in
// OsmTypes.h.
struct BUILDINGGRAMMARCORE_API FGrammarBarrierSegment
{
	TArray<FVector2D> Points; // (Lon, Lat) degrees, way order; always >= 2 points
	FString Type;             // raw `barrier` tag value (e.g. "wall", "fence", "guard_rail", "hedge")
	TOptional<double> Height; // meters, from the way's own `height` tag if present and parseable
	FString Material;         // raw `material` tag value; empty if the way doesn't have one
};

// Extracts OSM barrier=* ways as linear-feature polylines for wall/fence/guardrail generation (see
// PCGExtrudeBarrierWay.h). Not general-purpose barrier extraction -- only the linear barrier types
// this pipeline actually extrudes as a continuous mesh strip; point-scale barrier features (gates,
// bollards, ...) belong to UPCGLoadOsmPointFeaturesSettings instead.
class BUILDINGGRAMMARCORE_API FBarrierNetworkAssembler
{
public:
	// One entry per barrier=wall|fence|guard_rail|hedge way (inclusion list, unlike
	// FStreetNetworkAssembler's exclusion list -- there are far more non-linear barrier=* values than
	// linear ones) with at least two resolvable nodes.
	static TArray<FGrammarBarrierSegment> Assemble(const FOsmDocument& Document);
};
