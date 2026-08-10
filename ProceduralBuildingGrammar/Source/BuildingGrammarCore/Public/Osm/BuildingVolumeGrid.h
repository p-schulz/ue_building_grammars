#pragma once

#include "CoreMinimal.h"
#include "Osm/BuildingPartResolver.h"

// Square-grid spatial bucketing for FGrammarBuildingVolume, shared by UBuildingStreamingSubsystem's
// proximity streaming and UBuildingGenerationLibrary's one-shot chunked bake -- both need to group
// resolved volumes (post FBuildingPartResolver::Resolve, so building-part parent/child matching has
// already happened on the whole unsplit dataset) by footprint centroid into CellSizeCm-sized square
// cells, in UE-centimeter world space. Deliberately trivial floor-div bucketing (no quad-tree, no
// variable cell size) -- matches UBuildingStreamingSubsystem's original approach, pulled out here so
// every caller shares one tested implementation instead of drifting copies.
class BUILDINGGRAMMARCORE_API FBuildingVolumeGrid
{
public:
	static FIntPoint WorldLocationToCellCoord(const FVector& WorldLocationCm, double CellSizeCm);
	static FVector CellCenter(const FIntPoint& CellCoord, double CellSizeCm);

	// Buckets each Volume by its footprint centroid (BuildingGrammarCore's working unit, meters,
	// converted to UE centimeters here to match CellSizeCm/WorldLocationToCellCoord's units) into
	// CellSizeCm-sized square cells. Volumes with an empty OuterRing (no centroid to bucket by) are
	// skipped. Only cells with at least one volume appear in the result.
	static TMap<FIntPoint, TArray<FGrammarBuildingVolume>> BucketByCell(const TArray<FGrammarBuildingVolume>& Volumes, double CellSizeCm);
};
