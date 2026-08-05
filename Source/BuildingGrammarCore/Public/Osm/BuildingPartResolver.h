#pragma once

#include "CoreMinimal.h"
#include "Footprint/BuildingFootprint.h"
#include "Config/BuildingGrammarConfig.h"

// One footprint the grammar engine should actually generate a building/building-part volume for,
// after building-part parent/child resolution -- analogous to blender_adapter.py's SourceVolume.
struct BUILDINGGRAMMARCORE_API FGrammarBuildingVolume
{
	FBuildingFootprint Footprint;
	FString SourceName;
	TMap<FString, FString> VolumeTags;
	double MinHeight = 0.0;
	bool bIsBuildingPart = false;
	FString ParentSourceName;
};

// Port of blender_adapter.py's _source_volumes/_matching_parent. Footprints passed in must already
// be projected to a metric space (BuildingGrammarCore's working unit is meters -- see
// FLocalTangentPlaneProjection) -- BuildingPartMatchTolerance is a metric distance and
// containment/area comparisons are meaningless in raw lon/lat degrees.
class BUILDINGGRAMMARCORE_API FBuildingPartResolver
{
public:
	static TArray<FGrammarBuildingVolume> Resolve(const TArray<FBuildingFootprint>& ProjectedFootprints, const FBuildingGrammarConfig& Config);
};
