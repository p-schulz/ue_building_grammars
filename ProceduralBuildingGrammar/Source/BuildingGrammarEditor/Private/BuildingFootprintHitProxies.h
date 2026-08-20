#pragma once

#include "CoreMinimal.h"
#include "HitProxies.h"
#include "BuildingInstancePoolActor.h" // TWeakObjectPtr<ABuildingInstancePoolActor> below needs the complete type, not just a forward declaration.

// Lets FBuildingPickEdMode::HandleClick (Move tool) identify exactly which vertex of which
// already-generated hand-placed building's footprint ring a viewport click landed on -- same role as
// FlexNetwork's HFlexNodeHitProxy, just indexing straight into
// ABuildingInstancePoolActor::SourceVolumes[VolumeIndex].Footprint.OuterRing[PointIndex] instead of a
// shared node graph (building footprints have no junctions/sharing between buildings).
struct HBuildingFootprintNodeHitProxy : public HHitProxy
{
	DECLARE_HIT_PROXY()

	TWeakObjectPtr<ABuildingInstancePoolActor> Pool;
	FString SourceName;
	int32 PointIndex = INDEX_NONE;

	HBuildingFootprintNodeHitProxy(ABuildingInstancePoolActor* InPool, const FString& InSourceName, int32 InPointIndex)
		: HHitProxy(HPP_UI)
		, Pool(InPool)
		, SourceName(InSourceName)
		, PointIndex(InPointIndex)
	{
	}
};
