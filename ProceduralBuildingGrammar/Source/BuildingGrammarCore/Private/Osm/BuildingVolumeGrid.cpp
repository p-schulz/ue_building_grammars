#include "Osm/BuildingVolumeGrid.h"
#include "Geometry/GrammarGeometry2D.h"

FIntPoint FBuildingVolumeGrid::WorldLocationToCellCoord(const FVector& WorldLocationCm, double CellSizeCm)
{
	return FIntPoint(
		static_cast<int32>(FMath::FloorToDouble(WorldLocationCm.X / CellSizeCm)),
		static_cast<int32>(FMath::FloorToDouble(WorldLocationCm.Y / CellSizeCm)));
}

FVector FBuildingVolumeGrid::CellCenter(const FIntPoint& CellCoord, double CellSizeCm)
{
	return FVector((CellCoord.X + 0.5) * CellSizeCm, (CellCoord.Y + 0.5) * CellSizeCm, 0.0);
}

TMap<FIntPoint, TArray<FGrammarBuildingVolume>> FBuildingVolumeGrid::BucketByCell(const TArray<FGrammarBuildingVolume>& Volumes, double CellSizeCm)
{
	// Volume.Footprint coordinates are in BuildingGrammarCore's working unit, meters (see
	// FLocalTangentPlaneProjection's header comment) -- the centroid used purely for cell bucketing
	// here is converted to centimeters to match CellSizeCm's UE-world-space units; the volumes
	// themselves are returned untouched, still in meters, exactly like every other
	// FBuildingGrammarEngine consumer.
	constexpr double MetersToUnrealUnits = 100.0;

	TMap<FIntPoint, TArray<FGrammarBuildingVolume>> Buckets;
	for (const FGrammarBuildingVolume& Volume : Volumes)
	{
		if (Volume.Footprint.OuterRing.Num() == 0)
		{
			continue;
		}
		const FVector2D Centroid = FGrammarGeometry2D::Centroid2D(Volume.Footprint.OuterRing);
		const FIntPoint CellCoord = WorldLocationToCellCoord(FVector(Centroid.X, Centroid.Y, 0.0) * MetersToUnrealUnits, CellSizeCm);
		Buckets.FindOrAdd(CellCoord).Add(Volume);
	}
	return Buckets;
}
