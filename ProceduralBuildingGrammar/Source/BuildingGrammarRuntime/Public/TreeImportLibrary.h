#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TreeImportLibrary.generated.h"

class ATreeInstancePoolActor;

UCLASS()
class BUILDINGGRAMMARRUNTIME_API UTreeImportLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Parses GeoJsonFilePath (FGeoJsonTreeParser), keeps only trees whose Lat/Lon falls within
	// OsmFilePath's own region (FOsmDocument::GetBounds -- see its own comment for the <bounds>-
	// element-else-node-extent fallback), projects survivors into local-tangent-plane meters via
	// OriginLatitude/OriginLongitude (FLocalTangentPlaneProjection -- pass the SAME origin used for
	// OsmFilePath's own building generation, e.g. FOsmDocument::GetBoundsCenter, or the trees will
	// land in the wrong place relative to those buildings), and spawns one ATreeInstancePoolActor
	// containing every surviving tree, each with a random Z (yaw) rotation and a random uniform
	// scale (UTreeMeshSettings::MinScale/MaxScale, via PickScale -- RandomSeed drives both, so the
	// same seed always produces
	// the same layout, for reproducible regeneration) and its mesh picked via
	// UTreeMeshSettings::PickMeshForType (a tree whose type has no mesh configured yet in Project
	// Settings is silently skipped, not spawned with a placeholder). If bSnapToGround, each tree's
	// Z is the highest ECC_WorldStatic collision hit directly below its projected XY, falling back
	// to Z=0 if nothing is hit (e.g. no ground geometry at that location yet).
	//
	// Returns nullptr (and fills OutError) on a GeoJSON parse failure, an OSM parse failure, or an
	// OSM file with no usable region (no <bounds> and no nodes) -- otherwise returns a valid actor,
	// even if zero trees survived the filter/mesh-configured check (see NumInstances==0 in that
	// case, not a null return).
	UFUNCTION(BlueprintCallable, Category = "Building Grammar|Trees")
	static ATreeInstancePoolActor* ImportTreesFromGeoJson(
		UObject* WorldContextObject,
		const FString& GeoJsonFilePath,
		const FString& OsmFilePath,
		double OriginLatitude,
		double OriginLongitude,
		bool bSnapToGround,
		int32 RandomSeed,
		FString& OutError);
};
