#pragma once

#include "CoreMinimal.h"
#include "Osm/BuildingPartResolver.h"
#include "Osm/StreetNetworkAssembler.h"
#include "Config/BuildingGrammarConfig.h"

// Injects a synthetic grammar:roof:ridge_direction tag (the same "X,Y" format
// GrammarRoofDirection::RidgeDirectionFromTags already parses) onto each volume whose nearest
// matching street lies within a search radius, so gabled/hipped roofs configured with
// EGrammarRidgeAlignment::ClosestStreet run their ridge parallel to that street instead of falling
// back to the footprint's own longest edge. Mirrors FBuildingPartResolver's own "resolve geometry
// once, encode the result as a synthetic tag" pattern (grammar:is_building_part,
// grammar:building_parent) -- no changes to the grammar engine's existing tag-driven ridge
// resolution are needed, and the tag survives as part of ABuildingInstancePoolActor::SourceVolumes
// so post-import regeneration (RegenerateFromSource) reproduces the same alignment.
class BUILDINGGRAMMARCORE_API FGrammarStreetAlignment
{
public:
	// True if street alignment could actually affect generation with this config -- the default
	// Roof.RidgeAlignment, or any style's RoofOverride, is EGrammarRidgeAlignment::ClosestStreet.
	// RidgeAlignment is config-only (never OSM-tag-overridden -- see GrammarEngineInternal::
	// RoofFromTags), so this check is exhaustive: callers can skip extracting/matching streets
	// entirely when this returns false.
	static bool ConfigNeedsStreetAlignment(const FBuildingGrammarConfig& Config);

	// ProjectedStreets must already be in the same local-meter space as Volumes' footprints (see
	// FLocalTangentPlaneProjection). For each volume, in priority order:
	//   1. Skip entirely if it already carries an explicit roof:orientation/roof:direction/
	//      grammar:roof:ridge_direction/roof:ridge:direction tag -- never overwritten.
	//   2. If VolumeTags["addr:street"] matches (trimmed, case-insensitive) a street's `name`, use
	//      the nearest line segment among just the matching streets, regardless of SearchRadius (an
	//      explicit address is a stronger signal than raw proximity).
	//   3. Otherwise use the nearest line segment among all streets, only if within SearchRadius.
	// The injected direction is that nearest segment's own tangent, not the whole way's endpoint-to-
	// endpoint direction, so a curving street still gives a locally-correct result.
	static void ApplyRidgeDirectionTags(TArray<FGrammarBuildingVolume>& Volumes, const TArray<FGrammarStreetSegment>& ProjectedStreets, double SearchRadius);
};
