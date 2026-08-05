#pragma once

#include "CoreMinimal.h"

// Converts raw OSM (Longitude, Latitude) degrees into local-tangent-plane METERS via a simple
// equirectangular projection around a configurable origin. Appropriate for single
// city-block/district-scale extracts (the same scope the sibling osm_building prototype's README
// documents for its own equirectangular projection) -- not a substitute for a proper map
// projection library across country-scale extents.
//
// Output stays in meters (not UE centimeters) deliberately: every FBuildingGrammarConfig
// dimension (window/door/ledge/balcony/roof/antenna sizes, floor heights, epsilon thresholds) is a
// literal meter-denominated value ported straight from the Blender add-on's config.py/presets.py,
// and the grammar engine (BuildingGrammarCore/Grammar/*) combines those values directly with
// footprint coordinates throughout (wall heights, edge-length comparisons, tolerance checks). If
// this projection converted to centimeters here, footprint coordinates and every config-derived
// dimension would be in different units the moment they're combined -- exactly the "meter vs.
// centimeter" bug the whole engine must avoid. Keeping FBuildingGrammarEngine's output
// (FGrammarMeshSpec/FGrammarPlacementRecord) entirely in meters, matching the Python source
// 1:1, means the single meters->centimeters conversion only has to happen once, at the point
// Core's output is actually turned into Unreal geometry -- see FGrammarDynamicMeshBuilder::
// BuildDynamicMesh and ABuildingActor::ApplyBuildingSpec.
//
// Axis convention (arbitrary but fixed -- there is no single "correct" mapping from geographic
// East/North to UE's X-forward/Y-right/Z-up frame, so pick one and be consistent): output X is
// local North distance, output Y is local East distance, both in meters. Height/Z is never
// produced here -- footprints are 2D; the grammar engine adds Z separately per floor/roof.
//
// UE5.6's native double-precision Large World Coordinates means a single origin (rather than the
// sibling prototype's manual per-chunk origin rebasing) is sufficient even for fairly large
// imported extents.
class BUILDINGGRAMMARCORE_API FLocalTangentPlaneProjection
{
public:
	FLocalTangentPlaneProjection(double OriginLatitudeDegrees, double OriginLongitudeDegrees);

	FVector2D ProjectToLocalMeters(const FVector2D& LonLatDegrees) const;

	TArray<FVector2D> ProjectRing(const TArray<FVector2D>& LonLatRing) const;

private:
	double OriginLatRadians = 0.0;
	double OriginLonRadians = 0.0;
	double CosOriginLat = 1.0;

	// WGS84 equatorial radius in meters -- adequate for a local equirectangular approximation at
	// city-block/district scale.
	static constexpr double EarthRadiusMeters = 6378137.0;
};
