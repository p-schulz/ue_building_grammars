#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Config/BuildingGrammarConfig.h"
#include "BuildingGenerationLibrary.generated.h"

class ABuildingInstancePoolActor;

// The single end-to-end entry point tying every BuildingGrammarCore/Geometry/Runtime piece
// together: OSM file -> parsed document -> assembled footprints -> projected to UE-centimeter
// world space -> building-part parent/child resolution -> per-volume grammar generation -> spawned
// ABuildingActors + a shared ABuildingInstancePoolActor. Being a plain UBlueprintFunctionLibrary
// function (not a method on some Editor-only tool object) is what satisfies docs/PLAN.md's
// "runtime-capable core from day one" requirement concretely: an Editor Utility Widget can call
// this exact function from Blueprint, and so can game/runtime code -- there is no separate
// editor-only code path to keep in sync.
UCLASS()
class BUILDINGGRAMMARRUNTIME_API UBuildingGenerationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Kit-part meshes and per-style materials (docs/PLAN.md sections 4 and 6) don't exist yet, so
	// every placement's kit mesh currently resolves to null and every hero component stays on the
	// engine default material -- only facade walls and roof planes will actually be visible until
	// those phases land. Building-part min-height offsets (grammar:part:min_height) ARE applied.
	//
	// OriginLatitude/OriginLongitude set the projection origin (see FLocalTangentPlaneProjection);
	// pass the OSM extract's approximate center. If OutPool is null, a new
	// ABuildingInstancePoolActor is spawned and returned in it; pass an existing pool to add more
	// buildings into the same instance buckets. Returns the number of buildings/building-parts
	// successfully generated (footprints that fail -- excluded building value, degenerate geometry
	// -- are skipped, not fatal to the whole call).
	UFUNCTION(BlueprintCallable, Category = "Building Grammar")
	static int32 GenerateBuildingsFromOsmFile(
		const UObject* WorldContextObject,
		const FString& OsmFilePath,
		double OriginLatitude,
		double OriginLongitude,
		const FBuildingGrammarConfig& Config,
		UPARAM(ref) ABuildingInstancePoolActor*& OutPool);
};
