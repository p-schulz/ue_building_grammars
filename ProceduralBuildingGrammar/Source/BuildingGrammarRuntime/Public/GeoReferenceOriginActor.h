#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GeoReferenceOriginActor.generated.h"

// A level-wide anchor for the lat/lon projection origin every "generate/import" action in this
// plugin (and ProceduralRoads) uses -- see FLocalTangentPlaneProjection. Its whole purpose is
// stitching: without it, each import independently computes its own origin from its own file's
// bounds (FOsmDocument::GetBoundsCenter), so a second OSM extract imported into the same level lands
// centered on ITS OWN bounds rather than lining up with whatever was already generated. Once this
// actor exists, every import snaps to its stored OriginLatitude/OriginLongitude instead.
//
// Pure data holder, like ABuildingInstancePoolActor/ATreeInstancePoolActor's own "absolute-world-
// space content" convention -- this actor's own Transform is meaningless (moving it in the level does
// nothing; only OriginLatitude/OriginLongitude matter). Always spatially loaded (see the constructor)
// so it's reliably findable regardless of World Partition streaming state -- editor menu actions and
// PCG graph runs need to find it no matter where the camera/player currently is.
UCLASS(BlueprintType)
class BUILDINGGRAMMARRUNTIME_API AGeoReferenceOriginActor : public AActor
{
	GENERATED_BODY()

public:
	AGeoReferenceOriginActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geo Reference")
	double OriginLatitude = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geo Reference")
	double OriginLongitude = 0.0;

	// Finds the first AGeoReferenceOriginActor in World (TActorIterator -- the same idiom
	// BuildingGrammarEditorModule.cpp already uses for pool actors), logging a warning (not an error)
	// if more than one exists rather than picking arbitrarily without saying so. Returns false (Out
	// params left untouched) if none exists or World is null.
	static bool FindInWorld(UWorld* World, double& OutLatitude, double& OutLongitude);

	// Sets the level's geo reference to (Latitude, Longitude): updates the existing actor's values if
	// one is already present, or spawns a new one if not -- either way, never leaves more than one
	// instance behind. Returns the actor, or nullptr if World is null.
	static AGeoReferenceOriginActor* SetInWorld(UWorld* World, double Latitude, double Longitude);

	// The one function every import call site actually uses. FindInWorld first: if an existing
	// reference is found, FallbackLatitude/FallbackLongitude are ignored entirely and the existing
	// reference is returned -- this is what makes a second import snap to the first instead of
	// recentering on its own file. If none exists yet, spawns one from FallbackLatitude/
	// FallbackLongitude (the calling file's own GetBoundsCenter) and returns that, so the FIRST import
	// in a level establishes the anchor automatically with no extra steps. No-op (falls back to
	// Fallback* directly) if World is null.
	static void ResolveOrigin(UWorld* World, double FallbackLatitude, double FallbackLongitude, double& OutLatitude, double& OutLongitude);
};
