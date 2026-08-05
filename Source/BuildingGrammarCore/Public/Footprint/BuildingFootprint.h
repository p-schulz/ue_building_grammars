#pragma once

#include "CoreMinimal.h"
#include "BuildingFootprint.generated.h"

// UHT does not allow a TArray value to itself be a container, so a hole ring (itself a
// TArray<FVector2D>) needs this one-field wrapper to be legal inside FBuildingFootprint::Holes
// (TArray<FGrammarRing>) -- same pattern as FGrammarStringList for TMap values.
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FGrammarRing
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	TArray<FVector2D> Points;
};

// One building footprint extracted by FBuildingFootprintAssembler, analogous to
// blender_adapter.py's SourceFootprint. OuterRing/Holes are in raw (Lon, Lat) degrees exactly as
// read from OSM -- NOT yet projected to local meters, NOT yet oriented CCW (winding is only a
// meaningful concept once projected to a planar Cartesian frame; do that via
// FLocalTangentPlaneProjection, then FGrammarGeometry2D::OrientFootprintCCW, before feeding a
// footprint to the grammar engine).
USTRUCT(BlueprintType)
struct BUILDINGGRAMMARCORE_API FBuildingFootprint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	int64 OsmId = 0;

	// "way" or "relation" -- provenance, used to build a stable SourceName for the grammar engine.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	FString SourceType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	TArray<FVector2D> OuterRing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	TArray<FGrammarRing> Holes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	TMap<FString, FString> Tags;

	// True if Tags carries a truthy building:part (and it's therefore a candidate to be matched
	// against a parent footprint rather than generated as a standalone building) -- mirrors
	// blender_adapter.py's _is_building_part_tags.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Grammar")
	bool bIsBuildingPart = false;

	FString StableSourceName() const
	{
		return FString::Printf(TEXT("%s/%lld"), *SourceType, OsmId);
	}
};
