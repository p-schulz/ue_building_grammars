#pragma once

#include "CoreMinimal.h"
#include "Osm/BuildingGrammarOsmTypes.h"
#include "Footprint/BuildingFootprint.h"

// Turns a raw FOsmDocument into building footprints: one per building=*/building:part=* way not
// consumed by a relation, plus one per outer ring of each type=multipolygon relation carrying a
// building tag (inner-role members become holes, assigned to the smallest-area containing outer
// ring). Reimplemented independently for this plugin -- not derived from the sibling
// /Users/schulz/osm_building C++ prototype, though it follows the same well-established OSM
// multipolygon convention that prototype's README documents (outer/empty role ways stitched by
// shared endpoint node id; ways consumed by a relation are not re-emitted standalone).
class BUILDINGGRAMMARCORE_API FBuildingFootprintAssembler
{
public:
	static TArray<FBuildingFootprint> Assemble(const FOsmDocument& Document);

	// Center of the bounding box of every point across every Footprints' OuterRing (raw (Lon, Lat)
	// degrees, not yet projected -- see FBuildingFootprint's header comment) -- the projection origin
	// callers actually want (FLocalTangentPlaneProjection), since it's centered on the buildings
	// that will actually be generated. Deliberately NOT the same as a bounds-center over every node
	// in the source FOsmDocument: an extract's road/path/boundary data commonly covers a
	// larger and differently-shaped area than its building footprints (e.g. a district's admin
	// boundary or connecting roads extending well past where the buildings themselves cluster), so
	// that broader bounds center can end up hundreds of meters to kilometers away from where the
	// buildings actually are. False if Footprints is empty.
	static bool ComputeFootprintBoundsCenter(const TArray<FBuildingFootprint>& Footprints, double& OutCenterLatitude, double& OutCenterLongitude);
};
