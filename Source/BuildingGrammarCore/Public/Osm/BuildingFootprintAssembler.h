#pragma once

#include "CoreMinimal.h"
#include "Osm/OsmTypes.h"
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
};
