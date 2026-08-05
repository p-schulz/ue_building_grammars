#pragma once

#include "CoreMinimal.h"

// Converts raw OSM (Longitude, Latitude) degrees into UE-space centimeters via a simple
// equirectangular local tangent-plane projection around a configurable origin. Appropriate for
// single city-block/district-scale extracts (the same scope the sibling osm_building prototype's
// README documents for its own equirectangular projection) -- not a substitute for a proper
// map projection library across country-scale extents.
//
// Axis convention (arbitrary but fixed -- there is no single "correct" mapping from geographic
// East/North to UE's X-forward/Y-right/Z-up frame, so pick one and be consistent): output X is
// local North distance, output Y is local East distance, both in UE centimeters. Height/Z is
// never produced here -- footprints are 2D; the grammar engine adds Z separately per floor/roof.
//
// UE5.6's native double-precision Large World Coordinates means a single origin (rather than the
// sibling prototype's manual per-chunk origin rebasing) is sufficient even for fairly large
// imported extents.
class BUILDINGGRAMMARCORE_API FLocalTangentPlaneProjection
{
public:
	FLocalTangentPlaneProjection(double OriginLatitudeDegrees, double OriginLongitudeDegrees);

	FVector2D ProjectToUnrealCentimeters(const FVector2D& LonLatDegrees) const;

	TArray<FVector2D> ProjectRing(const TArray<FVector2D>& LonLatRing) const;

private:
	double OriginLatRadians = 0.0;
	double OriginLonRadians = 0.0;
	double CosOriginLat = 1.0;

	// WGS84 equatorial radius in meters -- adequate for a local equirectangular approximation at
	// city-block/district scale.
	static constexpr double EarthRadiusMeters = 6378137.0;
};
