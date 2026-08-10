#pragma once

#include "CoreMinimal.h"
#include "Footprint/BuildingFootprint.h"
#include "Config/BuildingGrammarConfig.h"
#include "BuildingPartResolver.generated.h"

// One footprint the grammar engine should actually generate a building/building-part volume for,
// after building-part parent/child resolution -- analogous to blender_adapter.py's SourceVolume.
// Reflected (USTRUCT) so ABuildingInstancePoolActor can persist a cell's resolved volumes as a
// UPROPERTY (SourceVolumes) -- needed to regenerate a cell on demand for post-import per-building
// customization; see that class's header comment.
USTRUCT()
struct BUILDINGGRAMMARCORE_API FGrammarBuildingVolume
{
	GENERATED_BODY()

	UPROPERTY()
	FBuildingFootprint Footprint;

	UPROPERTY()
	FString SourceName;

	UPROPERTY()
	TMap<FString, FString> VolumeTags;

	UPROPERTY()
	double MinHeight = 0.0;

	UPROPERTY()
	bool bIsBuildingPart = false;

	UPROPERTY()
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
