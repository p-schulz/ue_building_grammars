#pragma once

#include "CoreMinimal.h"
#include "Osm/OsmTypes.h"

// One OSM highway=* way, kept as a raw (Lon, Lat) polyline exactly as read -- not yet projected to
// local meters (same convention/reasoning as FBuildingFootprint; project via the same
// FLocalTangentPlaneProjection used for building footprints so coordinates line up). Plain struct,
// not USTRUCT/reflected -- an internal parse intermediate discarded once FGrammarStreetAlignment has
// consumed it, same as FOsmWay/FOsmNode in OsmTypes.h.
struct BUILDINGGRAMMARCORE_API FGrammarStreetSegment
{
	TArray<FVector2D> Points; // (Lon, Lat) degrees, way order; always >= 2 points
	FString Name;             // trimmed `name` tag value; empty if the way is unnamed

	// True if this way's own `lit` tag is "yes" -- OSM data very rarely maps individual
	// highway=street_lamp nodes, so this is the practical signal for "this road has streetlights"
	// used to synthesize placements along it (see UPCGPlaceStreetLightsAlongLitRoadsSettings).
	bool bLit = false;
};

// Extracts OSM highway=* ways as street polylines for roof-ridge street alignment (see
// Osm/StreetRidgeAlignment.h) and PCG's street-network/street-lighting nodes. Not general-purpose
// road network extraction -- only what those consumers need: a way's own node polyline, its `name`
// tag, and its `lit` tag.
class BUILDINGGRAMMARCORE_API FStreetNetworkAssembler
{
public:
	// One entry per highway=* way, skipping a small set of non-addressable highway values
	// (footway/cycleway/path/steps/bridleway/corridor/platform/proposed/construction -- not the kind
	// of "street" a building actually faces or is addressed against) and any way with fewer than two
	// resolvable nodes.
	static TArray<FGrammarStreetSegment> Assemble(const FOsmDocument& Document);
};
